cmake_minimum_required(VERSION 3.20)
project(dawn_nx CXX C)

set(DAWN_SRC "" CACHE PATH "Patched Dawn source tree")
set(DAWN_BUILD "" CACHE PATH "Dawn build tree (webgpu_dawn built)")

add_executable(dawn_nx main.cpp egl_sync_shim.cpp)
set_target_properties(dawn_nx PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(dawn_nx PRIVATE "$ENV{DEVKITPRO}/portlibs/switch/include" "${DAWN_SRC}/include" "${DAWN_BUILD}/gen/include")
target_link_libraries(dawn_nx PRIVATE
  "${DAWN_BUILD}/src/dawn/native/libwebgpu_dawn.a"
  "$ENV{DEVKITPRO}/portlibs/switch/lib/libEGL.a" "$ENV{DEVKITPRO}/portlibs/switch/lib/libglapi.a"
  "$ENV{DEVKITPRO}/portlibs/switch/lib/libdrm_nouveau.a")
nx_generate_nacp(dawn_nx.nacp NAME "Dawn NX spike" AUTHOR "MP2 port" VERSION "0.0.1")
nx_create_nro(dawn_nx NACP dawn_nx.nacp)

add_executable(gl_control gl_control.cpp)
set_target_properties(gl_control PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(gl_control PRIVATE "$ENV{DEVKITPRO}/portlibs/switch/include")
target_link_libraries(gl_control PRIVATE
  "$ENV{DEVKITPRO}/portlibs/switch/lib/libEGL.a" "$ENV{DEVKITPRO}/portlibs/switch/lib/libGLESv2.a"
  "$ENV{DEVKITPRO}/portlibs/switch/lib/libglapi.a" "$ENV{DEVKITPRO}/portlibs/switch/lib/libdrm_nouveau.a")
nx_generate_nacp(gl_control.nacp NAME "GL control" AUTHOR "MP2 port" VERSION "0.0.1")
nx_create_nro(gl_control NACP gl_control.nacp)
