#include <ESP_SSD1306.h>
#include <Adafruit_GFX.h>
#include <SPI.h>            // For SPI comm (needed for not getting compile error)
#include <Wire.h>           // For I2C comm, but needed for not getting compile error
#include <NTPClient.h>
#include <Arduino.h>
#include <ets_sys.h>
#include <osapi.h>
#include <pins_arduino.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

#define os_timer_t  ETSTimer
#define DHT_PORT D5

/*
    HardWare OLED ESP8266 I2C pins
    Default SCL -- D1
    Default SDA -- D2
*/

// Pin definitions
#define OLED_RESET  16  // Pin 15 -RESET digital signal


// Display demo definitions
#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2
#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16

#define NTP_OFFSET   -5 * 60 * 60      // In seconds
#define NTP_INTERVAL 0                 // always update on calling update()
#define NTP_ADDRESS  "0.north-america.pool.ntp.org"

#define IPADDR "192.168.1.103"
#define PORT 12290
#define IDLEN 7
#define TIMEOUT 800
#define WIFITOUT 30

void clock(void *arg);
void display_time(uint32_t seconds);
void display_temperature(float h, float t);
void timecorrect(void *arg);
void log2Server(void *arg);
void watchdog(void *arg);


ESP_SSD1306 display(OLED_RESET); // FOR I2C
DHT dht(DHT_PORT, DHT11); // the temperature sensor
WiFiUDP ntpUDP; // UDP for NTP
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);
WiFiClient client;
char id[IDLEN + 1];

bool flag = false, update_screen = true, update_time = false, update_temp = false,
     log_flag = false, log_ok = false, watch_dog = true, server_ok = true, wifi_ok = false;
os_timer_t timer, ntp_up_timer, log_timer, watch_dog_timer;
uint32_t prev_timer = 1, seconds = 0, day;
float h, t;
int watch_counter = 0;
String days[7] = {"Sun ", "Mon ", "Tue ", "Wed ", "Thr ", "Fri ", "Sat "};

void init_timer () {
    os_timer_disarm(&timer);
    os_timer_setfn(&timer, clock, NULL);
    os_timer_arm(&timer, 500, true);

    os_timer_disarm(&ntp_up_timer);
    os_timer_setfn(&ntp_up_timer, timecorrect, NULL);
    os_timer_arm(&ntp_up_timer, 1000 * 3600, true);

    os_timer_disarm(&log_timer);
    os_timer_setfn(&log_timer, log2Server, NULL);
    os_timer_arm(&log_timer, 1000, true);

    os_timer_disarm(&watch_dog_timer);
    os_timer_setfn(&watch_dog_timer, watchdog, NULL);
    os_timer_arm(&watch_dog_timer, 4000, true);
}

void setup ()
{
    // Start Serial
    Serial.begin(9600);
    Serial.println("started");

    EEPROM.begin(100);

    // SSD1306 Init
    display.begin(SSD1306_SWITCHCAPVCC);  // Switch OLED
    display.setTextSize(1);
    display.clearDisplay();
    display.display();

    dht.begin();

    WiFi.mode(WIFI_STA);
    delay(500);

    do {
        int wificlock = 0;
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
            if (wificlock ++ > WIFITOUT) {
                server_ok = false;
                break;
            }
        }

        if (WiFi.status() == WL_CONNECTED) {
                Serial.println("");
                Serial.println("WiFi connected");
                Serial.println("IP address: ");
                Serial.println(WiFi.localIP());
                wifi_ok = true;
                break;
        }
    } while(WiFi.beginSmartConfig());

    timeClient.begin();
    timeClient.update();
    seconds = timeClient.getSeconds() + timeClient.getMinutes() * 60 +
              timeClient.getHours() * 3600;
    day = timeClient.getDay();
    Serial.println("Time updated");
    Serial.println(timeClient.getFormattedTime());
    init_timer();  // lastly initialize the timers
}

void loop () {

    if (update_screen) {
        display.display();
        noInterrupts();
        update_screen = false;
        interrupts();
    }

    if (wifi_ok && update_time) {

        timeClient.update();
        Serial.println("Time updated");
        noInterrupts();
        seconds = timeClient.getSeconds() + timeClient.getMinutes() * 60 +
                  timeClient.getHours() * 3600;
        day = timeClient.getDay();
        update_time = false;
        interrupts();
    }

    if (update_temp) {
        /**
            Need to modify the DHT library to avoid too much nan.
            add __attribute__((section(".iram.text"))) to DHT::read and DHT::expectPulse
            see the discussion here: http://www.esp8266.com/viewtopic.php?f=32&t=4210&p=57237#p57237
        **/
        float loc_t = dht.readTemperature();
        float loc_h = dht.readHumidity();

        noInterrupts();
        t = loc_t;
        h = loc_h;
        update_temp = false;
        interrupts();
    }

    if (wifi_ok && log_flag && log_ok && server_ok) {

        String msg = String(id) + "," + String(t, 2) + "," + String(h, 2);
        WiFiClient client;
        IPAddress ipAddr;
        ipAddr.fromString(IPADDR);
        Serial.println("Trying to log");
        if (client.connect(ipAddr, PORT)) {
            client.print(msg);
            Serial.println("Logged");

            noInterrupts();
            watch_dog = true;
            interrupts();
        }

        log_flag = false;
    }

    if (wifi_ok && !log_ok && server_ok) {     // load the id from EEPROM or get one from server
        uint8_t value = EEPROM.read(0);

        if (value == 0xEF) {
            for (int i = 0; i < IDLEN; i++) {
                id[i] = EEPROM.read(i + 1);
            }

            Serial.println("ID is: " + String(id));
        } else {

            WiFiClient client;
            IPAddress ipAddr;
            ipAddr.fromString(IPADDR);
            client.connect(ipAddr, PORT);

            client.print("join");

            unsigned long timeout = millis();
            while (client.available() == 0) {
                if (millis() - timeout > TIMEOUT) {
                    Serial.println("Client Timeout !");
                    client.stop();
                    return;
                }
            }

            client.read((uint8_t *)id, IDLEN);

            for (int i = 0; i < IDLEN; i++) {
                EEPROM.write(i + 1, id[i]);
            }

            EEPROM.write(0, 0xEF);
            EEPROM.commit();
            Serial.println("new ID is: " + String(id));
        }
        log_ok = true;
        id[IDLEN] = '\0';
    }

}

void display_time (uint32_t seconds) {
    display.setTextSize(0);
    char s[9] = {0};
    int hours = seconds / 3600;
    int mins  = (seconds -= (hours * 3600)) / 60;
    int secs  = (seconds -= (mins * 60));
    s[0] = hours / 10 + '0';
    s[1] = hours % 10 + '0';
    s[3] = mins / 10 + '0';
    s[4] = mins % 10 + '0';
    s[6] = secs / 10 + '0';
    s[7] = secs % 10 + '0';
    s[2] = s[5] = ':';
    display.setCursor(55, 0);
    display.print(days[day]);
    display.println(String(s));
}

void display_temperature (float h, float t) {
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(1,0);
    display.println("");
    display.println(String(t) + " C");
    display.println(String(h) + " %");
    Serial.print("h = " + String(h));
    Serial.println(", t = " + String(t));
}

void log2Server (void *arg) {
    log_flag = true;
}

void clock (void *arg) {
    if (flag) {
        seconds ++;
        seconds %= 86400;

        display.clearDisplay();

        // update the time
        prev_timer = seconds;
        display_time(seconds);

        // display temperature
        display_temperature(h, t);

        if (!server_ok) {
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.println("");
            display.println(">>>Server Off Line<<<");
        } else if (!log_ok) {
            display.setTextSize(1);
            display.setTextColor(WHITE);
            display.println("");
            display.println(">>>Needs new ID<<<");
        }
        update_screen = true;
    } else {
        update_temp = true;
    }
    flag = !flag;
}

void timecorrect (void *arg) {
    update_time = true;
}

void watchdog (void *arg) {
    Serial.println("Watch dog");
    if (!watch_dog) {
        server_ok = false;
    }
    watch_dog = false;
    watch_counter ++;
    watch_counter %= 12;
    if (watch_counter == 0)
        server_ok = true;
}
