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
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *timeZoneInfo = "PST8PDT,M3.2.0,M11.1.0"; // Time zone for California (PDT)
const char *MAIL_SERVER = "smtp.gmail.com";
const char *MAIL_FROM = MAIL_SENDER; // Use the environment variable for the sender email
const char *MAIL_TO = "#@vtext.com";
const char *MAIL_SUBJECT = "TX Site SHT31-D Sensor Readings";
const char *MAIL_CONTENT = "Temperature and Humidity Readings from SHT31-D Sensor";
const char *MAIL_USER = MAIL_SENDER; // Use the environment variable for the user emai
const char *MAIL_PASS = "pw";
const int MAIL_PORT = 587; // SMTP port for Gmail TLS
const bool MAIL_USE_TLS = true;
bool timeSet = false;
int lastEmailHour = -1; // Initialize to an invalid hour

/* Declare the global used SMTPSession object for SMTP transport */
SMTPSession smtp;
/* Declare the global SMTP_Message object for email sending */
SMTP_Message message;
Session_Config config;
bool smtpReady = false;
char emailContentBuffer[256];
// Create an instance of the SHT31-D sensor object
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// Define pins for Serial2 (UART2) for the RS-232 TTL to RS232 Module
// IMPORTANT: Ensure these pins are not otherwise used and are safe to use for UART.
// GPIO 17 (TX2) and GPIO 16 (RX2) are common choices for UART2.
const int Serial2_TX_Pin = 17;
const int Serial2_RX_Pin = 16;
const long BAUD_RATE = 9600; // Match this to your PuTTY setting

bool syncTime()
{
    Serial.println("Starting time synchronization...");
    Serial2.println("Starting time synchronization...");

    // 1. Configure the ESP32 to use the correct time zone and NTP servers.
    //    The timeZoneInfo string is critical for getting local time, not just UTC.
    configTzTime(timeZoneInfo, "pool.ntp.org", "time.nist.gov", "time.google.com");

    // 2. Wait for the time to be synced.
    struct tm timeinfo;

    // getLocalTime() needs to be called to trigger the sync.
    // We check the return value and the year to confirm sync.
    // tm_year is years since 1900.
    if (!getLocalTime(&timeinfo, 10000))
    { // Give it up to 10 seconds to get an initial response
        Serial.println("Failed to get initial time response.");
        Serial2.println("Failed to get initial time response.");
        return false;
    }

    // Loop until the year is valid, indicating a successful sync.
    int retry_count = 0;
    const int max_retries = 10;
    while (timeinfo.tm_year < (2023 - 1900) && retry_count < max_retries)
    {
        Serial.printf("Waiting for NTP sync... (Attempt %d/%d)\n", retry_count + 1, max_retries);
        Serial2.printf("Waiting for NTP sync... (Attempt %d/%d)\n", retry_count + 1, max_retries);
        delay(2000); // Wait 2 seconds between checks
        if (!getLocalTime(&timeinfo))
        {
            Serial.println("Failed to get time on retry.");
            Serial2.println("Failed to get time on retry.");
        }
        retry_count++;
    }

    // 3. Check the final result.
    if (timeinfo.tm_year < (2023 - 1900))
    {
        Serial.println("ERROR: Could not synchronize time with NTP server after multiple attempts.");
        Serial2.println("ERROR: Could not synchronize time with NTP server after multiple attempts.");
        return false;
    }

    // SUCCESS!
    Serial.println("\nSUCCESS: NTP has synced.");
    Serial2.println("\nSUCCESS: NTP has synced.");
    char timeBuffer[50];
    strftime(timeBuffer, sizeof(timeBuffer), "%A, %B %d %Y %H:%M:%S %Z", &timeinfo);
    Serial.printf("Current California Time: %s\n", timeBuffer);
    Serial2.printf("Current California Time: %s\n", timeBuffer);

    return true;
}

void resyncTime()
{
    Serial.println("[System Check] Performing lightweight time resync...");
    Serial2.println("[System Check] Performing lightweight time resync...");

    // The configuration is already set from the initial syncTime() call.
    // We just need to trigger an update.
    struct tm timeinfo;

    // getLocalTime() will trigger a new NTP request.
    // We give it a short timeout (e.g., 2 seconds) to avoid blocking the loop for long.
    if (!getLocalTime(&timeinfo, 2000))
    {
        Serial.println("[System Check] Lightweight resync failed to get a response.");
        Serial2.println("[System Check] Lightweight resync failed to get a response.");
    }
    else
    {
        // We can optionally check if the year is still valid, just in case.
        if (timeinfo.tm_year < (2023 - 1900))
        {
            Serial.println("[System Check] Resync resulted in an invalid time. Marking time as not set.");
            Serial2.println("[System Check] Resync resulted in an invalid time. Marking time as not set.");
            timeSet = false; // The time is now invalid, trigger a full recovery on the next check.
        }
        else
        {
            Serial.println("[System Check] Time successfully resynchronized.");
            Serial2.println("[System Check] Time successfully resynchronized.");

            // No need to set timeSet = true, as it was already true.
        }
    }
}

void sendSensorEmail(const char *emailBody)
{
    if (!smtp.isLoggedIn())
    {
        Serial.println("SMTP session is not active. Attempting to reconnect...");
        Serial2.println("SMTP session is not active. Attempting to reconnect...");
        smtp.closeSession();

        // Reconnect using the already-known GLOBAL config.
        if (smtp.connect(&config))
        {
            Serial.println("SUCCESS: SMTP reconnected.");
            Serial2.println("SUCCESS: SMTP reconnected.");
        }
        else
        {
            Serial.println("ERROR: SMTP reconnect failed. Aborting send. Last Error: ");
            Serial.println(smtp.errorReason());
            Serial2.println("ERROR: SMTP reconnect failed. Aborting send. Last Error: ");
            Serial2.println(smtp.errorReason());
            return;
        }
    }

    message.clear();

    // 2. build the headers.
    message.sender.name = "ESD Petaluma CA";
    message.sender.email = MAIL_FROM;
    message.subject = MAIL_SUBJECT;
    message.addRecipient("ESD Petaluma CA", MAIL_TO);
    message.text.content = emailBody;

    // 3. Send the email.
    Serial.println("Sending email...");
    if (!MailClient.sendMail(&smtp, &message, true))
    {
        Serial.println("ERROR: Failed to send email. Last Error: ");
        Serial2.println("ERROR: Failed to send email. Last Error: ");
        Serial.println(smtp.errorReason());
        Serial2.println(smtp.errorReason());
    }
    else
    {
        Serial.println("Email sent successfully!");
        Serial2.println("Email sent successfully!");
    }
}

void readAndReportSensor(const struct tm &timeinfo)
{
    if (!smtpReady)
    {
        Serial.println("Skipping email: SMTP server is not connected.");
        Serial2.println("Skipping email: SMTP server is not connected.");
        return; // Exit the function immediately
    }
    float temperatureC = sht31.readTemperature();
    float humidity = sht31.readHumidity();

    if (isnan(temperatureC) || isnan(humidity))
    {
        Serial.println("ERROR: Failed to read from SHT31 sensor!");
        Serial2.println("ERROR: Failed to read from SHT31 sensor!");
        return;
    }

    float temperatureF = (temperatureC * 9 / 5) + 32;
    // Only send email if time is set and we haven't sent one this hour
    if (timeSet && timeinfo.tm_hour != lastEmailHour)
    {
        // Create the dynamic content string
        char timeBuffer[30];
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        snprintf(emailContentBuffer, sizeof(emailContentBuffer),
                 "Temperature: %.2f F | Humidity: %.2f %%\nTime of reading: %s",
                 temperatureF, sht31.readHumidity(), timeBuffer);

        // Call our new robust sending function with the content
        sendSensorEmail(emailContentBuffer);

        // If the email was sent successfully (which sendSensorEmail now handles internally), update the hour.
        // We might need to make smtpReady global to check this here, or just assume it worked for the timer.
        if (smtpReady)
        {
            lastEmailHour = timeinfo.tm_hour;
        }
    }
}
void performSensorReadingAndPrint()
{
    float temperatureC = sht31.readTemperature();
    float humidity = sht31.readHumidity();

    if (isnan(temperatureC) || isnan(humidity))
    {
        Serial.println("ERROR: Failed to read from SHT31 sensor!");
        Serial2.println("ERROR: Failed to read from SHT31 sensor!");
        return;
    }

    float temperatureF = (temperatureC * 9 / 5) + 32;

    Serial.printf("MANUAL READ -> Temp: %.2f F, Humidity: %.2f %%\n", temperatureF, humidity);
    Serial2.printf("MANUAL READ -> Temp: %.2f F, Humidity: %.2f %%\n", temperatureF, humidity);
    // Now check if we should also send an email
    // This part WILL NOT WORK if WiFi is down, which is expected.
    if (timeSet)
    {
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        // We can call your original function here to handle the email logic
        readAndReportSensor(timeinfo);
    }
    else
    {
        Serial.println("Cannot send email: WiFi is not connected or time is not set.");
        Serial2.println("Cannot send email: WiFi is not connected or time is not set.");
    }
}

void setup()
{
    // --- Setup Function ---
    // The setup() function runs once when you press reset or power the board.

    // Initialize Serial (UART0) for communication with the Arduino IDE Serial Monitor
    Serial.begin(BAUD_RATE); // Using the same baud rate for consistency
    delay(1000);             // Small delay to allow serial port to initialize
    WiFi.begin(ssid, password);

    unsigned long startAttemptTime = millis();
    const unsigned long wifiTimeout = 15000; // 15 seconds

    // Set the network reconnection option
    MailClient.networkReconnect(true);

    /** Enable the debug via Serial port
     * 0 for no debugging
     * 1 for basic level debugging
     *
     * Debug port can be changed via ESP_MAIL_DEFAULT_DEBUG_PORT in ESP_Mail_FS.h
     */
    smtp.debug(1);
    // Initialize Serial2 (UART2) for communication with the RS-232 TTL to RS232 Module
    // Format: Serial2.begin(baudrate, SERIAL_8N1, TX_pin, RX_pin);
    Serial2.begin(BAUD_RATE, SERIAL_8N1, Serial2_TX_Pin, Serial2_RX_Pin);
    Serial.print("Connecting to WiFi");
    Serial2.print("Connecting to WiFi");

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout)
    {
        delay(500);
        Serial.print(".");
    }

    // Inside setup(), after WiFi is connected...

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi connected!");
        Serial2.println("\nWiFi connected!");
        Serial.print("IP address: ");
        Serial2.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial2.println(WiFi.localIP());

        timeSet = syncTime(); // Attempt to sync time with NTP server

        // 3. ONLY NOW, ATTEMPT TO CONNECT TO SMTP
        //    (This requires time to be set correctly)
        Serial.println("Connecting to SMTP Server...");
        Serial2.println("Connecting to SMTP Server...");
        smtp.debug(1); // Enable debug
        if (timeSet)
        {
            Serial.println("Populating global SMTP configuration...");
            // NO "Session_Config config;" HERE. We are using the global one.
            config.server.host_name = MAIL_SERVER;
            config.server.port = MAIL_PORT;
            config.login.email = MAIL_FROM;
            config.login.password = MAIL_PASS;
            config.login.user_domain = ""; // For Gmail

            // Now, connect using the global config object.
            // The smtp object will store a reference to this persistent object.
            if (smtp.connect(&config))
            {
                Serial.println("SUCCESS: Connected to SMTP Server.");
                Serial2.println("SUCCESS: Connected to SMTP Server.");
                smtpReady = true;
            }
            else
            {
                Serial.println("ERROR: Failed to connect. Last Error: ");
                Serial2.println("ERROR: Failed to connect. Last Error: ");
                Serial.println(smtp.errorReason());
                Serial2.println(smtp.errorReason());
                smtpReady = false;
            }
        }
        else
        {
            Serial.println("Skipping SMTP connection: time is not set.");
            Serial2.println("Skipping SMTP connection: time is not set.");
            smtpReady = false;
        }
    }
    else
    {
        Serial.println("\nWiFi connection failed. Continuing without WiFi.");
        Serial2.println("\nWiFi connection failed. Continuing without WiFi.");
        
    }

    Serial.println("--- ESP32 (IDE Monitor) ---");
    Serial.println("ESP32 Temperature and Humidity Sensor Ready (SHT31-D).");
    Serial.println("Type 'r' or 'R' in Serial Monitor to current get readings.");

    
    Serial2.println("--- ESP32 (RS-232 Module) ---");
    Serial2.println("RS-232 Serial Link Active (SHT31-D Readings will appear here).");
    Serial2.println("Type 'r' or 'R' in Serial session to current get readings.");

    // Initialize I2C communication for the SHT31-D sensor
    // Default I2C pins for ESP32 are GPIO 21 (SDA) and GPIO 22 (SCL).
    Wire.begin();

    // Check if the SHT31-D sensor is found and initialized
    if (!sht31.begin(0x44))
    {                                                          // SHT31-D's default I2C address is 0x44
        Serial.println("ERROR: Couldn't find SHT31 sensor!");  // Output to IDE Monitor
        Serial2.println("ERROR: Couldn't find SHT31 sensor!"); // Output to RS-232 Module
        while (1)
            delay(1); // Halt execution if sensor not found
    }
    Serial.println("SHT31-D sensor found and initialized!"); // Output to IDE Monitor
    // Set the message headers
    message.sender.name = "ESD Petaluma CA";
    message.sender.email = MAIL_FROM;
    message.subject = MAIL_SUBJECT;
    message.addRecipient("ESD Petaluma CA", MAIL_TO); // Add recipient
    message.text.content = MAIL_CONTENT;
    message.text.charSet = "us-ascii"; // Set character set for email content
    message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
}
void loop()
{
    // --- 1. Handle IMMEDIATE Manual Triggers ---
    if (Serial.available() > 0)
    {
        char incomingChar = Serial.read(); // Read the character ONCE
        if (incomingChar == 'r' || incomingChar == 'R')
        {
            Serial.println("Manual trigger received from Serial Monitor.");
            performSensorReadingAndPrint(); // ACT ON IT NOW
        }
    }
    if (Serial2.available() > 0)
    {
        char incomingChar2 = Serial2.read(); // Read the character ONCE
        if (incomingChar2 == 'r' || incomingChar2 == 'R')
        {
            Serial.println("Manual trigger received from RS-232.");
            performSensorReadingAndPrint(); // ACT ON IT NOW
        }
    }

    // --- 2. Handle TIMED Automatic Triggers ---
    static unsigned long lastAutomaticCheck = 0;
    const unsigned long automaticCheckInterval = 60000; // 1 minute

    if (millis() - lastAutomaticCheck >= automaticCheckInterval)
    {
        lastAutomaticCheck = millis();

        // This block only runs if WiFi/Time is working.
        if (timeSet)
        {
            float temperatureF = (sht31.readTemperature() * 9 / 5) + 32;

            struct tm timeinfo;
            getLocalTime(&timeinfo);

            bool shouldSendEmail = false;

            // Condition 1: High Temperature
            if (temperatureF > 85.0)
            {
                Serial.println("High temperature detected. Triggering automatic email.");
                Serial2.println("High temperature detected. Triggering automatic email.");
                shouldSendEmail = true;
            }

            // Condition 2: Scheduled Time
            if ((timeinfo.tm_hour == 9 || timeinfo.tm_hour == 13 || timeinfo.tm_hour == 16) && timeinfo.tm_min == 00)
            {
                Serial.println("Scheduled time reached. Triggering automatic email.");
                Serial2.println("Scheduled time reached. Triggering automatic email.");
                shouldSendEmail = true;
            }

            if (shouldSendEmail)
            {
                readAndReportSensor(timeinfo);
            }
        }
    }
    // --- 3. PERIODIC SYSTEM HEALTH & RECOVERY TASK ---
    static unsigned long lastSystemCheck = 0;
    const unsigned long systemCheckInterval = 900000; // 15 minutes

    if (millis() - lastSystemCheck >= systemCheckInterval)
    {
        lastSystemCheck = millis();

        // A) CHECK WIFI CONNECTION
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("[System Check] WiFi is disconnected. Attempting to reconnect...");
            Serial2.println("[System Check] WiFi is disconnected. Attempting to reconnect...");
            WiFi.reconnect();
        }
        // B) IF WIFI IS CONNECTED, CHECK TIME & SMTP STATUS
        else
        {
            // If time was never set, this is our chance to recover from a boot failure.
            if (!timeSet)
            {
                Serial.println("[System Check] Time not set. Attempting initial NTP sync and SMTP connection...");
                Serial2.println("[System Check] Time not set. Attempting initial NTP sync and SMTP connection...");
                timeSet = syncTime(); // Attempt to get the time
                if (timeSet)
                {
                    // SUCCESS! Now we can finally try to connect SMTP.
                    Serial.println("[System Check] Time acquired. Now attempting SMTP connect.");
                    Serial2.println("[System Check] Time acquired. Now attempting SMTP connect.");

                    // Populate the global config (it was skipped in setup)
                    config.server.host_name = MAIL_SERVER;
                    config.server.port = MAIL_PORT;
                    config.login.email = MAIL_FROM;
                    config.login.password = MAIL_PASS;
                    config.login.user_domain = "";

                    if (smtp.connect(&config))
                    {
                        Serial.println("[System Check] SUCCESS: Connected to SMTP Server.");
                        Serial2.println("[System Check] SUCCESS: Connected to SMTP Server.");
                        smtpReady = true;
                    }
                    else
                    {
                        Serial.println("[System Check] ERROR: Failed to connect to SMTP. Will retry in 15 mins.");
                        Serial2.println("[System Check] ERROR: Failed to connect to SMTP. Will retry in 15 mins.");
                        smtpReady = false;
                    }
                }
                else
                {
                    Serial.println("[System Check] NTP sync failed. Will retry in 15 mins.");
                    Serial2.println("[System Check] NTP sync failed. Will retry in 15 mins.");
                }
            }
            resyncTime(); // Perform a lightweight time resync
        }
    }
}
