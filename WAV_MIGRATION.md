# WAV firmware migration (memory workaround)

Your boot log shows the ESP32 has only about 17 KB of heap after A2DP starts, then the MP3/Helix audio path fails an allocation. This variant swaps `MP3DecoderHelix` for `WAVDecoder` so there is no Helix MP3 decode workspace. AudioTools WAV decoding reads the PCM data and handles the WAV header.

## Required audio files
Upload these names to SPIFFS on BOTH boards:

- `birds.wav`
- `ball.wav`
- `cat.wav`
- `dog.wav`
- `mom.wav`
- `dad.wav`

Use uncompressed PCM WAV, 16-bit, 44.1 kHz, stereo for the first test. The six files must fit in the board's SPIFFS partition.

## Firmware changes already made
- `AudioSourceSPIFFS` searches for `.wav` files.
- `WAVDecoder` replaces `MP3DecoderHelix`.
- Path mapping and filesystem checks expect `.wav`.
- The Helix buffer settings were removed.
- The sketch logs free heap and requests release of unused BLE memory before initializing Classic Bluetooth.

## Upload order
1. Make sure the Arduino board partition scheme and SPIFFS uploader use the same CSV partition table.
2. Replace the Wi-Fi/server configuration values in both sketches.
3. Upload Left firmware to the left ESP32 and Right firmware to the right ESP32.
4. Upload the six WAV files to SPIFFS on each board.
5. Open Serial Monitor at 115200 and check for `Player initialized`.
6. Start the Express backend and issue a dashboard command.

The backend API does not change: it still sends labels like `birds` or `mom`; only the firmware's local filename mapping changes.
