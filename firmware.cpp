#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include "esp_camera.h"
#include <driver/i2s.h>

// --- Configuration ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* openai_key = "YOUR_OPENAI_API_KEY";

// Pin Definitions
#define BTN_A 1
#define BTN_B 2
#define TFT_CS 10
#define TFT_DC 14
#define TFT_RST 9

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
WebSocketsClient webSocket;

enum State { IDLE, RECORDING, PROCESSING, CAMERA_MODE };
State currentState = IDLE;

// --- UI Helpers ---
void updateUI(String status, uint16_t color) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(10, 20);
  tft.setTextColor(color);
  tft.setTextSize(2);
  tft.println(status);
}

// --- I2S Mic Setup ---
void setupMic() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64
  };
  i2s_pin_config_t pin_config = {
    .bck_io_num = 46,
    .ws_io_num = 45,
    .data_out_num = -1,
    .data_in_num = 21
  };
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

// --- WebSocket Event Handler ---
void onWebSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    StaticJsonDocument<2048> doc;
    deserializeJson(doc, payload);
    // Logic to extract "response.audio_transcript.delta"
    if (doc.containsKey("delta")) {
       tft.print(doc["delta"].as<String>());
    }
  }
}

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(1);
  updateUI("Connecting WiFi...", ILI9341_WHITE);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  
  updateUI("Ready.", ILI9341_GREEN);
  
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  
  setupMic();
  
  // Connect to OpenAI Realtime (Simplified URL for example)
  webSocket.beginSSL("api.openai.com", 443, "/v1/realtime?model=gpt-4o-realtime-preview-2024-10-01");
  String auth = "Bearer " + String(openai_key);
  webSocket.setExtraHeaders(("Authorization: " + auth + "\r\nOpenAI-Beta: realtime=v1").c_str());
  webSocket.onEvent(onWebSocketEvent);
}

void loop() {
  webSocket.loop();

  bool btnAPressed = (digitalRead(BTN_A) == LOW);
  bool btnBPressed = (digitalRead(BTN_B) == LOW);

  switch (currentState) {
    case IDLE:
      if (btnAPressed) {
        currentState = RECORDING;
        updateUI("Listening...", ILI9341_CYAN);
      } else if (btnBPressed) {
        currentState = CAMERA_MODE;
        updateUI("Camera Active", ILI9341_YELLOW);
      }
      break;

    case RECORDING:
      if (btnAPressed) {
        // 1. Read I2S Data
        // 2. Base64 Encode
        // 3. Send via webSocket.sendTXT() inside a JSON "input_audio_buffer.append"
      } else {
        currentState = PROCESSING;
        updateUI("Thinking...", ILI9341_MAGENTA);
        // Send "input_audio_buffer.commit"
      }
      break;

    case CAMERA_MODE:
      // Simple frame grab and display logic
      camera_fb_t * fb = esp_camera_fb_get();
      if (fb) {
        // Note: You would need to convert RGB565 and push to TFT
        esp_camera_fb_return(fb);
      }
      if (btnAPressed) {
         updateUI("Snap! Sending...", ILI9341_WHITE);
         // Logic to convert FB to JPG and send to Vision API
         delay(2000); 
         currentState = IDLE;
      }
      if (btnBPressed) {
        currentState = IDLE;
        updateUI("Ready.", ILI9341_GREEN);
      }
      break;
      
    case PROCESSING:
      // Wait for WebSocket response to finish, then return to IDLE
      delay(100); 
      break;
  }
}
