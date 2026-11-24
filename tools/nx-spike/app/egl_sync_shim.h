#pragma once

// eglGetProcAddress for Dawn's RequestAdapterOptionsGetGLProc: Mesa's procs, plus GL-fence-backed
// EGL_KHR_fence_sync when the display lacks it.
void (*EGLSyncShimGetProc(const char* name))();
