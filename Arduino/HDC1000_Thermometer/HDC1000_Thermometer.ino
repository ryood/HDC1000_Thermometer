/*
 * HDC100_Thermometer
 * 
 * 2017.02.26
 * 
 */

#include <Wire.h>
#include "U8glib.h"
#include <avr/sleep.h>
#include <avr/interrupt.h>

U8GLIB_PCD8544 u8g(13, 11, 10, 9, 8);   // SPI Com: SCK = 13, MOSI = 11, CS = 10, A0 = 9, Reset = 8

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

#define WAKEUP_PIN  2
#define LCD_PWR_PIN 3

#define WAKEUP_PERIOD 5

int count;

void setup() {
  // Sleep Mode
  pinMode(WAKEUP_PIN, INPUT_PULLUP);

  // LCD Power
  pinMode(LCD_PWR_PIN, OUTPUT);
  digitalWrite(LCD_PWR_PIN, HIGH);

  // LCD
  u8g.setColorIndex(1);         // pixel on

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

void loop() {
  float temperature, humidity;

  // HDC1000
  getTemperatureAndHumidity(&temperature, &humidity);
  Serial.print("Temperature = ");
  Serial.print(temperature);
  Serial.print(" degree, Humidity = ");
  Serial.print(humidity);
  Serial.print("%, Discomfort = ");
  Serial.println(calcDiscomfort(temperature, humidity));

  // LCD
  // picture loop
  u8g.firstPage();
  do {
    draw(temperature, humidity);
  } while ( u8g.nextPage() );

  delay(1000);

  // Sleep Mode
  count++;
  if (count >= WAKEUP_PERIOD) {
    Serial.println("Sleep mode start!!");
    count = 0;

    sleepAndWakeup(WAKEUP_PIN);
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

  // Sleep
  digitalWrite(LCD_PWR_PIN, LOW);
  attachInterrupt(digitalPinToInterrupt(interruptNo), wakeup, FALLING);
  noInterrupts();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  interrupts();
  sleep_cpu();

  // Wakeup
  sleep_disable();
  detachInterrupt(digitalPinToInterrupt(interruptNo));
  digitalWrite(LCD_PWR_PIN, HIGH);
  
  return 0;
}

//-----------------------------------------------------------------------------------------------
// LCD
//-----------------------------------------------------------------------------------------------
float calcDiscomfort(float temperature, float humidity) {
  return 0.81 * temperature + 0.01 * humidity * (0.99 * temperature - 14.3) + 46.3;
}

void draw(float temperature, float humidity) {
  // graphic commands to redraw the complete screen should be placed here
  static char strBuff[16];
  u8g.setFont(u8g_font_7x14B);
  int temp10 = temperature * 10;
  sprintf(strBuff, "%2d.%01d%cC", temp10 / 10, temp10 % 10, '\xB0');
  u8g.drawStr( 0, 12, strBuff);
  int humd10 = humidity * 10;
  sprintf(strBuff, "%2d.%01d%%", humd10 / 10, humd10 % 10);
  u8g.drawStr( 0, 26, strBuff);
  sprintf(strBuff, "%2d", (int)calcDiscomfort(temperature, humidity));
  u8g.drawStr( 0, 40, strBuff);
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

