# Callback firmware notes

This is a lower-memory rewrite for troubleshooting the native ESP32 Bluetooth crashes. It removes `AudioTools/A2DPStream.h`, `AudioPlayer`, and the `WAVDecoder`; it uses the ESP32-A2DP library's PCM callback and a 6144-byte FreeRTOS byte ring buffer. SPIFFS file reads occur from `loop()`, not inside the high-priority Bluetooth callback.

## Test sequence
1. Turn off the right ESP32 and right JBL speaker. Test left node with only its intended M4 speaker powered on.
2. Confirm both sketches use the same local Express server IP and token as `server.js`.
3. Upload firmware only; WAVs remain in SPIFFS, but confirm the firmware sees all six files.
4. Keep the FSRs unloaded during the short boot calibration.
5. Wait for `[SYSTEM] Ready`, then use the dashboard to play Birds.
6. Check free heap and whether `[AUDIO] Playback finished` appears.
7. Once the left node is stable, turn it off and repeat with right node/right speaker. Then test both.

## Format requirement
Use PCM WAV, 16-bit stereo, 44100 Hz. The parser scans RIFF chunks and checks these fields.

Hardware validation and compilation on the user's exact Arduino core and ESP32-A2DP library version have not been performed in this environment. If it does not compile, share the first compiler error. If it compiles but still produces HCI asserts, share the complete serial log and ESP32-A2DP library version.
