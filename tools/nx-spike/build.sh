#!/usr/bin/env bash
# Nintendo Switch feasibility spike: Dawn (GL/GLES) on Mesa/nouveau, built as a homebrew .nro.
# Usage: tools/nx-spike/build.sh [work dir]   (default /tmp/opencode/nx-spike)
# Env: DEVKITPRO (default ../toolchains/devkitpro-root/opt/devkitpro, installed here if missing),
#      T (cmake/ninja dir, default ../MetroidPrimePort/build/review-tools/bin).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
repo=$(cd "$here/../.." && pwd)
work=${1:-/tmp/opencode/nx-spike}
T=${T:-$repo/../MetroidPrimePort/build/review-tools/bin}
dkp_root=$repo/../toolchains/devkitpro-root
export DEVKITPRO=${DEVKITPRO:-$dkp_root/opt/devkitpro}
dawn_ref=$(sed -nE 's/.*AURORA_DAWN_REF "([0-9a-f]+)".*/\1/p' "$repo/extern/aurora/cmake/AuroraDependencyVersions.cmake")
mkdir -p "$work"

# devkitPro packages, unpacked without pacman/sudo. pkg.devkitpro.org's CDN rejects curl's UA.
if [[ ! -x $DEVKITPRO/devkitA64/bin/aarch64-none-elf-g++ ]]; then
  ua="pacman/7.0.0 (Linux x86_64) libalpm/15.0.0"
  db=$work/dkp-db
  mkdir -p "$db/libs" "$db/linux" "$work/dkp-pkgs" "$dkp_root"
  curl -sfL -A "$ua" -o "$db/libs.db" https://pkg.devkitpro.org/packages/dkp-libs.db
  curl -sfL -A "$ua" -o "$db/linux.db" https://pkg.devkitpro.org/packages/linux/x86_64/dkp-linux.db
  tar -xf "$db/libs.db" -C "$db/libs"
  tar -xf "$db/linux.db" -C "$db/linux"
  for name in libnx switch-mesa switch-libdrm_nouveau switch-cmake devkita64-cmake devkita64-newlib \
      devkita64-rules dkp-cmake-common-utils dkp-toolchain-vars switch-pkg-config switch-zlib \
      linux/devkita64-gcc linux/devkita64-binutils linux/switch-tools linux/general-tools linux/uam; do
    sub=libs repo_path=packages
    [[ $name == linux/* ]] && { sub=linux repo_path=packages/linux/x86_64; name=${name#linux/}; }
    desc=$(ls -d "$db/$sub/$name"-[0-9]* | head -1)/desc
    file=$(awk '/^%FILENAME%/{getline; print}' "$desc")
    curl -sfL -A "$ua" -o "$work/dkp-pkgs/$file" "https://pkg.devkitpro.org/$repo_path/$file"
    tar -xf "$work/dkp-pkgs/$file" -C "$dkp_root" --exclude=.PKGINFO --exclude=.MTREE --exclude=.BUILDINFO \
      --exclude=.INSTALL 2>/dev/null
  done
fi

# Dawn fork at Aurora's pin, its deps, and the Switch patches.
if [[ ! -d $work/src/dawn ]]; then
  mkdir -p "$work/src"
  curl -sfL "https://github.com/Odrannnn/dawn/archive/$dawn_ref.tar.gz" | tar -xz -C "$work/src"
  mv "$work/src/dawn-$dawn_ref" "$work/src/dawn"
  (cd "$work/src/dawn" && python3 tools/fetch_dawn_dependencies.py --shallow)
  patch -d "$work/src/dawn" -p1 < "$here/patches/dawn.patch"
  patch -d "$work/src/dawn/third_party/abseil-cpp" -p1 < "$here/patches/abseil.patch"
  patch -d "$work/src/dawn/third_party/EGL-Registry/src" -p1 < "$here/patches/egl-registry.patch"
fi

"$T/cmake" -S "$work/src/dawn" -B "$work/build-dawn" -G Ninja -DCMAKE_MAKE_PROGRAM="$T/ninja" \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC -DDAWN_ENABLE_VULKAN=OFF -DDAWN_ENABLE_D3D11=OFF \
  -DDAWN_ENABLE_D3D12=OFF -DDAWN_ENABLE_METAL=OFF -DDAWN_ENABLE_DESKTOP_GL=ON -DDAWN_ENABLE_OPENGLES=ON \
  -DDAWN_ENABLE_NULL=ON -DDAWN_USE_X11=OFF -DDAWN_USE_WAYLAND=OFF -DDAWN_USE_GLFW=OFF \
  -DDAWN_SUPPORTS_GLFW_FOR_WINDOWING=OFF -DDAWN_BUILD_SAMPLES=OFF -DDAWN_BUILD_TESTS=OFF \
  -DDAWN_BUILD_BENCHMARKS=OFF -DDAWN_BUILD_PROTOBUF=OFF -DDAWN_ENABLE_SPIRV_VALIDATION=OFF \
  -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_CMD_TOOLS=OFF -DTINT_BUILD_SPV_READER=OFF -DTINT_BUILD_SPV_WRITER=OFF \
  -DTINT_BUILD_MSL_WRITER=OFF -DTINT_BUILD_HLSL_WRITER=OFF -DTINT_BUILD_GLSL_WRITER=ON \
  -DTINT_BUILD_GLSL_VALIDATOR=OFF -DTINT_BUILD_WGSL_READER=ON -DTINT_BUILD_WGSL_WRITER=ON >/dev/null
"$T/ninja" -C "$work/build-dawn" webgpu_dawn

"$T/cmake" -S "$here/app" -B "$work/build-app" -G Ninja -DCMAKE_MAKE_PROGRAM="$T/ninja" \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" -DCMAKE_BUILD_TYPE=Release \
  -DDAWN_SRC="$work/src/dawn" -DDAWN_BUILD="$work/build-dawn" >/dev/null
"$T/ninja" -C "$work/build-app"
echo "$work/build-app/dawn_nx.nro"
