/*
ESP32-CAM DEEP SLEEP TIME LAPSE CAMERA  PROOF-OF-CONCEPT
PICTURE STORED IN SD CARD AND THEN UPLOADED VIA WIFI
TO A SERVER USING HTTP MULTIPART POST
*/

#include <Arduino.h>
#include <WiFi.h>
#include <EEPROM.h>
#include "FS.h"           
#include "SD_MMC.h"       
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"
#include "esp_camera.h"

const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASS";

String serverName = "1.2.3.4";      // REPLACE WITH YOUR Raspberry Pi IP ADDRESS OR DOMAIN NAME
String serverEndpoint = "/upload";        // Needs to match upload server endpoint
String keyName = "\"myFile\"";            // Needs to match upload server keyName
const int serverPort = 8080;

WiFiClient client;

// CAMERA_MODEL_AI_THINKER
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

#define FLASHLED_GPIO_NUM 4
#define RTCLED_GPIO_NUM   GPIO_NUM_4
#define DEBUGLED_GPIO_NUM 33

#define uS_TO_S_FACTOR    1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP_S   60          /* Time ESP32 will go to sleep (in seconds) */

#define WIFI_TIMEOUT_S    30          /* Max WiFI waiting connection time (in seconds) */
#define SERVER_TIMEOUT_S  10          /* Max response time waiting for server response */

#define EEPROM_SIZE 1

int picNumber = 0;
String sdpath = "/images/";
String picname = "image";
String filext = ".jpg";

void setup() {
  
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  rtc_gpio_hold_dis(RTCLED_GPIO_NUM);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_S * uS_TO_S_FACTOR);
  
  // Turn off debug led
  pinMode(DEBUGLED_GPIO_NUM, OUTPUT);
  digitalWrite(DEBUGLED_GPIO_NUM, HIGH);
  // Turns off the ESP32-CAM white on-board LED (flash)
  pinMode(FLASHLED_GPIO_NUM, OUTPUT);
  digitalWrite(FLASHLED_GPIO_NUM, LOW);
  
  Serial.begin(115200);

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  

  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    Serial.println("Going to sleep now ");
    Serial.flush(); 
    esp_deep_sleep_start();
  }
  
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    Serial.println("Going to sleep now ");
    Serial.flush(); 
    esp_deep_sleep_start();
  }

  // init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_CIF;
    config.jpeg_quality = 12;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    Serial.println("Going to sleep now ");
    Serial.flush(); 
    esp_deep_sleep_start();
  }

  // Turns on the ESP32-CAM white on-board LED (flash)  
  digitalWrite(FLASHLED_GPIO_NUM, HIGH);
      
  camera_fb_t * pbuff = NULL;

  // take first frame!!
  pbuff = esp_camera_fb_get();
  if(!pbuff) {
    Serial.println("Camera capture failed");
    delay(1000);
    Serial.println("Going to sleep now ");
    Serial.flush(); 
    esp_deep_sleep_start();
  }
  
  // discard first frame!!
  esp_camera_fb_return(pbuff);

  // take second frame!!
  pbuff = esp_camera_fb_get();
  if(!pbuff) {
    Serial.println("Camera capture failed");
    delay(1000);
    Serial.println("Going to sleep now ");
    Serial.flush(); 
    esp_deep_sleep_start();
  }

  // initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);
  picNumber = EEPROM.read(0) + 1;

  // Path where new picture will be saved in SD Card
  String fpath = sdpath + picname +String(picNumber) + filext;

  fs::FS &fs = SD_MMC; 
  Serial.printf("Picture file name: %s\n", fpath.c_str());
  
  File file = fs.open(fpath.c_str(), FILE_WRITE);
  if(!file){
    Serial.println("Failed to open file in writing mode");
  } 
  else {
    file.write(pbuff->buf, pbuff->len); // payload (image), payload length
    Serial.printf("Saved file to path: %s\n", fpath.c_str());
    EEPROM.write(0, picNumber);
    EEPROM.commit();
  }
  file.close();

  // Turns off the ESP32-CAM white on-board LED (flash)
  pinMode(FLASHLED_GPIO_NUM, OUTPUT);
  digitalWrite(FLASHLED_GPIO_NUM, LOW);
  rtc_gpio_hold_en(RTCLED_GPIO_NUM);

  int wTimer = WIFI_TIMEOUT_S*1000;
  long wStartTimer = millis();

  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);  
  
  while ((wStartTimer + wTimer) > millis()) {
      if( WiFi.status() == WL_CONNECTED )
      {
        break;
      }
  }
    
  if ( (WiFi.status() != WL_CONNECTED) ){
    Serial.println("connection to WiFI failed");
    Serial.println("Going to sleep now ");
    Serial.flush(); 
    esp_deep_sleep_start();    
  }
  
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());

  // When connected to WiFi turn ON debug led
  digitalWrite(DEBUGLED_GPIO_NUM, LOW);

  uploadPhoto(pbuff);   
  
  Serial.println("Going to sleep now ");
  Serial.flush(); 
  esp_deep_sleep_start();
  
   
}

void loop() {
  
}

String uploadPhoto(camera_fb_t * fb) {
  String getAll;
  String getBody;
  
  Serial.println("Connecting to server: " + serverName);

  if (client.connect(serverName.c_str(), serverPort)) {

    Serial.println("Connection successful!");    
    
    // *** Temp filename!!! ***    
    String fileName = picname +String(picNumber) + filext;         
    //String keyName = "\"myFile\"";
    
    // Make a HTTP request and add HTTP headers
    String boundary = "SolarCamBoundaryjg2qVIUS8teOAbN3";
    String contentType = "image/jpeg";
    String portString = String(serverPort);
    String hostString = serverName;

    // post header
    String postHeader = "POST " + serverEndpoint + " HTTP/1.1\r\n";
    postHeader += "Host: " + hostString + ":" + portString + "\r\n";
    postHeader += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
    postHeader += "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
    postHeader += "Accept-Encoding: gzip,deflate\r\n";
    postHeader += "Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7\r\n";
    postHeader += "User-Agent: ESP32/Solar-Cam\r\n";
    postHeader += "Keep-Alive: 300\r\n";
    postHeader += "Connection: keep-alive\r\n";
    postHeader += "Accept-Language: en-us\r\n";    

    // request header
    String requestHead = "--" + boundary + "\r\n";
    //requestHead += "Content-Disposition: form-data; name=\"myFile\"; filename=\"" + fileName + "\"\r\n";
    requestHead += "Content-Disposition: form-data; name="+keyName+"; filename=\"" + fileName + "\"\r\n";
    
    requestHead += "Content-Type: " + contentType + "\r\n\r\n";

    // request tail
    String tail = "\r\n--" + boundary + "--\r\n\r\n";

    uint32_t imageLen = fb->len;

    // content length
    //int contentLength = keyHeader.length() + requestHead.length() + imageLen + tail.length();
    int contentLength = requestHead.length() + imageLen + tail.length();
    postHeader += "Content-Length: " + String(contentLength, DEC) + "\n\n";

    // send post header
    char charBuf0[postHeader.length() + 1];
    postHeader.toCharArray(charBuf0, postHeader.length() + 1);

    client.write(charBuf0, postHeader.length());
    Serial.print(charBuf0);
    
    // send request buffer
    char charBuf1[requestHead.length() + 1];
    requestHead.toCharArray(charBuf1, requestHead.length() + 1);
    client.write(charBuf1,requestHead.length());
    Serial.print(charBuf1);

    // create buffer
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0; n<fbLen; n=n+1024) {
      if (n+1024 < fbLen) {
        client.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        client.write(fbBuf, remainder);
      }
    }   
    
    // send tail
    char charBuf3[tail.length() + 1];
    tail.toCharArray(charBuf3, tail.length() + 1);
    client.write(charBuf3,tail.length());
    Serial.print(charBuf3);

    // Free image memory
    esp_camera_fb_return(fb);

    // Read all the lines of the reply from server and print them to Serial
                 
    int timoutTimer = SERVER_TIMEOUT_S*1000;
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + timoutTimer) > millis()) {
      Serial.print(".");
      delay(100);      
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (getAll.length()==0) { state=true; }
          getAll = "";
        }
        else if (c != '\r') { getAll += String(c); }
        if (state==true) { getBody += String(c); }
        startTimer = millis();
      }
      if (getBody.length()>0) { break; }
    }
    Serial.println();
    client.stop();
    Serial.println(getBody);
  }
  else {
    getBody = "Connection to " + serverName +  " failed.";
    Serial.println(getBody);
  }
  return getBody;
}
