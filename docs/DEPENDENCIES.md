# Dependencies and licenses

The application is C++17. CMake configures the build; vcpkg installs the
Windows dependencies from the baseline pinned in `vcpkg.json`.

| Component | Required | Use in this project | License / source |
|---|---:|---|---|
| libplist | Yes | Parse and generate Apple binary property lists used by control messages | LGPL-2.1-or-later; [libimobiledevice/libplist](https://github.com/libimobiledevice/libplist) |
| OpenSSL Crypto | Yes | Ed25519, X25519, AES and SHA-512 cryptographic operations | Apache-2.0 for OpenSSL 3.x; [openssl.org](https://www.openssl.org/) |
| FFmpeg | Yes | H.264/HEVC and AAC decoding, media formats, scaling and recording | LGPL components required; optional encoder availability varies by build; [ffmpeg.org](https://ffmpeg.org/) |
| SDL2 | Yes | Window, renderer, input/event loop and audio output | zlib; [libsdl.org](https://www.libsdl.org/) |
| SDL2_ttf | Yes | Text in track metadata and playback overlays | zlib; [SDL_ttf](https://github.com/libsdl-org/SDL_ttf) |
| Dear ImGui | Yes (fetched by CMake) | Options panel, toolbar, status, dialogs and sidebar | MIT; pinned in `CMakeLists.txt`; [ocornut/imgui](https://github.com/ocornut/imgui) |
| GStreamer | Optional | Alternative backend for experimental HLS playback | LGPL-2.1-or-later; [gstreamer.freedesktop.org](https://gstreamer.freedesktop.org/) |
| Avahi | Optional, Linux | DNS-SD/mDNS advertisements when available | LGPL-2.1-or-later; [avahi.org](https://www.avahi.org/) |
| playfair | Bundled | FairPlay handshake routine | GPL-3.0; [EstebanKubata/playfair](https://github.com/EstebanKubata/playfair) |
| Windows SDK APIs | Windows | Winsock, DNS-SD, D3D11, USB enumeration, shell integration | Microsoft Windows SDK terms |

The Windows vcpkg manifest currently requests FFmpeg features `avcodec`,
`avformat`, `swscale` and `nvcodec`. Other hardware encoders can be used when
present in the FFmpeg build and supported by the machine. The CPU H.264 and
HEVC encoder choices depend on whether the FFmpeg build includes the relevant
encoders.

## Redistribution notes

The root [`LICENSE`](../LICENSE) applies to project code. Component licenses
remain applicable to their components. The repository also contains FairPlay
reply bytes derived from Apple TV output; the project does not claim ownership
of those bytes. Read [`../THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md)
and [FAIRPLAY.md](FAIRPLAY.md) for exact provenance and caveats before
redistributing binaries or source bundles.

AirPlay and FairPlay are Apple technologies. This project is an independent
open-source implementation and is not affiliated with or endorsed by Apple.
