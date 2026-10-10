# HTTPS/ngrok firmware fix

The Left and Right sketches now use `WiFiClientSecure` for the HTTPS ngrok URL in both command polling and sensor event reporting. Each request has its own `HTTPClient` declaration, and both requests send `ngrok-skip-browser-warning: 1`. This addresses the `http was not declared` compile error and avoids using plain HTTP with an HTTPS URL.

## Configure before upload
- Replace `WIFI_SSID` and `WIFI_PASSWORD` in both sketches with the actual Wi-Fi credentials.
- Keep `API_BASE_URL` set to your current ngrok HTTPS URL; it may change when ngrok restarts.
- Keep `DEVICE_API_KEY` matching the Express backend's `DEVICE_API_KEY`.
- `ngrokClient.setInsecure()` is for development only; it disables TLS certificate validation.

## Important RAM limitation
The ESP32 was previously reporting only a few KB of free heap after A2DP/player initialization. HTTPS/TLS handshakes can require considerably more memory, so this version may compile but still fail at runtime due to heap exhaustion. If that happens, the reliable low-memory option is HTTP over the local Wi-Fi LAN (with an appropriate firewall rule), or changing the architecture so the ESP32 does not establish TLS itself.
