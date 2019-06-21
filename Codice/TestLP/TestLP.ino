#include "ArduinoLowPower.h"
#define PIN_INT 3
#define PIN_OUT 7
int sleep_time = 5000;
volatile int repetitions = 1;
void setup() {
  pinMode(PIN_OUT, OUTPUT);
  pinMode(PIN_INT, INPUT_PULLUP);
  digitalWrite(PIN_OUT, HIGH);
  delay(6000);
  digitalWrite(PIN_OUT, LOW);
  LowPower.attachInterruptWakeup(PIN_INT, interrupt_routine, LOW);
  LowPower.attachInterruptWakeup(RTC_ALARM_WAKEUP, interrupt_routine, CHANGE);
}
void loop() {
   for (int i = 0; i < repetitions; i++) {
     digitalWrite(PIN_OUT, HIGH);
     delay(500);
      digitalWrite(PIN_OUT, LOW);
      delay(500);
    }
    LowPower.sleep(sleep_time);
}
void interrupt_routine() { 
   repetitions ++;
}