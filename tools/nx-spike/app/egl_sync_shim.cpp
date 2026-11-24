// EGL_KHR_fence_sync on GL sync objects, for Mesa's Switch EGL, which has no EGL sync at all.
// Dawn's GL backend requires an EGL fence extension; it only creates and waits on fences with its
// context current, so GL fences (core in GLES 3.0 / GL 3.2) serve. Modeled on the Dawn fork's
// EGLOnWGL shim.
#include "egl_sync_shim.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using GLsyncHandle = struct __GLsync*;
using PFN_glFenceSync = GLsyncHandle (*)(unsigned condition, unsigned flags);
using PFN_glClientWaitSync = unsigned (*)(GLsyncHandle sync, unsigned flags, unsigned long long timeout);
using PFN_glDeleteSync = void (*)(GLsyncHandle sync);
using PFN_glGetSynciv = void (*)(GLsyncHandle sync, unsigned pname, int bufSize, int* length, int* values);

constexpr unsigned kGL_SYNC_GPU_COMMANDS_COMPLETE = 0x9117;
constexpr unsigned kGL_SYNC_FLUSH_COMMANDS_BIT = 0x1;
constexpr unsigned kGL_ALREADY_SIGNALED = 0x911A;
constexpr unsigned kGL_TIMEOUT_EXPIRED = 0x911B;
constexpr unsigned kGL_CONDITION_SATISFIED = 0x911C;
constexpr unsigned kGL_SYNC_STATUS = 0x9114;
constexpr int kGL_SIGNALED = 0x9119;

struct GLSyncProcs {
  PFN_glFenceSync fenceSync = nullptr;
  PFN_glClientWaitSync clientWaitSync = nullptr;
  PFN_glDeleteSync deleteSync = nullptr;
  PFN_glGetSynciv getSynciv = nullptr;
};

std::mutex gMutex;
std::unordered_map<EGLDisplay, std::string> gExtensions;
std::vector<GLsyncHandle> gPendingDeletes;  // destroyed with no context current
thread_local EGLint tError = EGL_SUCCESS;

const GLSyncProcs& Procs() {
  static const GLSyncProcs procs = [] {
    GLSyncProcs p;
    p.fenceSync = reinterpret_cast<PFN_glFenceSync>(eglGetProcAddress("glFenceSync"));
    p.clientWaitSync = reinterpret_cast<PFN_glClientWaitSync>(eglGetProcAddress("glClientWaitSync"));
    p.deleteSync = reinterpret_cast<PFN_glDeleteSync>(eglGetProcAddress("glDeleteSync"));
    p.getSynciv = reinterpret_cast<PFN_glGetSynciv>(eglGetProcAddress("glGetSynciv"));
    return p;
  }();
  return procs;
}

EGLBoolean Fail(EGLint error) {
  tError = error;
  return EGL_FALSE;
}

bool HasContext() { return eglGetCurrentContext() != EGL_NO_CONTEXT; }

void DrainPendingDeletes() {
  std::vector<GLsyncHandle> pending;
  {
    std::lock_guard<std::mutex> lock(gMutex);
    pending.swap(gPendingDeletes);
  }
  for (GLsyncHandle sync : pending) {
    Procs().deleteSync(sync);
  }
}

const char* EGLAPIENTRY ShimQueryString(EGLDisplay dpy, EGLint name) {
  const char* real = eglQueryString(dpy, name);
  if (name != EGL_EXTENSIONS || dpy == EGL_NO_DISPLAY || real == nullptr ||
      std::strstr(real, "EGL_KHR_fence_sync") != nullptr) {
    return real;
  }
  std::lock_guard<std::mutex> lock(gMutex);
  std::string& ext = gExtensions[dpy];
  ext = std::string(real) + " EGL_KHR_fence_sync";
  return ext.c_str();
}

EGLint EGLAPIENTRY ShimGetError() {
  const EGLint shimError = tError;
  tError = EGL_SUCCESS;
  const EGLint realError = eglGetError();
  return shimError != EGL_SUCCESS ? shimError : realError;
}

EGLSyncKHR EGLAPIENTRY ShimCreateSyncKHR(EGLDisplay, EGLenum type, const EGLint* attribs) {
  if (type != EGL_SYNC_FENCE_KHR || (attribs != nullptr && attribs[0] != EGL_NONE)) {
    Fail(type != EGL_SYNC_FENCE_KHR ? EGL_BAD_PARAMETER : EGL_BAD_ATTRIBUTE);
    return EGL_NO_SYNC_KHR;
  }
  if (!HasContext() || Procs().fenceSync == nullptr) {
    Fail(EGL_BAD_MATCH);
    return EGL_NO_SYNC_KHR;
  }
  DrainPendingDeletes();
  GLsyncHandle sync = Procs().fenceSync(kGL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  if (sync == nullptr) {
    Fail(EGL_BAD_ALLOC);
    return EGL_NO_SYNC_KHR;
  }
  tError = EGL_SUCCESS;
  return reinterpret_cast<EGLSyncKHR>(sync);
}

EGLBoolean EGLAPIENTRY ShimDestroySyncKHR(EGLDisplay, EGLSyncKHR sync) {
  if (sync == EGL_NO_SYNC_KHR) {
    return Fail(EGL_BAD_PARAMETER);
  }
  if (HasContext()) {
    Procs().deleteSync(reinterpret_cast<GLsyncHandle>(sync));
  } else {
    std::lock_guard<std::mutex> lock(gMutex);
    gPendingDeletes.push_back(reinterpret_cast<GLsyncHandle>(sync));
  }
  tError = EGL_SUCCESS;
  return EGL_TRUE;
}

EGLint EGLAPIENTRY ShimClientWaitSyncKHR(EGLDisplay, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout) {
  if (sync == EGL_NO_SYNC_KHR) {
    return Fail(EGL_BAD_PARAMETER);
  }
  if (!HasContext()) {
    return Fail(EGL_BAD_MATCH);
  }
  // EGL_FOREVER_KHR and GL_TIMEOUT_IGNORED are both ~0ull.
  const unsigned glFlags = (flags & EGL_SYNC_FLUSH_COMMANDS_BIT_KHR) ? kGL_SYNC_FLUSH_COMMANDS_BIT : 0;
  switch (Procs().clientWaitSync(reinterpret_cast<GLsyncHandle>(sync), glFlags, timeout)) {
    case kGL_ALREADY_SIGNALED:
    case kGL_CONDITION_SATISFIED:
      tError = EGL_SUCCESS;
      return EGL_CONDITION_SATISFIED_KHR;
    case kGL_TIMEOUT_EXPIRED:
      tError = EGL_SUCCESS;
      return EGL_TIMEOUT_EXPIRED_KHR;
    default:
      return Fail(EGL_BAD_ACCESS);
  }
}

EGLBoolean EGLAPIENTRY ShimGetSyncAttribKHR(EGLDisplay, EGLSyncKHR sync, EGLint attribute, EGLint* value) {
  if (sync == EGL_NO_SYNC_KHR || value == nullptr) {
    return Fail(EGL_BAD_PARAMETER);
  }
  switch (attribute) {
  case EGL_SYNC_TYPE_KHR:
    *value = EGL_SYNC_FENCE_KHR;
    break;
  case EGL_SYNC_CONDITION_KHR:
    *value = EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR;
    break;
  case EGL_SYNC_STATUS_KHR: {
    if (!HasContext()) {
      return Fail(EGL_BAD_MATCH);
    }
    int status = 0;
    Procs().getSynciv(reinterpret_cast<GLsyncHandle>(sync), kGL_SYNC_STATUS, 1, nullptr, &status);
    *value = status == kGL_SIGNALED ? EGL_SIGNALED_KHR : EGL_UNSIGNALED_KHR;
    break;
  }
  default:
    return Fail(EGL_BAD_ATTRIBUTE);
  }
  tError = EGL_SUCCESS;
  return EGL_TRUE;
}

#define SHIM_PROC(name, fn) {name, reinterpret_cast<EGLGetProcResult>(fn)}
using EGLGetProcResult = void (*)();
struct ShimProc {
  const char* name;
  EGLGetProcResult proc;
};
const ShimProc kShimProcs[] = {
    SHIM_PROC("eglQueryString", ShimQueryString),
    SHIM_PROC("eglGetError", ShimGetError),
    SHIM_PROC("eglCreateSyncKHR", ShimCreateSyncKHR),
    SHIM_PROC("eglDestroySyncKHR", ShimDestroySyncKHR),
    SHIM_PROC("eglClientWaitSyncKHR", ShimClientWaitSyncKHR),
    SHIM_PROC("eglGetSyncAttribKHR", ShimGetSyncAttribKHR),
};
#undef SHIM_PROC

}  // namespace

void (*EGLSyncShimGetProc(const char* name))() {
  // Mesa's Switch EGL has no fence sync; the GL-fence versions are correct even where it does.
  for (const ShimProc& p : kShimProcs) {
    if (std::strcmp(p.name, name) == 0) {
      return p.proc;
    }
  }
  return eglGetProcAddress(name);
}
