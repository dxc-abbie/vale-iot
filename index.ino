//Wiring
//RST D1
//SDA D2
//MOSI D7
//MISO D6
//SCK D5
//GND GND
//3.3V 3.3V
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Time.h>
#define TIME_ZONE 8  //  Philippines: UTC+8
#include "DHT.h"  //Library for the sensor
#define DHTTYPE DHT22 
#define DHTPIN D8 // DHT22 sensor's data line is connected to pin D8

float temperature; // define variable for temperature

#define SS_PIN 4  // Define the SS (Slave Select) pin for RC522
#define RST_PIN 5 // Define the RST pin for RC522

MFRC522 mfrc522(SS_PIN, RST_PIN); // Create MFRC522 instance

WiFiClientSecure client;

// change the ssid and password based on WIFI network

const char *ssid = "WiFI-JA-ROOM";
const char *password = "nevermeltcheese";

// define variable for date and time of access
char date_of_access[11]; // "YYYY-MM-DD\0"
char time_of_access[9]; // "HH:MM:SS\0"

String tagUID = ""; 
String formattedName = "";

// Server URL
String serverUrl = "https://vale-n93a.onrender.com";

time_t now;
time_t nowish = 1510592825;

DHT dht(DHTPIN, DHTTYPE);

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


String getLocation(String idTag) {
  if (idTag == "82BAB451") {
    return "FD001-ENT";
  } else if (idTag == "72DFD751" ) {
    return "FD001-EXT";
  } else if (idTag == "A7A6585C" ) {
    return "LR001-ENT";
  } else if (idTag == "E8E5CE82" ) {
    return "FD002-ENT";
  } else {
    return "LR002-EXT";
  } 
}
void setup() {
  Serial.begin(115200);
  SPI.begin();        // Initiate SPI bus
  mfrc522.PCD_Init(); // Initiate MFRC522
 
  // Start the temperature sensor
  dht.begin();
  
  // connectToWifi 
  connectToWiFi();
  
  // get date and time of access
  String date;
  String time;
  NTPConnect(date, time);

  Serial.print("Date of access: ");
  Serial.println(date);
  Serial.print("Time of access: ");
  Serial.println(time);

  // random number generator
  randomSeed(analogRead(A0));


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

    // // get details from records table
    // getDetailsFromRecords(content);

    // // Generate random temperature between 36 - 39
    // float randomTemperature = random(364, 389) / 10.0;
    // Serial.print("Temperature: ");
    // Serial.println(randomTemperature);

    //Store temperature value in variable temperature
    temperature = dht.readTemperature();
    Serial.print("Temperature: ");
    Serial.println(temperature);

    // get location
    String location = getLocation(content);
    Serial.print("Location: ");
    Serial.println(location);

    // Send data to server
    postDataToServer(content, date, time, location, temperature);
    

    delay(1000);
  }
}
