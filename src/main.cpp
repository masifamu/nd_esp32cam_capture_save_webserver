#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "WiFi.h"
#include "WebServer.h"
#include "EEPROM.h"
#include "soc/soc.h" 
#include "soc/rtc_cntl_reg.h"

// Replace with your network credentials
const char* ssid = "Airtel_Darakhshan.naqab";
const char* password = "4321@4321";

WebServer server(80);

// Global variable to store the last captured image
camera_fb_t * capturedImage = NULL;

// Camera configuration
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
// #include "camera_pins.h"
// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Function to initialize the camera
void initCamera() {
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
    Serial.println("PSRAM available");
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("No PSRAM available");
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }
  // Turn off the onboard LED initially
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
}

// Function to initialize the SD card
void initSDCard() {
  if(!SD_MMC.begin("/sdcard", true)){
    Serial.println("SD Card Mount Failed");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return;
  }
}

// Function to handle the root path and return the HTML page
void handleRoot() {
  String html = "<html><body>";
  html += "<h1>ESP32-CAM Capture and Save</h1>";
  html += "<img src=\"\" id=\"imageView\" width=\"640\">";
  html += "<br>";
  html += "<button onclick=\"captureImage()\">Capture Image</button>";
  html += "<br><br>";
  html += "File Name: <input type=\"text\" id=\"fileName\" value=\"picture.jpg\">";
  html += "<button onclick=\"saveImage()\">Save Image</button>";
  html += "<br><br>";
  html += "<div id=\"message\"></div>";
  html += "<script>";
  html += "function captureImage() {";
  html += "fetch('/capture').then(response => response.blob()).then(blob => {";
  html += "document.getElementById('imageView').src = URL.createObjectURL(blob);";
  html += "document.getElementById('message').innerText = 'Image captured!';";
  html += "});";
  html += "}";
  html += "function saveImage() {";
  html += "var fileName = document.getElementById('fileName').value;";
  html += "fetch('/save?name=' + fileName).then(response => response.text()).then(data => {";
  html += "document.getElementById('message').innerText = data;";
  html += "});";
  html += "}";
  html += "</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}


// Function to capture an image and return it in the response
void handleCapture() {
  Serial.println("Capture clicked!");
  if (capturedImage) {
    esp_camera_fb_return(capturedImage);
  }

  // // Get the frame size from the request
  // if (server.hasArg("frameSize")) {
  //   int frameSize = server.arg("frameSize").toInt();
  //   Serial.print("Selected frame size: ");
  //   Serial.println(frameSize);

  //   // Reinitialize the camera with the selected frame size
  //   // Serial.println("trying to reinit the camera");
  //   esp_camera_deinit();
  //   initCamera((framesize_t)frameSize);
  //   Serial.println("Camera reinit done!");
  // }

  // Turn on the onboard LED
  digitalWrite(4, HIGH);
  capturedImage = esp_camera_fb_get();
  if (!capturedImage) {
    Serial.println("Camera capture failed");
    server.send(500, "text/plain", "Camera capture failed");
    // Turn off the onboard LED
    digitalWrite(4, LOW);
    return;
  }

  // Turn off the onboard LED
  digitalWrite(4, LOW);

  server.send_P(200, "image/jpeg", (const char *)capturedImage->buf, capturedImage->len);
}

// Function to save the captured image to the SD card
void handleSave() {
  Serial.println("Save clicked!");
  if (!capturedImage) {
    server.send(400, "text/plain", "No image to save");
    return;
  }

  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing file name");
    return;
  }

  String fileName = "/" + server.arg("name");

  // Save image to SD card
  File file = SD_MMC.open(fileName, FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file in writing mode");
    server.send(500, "text/plain", "Failed to open file");
    return;
  }
  file.write(capturedImage->buf, capturedImage->len);
  file.close();
  server.send(200, "text/plain", "Image saved successfully!");
}

void setup() {
  Serial.begin(115200);

  // Initialize camera
  // initCamera(FRAMESIZE_SVGA);
  initCamera();

  // Initialize SD card
  initSDCard();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  // Print the IP address
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Set up server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/save", HTTP_GET, handleSave);

  // Start the server
  server.begin();
}

void loop() {
  server.handleClient();
}
