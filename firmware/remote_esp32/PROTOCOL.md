# Remote protocol compatibility

`remote_esp32` is the handheld walkie/karaoke controller. It samples the
joystick, buttons, push-to-talk ADC, and karaoke mode, then emits the packed
`remote_packet_t` over ESP-NOW. The packet layout is intentionally identical
to `home_esp32/main/home_protocol.h`: magic `0x4B4E`, control type `1`, audio
type `2`, mode type `3`, signed joystick bytes, and 96 PCM16 samples.

The home device is a WiFi client of the PC server, not an ESP-NOW-only peer.
Therefore the PC server (or a future gateway) must receive/translate this
packet format and forward the resulting control/audio command to the robot.
Karaoke mode is encoded as `mode = 1`; both controllers force joystick
movement to zero while it is active. Relay/light commands are server-to-home
commands and are not emitted by the remote.
