#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <SoftwareSerial.h>

#define OLED_ADDR 0x3C

RTC_DS3231 rtc;
Adafruit_SH1106G display(128, 64, &Wire, -1);
#define BT Serial

const int buzzerPin = 9;
const int trigPin = 6;
const int echoPin = 7;

struct Alarm {
  int hour;
  int minute;
  bool active;
};

Alarm alarms[5];
int alarmCount = 0;

bool alarmRinging = false;
int currentMelody = 1;

int noteIndex = 0;
unsigned long lastNote = 0;
unsigned long lastUltra = 0;
unsigned long lastOLED = 0;

char cmd[10];
uint8_t cmdIdx = 0;

int melody1[] = {523, 587, 659, 0};
int melody2[] = {784, 698, 659, 587, 0};
int melody3[] = {440, 440, 880, 0};

void addAlarm(int h, int m) {
  if (alarmCount >= 5) return;

  alarms[alarmCount].hour = h;
  alarms[alarmCount].minute = m;
  alarms[alarmCount].active = true;

  alarmCount++;
}

void deleteAlarm(int h, int m) {
  for (int i = 0; i < alarmCount; i++) {
    if (alarms[i].hour == h && alarms[i].minute == m) {
      for (int j = i; j < alarmCount - 1; j++) {
        alarms[j] = alarms[j + 1];
      }
      alarmCount--;
      return;
    }
  }
}

float readDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 8000);
  if (duration == 0) return -1;

  return duration * 0.034 / 2;
}

void updateBLE() {
  while (BT.available()) {
    char c = BT.read();

    if (c == '#') {
      cmd[cmdIdx] = '\0';

      if (cmdIdx == 2 && cmd[0] == 'D' && cmd[1] == 'A') {

        alarmCount = 0;

        for (int i = 0; i < 5; i++) {
          alarms[i].hour = 0;
          alarms[i].minute = 0;
          alarms[i].active = false;
        }

        BT.print("CLEARED#");
      }

      else if (cmdIdx == 5) {

        int h = (cmd[1] - '0') * 10 + (cmd[2] - '0');
        int m = (cmd[3] - '0') * 10 + (cmd[4] - '0');

        if (cmd[0] == 'A') addAlarm(h, m);
        else if (cmd[0] == 'D') deleteAlarm(h, m);
      }

      if (cmdIdx == 2 && cmd[0] == 'M') {
        if (cmd[1] == '1') currentMelody = 1;
        if (cmd[1] == '2') currentMelody = 2;
        if (cmd[1] == '3') currentMelody = 3;
      }

      cmdIdx = 0;
      return;
    }

    if (cmdIdx < sizeof(cmd) - 1) {
      cmd[cmdIdx++] = c;
    } else {
      cmdIdx = 0;
    }
  }
}

void checkAlarms(DateTime now) {
  for (int i = 0; i < alarmCount; i++) {
    if (now.hour() == alarms[i].hour &&
        now.minute() == alarms[i].minute &&
        now.second() < 2) {
      alarmRinging = true;
    }
  }
}

void updateSound() {
  if (!alarmRinging) {
    noTone(buzzerPin);
    noteIndex = 0;
    return;
  }

  if (millis() - lastNote < 200) return;
  lastNote = millis();

  int freq = 0;

  if (currentMelody == 1) freq = melody1[noteIndex];
  if (currentMelody == 2) freq = melody2[noteIndex];
  if (currentMelody == 3) freq = melody3[noteIndex];

  noteIndex++;
  if (noteIndex >= 4) noteIndex = 0;

  if (freq == 0) noTone(buzzerPin);
  else tone(buzzerPin, freq);
}

void updateUltrasonic() {
  if (!alarmRinging) return;

  float d = readDistance();

  if (d > 0 && d < 12) {
    alarmRinging = false;
    noTone(buzzerPin);
  }
}

void updateOLED(DateTime now) {
  if (millis() - lastOLED < 500) return;
  lastOLED = millis();

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(alarmRinging ? "RING" : "OK");

  display.setCursor(90, 0);
  display.print("M");
  display.print(currentMelody);

  display.setTextSize(2);

  char t[9];
  sprintf(t, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  display.setCursor(10, 22);
  display.print(t);

  display.setTextSize(1);
  display.setCursor(0, 54);
  display.print("Alarms:");
  display.print(alarmCount);

  display.display();
}

void setup() {
  Serial.begin(9600);

  Wire.begin();
  rtc.begin();
  display.begin(OLED_ADDR, true);

  pinMode(buzzerPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  noTone(buzzerPin);
}

void loop() {
  updateBLE();

  DateTime now = rtc.now();

  checkAlarms(now);
  updateSound();
  updateUltrasonic();
  updateOLED(now);
}
