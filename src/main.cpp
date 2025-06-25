// --- Library Includes ---
// Wire.h is for I2C communication, required by the SHT31-D sensor.
#include <Wire.h>
// Adafruit_Sensor.h is a foundational library for many Adafruit sensors.
#include <Adafruit_Sensor.h>
// Adafruit_SHT31.h is the specific library for the SHT31-D sensor.
#include <Adafruit_SHT31.h>

#include <WiFi.h>
#include <time.h>
#include <ESP_Mail_Client.h>

// --- Variable Declarations ---
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* timeZoneInfo = "PST8PDT,M3.2.0,M11.1.0"; // Time zone for California (PDT)
const char* MAIL_SERVER = "smtp.gmail.com";
const char* MAIL_FROM = "ESDPetalumaCA@gmail.com";
const char* MAIL_TO = "ESDPetalumaCA@gmail.com";
const char* MAIL_SUBJECT = "SHT31-D Sensor Readings";
const char* MAIL_CONTENT = "Temperature and Humidity Readings from SHT31-D Sensor";
const char* MAIL_USER = "ESDPetalumaCA@gmail.com";
const char* MAIL_PASS = MAIL_PASSWORD;
const int MAIL_PORT = 587; // SMTP port for Gmail TLS
const bool MAIL_USE_TLS = true;
bool timeSet = false;
int lastEmailHour = -1; // Initialize to an invalid hour

/* Declare the global used SMTPSession object for SMTP transport */
SMTPSession smtp;

/* Declare the global SMTP_Message object for email sending */
SMTP_Message message;

// Create an instance of the SHT31-D sensor object
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// Define pins for Serial2 (UART2) for the RS-232 TTL to RS232 Module
// IMPORTANT: Ensure these pins are not otherwise used and are safe to use for UART.
// GPIO 17 (TX2) and GPIO 16 (RX2) are common choices for UART2.
const int Serial2_TX_Pin = 17;
const int Serial2_RX_Pin = 16;
const long BAUD_RATE = 9600; // Match this to your PuTTY setting

void readAndReportSensor(const struct tm& timeinfo) {
    float temperatureC = sht31.readTemperature();
    float humidity = sht31.readHumidity();

    if (isnan(temperatureC) || isnan(humidity)) {
        Serial.println("ERROR: Failed to read from SHT31 sensor!");
        Serial2.println("ERROR: Failed to read from SHT31 sensor!");
        return;
    }

    float temperatureF = (temperatureC * 9 / 5) + 32;
    // Only send email if time is set and we haven't sent one this hour
    if (timeSet && timeinfo.tm_hour != lastEmailHour) {
        char timeBuffer[30];
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        Serial.println("Sending email with sensor data...");
        char emailBuffer[256];
        snprintf(emailBuffer, sizeof(emailBuffer),
                 "Temperature: %.2f C | %.2f F | Humidity: %.2f %%\nTime of reading: %s",
                 temperatureC, temperatureF, humidity, timeBuffer);
                 message.text.content = emailBuffer;

        if (!MailClient.sendMail(&smtp, &message)) {
            Serial.println("ERROR: Failed to send email.");
            Serial2.println("ERROR: Failed to send email.");
        } else {
            Serial.println("Email sent successfully!");
            Serial2.println("Email sent successfully!");
            lastEmailHour = timeinfo.tm_hour; // Update last email hour
        }
    }
}

void setup() {
  // --- Setup Function ---
  // The setup() function runs once when you press reset or power the board.
  

  // Initialize Serial (UART0) for communication with the Arduino IDE Serial Monitor
  Serial.begin(BAUD_RATE); // Using the same baud rate for consistency
  delay(100); // Small delay to allow serial port to initialize
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  const unsigned long wifiTimeout = 10000; // 10 seconds

   struct tm timeinfo = {0};
   time_t epochTime = mktime(&timeinfo);
   struct timeval tv = { .tv_sec = epochTime };
   settimeofday(&tv, NULL);
   
  //Set the network reconnection option
  MailClient.networkReconnect(true);

  /** Enable the debug via Serial port
   * 0 for no debugging
   * 1 for basic level debugging
   *
   * Debug port can be changed via ESP_MAIL_DEFAULT_DEBUG_PORT in ESP_Mail_FS.h
   */
  smtp.debug(1);
  /* Declare the Session_Config for user defined session credentials */
  Session_Config config;

  /* Set the session config */
  config.server.host_name = MAIL_SERVER;
  config.server.port = MAIL_PORT;
  config.login.email = MAIL_FROM;
  config.login.password = MAIL_PASS;
  config.login.user_domain = "";

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout) {
      delay(500);
      Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi connected!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.println("NTP time will be set using WiFi connection.");
      configTzTime(timeZoneInfo, "pool.ntp.org");
      struct tm timeinfo;
      if (getLocalTime(&timeinfo)) {
        Serial.println("SUCCESS: Setup NTP has synced. System time has been set.");
        timeSet = true;
        Serial.print("Current RTC Time: ");
        Serial.print(timeinfo.tm_hour);
        Serial.print(":");
        Serial.print(timeinfo.tm_min);
        Serial.print(":");
        Serial.println(timeinfo.tm_sec);
      } else {
        Serial.println("ERROR: Setup Failed to get local time from NTP server.");
      }
  } else {
      Serial.println("\nWiFi connection failed. Continuing without WiFi...");
  }

  Serial.println("--- ESP32 (Arduino IDE Monitor) ---");
  Serial.println("ESP32 Temperature and Humidity Sensor Ready (SHT31-D).");
  Serial.println("Type 'r' or 'R' in Serial Monitor to current get readings.");

  // Initialize Serial2 (UART2) for communication with the RS-232 TTL to RS232 Module
  // Format: Serial2.begin(baudrate, SERIAL_8N1, TX_pin, RX_pin);
  Serial2.begin(BAUD_RATE, SERIAL_8N1, Serial2_TX_Pin, Serial2_RX_Pin);
  Serial2.println("--- ESP32 (RS-232 Module) ---");
  Serial2.println("RS-232 Serial Link Active (SHT31-D Readings will appear here).");
  Serial2.println("Type 'r' or 'R' in Serial session to current get readings.");

  // Initialize I2C communication for the SHT31-D sensor
  // Default I2C pins for ESP32 are GPIO 21 (SDA) and GPIO 22 (SCL).
  Wire.begin();

  // Check if the SHT31-D sensor is found and initialized
  if (! sht31.begin(0x44)) {   // SHT31-D's default I2C address is 0x44
    Serial.println("ERROR: Couldn't find SHT31 sensor!"); // Output to IDE Monitor
    Serial2.println("ERROR: Couldn't find SHT31 sensor!"); // Output to RS-232 Module
    while (1) delay(1); // Halt execution if sensor not found
  }
  Serial.println("SHT31-D sensor found and initialized!"); // Output to IDE Monitor
  // Set the message headers
  message.sender.name = "ESD Petaluma CA";
  message.sender.email = MAIL_FROM;
  message.subject = MAIL_SUBJECT;
  message.addRecipient("ESD Petaluma CA", MAIL_TO); // Add recipient
  message.text.charSet = "utf-8";
  message.text.content_type = "text/plain";
  message.text.content = MAIL_CONTENT; 

}
void loop() {
  // --- Loop Function ---
  // The loop() function runs repeatedly forever after setup() finishes.
  // Flag to determine if a sensor reading should occur
  bool triggerSensorRead = false;

  // --- Check for Serial Console Input (from Arduino IDE Monitor) ---
  if (Serial.available() > 0) {
    char incomingChar = Serial.read();
    if (incomingChar == 'r' || incomingChar == 'R') {
      triggerSensorRead = true;
    }
  }

  // --- Check for Serial2 Console Input (from RS-232 Module / PuTTY) ---
  // If you also want to trigger readings by typing 'r' into PuTTY:
    if (Serial2.available() > 0) {
    char incomingChar2 = Serial2.read();
    if (incomingChar2 == 'r' || incomingChar2 == 'R') {
      triggerSensorRead = true;
    }
  }

  // Attempt to sync time with NTP server if WiFi is connected once an hour
  static unsigned long lastNTPSync = 0;
  const unsigned long NTPimeout = 3600000; // 1 hour in milliseconds
  if (WiFi.status() == WL_CONNECTED && millis() - lastNTPSync > NTPimeout) {
    Serial.println("Attempting to sync time with NTP server...");
    configTzTime(timeZoneInfo, "pool.ntp.org ","time.nist.gov", "time.google.com");
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      Serial.println("SUCCESS: NTP has synced. System time has been set.");
      Serial.print("Current RTC Time: ");
      Serial.print(timeinfo.tm_hour);
      Serial.print(":");
      Serial.print(timeinfo.tm_min);
      Serial.print(":");
      Serial.println(timeinfo.tm_sec);
      timeSet = true; // Set a flag to indicate time is set
      if ((timeinfo.tm_hour == 9 || timeinfo.tm_hour == 13 || timeinfo.tm_hour == 16) && timeinfo.tm_min == 0) {
      triggerSensorRead = true; // Trigger reading at these specific times
      }
    }else {
      // Still waiting for NTP, will retry in 1 hour
      Serial.println("ERROR: Failed to get local time from NTP server.");
    }
    lastNTPSync = millis();
  }
    
  // --- Sensor Reading and Print Logic (Execute if any trigger is active) ---
  if (triggerSensorRead) {
    float temperatureC = sht31.readTemperature();
    float humidity = sht31.readHumidity();
    if (isnan(temperatureC) || isnan(humidity)) {
      Serial.println("ERROR: Failed to read from SHT31 sensor!");
      Serial2.println("ERROR: Failed to read from SHT31 sensor!");
    } else {
      float temperatureF = (temperatureC * 9 / 5) + 32;

      // Output to Arduino IDE Serial Monitor
      Serial.print("Temperature: ");
      Serial.print(temperatureC);
      Serial.print(" C | ");
      Serial.print(temperatureF);
      Serial.print(" F | Humidity: ");
      Serial.print(humidity);
      Serial.println(" % (IDE Triggered)"); // Indicate source of trigger for debug

      // Output to RS-232 TTL to RS232 Module (PuTTY)
      Serial2.print("Temperature: ");
      Serial2.print(temperatureC);
      Serial2.print(" C | ");
      Serial2.print(temperatureF);
      Serial2.print(" F | Humidity: ");
      Serial2.print(humidity);
      Serial2.println(" % (RS-232 Triggered)"); // Indicate source of trigger for debug

      // send email if time is set and reading is successful
      if(timeSet) {
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        readAndReportSensor(timeinfo);
      }
    }
  } // End of triggered sensor reading logic

  // Don't read temperature more than once per 2 minutes without a trigger
  static unsigned long lastReadTime = 0;
  unsigned long currentMillis = millis();
  float temperatureC = sht31.readTemperature();
  float humidity = sht31.readHumidity();
  if (isnan(temperatureC) || isnan(humidity)) {
    Serial.println("ERROR: Failed to read from SHT31 sensor!");
    Serial2.println("ERROR: Failed to read from SHT31 sensor!");
  } else {
    float temperatureF = (temperatureC * 9 / 5) + 32;
    if (currentMillis - lastReadTime >= 120000) { // 2 minutes in milliseconds
      lastReadTime = currentMillis; // Update last read time
      // Check if the temperature is above 85F and time is set before reading
      if (temperatureF > 85.0 && timeSet) {
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        readAndReportSensor(timeinfo);
      }
    }
  }
  // Add a small delay to avoid overwhelming the Serial output
  delay(1000); 
} // End of loop function




