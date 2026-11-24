// Control for the Dawn spike: the same clear + triangle in plain GLES 3 on Mesa, read back with
// glReadPixels (direct and through a pixel-pack buffer, as Dawn's CopyTextureToBuffer does).
// If this reads back wrong under Ryujinx too, the Dawn readback result is the emulator's.
#include <switch.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

static void Log(const char* fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  svcOutputDebugString(buf, n < 0 ? 0 : (n < int(sizeof(buf)) ? n : int(sizeof(buf)) - 1));
}

static GLuint Compile(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    Log("shader: %s\n", log);
  }
  return s;
}

static void Summarize(const char* tag, const std::vector<uint8_t>& px, int w, int h) {
  int clear = 0, tri = 0, zero = 0;
  for (int i = 0; i < w * h; ++i) {
    const uint8_t* p = &px[i * 4];
    if (p[0] == 0 && p[1] == 0 && p[2] == 0) ++zero;
    else if (p[0] == 13 && p[1] == 13 && p[2] == 51) ++clear;
    else ++tri;
  }
  const uint8_t* c = &px[(h / 2 * w + w / 2) * 4];
  Log("%s: clear %d, triangle %d, zero %d; center %u,%u,%u\n", tag, clear, tri, zero, c[0], c[1], c[2]);
}

int main(int, char**) {
  constexpr int W = 1280, H = 720;
  EGLDisplay d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  eglInitialize(d, nullptr, nullptr);
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint cfgAttr[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                            EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
  EGLConfig cfg;
  EGLint n = 0;
  eglChooseConfig(d, cfgAttr, &cfg, 1, &n);
  EGLSurface surf = eglCreateWindowSurface(d, cfg, nwindowGetDefault(), nullptr);
  const EGLint ctxAttr[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE};
  EGLContext ctx = eglCreateContext(d, cfg, EGL_NO_CONTEXT, ctxAttr);
  eglMakeCurrent(d, surf, surf, ctx);
  Log("gl-control: %s / %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));

  GLuint prog = glCreateProgram();
  glAttachShader(prog, Compile(GL_VERTEX_SHADER, R"(#version 300 es
    const vec2 p[3] = vec2[3](vec2(0.0, 0.6), vec2(-0.6, -0.5), vec2(0.6, -0.5));
    void main() { gl_Position = vec4(p[gl_VertexID] * vec2(0.5625, 1.0), 0.0, 1.0); })"));
  glAttachShader(prog, Compile(GL_FRAGMENT_SHADER, R"(#version 300 es
    precision mediump float; out vec4 o; void main() { o = vec4(1.0, 0.5, 0.0, 1.0); })"));
  glLinkProgram(prog);
  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  GLuint tex, fbo;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, W, H);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  glViewport(0, 0, W, H);
  glClearColor(13 / 255.f, 13 / 255.f, 51 / 255.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(prog);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  std::vector<uint8_t> px(W * H * 4);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
  Summarize("readpixels", px, W, H);

  // A 256x256 rect at the centre: fits inside the first quarter of a row's bytes.
  {
    std::vector<uint8_t> sub(256 * 256 * 4);
    glReadPixels(W / 2 - 128, H / 2 - 128, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, sub.data());
    Summarize("readpixels centre 256x256", sub, 256, 256);
  }

  GLuint pbo;
  glGenBuffers(1, &pbo);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
  glBufferData(GL_PIXEL_PACK_BUFFER, px.size(), nullptr, GL_STREAM_READ);
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  const void* mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, px.size(), GL_MAP_READ_BIT);
  if (mapped) {
    std::memcpy(px.data(), mapped, px.size());
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    Summarize("pbo", px, W, H);
  } else {
    Log("pbo map failed 0x%x\n", glGetError());
  }
  Log("gl error 0x%x\n", glGetError());

  // Present: 20 s drawn straight into the window, then 20 s drawn into the FBO and blitted
  // y-flipped with GL_LINEAR, as Dawn's EGL swapchain presents.
  for (int i = 0; i < 60 * 40 && appletMainLoop(); ++i) {
    const bool blit = i >= 60 * 20;
    if (i == 60 * 20) {
      Log("gl-control: switching to blit present\n");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, blit ? fbo : 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (blit) {
      glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
      glBlitFramebuffer(0, 0, W, H, 0, H, W, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    eglSwapBuffers(d, surf);
  }
  Log("gl error 0x%x\n", glGetError());
  Log("gl-control done\n");
  eglMakeCurrent(d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(d, ctx);
  eglDestroySurface(d, surf);
  eglTerminate(d);
  return 0;
}
