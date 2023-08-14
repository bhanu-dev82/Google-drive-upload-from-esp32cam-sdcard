#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Base64.h"
#include "esp_sleep.h"

#include "esp_camera.h"

#include <SD_MMC.h> // Include the SD_MMC card library

const char* ssid     = "Pixel 6";   // your network SSID
const char* password = "password";   // your network password
const char* myDomain = "script.google.com";
String myScript = "/macros/s/----------/exec";    // Replace with your own URL whole line after .com
String myFilename = "filename=ESP32-CAM.jpg";
String mimeType = "&mimetype=image/jpeg";
String myImage = "&data=";

const int BUTTON_PIN = 13; //button pin
int buttonState = 0;

int imageCount = 1;

int waitingTime = 30000; // Wait 30 seconds for the Google Drive response.

void setup()
{ 
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  delay(10);
  
  WiFi.mode(WIFI_STA);

  Serial.println("");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);  

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("STAIP address: ");
  Serial.println(WiFi.localIP());
    
  Serial.println("");

  //initialize camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 8;
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  pinMode(4, OUTPUT);              //GPIO for LED flash
  digitalWrite(4, LOW);            //turn OFF flash LED
  gpio_hold_en(GPIO_NUM_4);    //make sure flash is held LOW in sleep


  //initialize SD card
  if (!SD_MMC.begin("/root", true, false, SDMMC_FREQ_DEFAULT)) {
    Serial.println("SD card initialization failed");
    return;
  }
  //initialize button pin
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
}

boolean enviar = true;

void loop() {
  //check for button press
  buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == LOW ) {
    // Button is pressed, upload the photo to Google Drive from SD_MMC card
    Serial.println("Button pressed : preparing upload");
    uploadPhotoFromSD_MMCCard();
    enviar = false;
    delay(3000); // Wait for a short time before re-checking the button
  } else {
    // Button is not pressed, capture and save photos to SD_MMC card
    Serial.println("No Button pressed : Capturing photos");
    captureAndSavePhotos();
    delay(2000); // A short delay before capturing the next photo
    enviar = true;
  }
}

void uploadPhotoFromSD_MMCCard() {
  Serial.println("Connect to " + String(myDomain));
  WiFiClientSecure client;
  client.setInsecure();
  
  if (client.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
    File photoFile;
    for (int i = 1; i <= 6; i++) {
      String filename = "/ESP32-CAM/" + String(i) + ".jpg";
      photoFile = SD_MMC.open(filename, FILE_READ);
      
      if (!photoFile) {
        Serial.println("Failed to open photo file: " + filename);
        continue; // Move to the next photo if the current one cannot be opened
      }
      
      Serial.println("Uploading photo: " + filename);
      
      char output[base64_enc_len(3)];
      String imageFile = "";
      
      while (photoFile.available()) {
        byte buffer[3] = {0};
        int bytesRead = photoFile.read(buffer, 3);
        base64_encode(output, (char*)buffer, bytesRead);
        imageFile += urlencode(String(output));
      }
      
      photoFile.close();
      
      String Data = myFilename + mimeType + myImage;
    
      Serial.println("Send a photo to Google Drive.");
      
      client.println("POST " + myScript + " HTTP/1.1");
      client.println("Host: " + String(myDomain));
      client.println("Content-Length: " + String(Data.length() + imageFile.length()));
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.println();
      
      client.print(Data);
      int Index;
      for (Index = 0; Index < imageFile.length(); Index = Index + 1000) {
        client.print(imageFile.substring(Index, Index + 1000));
      }
      
      Serial.println("Waiting for response.");
      long int StartTime = millis();
      while (!client.available()) {
        Serial.print(".");
        delay(100);
        if ((StartTime + waitingTime) < millis()) {
          Serial.println();
          Serial.println("No response.");
          // If you have no response, maybe need a greater value of waitingTime
          break;
        }
      }
      Serial.println();
      while (client.available()) {
        Serial.print(char(client.read()));
      }
      
      delay(1000); // Wait for a short time before uploading the next photo
    }
  } else {         
    Serial.println("Connected to " + String(myDomain) + " failed.");
  }
  client.stop();
}

void captureAndSavePhotos() {
  
  //capture image
  camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      delay(1000);
      ESP.restart();
      return;
    }

    //rename image
    String imageName = String(imageCount) + ".jpg";

    //write image to file
  File file = SD_MMC.open("/ESP32-CAM/" + imageName, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }

    file.write(fb->buf, fb->len);
    file.close();
    esp_camera_fb_return(fb);

    Serial.println("Photo " + imageName + " saved to SD card ");

    //increment image count, if greater than 5 reset count
    imageCount++;
    if (imageCount == 7) {
    imageCount = 1;
    }

}

//https://github.com/zenmanenergy/ESP8266-Arduino-Examples/
String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
}
