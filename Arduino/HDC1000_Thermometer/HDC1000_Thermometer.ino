/*
 * HDC100_Thermometer
 * 
 * 2017.02.26
 * 
 * LCD5110_Basic Library: http://www.rinkydinkelectronics.com/library.php?id=44
 * 
 */

#include <Wire.h>
#include <LCD5110_Basic.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

#define HDC1000_ADDRESS 0x40 /* or 0b1000000 */
#define HDC1000_RDY_PIN A3   /* Data Ready Pin */

#define HDC1000_TEMPERATURE_POINTER     0x00
#define HDC1000_HUMIDITY_POINTER        0x01
#define HDC1000_CONFIGURATION_POINTER   0x02
#define HDC1000_SERIAL_ID1_POINTER      0xfb
#define HDC1000_SERIAL_ID2_POINTER      0xfc
#define HDC1000_SERIAL_ID3_POINTER      0xfd
#define HDC1000_MANUFACTURER_ID_POINTER 0xfe

#define HDC1000_CONFIGURE_MSB 0x10 /* Get both temperature and humidity */
#define HDC1000_CONFIGURE_LSB 0x00 /* 14 bit resolution */

#define WAKEUP_PIN     2
#define SLEEP_MODE_PIN 3
#define BACK_LIGHT_PIN 7

#define WAKEUP_PERIOD 5

LCD5110 myGLCD(8,9,10,11,12);

bool isSleepMode = true;
int count;

extern uint8_t SmallFont[];
extern uint8_t MediumNumbers[];
extern uint8_t BigNumbers[];

void setup() {
  // Sleep Mode
  pinMode(WAKEUP_PIN, INPUT_PULLUP);
  pinMode(SLEEP_MODE_PIN, INPUT_PULLUP);

  // LCD Power
  pinMode(BACK_LIGHT_PIN, OUTPUT);
  digitalWrite(BACK_LIGHT_PIN, HIGH);

  // LCD
  myGLCD.InitLCD();

  // HDC1000
  Wire.begin();
  pinMode(HDC1000_RDY_PIN, INPUT);
  delay(15); /* Wait for 15ms */
  configure();

  Serial.begin(9600);
  Serial.print("Manufacturer ID = 0x");
  Serial.println(getManufacturerId(), HEX);
  Serial.println();
}

float calcDiscomfort(float temperature, float humidity) {
  return 0.81 * temperature + 0.01 * humidity * (0.99 * temperature - 14.3) + 46.3;
}

void loop() {
  float temperature, humidity, discomfort;
 
  getTemperatureAndHumidity(&temperature, &humidity);
  discomfort = calcDiscomfort(temperature, humidity);
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.print(" degree, Humidity = ");
  Serial.print(humidity);
  Serial.print("%, Discomfort = ");
  Serial.println(discomfort);

  myGLCD.clrScr();
  myGLCD.setFont(BigNumbers);
  myGLCD.printNumF(temperature, 1, RIGHT, 0);
  myGLCD.setFont(MediumNumbers);
  myGLCD.printNumF(humidity, 1, LEFT, 32);
  myGLCD.printNumF(discomfort, 0, RIGHT, 32);
  myGLCD.setFont(SmallFont);
  if (isSleepMode) {
    myGLCD.print("SLP", LEFT, 0);
  } else {
    myGLCD.print("CON", LEFT, 0);
  }

  delay(1000);

  // Check Sleep Mode
  isSleepMode = digitalRead(SLEEP_MODE_PIN);

  // Sleep
  if (isSleepMode) {
    count++;
    if (count >= WAKEUP_PERIOD) {
      Serial.println("Sleep mode start!!");
      count = 0;
  
      sleepAndWakeup(WAKEUP_PIN);
    }
  }
}

//-----------------------------------------------------------------------------------------------
// Sleep Mode
//-----------------------------------------------------------------------------------------------
void wakeup() {
  Serial.println("Wakeup!!");
}

int sleepAndWakeup(int interruptNo) {
  Serial.println("sleepAndWake Process start!!");
  if (digitalRead(WAKEUP_PIN) == LOW) {
    Serial.println("WAKEUP_PIN Low Level");
  } else {
    Serial.println("WAKEUP_PIN High Level");
  }
  Serial.println("sleep enable");
  delay(100);

  // Sleep
  digitalWrite(BACK_LIGHT_PIN, LOW);
  myGLCD.enableSleep();
  attachInterrupt(digitalPinToInterrupt(interruptNo), wakeup, FALLING);
  noInterrupts();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  interrupts();
  sleep_cpu();

  // Wakeup
  sleep_disable();
  detachInterrupt(digitalPinToInterrupt(interruptNo));
  digitalWrite(BACK_LIGHT_PIN, HIGH);
  myGLCD.disableSleep();
  
  return 0;
}

//-----------------------------------------------------------------------------------------------
// HDC1000
//-----------------------------------------------------------------------------------------------
void configure() {
  Wire.beginTransmission(HDC1000_ADDRESS);
  Wire.write(HDC1000_CONFIGURATION_POINTER);
  Wire.write(HDC1000_CONFIGURE_MSB);
  Wire.write(HDC1000_CONFIGURE_LSB);
  Wire.endTransmission();
}

int getManufacturerId() {
  int manufacturerId;

  Wire.beginTransmission(HDC1000_ADDRESS);
  Wire.write(HDC1000_MANUFACTURER_ID_POINTER);
  Wire.endTransmission();

  Wire.requestFrom(HDC1000_ADDRESS, 2);
  while (Wire.available() < 2) {
    ;
  }

  manufacturerId = Wire.read() << 8;
  manufacturerId |= Wire.read();

  return manufacturerId;
}

void getTemperatureAndHumidity(float *temperature, float *humidity) {
  unsigned int tData, hData;

  Wire.beginTransmission(HDC1000_ADDRESS);
  Wire.write(HDC1000_TEMPERATURE_POINTER);
  Wire.endTransmission();

  while (digitalRead(HDC1000_RDY_PIN) == HIGH) {
    ;
  }

  Wire.requestFrom(HDC1000_ADDRESS, 4);
  while (Wire.available() < 4) {
    ;
  }

  tData = Wire.read() << 8;
  tData |= Wire.read();

  hData = Wire.read() << 8;
  hData |= Wire.read();

  *temperature = tData / 65536.0 * 165.0 - 40.0;
  *humidity = hData / 65536.0 * 100.0;
}

