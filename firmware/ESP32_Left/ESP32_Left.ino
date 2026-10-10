/*
 * Inclusive Learning Mat - LEFT node (lower-memory A2DP callback build)
 * Target: original ESP32 / ESP32 Dev Module with Bluetooth Classic.
 * SPIFFS files: birds.wav, ball.wav, cat.wav, dog.wav, mom.wav, dad.wav
 * WAV requirement: PCM, 16-bit, stereo, 44100 Hz.
 *
 * This version removes AudioTools A2DPStream + AudioPlayer. The A2DPStream
 * convenience API allocates a large buffer; this version streams PCM from
 * SPIFFS through a small FreeRTOS byte ring buffer to BluetoothA2DPSource.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <esp_bt.h>
#include <esp_err.h>
#include "BluetoothA2DPSource.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"

// -------------------- CONFIGURATION --------------------
const char* WIFI_SSID = "abcd";
const char* WIFI_PASSWORD = "abcd1234";
const char* API_BASE_URL = "http://10.190.101.155:3000"; // Laptop IPv4 + Express port
const char* DEVICE_API_KEY = "local-dev-token-change-before-deployment";
const char* DEVICE_ID = "left";
const char* BT_SPEAKER_NAME = "M4";

// Use one 10k resistor per FSR, wired as 3V3 -> FSR -> GPIO node -> 10k -> GND.
#define FSR_USE_EXTERNAL_RESISTORS 1
#if FSR_USE_EXTERNAL_RESISTORS
const uint8_t SENSOR_PINS[3] = { 32, 33, 34 };
#else
const uint8_t SENSOR_PINS[3] = { 32, 33, 25 };
#endif
const char* SENSOR_LABELS[3] = {"birds", "ball", "cat"};

const uint32_t PRESS_DEBOUNCE_MS = 60;
const uint32_t RELEASE_DEBOUNCE_MS = 100;
const uint32_t COMMAND_POLL_MS = 3000;
const uint8_t SENSOR_CALIBRATION_SAMPLES = 48;
const uint32_t SENSOR_CALIBRATION_DELAY_MS = 5;
const uint16_t MIN_PRESS_DELTA = 200;
const uint16_t RELEASE_HYSTERESIS = 100;

// A small audio queue: 6144 bytes is ~35 ms of 44.1 kHz, 16-bit stereo PCM.
static constexpr size_t PCM_RING_BYTES = 6144;
static constexpr size_t PCM_FEED_CHUNK = 512;
static constexpr uint8_t PCM_FEED_CHUNKS_PER_LOOP = 4;
RingbufHandle_t pcmRing = nullptr;
BluetoothA2DPSource a2dpSource;
WiFiClient localClient;
File activeWav;
uint32_t wavBytesRemaining = 0;
bool wavFileExhausted = false;
bool audioPlaying = false;
bool audioFilesReady = false;

struct AudioCommand { uint32_t id; char label[16]; };
struct SensorEvent { char label[16]; uint16_t raw; };
QueueHandle_t commandQueue = nullptr;
QueueHandle_t sensorQueue = nullptr;

uint16_t sensorBaseline[3] = {0, 0, 0};
uint16_t pressThreshold[3] = {1100, 1100, 1100};
bool sensorLatched[3] = {false, false, false};
uint32_t pressCandidateAt[3] = {0, 0, 0};
uint32_t releaseCandidateAt[3] = {0, 0, 0};
uint32_t lastStatusAt = 0;
uint16_t lastRawValues[3] = {0, 0, 0};

void logAudioMemory(const char* stage) {
  Serial.printf("[MEM] %s | free=%u | largest8bit=%u | internalFree=%u | minFree=%u\n",
    stage,
    (unsigned int)ESP.getFreeHeap(),
    (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
    (unsigned int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
    (unsigned int)ESP.getMinFreeHeap());
}

const char* pathForLabel(const char* label) {
  if (strcmp(label, "birds") == 0) return "/birds.wav";
  if (strcmp(label, "ball") == 0) return "/ball.wav";
  if (strcmp(label, "cat") == 0) return "/cat.wav";
  if (strcmp(label, "dog") == 0) return "/dog.wav";
  if (strcmp(label, "mom") == 0) return "/mom.wav";
  if (strcmp(label, "dad") == 0) return "/dad.wav";
  return nullptr;
}

uint16_t readLE16(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
uint32_t readLE32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// Parse RIFF chunks, so WAV files with extra metadata chunks are handled too.
bool locatePcmData(File& file, uint32_t& dataSize) {
  uint8_t riff[12];
  if (file.read(riff, sizeof(riff)) != sizeof(riff)) return false;
  if (memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) return false;

  bool validFmt = false;
  const size_t fileSize = file.size();
  while (file.position() + 8 <= fileSize) {
    uint8_t chunk[8];
    if (file.read(chunk, sizeof(chunk)) != sizeof(chunk)) return false;
    const uint32_t chunkSize = readLE32(chunk + 4);
    const size_t chunkDataPos = file.position();
    if ((uint64_t)chunkDataPos + chunkSize > fileSize) return false;

    if (memcmp(chunk, "fmt ", 4) == 0) {
      if (chunkSize < 16) return false;
      uint8_t fmt[16];
      if (file.read(fmt, sizeof(fmt)) != sizeof(fmt)) return false;
      const uint16_t format = readLE16(fmt + 0);
      const uint16_t channels = readLE16(fmt + 2);
      const uint32_t sampleRate = readLE32(fmt + 4);
      const uint16_t bitsPerSample = readLE16(fmt + 14);
      validFmt = (format == 1 && channels == 2 && sampleRate == 44100 && bitsPerSample == 16);
      file.seek(chunkDataPos + chunkSize + (chunkSize & 1));
    } else if (memcmp(chunk, "data", 4) == 0) {
      if (!validFmt) return false;
      dataSize = chunkSize;
      const uint32_t remainingInFile = (uint32_t)(fileSize - chunkDataPos);
      if (dataSize > remainingInFile) dataSize = remainingInFile;
      dataSize -= (dataSize % 4); // preserve 16-bit stereo frame alignment
      return dataSize > 0;
    } else {
      file.seek(chunkDataPos + chunkSize + (chunkSize & 1));
    }
  }
  return false;
}

// Bluetooth A2DP callback: return PCM bytes; silence is returned on underrun/idle.
int32_t supplyPcmToA2dp(uint8_t* out, int32_t byteCount) {
  if (!out || byteCount <= 0) return 0;
  size_t copied = 0;
  while (pcmRing != nullptr && copied < (size_t)byteCount) {
    size_t available = 0;
    const size_t wanted = (size_t)byteCount - copied;
    void* item = xRingbufferReceiveUpTo(pcmRing, &available, 0, wanted);
    if (item == nullptr || available == 0) break;
    memcpy(out + copied, item, available);
    copied += available;
    vRingbufferReturnItem(pcmRing, item);
  }
  if (copied < (size_t)byteCount) memset(out + copied, 0, (size_t)byteCount - copied);
  return byteCount;
}

void drainPcmRing() {
  if (!pcmRing) return;
  for (;;) {
    size_t bytes = 0;
    void* item = xRingbufferReceiveUpTo(pcmRing, &bytes, 0, PCM_RING_BYTES);
    if (!item) break;
    vRingbufferReturnItem(pcmRing, item);
  }
}

bool beginWavPlayback(const char* label, uint32_t commandId) {
  const char* path = pathForLabel(label);
  if (!path) {
    Serial.printf("[ERROR] Unknown label: %s\n", label);
    return false;
  }
  if (audioPlaying) {
    Serial.printf("[AUDIO] Busy playing; ignoring command %lu (%s).\n",
                  (unsigned long)commandId, label);
    return false;
  }
  if (activeWav) activeWav.close();
  drainPcmRing();
  if (!SPIFFS.exists(path)) {
    Serial.printf("[ERROR] Missing audio file: %s\n", path);
    return false;
  }
  activeWav = SPIFFS.open(path, FILE_READ);
  if (!activeWav) {
    Serial.printf("[ERROR] Cannot open %s\n", path);
    return false;
  }
  uint32_t dataSize = 0;
  if (!locatePcmData(activeWav, dataSize)) {
    Serial.printf("[ERROR] %s must be PCM WAV, 16-bit stereo, 44100 Hz.\n", path);
    activeWav.close();
    return false;
  }
  wavBytesRemaining = dataSize;
  wavFileExhausted = false;
  audioPlaying = true;
  Serial.printf("[AUDIO] Command %lu: streaming %s (%s), PCM bytes=%lu\n",
                (unsigned long)commandId, label, path, (unsigned long)dataSize);
  return true;
}

// Keep the callback lightweight: this loop task fills the PCM ring from flash.
void feedPcmRing() {
  if (!pcmRing) return;
  static uint8_t chunk[PCM_FEED_CHUNK];

  if (activeWav && wavBytesRemaining > 0) {
    for (uint8_t i = 0; i < PCM_FEED_CHUNKS_PER_LOOP; ++i) {
      const size_t freeBytes = xRingbufferGetCurFreeSize(pcmRing);
      if (freeBytes < PCM_FEED_CHUNK) break;
      size_t want = PCM_FEED_CHUNK;
      if (want > wavBytesRemaining) want = wavBytesRemaining;
      want -= (want % 4);
      if (want == 0) break;

      const size_t got = activeWav.read(chunk, want);
      if (got == 0) {
        wavBytesRemaining = 0;
        break;
      }
      if (xRingbufferSend(pcmRing, chunk, got, 0) != pdTRUE) {
        activeWav.seek(activeWav.position() - got);
        break;
      }
      wavBytesRemaining -= got;
    }
  }

  if (activeWav && wavBytesRemaining == 0) {
    activeWav.close();
    wavFileExhausted = true;
  }
  // When the ring is nearly empty after EOF, all queued PCM was consumed.
  if (wavFileExhausted && xRingbufferGetCurFreeSize(pcmRing) >= PCM_RING_BYTES - 64) {
    audioPlaying = false;
    wavFileExhausted = false;
    Serial.println("[AUDIO] Playback finished.");
  }
}

uint16_t readSensorRaw(uint8_t index) {
#if FSR_USE_EXTERNAL_RESISTORS
  return (uint16_t)analogRead(SENSOR_PINS[index]);
#else
  return digitalRead(SENSOR_PINS[index]) == HIGH ? 4095 : 0;
#endif
}

void calibrateSensors() {
#if FSR_USE_EXTERNAL_RESISTORS
  Serial.println("[SENSOR] Keep all FSRs untouched for startup calibration...");
  uint32_t sums[3] = {0, 0, 0};
  for (uint8_t n = 0; n < SENSOR_CALIBRATION_SAMPLES; ++n) {
    for (uint8_t i = 0; i < 3; ++i) sums[i] += analogRead(SENSOR_PINS[i]);
    delay(SENSOR_CALIBRATION_DELAY_MS);
  }
  for (uint8_t i = 0; i < 3; ++i) {
    sensorBaseline[i] = sums[i] / SENSOR_CALIBRATION_SAMPLES;
    uint16_t delta = sensorBaseline[i] / 12;
    if (delta < MIN_PRESS_DELTA) delta = MIN_PRESS_DELTA;
    uint32_t threshold = (uint32_t)sensorBaseline[i] + delta;
    if (threshold > 4080) threshold = 4080;
    pressThreshold[i] = (uint16_t)threshold;
    Serial.printf("[SENSOR] %s baseline=%u threshold=%u release<%u\n",
      SENSOR_LABELS[i], sensorBaseline[i], pressThreshold[i],
      pressThreshold[i] > RELEASE_HYSTERESIS ? pressThreshold[i] - RELEASE_HYSTERESIS : 0);
  }
#else
  for (uint8_t i = 0; i < 3; ++i) pinMode(SENSOR_PINS[i], INPUT_PULLDOWN);
  Serial.println("[SENSOR] Digital mode: FSR between 3V3 and GPIO, internal pull-down enabled.");
#endif
}

bool sensorPressureCondition(uint8_t index, uint16_t raw) {
#if FSR_USE_EXTERNAL_RESISTORS
  return raw >= pressThreshold[index];
#else
  (void)index;
  return raw == 4095;
#endif
}
bool sensorReleaseCondition(uint8_t index, uint16_t raw) {
#if FSR_USE_EXTERNAL_RESISTORS
  const uint16_t releasePoint = pressThreshold[index] > RELEASE_HYSTERESIS
    ? pressThreshold[index] - RELEASE_HYSTERESIS : 0;
  return raw < releasePoint;
#else
  (void)index;
  return raw == 0;
#endif
}

void enqueueSensorEvent(uint8_t index, uint16_t raw) {
  if (!sensorQueue) return;
  SensorEvent event{};
  strlcpy(event.label, SENSOR_LABELS[index], sizeof(event.label));
  event.raw = raw;
  if (xQueueSend(sensorQueue, &event, 0) != pdTRUE) {
    Serial.printf("[WARN] Sensor event queue full; dropped %s\n", event.label);
  } else {
    Serial.printf("[SENSOR] PRESS label=%s raw=%u\n", event.label, raw);
  }
}

void playQueuedCommand() {
  if (audioPlaying || !commandQueue) return;
  AudioCommand command{};
  if (xQueueReceive(commandQueue, &command, 0) == pdTRUE) {
    beginWavPlayback(command.label, command.id);
  }
}

void reportSensorEvent(const SensorEvent& event) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  const String url = String(API_BASE_URL) + "/api/device/" + DEVICE_ID + "/sensor";
  http.setReuse(false);
  http.setConnectTimeout(1500);
  http.setTimeout(1500);
  if (!http.begin(localClient, url)) {
    Serial.println("[HTTP] Could not start sensor POST.");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", DEVICE_API_KEY);
  StaticJsonDocument<128> doc;
  doc["label"] = event.label;
  doc["raw"] = event.raw;
  char body[96];
  const size_t bodyLen = serializeJson(doc, body, sizeof(body));
  const int code = bodyLen > 0 && bodyLen < sizeof(body)
    ? http.POST((uint8_t*)body, bodyLen) : -1;
  if (code >= 200 && code < 300) {
    Serial.printf("[HTTP] Sensor accepted (%d): %s\n", code, event.label);
  } else {
    Serial.printf("[HTTP] Sensor POST error code=%d\n", code);
  }
  http.end();
}

void pollCommand() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  const String url = String(API_BASE_URL) + "/api/device/" + DEVICE_ID + "/command";
  http.setReuse(false);
  http.setConnectTimeout(1500);
  http.setTimeout(1500);
  if (!http.begin(localClient, url)) {
    Serial.println("[HTTP] Could not start command poll.");
    return;
  }
  http.addHeader("X-Device-Key", DEVICE_API_KEY);
  const int code = http.GET();
  if (code == 200) {
    StaticJsonDocument<96> filter;
    filter["command"]["id"] = true;
    filter["command"]["label"] = true;
    StaticJsonDocument<192> doc;
    const int responseSize = http.getSize();
    const DeserializationError error = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
    if (error) {
      Serial.printf("[JSON] Parse error=%s bytes=%d heap=%u\n",
        error.c_str(), responseSize, (unsigned int)ESP.getFreeHeap());
    } else if (!doc["command"].isNull()) {
      JsonObject command = doc["command"].as<JsonObject>();
      AudioCommand queued{};
      queued.id = command["id"] | 0;
      strlcpy(queued.label, command["label"] | "", sizeof(queued.label));
      if (!pathForLabel(queued.label)) {
        Serial.printf("[ERROR] Unknown command label: %s\n", queued.label);
      } else if (xQueueSend(commandQueue, &queued, 0) != pdTRUE) {
        Serial.println("[WARN] Command queue full; dropped command.");
      } else {
        Serial.printf("[HTTP] Received command id=%lu label=%s\n",
          (unsigned long)queued.id, queued.label);
      }
    }
  } else if (code < 0) {
    Serial.printf("[HTTP] Poll failed: %s\n", HTTPClient::errorToString(code).c_str());
  } else if (code != 204) {
    Serial.printf("[HTTP] Poll returned HTTP %d\n", code);
  }
  http.end();
  Serial.printf("[MEM] after poll free=%u largest=%u\n",
    (unsigned int)ESP.getFreeHeap(),
    (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void networkWorker(void* parameter) {
  (void)parameter;
  Serial.printf("[NET] Worker running; heap=%u\n", (unsigned int)ESP.getFreeHeap());
  uint32_t lastPoll = millis() - COMMAND_POLL_MS;
  uint32_t lastWifiRetry = 0;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      if (millis() - lastWifiRetry >= 5000) {
        lastWifiRetry = millis();
        WiFi.reconnect();
      }
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }
    SensorEvent event{};
    if (xQueueReceive(sensorQueue, &event, 0) == pdTRUE) reportSensorEvent(event);
    if (millis() - lastPoll >= COMMAND_POLL_MS) {
      lastPoll = millis();
      pollCommand();
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup() {
  Serial.begin(115200);
  delay(700);
  Serial.println("\n======================================");
  Serial.println(" Learning Mat / LEFT ESP32 booting");
  Serial.println("======================================");

  Serial.println("[MEM] Releasing unused BLE memory...");
  const esp_err_t bleResult = esp_bt_mem_release(ESP_BT_MODE_BLE);
  Serial.printf("[MEM] BLE release: %s (%d)\n",
    bleResult == ESP_OK ? "SUCCESS" : esp_err_to_name(bleResult), (int)bleResult);
  logAudioMemory("after BLE release");

  commandQueue = xQueueCreate(4, sizeof(AudioCommand));
  sensorQueue = xQueueCreate(6, sizeof(SensorEvent));
  pcmRing = xRingbufferCreate(PCM_RING_BYTES, RINGBUF_TYPE_BYTEBUF);
  if (!commandQueue || !sensorQueue || !pcmRing) {
    Serial.println("[FATAL] Could not allocate queues/audio ring; stopping.");
    while (true) delay(1000);
  }

#if FSR_USE_EXTERNAL_RESISTORS
  for (uint8_t i = 0; i < 3; ++i) {
    pinMode(SENSOR_PINS[i], INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(SENSOR_PINS[i], ADC_11db);
  }
#endif
  calibrateSensors();

  Serial.println("[FS] Mounting SPIFFS without formatting...");
  if (!SPIFFS.begin(false)) {
    Serial.println("[FATAL] SPIFFS mount failed.");
  } else {
    const char* expected[] = {"/birds.wav", "/ball.wav", "/cat.wav", "/dog.wav", "/mom.wav", "/dad.wav"};
    uint8_t found = 0;
    for (const char* name : expected) {
      if (SPIFFS.exists(name)) {
        File file = SPIFFS.open(name, FILE_READ);
        Serial.printf("[FS] OK %-12s %u bytes\n", name, file ? (unsigned int)file.size() : 0);
        if (file) file.close();
        ++found;
      } else {
        Serial.printf("[FS ERROR] Missing %s\n", name);
      }
    }
    audioFilesReady = (found == 6);
    Serial.printf("[FS] Files ready=%s\n", audioFilesReady ? "yes" : "no");
  }

  Serial.printf("[Wi-Fi] Joining SSID: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true); // reduce Wi-Fi power; audio ring smooths short scheduling gaps
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  const uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
    delay(300);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[Wi-Fi] IP=%s RSSI=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("[Wi-Fi WARN] Not connected; worker will retry.");
  }

  logAudioMemory("before BluetoothA2DPSource");
  a2dpSource.set_data_callback(supplyPcmToA2dp);
  a2dpSource.set_auto_reconnect(false); // Debug first: prevent reconnect churn while testing.
  Serial.printf("[BT] Starting direct callback A2DP source for sink '%s'.\n", BT_SPEAKER_NAME);
  Serial.println("[BT] During setup, power off the other M4 speaker to prevent same-name ambiguity.");
  a2dpSource.start(BT_SPEAKER_NAME);
  logAudioMemory("after BluetoothA2DPSource");

  BaseType_t taskResult = xTaskCreatePinnedToCore(
    networkWorker, "networkWorker", 4096, nullptr, 1, nullptr, 0);
  if (taskResult != pdPASS) {
    Serial.printf("[FATAL] networkWorker creation failed; free heap=%u\n", (unsigned int)ESP.getFreeHeap());
  } else {
    Serial.println("[NET] networkWorker created (4096-byte stack).");
  }
  Serial.printf("[SYSTEM] Device=%s; labels=%s\n", DEVICE_ID,
    strcmp(DEVICE_ID, "left") == 0 ? "birds, ball, cat" : "dog, mom, dad");
  logAudioMemory("ready");
  Serial.println("[SYSTEM] Ready. Select a sound on the dashboard.");
}

void loop() {
  // File I/O and ring-buffer producer happen here, outside the Bluetooth callback.
  playQueuedCommand();
  feedPcmRing();

  const uint32_t now = millis();
  for (uint8_t i = 0; i < 3; ++i) {
    const uint16_t raw = readSensorRaw(i);
    lastRawValues[i] = raw;
    if (!sensorLatched[i]) {
      if (sensorPressureCondition(i, raw)) {
        if (pressCandidateAt[i] == 0) pressCandidateAt[i] = now;
        if (now - pressCandidateAt[i] >= PRESS_DEBOUNCE_MS) {
          sensorLatched[i] = true;
          pressCandidateAt[i] = 0;
          releaseCandidateAt[i] = 0;
          enqueueSensorEvent(i, raw);
        }
      } else {
        pressCandidateAt[i] = 0;
      }
    } else {
      if (sensorReleaseCondition(i, raw)) {
        if (releaseCandidateAt[i] == 0) releaseCandidateAt[i] = now;
        if (now - releaseCandidateAt[i] >= RELEASE_DEBOUNCE_MS) {
          sensorLatched[i] = false;
          releaseCandidateAt[i] = 0;
          Serial.printf("[SENSOR] RELEASE %s raw=%u\n", SENSOR_LABELS[i], raw);
        }
      } else {
        releaseCandidateAt[i] = 0;
      }
    }
  }

  if (now - lastStatusAt >= 3000) {
    lastStatusAt = now;
    Serial.printf("[STATUS] WiFi=%s RSSI=%d | audio=%s | heap=%u | largest=%u\n",
      WiFi.status() == WL_CONNECTED ? "connected" : "offline",
      WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0,
      audioPlaying ? "playing" : "idle",
      (unsigned int)ESP.getFreeHeap(),
      (unsigned int)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    Serial.printf("[RAW] %s=%u(th=%u), %s=%u(th=%u), %s=%u(th=%u)\n",
      SENSOR_LABELS[0], lastRawValues[0], pressThreshold[0],
      SENSOR_LABELS[1], lastRawValues[1], pressThreshold[1],
      SENSOR_LABELS[2], lastRawValues[2], pressThreshold[2]);
  }
  delay(1);
}
