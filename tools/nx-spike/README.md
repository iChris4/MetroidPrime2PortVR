# Nintendo Switch (NX) feasibility spike

Question: can Aurora's renderer (Dawn/WebGPU) run on a homebrew Switch, so the port can target it?
This builds Dawn's GL/GLES backend with devkitA64 against devkitPro's Mesa (nouveau) and runs a
WGSL triangle as an `.nro`. Nothing here is wired into the game build yet.

    tools/nx-spike/build.sh [work dir]          # toolchain (no sudo), Dawn, patches, dawn_nx.nro
    tools/nx-spike/run_ryujinx.sh <nro> 60 [work] [shot-at-s...]   # headless Ryujinx, guest log, screenshots
    # <work>/build-app/gl_control.nro: plain-GLES control (no Dawn), also built by build.sh

`main` starts ~75 s after launch on a cold Ryujinx JIT cache, ~11 s once warm (cache in the data dir).

## Result (2026-11-08, Ryujinx 475615f, Mesa 20.1 nouveau, libnx 4.12, gcc 16.1)

- Dawn (pinned fork) cross-compiles for Switch with three small patches (`patches/`): abseil's
  newlib gaps (no `link.h`, `tm_gmtoff`, signals), Khronos `eglplatform.h` typedefs for `__SWITCH__`,
  and Dawn itself: an EGL `SwitchNWindow` swapchain case, optional context robustness, and a
  present-blit size fallback (Mesa's Switch EGL reports an NWindow surface as 0x0, so Dawn blitted
  into nothing: a black screen on hardware too). Upstream Dawn already knows the Switch platform
  and the `SurfaceSourceSwitchNWindow` surface.
- No `dlopen`: the app hands Dawn its `eglGetProcAddress` (`RequestAdapterOptionsGetGLProc`).
- Mesa's Switch EGL is 1.4 with no fence sync; `app/egl_sync_shim.cpp` provides
  `EGL_KHR_fence_sync` on GL fences (as the fork's EGLOnWGL does), so Dawn needs no queue change.
- GL adapters are Compatibility-level only (`featureLevel = Compatibility`, as Aurora's GL path).
- Ryujinx: adapter `NV120`, `OpenGL ES 3.2 Mesa 20.1.0-rc3`; surface configures (RGBA8Unorm) and
  presents at a steady 60 fps (Fifo) for 6000+ frames. Application mode, 3.2 GiB.
- Pixels verified in Ryujinx: the window shows Dawn's rotating vertex-coloured triangle
  (screenshots via `run_ryujinx.sh`, which runs Vulkan with `MESA_VK_WSI_DEBUG=sw` on Xvfb as the
  MP1 Remastered rig does), and a centre 256x256 readback matches the plain-GLES control exactly.
- Ryujinx quirk: a full-width (1280 px) readback of Mesa's RGBA8 keeps only the first quarter of
  each row (looks like the emulated copy counts pixels as bytes), for plain GLES too; narrow rects read
  back right, so the app dumps the centre block. One early run hit `GL_OUT_OF_MEMORY` creating the
  full-size readback texture; not seen since.

## Next steps toward a port

1. Hardware check, optional (copy the `.nro` to `sdmc:/switch/`, launch over a title for full memory;
   log goes to `sdmc:/dawn-nx.log`, centre block to `sdmc:/dawn-nx.ppm`): performance on nouveau.
2. Aurora: `AuroraDawnProvider` Switch branch (vendor build, GL+GLES on), a libnx window/input/audio
   layer in place of SDL3 (devkitPro ships SDL2 only), `SurfaceSourceSwitchNWindow` surface.
3. Game: devkitA64 build of the port (static, newlib), disc image from SD, MusyX on audren.
4. Performance: Dawn GL on nouveau vs. a deko3d backend, decided by numbers from step 1-3.
