# Building for Linux
Documentation on how to build Sudachi for Linux

## Requirements
> [!NOTE]
> Below is a list of development tools, software, etc. required for building Sudachi for Linux

### Development Packages
```
sudo apt install autoconf cmake g++-11 gcc-11 git glslang-tools libasound2 libboost-context-dev libglu1-mesa-dev libhidapi-dev libpulse-dev libtool libudev-dev libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-xinerama0 libxcb-xkb1 libxext-dev libxkbcommon-x11-0 mesa-common-dev nasm ninja-build qtbase5-dev qtbase5-private-dev qtwebengine5-dev qtmultimedia5-dev libmbedtls-dev catch2 libfmt-dev liblz4-dev nlohmann-json3-dev libzstd-dev libssl-dev libavfilter-dev libavcodec-dev libswscale-dev libunistring-dev libaom-dev libdav1d-dev autoconf automake build-essential cmake git-core libass-dev libfreetype6-dev libgnutls28-dev libmp3lame-dev libsdl2-dev libtool libva-dev libvdpau-dev libvorbis-dev libxcb1-dev libxcb-shm0-dev libxcb-xfixes0-dev meson ninja-build pkg-config texinfo wget yasm zlib1g-dev nasm libx264-dev libx265-dev libnuma-dev libvpx-dev libfdk-aac-dev libopus-dev libsvtav1-dev libsvtav1enc-dev libsvtav1dec-dev libdav1d-dev
```

## Steps
### Step I - Cloning
```sh
cd path/to/directory
git clone --recursive https://github.com/sudachi-emu/sudachi
```

### Step II - Building
#### Step II (a)
```sh
mkdir build; cd build
cmake .. -DSUDACHI_USE_BUNDLED_VCPKG=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-march=x86-64-v2" -DCMAKE_CXX_COMPILER=/usr/lib/ccache/g++ -DCMAKE_C_COMPILER=/usr/lib/ccache/gcc -DENABLE_COMPATIBILITY_LIST_DOWNLOAD=ON -DENABLE_QT_TRANSLATION=OFF -DUSE_DISCORD_PRESENCE=ON -DSUDACHI_ENABLE_COMPATIBILITY_REPORTING=${ENABLE_COMPATIBILITY_REPORTING:-"OFF"} -DSUDACHI_USE_BUNDLED_FFMPEG=ON -DSUDACHI_ENABLE_LTO=ON -DSUDACHI_CRASH_DUMPS=OFF -DCMAKE_INSTALL_PREFIX="/usr" -DSUDACHI_ROOM=OFF -GNinja
```

#### Step II (b)
```sh
cmake --build . --config Release
```