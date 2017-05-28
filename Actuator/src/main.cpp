#include <Arduino.h>
#include <Servo.h>
#include <ets_sys.h>
#include <osapi.h>
#include <os_type.h>
#include <ESP8266WiFi.h>

#define UPPER 53
#define LOWER 81
#define IPADDR "192.168.1.103"
#define PORT 12290
#define TIMEOUT 800
#define WIFITOUT 30

int detach_timer = 0;

Servo myservo;
os_timer_t timer;

void up ();
void down ();
void clock (void *arg);

void init_timer() {
    os_timer_disarm(&timer);
    os_timer_setfn(&timer, clock, NULL);
    os_timer_arm(&timer, 1000, true);
}

void setup () {

    // set the servo signal pin to high to prevent self rotation
    pinMode(D1, OUTPUT);
    digitalWrite(D1, LOW);
    delay(1000);
    pinMode(D5, OUTPUT);
    digitalWrite(D5, HIGH);
    // Start Serial
    Serial.begin(9600);
    Serial.println("started");

    WiFi.mode(WIFI_STA);
    delay(500);

    do {
        int wificlock = 0;
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
            if (wificlock ++ > WIFITOUT) {
                break;
            }
        }

        if (WiFi.status() == WL_CONNECTED) {
                Serial.println("");
                Serial.println("WiFi connected");
                Serial.println("IP address: ");
                Serial.println(WiFi.localIP());
                break;
        }
        Serial.println(WiFi.beginSmartConfig());
    } while(1);

    init_timer();
}

void loop () {
    WiFiClient client;
    IPAddress ipAddr;
    ipAddr.fromString(IPADDR);
    client.connect(ipAddr, PORT);

    client.print("act");

    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > TIMEOUT) {
            Serial.println("Client Timeout !");
            client.stop();
            return;
        }
    }

    char ret;
    client.read((uint8_t *)&ret, 1);

    switch(ret) {
        case '0': down(); break;
        case '1': up(); break;
        default: break;
    }

    delay(1000);
}

void up () {
    if (!myservo.attached())
        myservo.attach(D1);
    myservo.write(UPPER);
    detach_timer = 3;
}

void down () {
    if (!myservo.attached())
        myservo.attach(D1);
    myservo.write(LOWER);
    detach_timer = 3;
}

void clock(void *arg) {

    if (detach_timer > 1) {
        detach_timer --;
    } else if (detach_timer == 1) {
        myservo.detach();
        detach_timer --;
    }

}
