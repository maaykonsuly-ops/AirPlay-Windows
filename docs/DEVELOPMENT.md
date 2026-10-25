# Development guide

## Requirements

The primary supported build is Windows x64 with Visual Studio 2022 or 2026
(Desktop development with C++ workload), a Windows SDK, CMake 3.21 or newer,
Git, and network access for first-time dependency setup. The vcpkg baseline is
pinned in `vcpkg.json` and is used by GitHub Actions.

The project can also use system packages through pkg-config on Linux. Required
development packages are libplist, OpenSSL, FFmpeg development libraries
(`libavcodec`, `libavformat`, `libavutil`, `libswscale`), SDL2 and SDL2_ttf.
Linux discovery uses Avahi if available. Windows uses native DNS-SD APIs. GPU
decode/display and the GPU integration suite require Windows and compatible
D3D11 hardware and drivers.

## Configure and build (Windows)

```powershell
git clone https://github.com/maaykonsuly-ops/AirPlay-Windows.git
cd AirPlay-Windows
git clone https://github.com/microsoft/vcpkg.git build/vcpkg
$baseline = (Get-Content vcpkg.json | ConvertFrom-Json).'builtin-baseline'
git -C build/vcpkg checkout $baseline
.\build\vcpkg\bootstrap-vcpkg.bat -disableMetrics

cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="$PWD/build/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\airplay-windows.exe
```

Use `-G "Visual Studio 18 2026"` with a CMake release that supports that
generator if building with Visual Studio 2026. First configure downloads and
builds the dependencies and can take several minutes. CMake copies linked
runtime DLLs beside the Windows executable after building.

## Optional build settings

| CMake option | Default | Effect |
|---|---:|---|
| `USE_GSTREAMER_HLS` | `AUTO` | `AUTO`, `ON`, or `OFF` for the experimental HLS player backend |
| `GSTREAMER_ROOT` | unset | GStreamer MSVC installation used to locate pkg-config metadata on Windows |
| `BUILD_NETWORK_TESTS` | `OFF` | Build socket and service lifecycle regression tests |
| `BUILD_GPU_TESTS` | `OFF` | Build the Windows D3D11 GPU integration test |

Example: add `-DBUILD_NETWORK_TESTS=ON` to the configure command to enable the
network lifecycle test executable. GPU integration tests additionally need a
compatible GPU and suitable FFmpeg encoders/decoders. Their fixtures validate
local media processing; they do not measure end-to-end AirPlay latency.

## Protocol checks

The Python harness emulates a sender for pairing, signature rejection,
concurrent connection handling, FairPlay setup framing and the legacy audio
handshake. Start the receiver first; it expects TCP port 7000 on localhost.

```powershell
python -m pip install cryptography
python tools/test_pair_verify.py
```

The network tests are run through CTest when enabled:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

See the [README](../README.md#tests) for GPU test commands and hardware
requirements. GitHub Actions runs a Windows build and protocol suite, while
hardware-dependent GPU checks are documented for local execution.

## Repository hygiene

Build output, IDE state, installed dependencies, executables and receiver
identity files are local artifacts. Keep them out of commits. Check
`THIRD_PARTY_NOTICES.md` whenever adding, replacing or redistributing a
dependency or bundled protocol asset.
