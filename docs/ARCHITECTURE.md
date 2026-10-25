# Architecture

This document describes the implementation in this repository. AirPlay is a
reverse-engineered protocol; wire details below reflect this receiver and its
open-source references, not an Apple specification.

## Runtime flow

```text
Apple sender
  ├─ DNS-SD discovery ──> mDNS advertisements (_airplay._tcp, _raop._tcp)
  ├─ RTSP-like control ─> TCP server (port 7000)
  │                         ├─ pairing and FairPlay setup
  │                         ├─ stream setup and route dispatch
  │                         └─ session / metadata / remote-control state
  ├─ mirror video ──────> decrypt → NAL framing → FFmpeg decode → SDL/D3D11
  └─ RAOP audio ────────> UDP receiver → AAC-ELD/ALAC decode → SDL audio
                                                        └→ AAC/MP4 recording
```

The application starts a single receiver process and supports one active sender
session at a time. `main.cpp` loads settings and the persistent receiver
identity, initializes networking, starts the renderer, AirPlay server and
service advertisements, then coordinates shutdown.

## Source map

| Area | Files | Responsibility |
|---|---|---|
| Application | `src/main.cpp`, `src/settings.*`, `src/log.*` | Startup, command-line options, saved preferences and logging |
| Transport | `src/net/` | Winsock or POSIX sockets, TCP listener and address helpers |
| Discovery | `src/mdns/` | Advertise AirPlay and RAOP with Windows DNS-SD APIs; Avahi is available on Linux builds |
| Control protocol | `src/airplay/rtsp_parser.*`, `routes.*`, `server.*` | Parse RTSP-like requests, route methods and manage receiver sessions |
| Session/media negotiation | `src/airplay/streams.*`, `sdp.*`, `info_plist.*`, `airplay2_setup.*` | Advertised capabilities, SDP and legacy/AirPlay 2 stream setup |
| Pairing and cryptography | `src/crypto/`, `src/airplay/fcup.*`, `third_party/playfair/` | Identity keys, pair verification, FairPlay setup and payload decryption |
| Video | `src/airplay/mirror_listener.*`, `h264_nal.*`, `h264_sps.*`, `src/video/` | Receive mirror packets, parse H.264 framing, decode, render and record video |
| Audio | `src/audio/`, `src/airplay/ntp_client.*` | Receive RAOP audio, decode AAC, output audio and maintain timing support |
| Direct media | `src/airplay/hls_*`, `src/video/hls_player*` | Experimental opt-in HLS proxy and playback backend |
| Windows integration | `src/platform/`, `src/usb/`, `resources/` | Single-instance guard, USB device hints, executable icon and native APIs |
| UI | `src/video/video_renderer.*` | SDL window, controls, status/metadata overlays and Dear ImGui options |
| Developer tools | `tools/` | Protocol, network lifecycle and GPU integration test programs |

## Connection and media lifecycle

1. DNS-SD publishes `_airplay._tcp` and `_raop._tcp` entries so senders can
   discover the receiver. The receiver listens for control traffic on TCP 7000;
   negotiated media streams use additional sockets and ports.
2. The RTSP-like server parses each request and dispatches it to handlers in
   `routes.cpp`. Pairing establishes sender trust and session crypto; FairPlay
   setup provides the key material needed by the mirror path.
3. Stream setup creates per-session state. The mirror listener receives framed
   media, decrypts protected payloads and passes compressed video to FFmpeg.
4. The video renderer presents decoded frames. When enabled and supported,
   D3D11 hardware decoding and shared textures keep frames on the GPU through
   display and can feed the hardware recording path.
5. RAOP audio arrives separately. The audio receiver and decoder send samples
   to SDL's audio output and, when recording, to the MP4 audio stream.
6. Disconnect and shutdown stop listeners, workers, playback and advertisements
   so a later session can reconnect cleanly.

## Crypto and receiver identity

OpenSSL supplies Ed25519 identity signatures, X25519 key agreement, AES-CTR and
SHA-512 operations used by the handshake and media path. `identity.key` is
created for the local installation and stored beside the executable; it should
not be committed or shared. The application also saves user preferences under
`%APPDATA%/AirPlay-Windows/settings.ini` on Windows.

The FairPlay implementation and bundled reply data have separate provenance
and licensing caveats. Read [FAIRPLAY.md](FAIRPLAY.md) and
[`../THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) before redistributing
the project.

## Build choices and limits

- `USE_GSTREAMER_HLS=AUTO` selects GStreamer when detected; `ON` requires it and
  `OFF` selects FFmpeg/libavformat. This affects only experimental HLS playback.
- `BUILD_NETWORK_TESTS` enables socket lifecycle tests.
- `BUILD_GPU_TESTS` enables Windows D3D11/FFmpeg integration tests.
- H.264 and HEVC mirror decoding, GPU display, audio output and MP4 recording
  are separate paths; the GPU decoder setting does not select the HLS backend.
- DRM-protected direct streaming is not supported. HLS compatibility depends on
  the sender application and stream format.

See [DEVELOPMENT.md](DEVELOPMENT.md) for building and
[PROTOCOL.md](PROTOCOL.md) for protocol behavior and support boundaries.
