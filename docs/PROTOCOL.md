# Protocol behavior

This is an implementation guide for the receiver. AirPlay protocol details are
not fully published by Apple; behavior can differ between sender versions and
apps. For cryptographic and FairPlay asset provenance, see
[FAIRPLAY.md](FAIRPLAY.md).

## Discovery and control connection

The receiver advertises `_airplay._tcp` and `_raop._tcp` DNS-SD services on the
local network. The control server listens on TCP port 7000. Discovery and
connection can fail when multicast is blocked by a guest network, firewall,
VPN or access-point isolation. A sender and receiver need a route between them.

The control channel uses RTSP-like request framing. `rtsp_parser.*` reads the
request line, headers and optional body; `routes.*` handles capability queries,
pairing, stream setup, playback/session controls, metadata and teardown. Apple
binary property lists are parsed or generated through libplist. Stream and
session state is represented in `streams.*` and `client_session.h`.

## Pairing and encrypted payloads

Pair-verify uses the receiver identity and ephemeral key agreement to verify a
sender and derive session keys. OpenSSL provides Ed25519, X25519, AES and hash
primitives. FairPlay setup uses the bundled playfair code and reply data
documented in [FAIRPLAY.md](FAIRPLAY.md). The persistent receiver identity is
generated locally; keep `identity.key` private.

After setup, the mirror path decrypts protected payload data before parsing the
video framing and passing compressed frames to FFmpeg. These mechanisms enable
normal screen mirroring. They do not provide a DRM streaming license.

## Media paths

### Screen mirroring

The receiver advertises H.264 and HEVC mirror support. Media arrives through
negotiated streams, is parsed by the mirror listener and H.264/NAL helpers, then
decoded by FFmpeg. The software path converts frames for SDL display. The
Windows hardware path can use D3D11 video decoding and GPU surfaces for display
and recording. Actual sender resolution is chosen during negotiation and can
differ from the receiver's advertised maximum dimensions.

### RAOP audio

Audio uses a separate RAOP stream. The receiver supports AAC-ELD and ALAC input,
stereo output through SDL, and sender-provided volume and track metadata. Some
remote playback controls use the sender's DACP service. Audio playback timing
against RTP/NTP timestamps is still incomplete; the current output path should
not be treated as a synchronized multi-room audio implementation.

### Recording

The recorder writes MP4 with H.264 or HEVC video and a stereo AAC track when
RAOP audio is present. Hardware encoders are used when available and selected;
the automatic mode can choose a CPU path. GPU residency depends on the FFmpeg
build and encoder. If no audio stream is received, the recording has no audio
track. Recording is local and does not capture microphone or system audio.

### Experimental direct HLS

`--hls-proxy-playback` opts into an experimental direct media URL/HLS proxy
path. It uses FFmpeg/libavformat by default or GStreamer when selected at build
time. Sender apps, redirect behavior and stream formats vary, so this is not
the primary mirroring path and may buffer or fail. DRM-protected streaming is
unsupported.

## Support boundaries

- One active sender session is supported at a time.
- TCP 7000 is the control port; media streams use negotiated ports.
- Wi-Fi discovery depends on local multicast and firewall configuration.
- The optional USB route is Apple Personal Hotspot networking through Windows
  USB Ethernet drivers. This project does not implement native USB video
  capture or install Apple drivers.
- Compatibility notes and known sender behavior are maintained in the README
  and release notes. Historical design notes are intentionally not part of this
  current protocol reference.
