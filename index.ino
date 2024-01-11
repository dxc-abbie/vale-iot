#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Time.h>
#define TIME_ZONE 8  //  Philippines: UTC+8


#define SS_PIN D4  // Define the SS (Slave Select) pin for RC522
#define RST_PIN D3 // Define the RST pin for RC522

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance


WiFiClient wifiClient;
WiFiClientSecure client;

// change the ssid and password based on WIFI network

const char *ssid = "HUAWEI-2.4G-H2ah";
const char *password = "KchqP5P6";

// define variable for date and time of access
char date_of_access[11]; // "YYYY-MM-DD\0"
char time_of_access[9]; // "HH:MM:SS\0"

String tagUID = ""; 
String formattedName = "";

// Server URL
String serverUrl = "https://vale-n93a.onrender.com";

time_t now;
time_t nowish = 1510592825;

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

void getDetailsFromRecords(const String &tagUID) {
  // ignore certificates 
  client.setInsecure();

  HTTPClient http;

  String url = serverUrl + "/person?searchBy=person_id&searchKey=" + tagUID  ;
  Serial.println(url);

  http.begin(client, url); // Specify the URL

  int httpResponseCode = http.GET(); // Make the request
  Serial.println(httpResponseCode);

  // Check for a successful response
  if (httpResponseCode > 0) {
      if (httpResponseCode == HTTP_CODE_OK) {
          String payload = http.getString(); // Get the response payload
          Serial.println("Server Response: " + payload);

               // Parse JSON response
          DynamicJsonDocument doc(1024); // Adjust the size accordingly
          deserializeJson(doc, payload);

          // Extract values
          String personId = doc["person_id"];
          String firstName = doc["first_name"];
          String middleName = doc["middle_name"];
          String lastName = doc["last_name"];

          // Format the name
          formattedName = lastName + ", " + firstName;
          if (middleName.length() > 0) {
            formattedName += " " + middleName;
          }

      } else {
          Serial.println("HTTP Request Failed");
      }
    } else {
        Serial.println("Unable to connect to server");
        Serial.println("Error: " + String(http.errorToString(httpResponseCode).c_str()));
  }
  http.end(); // Close connection
}

void postDataToServer(const String &person_id, const String &date, const String &time, const String &location_id, float temperature)
{
  // ignore certificates 
  client.setInsecure();

  HTTPClient http;

  // Create a JSON document
  DynamicJsonDocument doc(256);
  doc["person_id"] = person_id;
  doc["date_of_access"] = date;
  doc["time_of_access"] = time;
  doc["location_id"] = location_id;

  
  // Format temperature to one decimal place
  char tempStr[5]; // Assuming temperature won't exceed 999.9
  dtostrf(temperature, 4, 1, tempStr);
  doc["temperature"] = tempStr;


  // Serialize the JSON document to a string
  String payload;
  serializeJson(doc, payload);

  // Start the HTTP request
  http.begin(client, serverUrl + "/records");
  http.addHeader("Content-Type", "application/json");

  Serial.print("Payload: ");
  Serial.println(payload);
  // Send the payload
  int httpResponseCode = http.POST(payload);

  // Check for a successful response
  if (httpResponseCode > 0) {
      if (httpResponseCode == HTTP_CODE_OK) {
          String payload = http.getString(); // Get the response payload
          Serial.println("Server Response: " + payload);
      } else {
          Serial.println("HTTP POST Request Failed");
      }
    } else {
        Serial.println("Unable to connect to server");
        Serial.println("Error: " + String(http.errorToString(httpResponseCode).c_str()));
  }

  // End the request
  http.end();
}


void NTPConnect(String &date_of_access, String &time_of_access)
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
  if (timeinfo.tm_hour >= 24)
  {
    timeinfo.tm_hour -= 24;
    timeinfo.tm_mday += 1;
  }

  char date_buffer[11]; // "YYYY-MM-DD\0"
  char time_buffer[9]; // "HH:MM:SS\0"

  strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%d", &timeinfo);
  strftime(time_buffer, sizeof(time_buffer), "%H:%M:%S", &timeinfo);

  date_of_access = String(date_buffer);
  time_of_access = String(time_buffer);
}

void setup() {
  Serial.begin(115200);
  SPI.begin();        // Initiate SPI bus
  mfrc522.PCD_Init(); // Initiate MFRC522

  // connectToWifi 
  connectToWiFi();
  
  String date;
  String time;
  NTPConnect(date, time);

  // get time
  // Serial.print("Current time: ");
  Serial.print("Date of access: ");
  Serial.println(date);
  Serial.print("Time of access: ");
  Serial.println(time);
  // NTPConnect();

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
    Serial.println(content);
    
    // Get date and time format
    String date;
    String time;
    NTPConnect(date, time);

    // get details from records table
    getDetailsFromRecords(content);

    // Send data to server
    postDataToServer(content, date, time, "LR002-EXT", 36.1);
    

    delay(1000);
  }
}
