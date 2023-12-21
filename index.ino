#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#define TIME_ZONE 8  //  Philippines: UTC+8


#define SS_PIN D4  // Define the SS (Slave Select) pin for RC522
#define RST_PIN D3 // Define the RST pin for RC522

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance


WiFiClient wifiClient;


String ssid = "**************"; Replace with your WIFI SSID
String password = "**************"; Replace with your WIFI Password
String serverAddress = "**************"; // Change this to your server address

// Define the authorized RFID tag UID
String authorizedTagUID = "82BAB451";

time_t now;
time_t nowish = 1510592825;

// function to connect to WIFI
void connectToWiFi() {
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void sendUIDToServer(String uid) {
    HTTPClient http;

    String url = serverAddress;
    Serial.println(url);

    http.setTimeout(10000);

    http.begin(wifiClient, url); // Specify the URL

    int httpCode = http.GET(); // Make the request
    Serial.println(httpCode);

    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
          String payload = http.getString(); // Get the response payload
          Serial.println("Server Response: " + payload);
      } else {
          Serial.println("HTTP Request Failed");
      }
    } else {
        Serial.println("Unable to connect to server");
        Serial.println("Error: " + String(http.errorToString(httpCode).c_str()));
    }

    http.end(); // Close connection
}

// get the time, PH time
void NTPConnect(void)
{
  configTime(TIME_ZONE * 3600, 0, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < nowish)
  {
    delay(500);
    now = time(nullptr);
  }
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);

  timeinfo.tm_hour += TIME_ZONE;
  if(timeinfo.tm_hour >= 24) {
    timeinfo.tm_hour -= 24;
    timeinfo.tm_mday += 1;
  }
  Serial.print(asctime(&timeinfo));
}

void setup() {
  Serial.begin(115200);
  SPI.begin();        // Initiate SPI bus
  mfrc522.PCD_Init(); // Initiate MFRC522

  // connect to wifi connection 
  connectToWiFi();
  
  // Get the Current Time
  Serial.print("Current time: ");
  // call function to get the current time
  NTPConnect();
}

void loop() {
  // Look for new cards
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {

    Serial.println("Card Detected!");

    // Show UID on serial monitor
    Serial.print("UID Tag : ");
    String content = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
      content.concat(String(mfrc522.uid.uidByte[i], HEX));
    }
    // Convert to uppercase
    content.toUpperCase();
    Serial.print(content + " | Current Time: ");
    NTPConnect();

    // Check if the detected UID matches the authorized tag UID
    if (content.substring(1) == authorizedTagUID) {
      Serial.println("Access Granted");

      // Send UID to Node.js server
      sendUIDToServer(content);

    
    } else {
      Serial.println("Access Denied");
    }

    delay(1000);
  }
}
