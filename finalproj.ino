#include <Wire.h>
#include "MAX30105.h"

MAX30105 sensor1;
MAX30105 sensor2;

// LED Strip 1 pins (for sensor 1)
const int RED1 = 9;
const int GREEN1 = 10;
const int BLUE1 = 11;

// LED Strip 2 pins (for sensor 2)
const int RED2 = 6;
const int GREEN2 = 5;
const int BLUE2 = 3;

// Sensor 1 variables
long irBuffer1[100];
int bufferIndex1 = 0;
long irAvg1 = 0;
long lastBeatTime1 = 0;
const int BPM_BUFFER_SIZE = 7;
int bpmBuffer1[BPM_BUFFER_SIZE];
int bpmBufferIndex1 = 0;
int smoothBpm1 = 0;
int beatCount1 = 0;

// Sensor 2 variables
long irBuffer2[100];
int bufferIndex2 = 0;
long irAvg2 = 0;
long lastBeatTime2 = 0;
int bpmBuffer2[BPM_BUFFER_SIZE];
int bpmBufferIndex2 = 0;
int smoothBpm2 = 0;
int beatCount2 = 0;

struct LEDColor {
  float r, g, b;
  float targetR, targetG, targetB;
};

LEDColor led1 = {0, 0, 255, 0, 0, 255}; // Start blue
LEDColor led2 = {0, 0, 255, 0, 0, 255};

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Setup LED pins
  pinMode(RED1, OUTPUT);
  pinMode(GREEN1, OUTPUT);
  pinMode(BLUE1, OUTPUT);
  pinMode(RED2, OUTPUT);
  pinMode(GREEN2, OUTPUT);
  pinMode(BLUE2, OUTPUT);
  
  Serial.println("Starting sensors...");
  
  // Initialize sensor 1 (default I2C address 0x57)
  if (!sensor1.begin(Wire, I2C_SPEED_FAST, 0x57)) {
    Serial.println("Sensor 1 ERROR");
  } else {
    sensor1.setup();
    sensor1.setPulseAmplitudeRed(0x0A);
    Serial.println("Sensor 1 ready!");
  }
  
  // Initialize sensor 2 (alternative I2C address 0x5F)
  if (!sensor2.begin(Wire, I2C_SPEED_FAST, 0x5F)) {
    Serial.println("Sensor 2 ERROR");
  } else {
    sensor2.setup();
    sensor2.setPulseAmplitudeRed(0x0A);
    Serial.println("Sensor 2 ready!");
  }
}

void loop() {
  // Read both sensors
  long ir1 = sensor1.getIR();
  long ir2 = sensor2.getIR();
  
  // Process Sensor 1
  processSensor(ir1, irBuffer1, bufferIndex1, irAvg1, lastBeatTime1, 
                bpmBuffer1, bpmBufferIndex1, smoothBpm1, beatCount1, 1);
  
  // Process Sensor 2
  processSensor(ir2, irBuffer2, bufferIndex2, irAvg2, lastBeatTime2, 
                bpmBuffer2, bpmBufferIndex2, smoothBpm2, beatCount2, 2);

  updateLEDColor(smoothBpm1, led1);
  updateLEDColor(smoothBpm2, led2);

  smoothLEDTransition(led1, RED1, GREEN1, BLUE1);
  smoothLEDTransition(led2, RED2, GREEN2, BLUE2);
  
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    
    int finger1 = (ir1 > 50000) ? 1 : 0;
    int finger2 = (ir2 > 50000) ? 1 : 0;
    
    Serial.print("HR1:");
    Serial.print(smoothBpm1);
    Serial.print(",IR1:");
    Serial.print(ir1);
    Serial.print(",FINGER1:");
    Serial.print(finger1);
    Serial.print(",HR2:");
    Serial.print(smoothBpm2);
    Serial.print(",IR2:");
    Serial.print(ir2);
    Serial.print(",FINGER2:");
    Serial.println(finger2);
  }
  
  delay(20);
}

void processSensor(long ir, long* irBuffer, int& bufferIndex, long& irAvg, 
                   long& lastBeatTime, int* bpmBuffer, int& bpmBufferIndex, 
                   int& smoothBpm, int& beatCount, int sensorNum) {
  
  // Store IR values in buffer
  irBuffer[bufferIndex] = ir;
  bufferIndex = (bufferIndex + 1) % 100;
  
  // Calculate average
  long sum = 0;
  for (int i = 0; i < 100; i++) {
    sum += irBuffer[i];
  }
  irAvg = sum / 100;
  
  // Beat detection
  static bool beatDetected1 = false;
  static bool beatDetected2 = false;
  static long lastIR1 = 0;
  static long lastIR2 = 0;
  
  bool* beatDetected = (sensorNum == 1) ? &beatDetected1 : &beatDetected2;
  long* lastIR = (sensorNum == 1) ? &lastIR1 : &lastIR2;
  
  // Detect when IR crosses below average (heartbeat)
  if (*lastIR > irAvg && ir < irAvg && !(*beatDetected) && ir > 50000) {
    long now = millis();
    long delta = now - lastBeatTime;
    
    if (delta > 500 && delta < 1800) {
      int newBpm = 60000 / delta;
      
      if (newBpm >= 40 && newBpm <= 150) {
        bpmBuffer[bpmBufferIndex] = newBpm;
        bpmBufferIndex = (bpmBufferIndex + 1) % BPM_BUFFER_SIZE;
        beatCount++;
        
        if (beatCount >= BPM_BUFFER_SIZE) {
          int sum = 0;
          for (int i = 0; i < BPM_BUFFER_SIZE; i++) {
            sum += bpmBuffer[i];
          }
          smoothBpm = sum / BPM_BUFFER_SIZE;
        } else {
          smoothBpm = newBpm;
        }
        
        Serial.print("BEAT! Sensor ");
        Serial.print(sensorNum);
        Serial.print(" - Raw: ");
        Serial.print(newBpm);
        Serial.print(" Smooth: ");
        Serial.println(smoothBpm);
      }
    }
    
    lastBeatTime = now;
    *beatDetected = true;
  }
  
  if (ir > irAvg) {
    *beatDetected = false;
  }
  
  *lastIR = ir;
}

void updateLEDColor(int bpm, LEDColor& led) {
  // Map BPM to color (same as frontend)
  // 30-130 BPM range with 4 color stages: blue -> green -> orange -> red
  
  float intensity = constrain((bpm - 30) / 100.0, 0.0, 1.0);
  
  struct ColorStage {
    float r, g, b;
  };
  
  ColorStage blue = {0, 100, 255};
  ColorStage green = {0, 255, 150};
  ColorStage orange = {255, 140, 0};
  ColorStage red = {255, 0, 50};
  
  ColorStage from, to;
  float blend;
  
  if (intensity < 0.33) {
    from = blue;
    to = green;
    blend = intensity * 3;
  } else if (intensity < 0.66) {
    from = green;
    to = orange;
    blend = (intensity - 0.33) * 3;
  } else {
    from = orange;
    to = red;
    blend = (intensity - 0.66) * 3;
  }
  
  led.targetR = from.r + (to.r - from.r) * blend;
  led.targetG = from.g + (to.g - from.g) * blend;
  led.targetB = from.b + (to.b - from.b) * blend;
}

void smoothLEDTransition(LEDColor& led, int redPin, int greenPin, int bluePin) {
  float speed = 0.05;
  
  led.r += (led.targetR - led.r) * speed;
  led.g += (led.targetG - led.g) * speed;
  led.b += (led.targetB - led.b) * speed;
  
  analogWrite(redPin, (int)led.r);
  analogWrite(greenPin, (int)led.g);
  analogWrite(bluePin, (int)led.b);
}

float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}