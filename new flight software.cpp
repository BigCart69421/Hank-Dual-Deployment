// Software for the HANK R1 flight computer by Delta Aerospace
// HankOS 1.0.1

#include <Arduino.h>
#include "MS5611.h"
#include <Servo.h>
#include "SparkFun_External_EEPROM.h"
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <TinyGPS.h>

TinyGPS gps;

// Pins (keeping your original assignments)
int SCL = 5;
int SDA = 4;
int S1 = 10;
int S2 = 9;
int LED = 8;
int TX = 7;
int RX = 6;
int GTX = 1;
int GRX = 2;
int BV = A0;

float rawBV = 0;
float curedBV = 0;

// Servos
Servo mainChute;
Servo apogeeChute;
int servoLocked = 0;
int servoDeploy = 180;

// Flight variables
float groundAlt = 0;
float zeroedAlt = 0;
float velocity = 0;
float lastAlt = 0;

bool APOGEE = false;
bool LAUNCH = false;

// Delta time
unsigned long currentTime = 0;
unsigned long oldTime = 0;
float deltaTime = 0;

int counter = 0;

// Sensors
MS5611 ms5611(0x77);
ExternalEEPROM myMem;
Adafruit_NeoPixel strip(1, LED, NEO_GRB + NEO_KHZ800);

void setup() {
    Serial1.begin(9600); // Telemetry
    Serial.begin(9600);  // Debug USB
    pinMode(A0, INPUT);
    Serial2.begin(9600, SERIAL_8N1, GRX, GTX); // GPS on your pins

    if (!Wire.begin(SDA, SCL)) {
        Serial.println("I2C bus init fail!");
    }
    if (!ms5611.begin()) {
        Serial.println("Baro init fail!");
    }
    myMem.setMemoryType(512);
    if(!myMem.begin()) {
        Serial.println("EEPROM init fail");
    }

    apogeeChute.attach(S1);
    mainChute.attach(S2);
    strip.begin();
    strip.clear();
    strip.setPixelColor(0, strip.Color(0, 255, 0));
    strip.show();

    groundAlt = ms5611.getAltitude(1013.5);
    apogeeChute.write(servoLocked);
    mainChute.write(servoLocked);

    oldTime = micros();
}

void loop() {
    rawBV = analogRead(A0);
    curedBV = rawBV * 2;
    // --- Feed GPS data ---
    while (Serial2.available()) {
        char c = Serial2.read();
        gps.encode(c);
    }

    // --- Get GPS data ---
    float lat, lon;
    unsigned long age;
    gps.f_get_position(&lat, &lon, &age);
    int sats = gps.satellites();
    Serial.print("Battery Voltage: ");
    Serial.println(curedBV);
    // --- Time & Altitude ---
    currentTime = micros();
    deltaTime = (currentTime - oldTime) / 1e6; // seconds

    zeroedAlt = ms5611.getAltitude() - groundAlt;

    // --- Velocity ---
    if (deltaTime > 0) velocity = (zeroedAlt - lastAlt) / deltaTime;

    // --- Launch detection ---
    if (velocity > 10) LAUNCH = true;

    // --- Apogee detection ---
    if (velocity < 2 && LAUNCH == true && !APOGEE) {
        APOGEE = true;
        apogeeChute.write(servoDeploy);
    }

    // --- Main chute deploy ---
    if (zeroedAlt < 100 && APOGEE == true) {
        mainChute.write(servoDeploy);
    }

    // --- Logging: only after launch ---
    if (LAUNCH == true && !APOGEE) {
        myMem.put(counter, zeroedAlt);
        counter += sizeof(zeroedAlt);
    }

    // --- Telemetry: send EVERY loop ---
    Serial1.print(zeroedAlt); 
    Serial1.print(",");
    Serial1.print(velocity); 
    Serial1.print(",");
    Serial1.print(lat,6); 
    Serial1.print(",");
    Serial1.print(lon,6); 
    Serial1.print(",");
    Serial1.print(sats);
    Serial1.print(",");
    Serial.println(curedBV)

    // --- Update last altitude & time ---
    lastAlt = zeroedAlt;
    oldTime = currentTime;
}
