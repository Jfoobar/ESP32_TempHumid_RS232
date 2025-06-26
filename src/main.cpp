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
const char* MAIL_FROM = "Email@gmail.com";
const char* MAIL_TO = "Email@gmail.com";
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
bool smtpReady = false; 

// Create an instance of the SHT31-D sensor object
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// Define pins for Serial2 (UART2) for the RS-232 TTL to RS232 Module
// IMPORTANT: Ensure these pins are not otherwise used and are safe to use for UART.
// GPIO 17 (TX2) and GPIO 16 (RX2) are common choices for UART2.
const int Serial2_TX_Pin = 17;
const int Serial2_RX_Pin = 16;
const long BAUD_RATE = 9600; // Match this to your PuTTY setting

void readAndReportSensor(const struct tm& timeinfo) {
     if (!smtpReady) {
        Serial.println("Skipping email: SMTP server is not connected.");
        return; // Exit the function immediately
    }
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
            smtpReady = false; // Mark connection as stale/broken
        } else {
            Serial.println("Email sent successfully!");
            Serial2.println("Email sent successfully!");
            lastEmailHour = timeinfo.tm_hour; // Update last email hour
        }
    }
}
void performSensorReadingAndPrint() {
    float temperatureC = sht31.readTemperature();
    float humidity = sht31.readHumidity();

    if (isnan(temperatureC) || isnan(humidity)) {
        Serial.println("ERROR: Failed to read from SHT31 sensor!");
        Serial2.println("ERROR: Failed to read from SHT31 sensor!");
        return;
    }

    float temperatureF = (temperatureC * 9 / 5) + 32;

    Serial.printf("MANUAL READ -> Temp: %.2f F, Humidity: %.2f %%\n", temperatureF, humidity);
    Serial2.printf("MANUAL READ -> Temp: %.2f F, Humidity: %.2f %%\n", temperatureF, humidity);
    // Now check if we should also send an email
    // This part WILL NOT WORK if WiFi is down, which is expected.
    if (timeSet) {
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        // We can call your original function here to handle the email logic
        readAndReportSensor(timeinfo); 
    } else {
        Serial.println("Cannot send email: WiFi is not connected or time is not set.");
    }
}



void setup() {
  // --- Setup Function ---
  // The setup() function runs once when you press reset or power the board.
  

  // Initialize Serial (UART0) for communication with the Arduino IDE Serial Monitor
  Serial.begin(BAUD_RATE); // Using the same baud rate for consistency
  delay(1000); // Small delay to allow serial port to initialize
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  const unsigned long wifiTimeout = 15000; // 15 seconds

  //  struct tm timeinfo = {0};
  //  time_t epochTime = mktime(&timeinfo);
  //  struct timeval tv = { .tv_sec = epochTime };
  //  settimeofday(&tv, NULL);
   
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

// Inside setup(), after WiFi is connected...

if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // 2. NOW, GET THE TIME. THIS MUST HAPPEN BEFORE SMTP CONNECT.
      Serial.println("Configuring time from NTP server...");
      configTzTime(timeZoneInfo, "pool.ntp.org", "time.nist.gov");
      struct tm timeinfo;
      unsigned long startSync = millis();
      while (!getLocalTime(&timeinfo, 500) || timeinfo.tm_year < (2023 - 1900)) {
          Serial.print(".");
          if (millis() - startSync > 15000) {
              Serial.println("\nERROR: Timeout waiting for NTP sync.");
              break; 
          }
          delay(500);
      }
      
      // If we got the time, set the timeSet flag
      if (timeinfo.tm_year > (2023 - 1900)) {
          Serial.println("\nSUCCESS: NTP has synced. System time is set.");
          timeSet = true;
      }

      // 3. ONLY NOW, ATTEMPT TO CONNECT TO SMTP
      //    (This requires time to be set correctly)
      Serial.println("Connecting to SMTP Server...");
      smtp.debug(1); // Enable debug
      Session_Config config;
      // ... fill in your config details ...
      config.server.host_name = MAIL_SERVER;
      config.server.port = MAIL_PORT;
      config.login.email = MAIL_FROM;
      config.login.password = MAIL_PASS;

      if (!smtp.connect(&config)) {
        Serial.println("ERROR: Failed to connect to SMTP server. Email sending will be disabled.");
        smtpReady = false; // Set our flag to false
        } else {
        Serial.println("SUCCESS: Connected to SMTP Server.");
        smtpReady = true; // Set our flag to true
    }   

  } else {
      Serial.println("\nWiFi connection failed. Continuing without WiFi.");
  }


  Serial.println("--- ESP32 (IDE Monitor) ---");
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
       // --- 1. Handle IMMEDIATE Manual Triggers ---
    if (Serial.available() > 0) {
        char incomingChar = Serial.read(); // Read the character ONCE
        if (incomingChar == 'r' || incomingChar == 'R') {
            Serial.println("Manual trigger received from Serial Monitor.");
            performSensorReadingAndPrint(); // ACT ON IT NOW
        }
    }
    if (Serial2.available() > 0) {
        char incomingChar2 = Serial2.read(); // Read the character ONCE
        if (incomingChar2 == 'r' || incomingChar2 == 'R') {
            Serial.println("Manual trigger received from RS-232.");
            performSensorReadingAndPrint(); // ACT ON IT NOW
        }
    }

    // --- 2. Handle TIMED Automatic Triggers ---
    static unsigned long lastAutomaticCheck = 0;
    const unsigned long automaticCheckInterval = 60000; // 1 minute

    if (millis() - lastAutomaticCheck >= automaticCheckInterval) {
        lastAutomaticCheck = millis();

        // This block only runs if WiFi/Time is working.
        if (timeSet) {
            float temperatureF = (sht31.readTemperature() * 9 / 5) + 32;
            
            struct tm timeinfo;
            getLocalTime(&timeinfo);
            
            bool shouldSendEmail = false;

            // Condition 1: High Temperature
            if (temperatureF > 85.0) {
                Serial.println("High temperature detected. Triggering automatic email.");
                shouldSendEmail = true;
            }

            // Condition 2: Scheduled Time
            if ((timeinfo.tm_hour == 9 || timeinfo.tm_hour == 13 || timeinfo.tm_hour == 16) && timeinfo.tm_min == 0) {
                Serial.println("Scheduled time reached. Triggering automatic email.");
                shouldSendEmail = true;
            }
            
            if (shouldSendEmail) {
                readAndReportSensor(timeinfo);
            }
        }
    }
    // Periodic NTP sync 
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
    }else {
      // Still waiting for NTP, will retry in 1 hour
      Serial.println("ERROR: Failed to get local time from NTP server.");
    }
    lastNTPSync = millis();
  }
}
