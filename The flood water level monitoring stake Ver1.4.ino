/*********
  Rui Santos
  Complete instructions at: https://RandomNerdTutorials.com/esp32-cam-save-picture-firebase-storage/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  Based on the example provided by the ESP Firebase Client Library
*********/
#include <EEPROM.h>
#include "Arduino.h"
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include "WiFi.h"
#include "esp_camera.h"
#include "soc/soc.h"           // Disable brownout problems
#include "soc/rtc_cntl_reg.h"  // Disable brownout problems
#include "driver/rtc_io.h"
#include <LittleFS.h>
#include <FS.h>
#include <Firebase_ESP_Client.h>
#include <WiFiMulti.h>
//Provide the token generation process info.
#include <addons/TokenHelper.h>
#include <time.h>

//Replace with your network credentials
const char* ssid = "4G-coc-do-muc-nuoc";
const char* password = "1234567890";
WiFiMulti wifiMulti;
// const char* ssid = "cocdonuoc";
// const char* password = "66668888";

const int EEPROM_SIZE = 512;  // Kích thước EEPROM
const int SSID_ADDR = 0;      // Địa chỉ bắt đầu cho SSID
const int PASS_ADDR = 50;
char ssid1[50];  // Biến toàn cục để lưu SSID
char pass1[50];  //
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// Máy chủ NTP mặc định
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;  // UTC
const int daylightOffset_sec = 0;     // Không có thay đổi giờ mùa hè
unsigned long LastCapture = 0;
unsigned long LastUpdateData = 0;
unsigned long LastUpdateCallback = 0;
int hours, mins, days, months, years, maxDistance = 0, minDistance = 0, weeks;
bool dayReport = true;
int TimeINTERVAL = 5;
// Insert Firebase project API Key
#define API_KEY "AIzaSyDOSjdZ6GGLfaNFE3fuquljB5jf3o7bz0Q"

// Insert Authorized Email and Corresponding Password
#define USER_EMAIL "thietbidonuoc.iot@gmail.com"
#define USER_PASSWORD "Quan2004"

// Insert Firebase storage bucket ID e.g bucket-name.appspot.com
#define STORAGE_BUCKET_ID "do-muc-nuoc-hdmq.appspot.com"
#define DATABASE_URL "https://do-muc-nuoc-hdmq-default-rtdb.asia-southeast1.firebasedatabase.app"  // Thay thế bằng URL Firebase Realtime Database của bạn

// For example:
//#define STORAGE_BUCKET_ID "esp-iot-app.appspot.com"

// Photo File Name to save in LittleFS
#define FILE_PHOTO_PATH "/photo.jpg"
#define BUCKET_PHOTO "/MayTest/photo.jpg"

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

boolean takeNewPhoto = true;
int iVal = 0;
//Define Firebase Data objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig configF;
const int trigPin = 13;
const int echoPin = 15;
/////////////////////////////////////////
//define sound speed in cm/uS
#define SOUND_SPEED 0.034

/////////////////////////////////////////////
int partCount = 0;       // Chỉ số cho mảng
String combinedData;     // Chuỗi để lưu dữ liệu ghép
String DaycombinedData;  // Chuỗi để lưu dữ liệu ghép
String WeekcombinedData;
int WeekpartCount = 0;
double Total = 0, Average = 0, WeekTotal = 0, WeekAverage = 0;
int readCM;
void fcsUploadCallback(FCS_UploadStatusInfo info);

bool taskCompleted = true;

// Capture Photo and Save it to LittleFS
void capturePhotoSaveLittleFS(void) {
  // Dispose first pictures because of bad quality
  camera_fb_t* fb = NULL;
  // Skip first 3 frames (increase/decrease number as needed).
  for (int i = 0; i < 4; i++) {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    fb = NULL;
  }

  // Take a new photo
  fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
 //  Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
  }

  // Photo file name//
 Serial.printf("Picture file name: %s\n", FILE_PHOTO_PATH);
  File file = LittleFS.open(FILE_PHOTO_PATH, FILE_WRITE);

  // Insert the data in the photo file
  if (!file) {
 //  Serial.println("Failed to open file in writing mode");
  } else {
    file.write(fb->buf, fb->len);  // payload (image), payload length
 //  Serial.print("The picture has been saved in ");
 //  Serial.print(FILE_PHOTO_PATH);
 //  Serial.print(" - Size: ");
 //  Serial.print(fb->len);
 //  Serial.println(" bytes");
  }
  // Close the file
  file.close();
  esp_camera_fb_return(fb);
}

void initWiFi() {
  // WiFi.begin(ssid, password);
  readWiFiCredentials();
  wifiMulti.addAP(ssid1, pass1);
  wifiMulti.addAP(ssid, password);
  wifiMulti.addAP("cocdonuoc", "66668888");


  while (wifiMulti.run() != WL_CONNECTED) {
    delay(1000);
 //  Serial.println("Connecting to WiFi...");
    connecting();
  }
  digitalWrite(4, LOW);
}

void initLittleFS() {
  if (!LittleFS.begin(true)) {
 //  Serial.println("An Error has occurred while mounting LittleFS");
    ESP.restart();
  } else {
    delay(500);
 //  Serial.println("LittleFS mounted successfully");
  }
}

void initCamera() {
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
 //  Serial.printf("Camera init failed with error 0x%x", err);
    Firebase.RTDB.setString(&fbdo, "/Device_1/error/camera", "lỗi cam!");
    delay(5000);
    ESP.restart();
  }
  Firebase.RTDB.setString(&fbdo, "/Device_1/error/camera", "hoạt động bình thường");
}

void setup() {
// Serial port for debugging purposes
  EEPROM.begin(512);
  Wire.begin(14, 2);//
 Serial.begin(115200);
  pinMode(trigPin, OUTPUT);  // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);   // Sets the echoPin as an Input
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  delay(3000);
  digitalWrite(4, LOW);
  initWiFi();
  initLittleFS();
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  //Firebase
  // Assign the api key
  configF.api_key = API_KEY;
  configF.database_url = DATABASE_URL;
  //Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  //Assign the callback function for the long running token generation task
  configF.token_status_callback = tokenStatusCallback;  //see addons/TokenHelper.h

  Firebase.begin(&configF, &auth);
  Firebase.reconnectWiFi(true);
  if (Firebase.ready()) {
 //  Serial.println("Đăng nhập Firebase thành công!");
  } else {
 //  Serial.println("Đăng nhập thất bại, kiểm tra lại thông tin.");
 //  Serial.println("Lỗi: ");
    error();
  }
  initCamera();
  if (!sht31.begin(0x44)) {
    Firebase.RTDB.setString(&fbdo, "/Device_1/error/SHTsensor", "lỗi cảm biến SHT");
  } else {
    Firebase.RTDB.setString(&fbdo, "/Device_1/error/SHTsensor", "hoạt động bình thường");
  }


  Firebase.RTDB.getString(&fbdo, F("/Device_1/data/live"), &combinedData);
  Firebase.RTDB.getString(&fbdo, F("/Device_1/data/day"), &DaycombinedData);
  Firebase.RTDB.getString(&fbdo, F("/Device_1/data/week"), &WeekcombinedData);


  // Đếm số lần xuất hiện của dấu ":"

  int index = combinedData.indexOf(':');
  int index1 = DaycombinedData.indexOf(':');
  while (index != -1) {
    partCount++;
    // Tìm vị trí tiếp theo của dấu ":"
    index = combinedData.indexOf(':', index + 1);
  }
  while (index1 != -1) {
    WeekpartCount++;
    // Tìm vị trí tiếp theo của dấu ":"
    index1 = DaycombinedData.indexOf(':', index1 + 1);
  }

  Firebase.RTDB.setInt(&fbdo, "/Device_1/data/RESETESP", 0);

  setCpuFrequencyMhz(80);
  Firebase.RTDB.setInt(&fbdo, "/Device_1/error/xung", getCpuFrequencyMhz());//
 Serial.print("xung hiện tại: ");//
 Serial.println(getCpuFrequencyMhz());
}

void loop() {

  if (wifiMulti.run() != WL_CONNECTED) {
    WiFi.disconnect(true);

    delay(10000);
    readWiFiCredentials();
    wifiMulti.addAP(ssid1, pass1);
    wifiMulti.addAP(ssid, password);
    wifiMulti.addAP("cocdonuoc", "66668888");

    // ESP.restart();
  }


  if (Firebase.ready() && (millis() - LastUpdateCallback > 10000)) {
    LastUpdateCallback = millis();
    int RESETESP = 0;
    String ssid_passInput = "";  // Đầu vào SSID
    //String passwordInput = "";   // Đầu vào mật khẩu
    // Chờ 5 giây trước khi gửi lại dữ liệu
    Firebase.RTDB.getInt(&fbdo, F("/Device_1/image/capture"), &iVal);
    Firebase.RTDB.getInt(&fbdo, F("/Device_1/data/TimeINTERVAL"), &TimeINTERVAL);
    Firebase.RTDB.getString(&fbdo, F("/Device_1/data/WIFI/SSID_PASS"), &ssid_passInput);
    Firebase.RTDB.getInt(&fbdo, F("/Device_1/data/RESETESP"), &RESETESP);

    int commaIndex = ssid_passInput.indexOf(',');  // Tìm vị trí dấu phẩy

    if (commaIndex != -1) {                                    // Nếu tìm thấy dấu phẩy
      String ssid2 = ssid_passInput.substring(0, commaIndex);  // Lưu chuỗi SSID vào biến tạm
      String password2 = ssid_passInput.substring(commaIndex + 1);
      saveWiFiCredentials(ssid2, password2);
    }


    if (RESETESP == 1) {
      ESP.restart();
    }
    //if ((old_ssid != ssidInput) || (old_pass != passwordInput)) {
    //saveWiFiCredentials(ssidInput, passwordInput);
    // }
 //  Serial.print("partcount :  ");
 //  Serial.print(partCount);

 //  Serial.print("\tday :  ");
 //  Serial.print(DaycombinedData);

 //  Serial.print("\tweekpartcount :  ");
 //  Serial.print(WeekpartCount);

 //  Serial.print("\tweek :  ");
 //  Serial.print(WeekcombinedData);
 //  Serial.print("\tSSID1 :  ");
 //  Serial.print(ssid1);
 //  Serial.print("\tPASS1 :  ");
 //  Serial.print(pass1);
 //  Serial.print("\tiVal :  ");
 //  Serial.print(iVal);
 //  Serial.print("\t TimeINTERVAL :  ");
 //  Serial.println(TimeINTERVAL);
    //old_ssid = ssidInput;
    //old_pass = passwordInput;
  }
  //Serial.println("hsahhdabsd");
  if ((millis() - LastUpdateData > TimeINTERVAL * 1000 * 60)) {
    readWiFiCredentials();
    // Tạo chuỗi mới nhất
    int WaterLevel = readWaterCM();
    String mau = String(printLocalTime() / 1000) + "," + String(WaterLevel) + ":";  // Tạo chuỗi dữ liệu
    // Thêm phần tử mới nhất vào cuối combinedData
    combinedData += mau;
    partCount++;  // Tăng khi gặp dấu phân cách
    Total += WaterLevel;
    if (partCount > 100) {
      int firstColon = combinedData.indexOf(":");             // Tìm vị trí dấu phân cách đầu tiên
      combinedData = combinedData.substring(firstColon + 1);  // Xóa phần tử đầu
    }
    // Tạo chuỗi mới nhất
    // int WaterLevel = readWaterCM();
    // float t = sht31.readTemperature();
    // float h = sht31.readHumidity();
    // String mau = String(printLocalTime() / 1000) + "," + String(WaterLevel) + ":";  // Tạo chuỗi dữ liệu
    // // Thêm phần tử mới nhất vào cuối combinedData
    // combinedData += mau;
    // partCount++;  // Tăng khi gặp dấu phân cách
    // Total += WaterLevel;
    // if (partCount > 100) {
    //   int firstColon = combinedData.indexOf(":");             // Tìm vị trí dấu phân cách đầu tiên
    //   combinedData = combinedData.substring(firstColon + 1);  // Xóa phần tử đầu
    // }
    // Đẩy dữ liệu lên Firebase

    if (Firebase.ready()) {
      float t = sht31.readTemperature();
      float h = sht31.readHumidity();
      Firebase.RTDB.setString(&fbdo, "/Device_1/data/temp_hum", String(t) + "," + String(h) + ":");
      if (Firebase.RTDB.setString(&fbdo, "/Device_1/data/live", combinedData.c_str())) {
     //  Serial.println("Đẩy dữ liệu thành công");
     //  Serial.println("Đã gửi: " + String(partCount) + "Data: " + combinedData);
      } else {
     //  Serial.printf("Không thể đẩy dữ liệu: %s\n", fbdo.errorReason().c_str());
      }

      if (hours == 23 && mins >= 30 && mins <= 59 && dayReport == true) {
        Average = Total / partCount;
        WeekTotal += Average;
        WeekpartCount++;
        // Thêm phần tử mới nhất vào cuối combinedData
        DaycombinedData += String(printLocalTime() / 1000) + "," + String(Average) + String(maxDistance) + "," + String(minDistance) + ":";
        dayReport = false;
        if (Firebase.RTDB.setString(&fbdo, "/Device_1/data/day", DaycombinedData.c_str())) {
       //  Serial.println("Đẩy dữ liệu thành công");
       //  Serial.println("DayData: " + DaycombinedData);
        } else {
       //  Serial.printf("Không thể đẩy dữ liệu: %s\n", fbdo.errorReason().c_str());
        }

        if (weeks == 0) {
          WeekAverage = WeekTotal / WeekpartCount;
          if (Firebase.RTDB.setString(&fbdo, "/Device_1/data/week", WeekcombinedData.c_str())) {
         //  Serial.println("Đẩy dữ liệu thành công");
         //  Serial.println("DayData: " + WeekcombinedData);
          } else {
         //  Serial.printf("Không thể đẩy dữ liệu: %s\n", fbdo.errorReason().c_str());
          }
        }


      } else if (hours != 23 && mins < 30) {  // cần sửa đoạn này
        dayReport = true;
      }
    }


    LastUpdateData = millis();
  }





  if (iVal == 1 && (millis() - LastCapture > 30000)) {
    LastCapture = millis();
    capturePhotoSaveLittleFS();
    iVal = 0;

    taskCompleted = false;
  }
  delay(1);
  if (Firebase.ready() && !taskCompleted) {
    taskCompleted = true;
 //  Serial.print("Uploading picture... ");
    Firebase.RTDB.setString(&fbdo, "/Device_1/image/capture", "uploading..");
    //MIME type should be valid to avoid the download problem.
    //The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
    if (Firebase.Storage.upload(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, FILE_PHOTO_PATH /* path to local file */, mem_storage_type_flash /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, BUCKET_PHOTO /* path of remote file stored in the bucket */, "image/jpeg" /* mime type */, fcsUploadCallback)) {
   //  Serial.printf("\nDownload URL: %s\n", fbdo.downloadURL().c_str());
    } else {
   //  Serial.println(fbdo.errorReason());
    }
    Firebase.RTDB.setInt(&fbdo, "/Device_1/image/capture", iVal);
  }
}

// The Firebase Storage upload callback function
void fcsUploadCallback(FCS_UploadStatusInfo info) {
  if (info.status == firebase_fcs_upload_status_init) {
 //  Serial.printf("Uploading file %s (%d) to %s\n", info.localFileName.c_str(), info.fileSize, info.remoteFileName.c_str());
  } else if (info.status == firebase_fcs_upload_status_upload) {
 //  Serial.printf("Uploaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
    // Firebase.RTDB.setString(&fbdo, "/Device_2/image/capture", String((int)info.progress)+ "%");
  } else if (info.status == firebase_fcs_upload_status_complete) {
 //  Serial.println("Upload completed\n");
    FileMetaInfo meta = fbdo.metaData();
 //  Serial.printf("Name: %s\n", meta.name.c_str());
 //  Serial.printf("Bucket: %s\n", meta.bucket.c_str());
 //  Serial.printf("contentType: %s\n", meta.contentType.c_str());
 //  Serial.printf("Size: %d\n", meta.size);
 //  Serial.printf("Generation: %lu\n", meta.generation);
 //  Serial.printf("Metageneration: %lu\n", meta.metageneration);
 //  Serial.printf("ETag: %s\n", meta.etag.c_str());
 //  Serial.printf("CRC32: %s\n", meta.crc32.c_str());
 //  Serial.printf("Tokens: %s\n", meta.downloadTokens.c_str());
 //  Serial.printf("Download URL: %s\n\n", fbdo.downloadURL().c_str());
    Firebase.RTDB.setString(&fbdo, "/Device_1/image/link", fbdo.downloadURL().c_str());
  } else if (info.status == firebase_fcs_upload_status_error) {
 //  Serial.printf("Upload failed, %s\n", info.errorMsg.c_str());
    Firebase.RTDB.setString(&fbdo, "/Device_1/image/capture", "gặp lỗi trong quá trình tải ảnh lên");
  }
}
uint64_t printLocalTime() {
  // Lấy thời gian hiện tại từ NTP
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
 //  Serial.println("Không lấy được thời gian từ NTP.");
    return 0;
  }


  // In thời gian hiện tại (ngày, tháng, năm, giờ, phút, giây)//
 Serial.print("Thời gian thực tế: ");


  hours = timeinfo.tm_hour;
  mins = timeinfo.tm_min;
  days = timeinfo.tm_mday;
  months = timeinfo.tm_mon + 1;
  years = timeinfo.tm_year + 1900;
  weeks = timeinfo.tm_wday;

  // Chuyển đổi sang milliseconds since epoch
  time_t epochTime = mktime(&timeinfo);  // Thời gian tính bằng giây kể từ epoch
  uint64_t millisecondsSinceEpoch = (uint64_t)epochTime * 1000;//
 Serial.print("Milliseconds since epoch: ");//
 Serial.println(millisecondsSinceEpoch);
  return millisecondsSinceEpoch;
}

int readWaterCM() {

  int distanceCm;
  float Total10times = 0;

  for (int i = 0; i < 10; i++) {

    long duration;
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    duration = pulseIn(echoPin, HIGH);

    Total10times += duration * SOUND_SPEED / 2;
    delay(500);
  }
  distanceCm = int(Total10times / 10);
//
 Serial.print("Distance (cm): ");//
 Serial.println(distanceCm);


  // Cập nhật giá trị max
  if (distanceCm > maxDistance) {
    maxDistance = distanceCm;  // Cập nhật giá trị lớn nhất
  }
  // Cập nhật giá trị max
  if (distanceCm < minDistance) {
    maxDistance = distanceCm;  // Cập nhật giá trị lớn nhất
  }
  return distanceCm;
}


void connecting() {
  digitalWrite(4, HIGH);
  delay(1000);
  digitalWrite(4, LOW);
  delay(1000);
  digitalWrite(4, HIGH);
  delay(1000);
  digitalWrite(4, LOW);
  delay(1000);
  digitalWrite(4, HIGH);
  delay(1000);
  digitalWrite(4, LOW);
  delay(1000);
}

void error() {
  digitalWrite(4, HIGH);
  delay(500);
  digitalWrite(4, LOW);
  delay(500);
  digitalWrite(4, HIGH);
  delay(500);
  digitalWrite(4, LOW);
  delay(500);
  digitalWrite(4, HIGH);
  delay(500);
  digitalWrite(4, LOW);
  delay(500);
  digitalWrite(4, HIGH);
  delay(500);
  digitalWrite(4, LOW);
  delay(500);
  digitalWrite(4, HIGH);
  delay(500);
  digitalWrite(4, LOW);
}

void saveWiFiCredentials(String& ssidInput, String& passwordInput) {
  // Lưu SSID
  for (int i = 0; i < ssidInput.length(); i++) {
    EEPROM.write(SSID_ADDR + i, ssidInput[i]);
  }
  EEPROM.write(SSID_ADDR + ssidInput.length(), '\0');  // Kết thúc chuỗi

  // Lưu mật khẩu
  for (int i = 0; i < passwordInput.length(); i++) {
    EEPROM.write(PASS_ADDR + i, passwordInput[i]);
  }
  EEPROM.write(PASS_ADDR + passwordInput.length(), '\0');  // Kết thúc chuỗi
  EEPROM.commit();                                         // Lưu thay đổi
}
void readWiFiCredentials() {
  // Đọc SSID
  for (int i = 0; i < 50; i++) {  // 50 là kích thước tối đa cho SSID
    ssid1[i] = EEPROM.read(SSID_ADDR + i);
    if (ssid1[i] == '\0') break;  // Kết thúc chuỗi
  }

  // Đọc mật khẩu
  for (int i = 0; i < 50; i++) {  // 50 là kích thước tối đa cho mật khẩu
    pass1[i] = EEPROM.read(PASS_ADDR + i);
    if (pass1[i] == '\0') break;  // Kết thúc chuỗi
  }
}