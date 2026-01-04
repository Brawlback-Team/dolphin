{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = with pkgs; [
    # Build tools
    cmake gcc git pkg-config ninja

    # Qt6
    qt6.qtbase
    qt6.qtsvg
    qt6.wrapQtAppsHook

    # Dolphin dependencies
    libxkbcommon xorg.libXrandr xorg.libXi xorg.libX11
    SDL2 libevdev miniupnpc lzo alsa-lib pulseaudio
    bluez ffmpeg libusb1 pugixml cubeb libspng
    hidapi sfml zstd lz4 xxHash mbedtls curl

    # LLVM for JIT
    llvmPackages.llvm

    # Python for brawlback-asm
    (python3.withPackages (ps: with ps; [ click requests rich ]))

    # SD card tools
    mtools
    dosfstools

    # Wine for GCTRealMate
    wineWowPackages.stable
  ];

  # Use bundled fmt (system fmt v12+ has breaking changes)
  CMAKE_ARGS = "-DUSE_SYSTEM_FMT=OFF";

  # Wayland workaround - force X11/XCB
  QT_QPA_PLATFORM = "xcb";

  shellHook = ''
    echo "Brawlback development shell"
    echo "Build: mkdir -p build && cd build && cmake .. -DLINUX_LOCAL_DEV=ON -DUSE_SYSTEM_FMT=OFF && cmake --build . -j$(nproc)"
    echo "Setup: cd build/Binaries && ln -sf ../../Data/Sys Sys"
    echo "Run:   QT_QPA_PLATFORM=xcb ./build/Binaries/dolphin-emu"
  '';
}
