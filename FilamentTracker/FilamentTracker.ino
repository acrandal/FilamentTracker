/**
 *  3D Printer range finder based filament quantity tracker
 *
 *  @author Aaron S. Crandall <crandall@gonzaga.edu>
 *  @copyright 2020
 *  @license MIT
 */


#include "Adafruit_SSD1306.h"
#include "Adafruit_VL53L0X.h"

#include <EEPROM.h>

#define OLED_RESET 0  // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

Adafruit_VL53L0X lox = Adafruit_VL53L0X();

int count = 0;

#define RANGE_BUF_SIZE 40
uint16_t g_range_buf[RANGE_BUF_SIZE];
int range_buff_curr_index = 0;

#define UPPER_BUTTON_PIN D6
#define LOWER_BUTTON_PIN D7

int empty_spool_range = 2;
int full_spool_range = 1;

#define EMPTY_SPOOL_EEPROM_ADDRESS 0
#define FULL_SPOOL_EEPROM_ADDRESS 4
#define EEPROM_ALLOCATE 256

int g_dist_pct = 0;


// ****************************************************
void eeprom_init() {
  EEPROM.begin(EEPROM_ALLOCATE);
}

void eeprom_load() {
  EEPROM.get(EMPTY_SPOOL_EEPROM_ADDRESS, empty_spool_range);
  EEPROM.get(FULL_SPOOL_EEPROM_ADDRESS, full_spool_range);
}

void eeprom_save() {
  EEPROM.put(EMPTY_SPOOL_EEPROM_ADDRESS, empty_spool_range);
  EEPROM.put(FULL_SPOOL_EEPROM_ADDRESS, full_spool_range);
  EEPROM.commit();
}


// ****************************************************
void display_settings() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  display.println("Conf:");
  display.setTextSize(1);
  display.print("Empty: ");
  display.println(empty_spool_range);
  display.print("Full:  ");
  display.println(full_spool_range);
  display.display();
}


// ****************************************************
void display_clear() {
  display.clearDisplay();
  display.display();
}


// ****************************************************
bool take_range_sample() {
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false); // pass in 'true' to get debug data printout!

  if (measure.RangeStatus != 4) {  // phase failures have incorrect data
    // Serial.print("Distance (mm): "); Serial.println(measure.RangeMilliMeter);
    g_range_buf[range_buff_curr_index++] = measure.RangeMilliMeter;
    range_buff_curr_index %= RANGE_BUF_SIZE;
    return true;
  } else {
    return false;
  }
}


// ****************************************************
int calc_ave_range() {
  int total = 0;
  for( int i = 0; i < RANGE_BUF_SIZE; i++ ) {
    total += g_range_buf[i];
  }
  total /= RANGE_BUF_SIZE;
  return total;
}


// ****************************************************
int calc_curr_dist_pct() {
  float pct = 0.0;
  int denomenator = empty_spool_range - full_spool_range;
  if( denomenator == 0 ) { denomenator = 1; }
  Serial.print("Denomenator: ");
  Serial.println(denomenator);
  pct = ((float)(empty_spool_range - calc_ave_range()) / denomenator);
  Serial.print("Pct: ");
  Serial.println(pct);
  pct *= 100;
  return pct;
}


// ****************************************************
void update_screen(bool got_range) {
  int range_ave = calc_ave_range();
  g_dist_pct = calc_curr_dist_pct();
   
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  display.print(range_ave);
  display.println("mm");

  display.setTextSize(1);
  display.println("");
  display.setTextSize(2);

  display.print(" ");
  if( g_dist_pct < 10 ) { display.print(" "); }
  display.print(g_dist_pct);
  display.println("%");
  
  display.display();
}


// *************************************************************************************************** //
void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.display();

  pinMode(UPPER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LOWER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);     // HIGH means off - the boards are backwards...

  Serial.println("Adafruit VL53L0X Initializing");
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while(1);
  }

  for( int i = 0; i < RANGE_BUF_SIZE; i++ ) {
    g_range_buf[i] = 0;
  }

  eeprom_init();
  eeprom_load();

  display_settings();
  delay(10000);
  display_clear();

}


// *************************************************************************************************** //
void set_spool_distance(int button_pin, int * save_dist, char* name_dist) {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(2);
  display.println(name_dist);
  display.println("Dist:");
  display.print(*save_dist);
  display.println("mm");
  display.display();

  unsigned long start_time = millis();
  while( digitalRead(button_pin) == LOW && start_time + 5000 > millis() ) {
    delay(50);
  }
  
  // Button released under 5 secs - do nothing.
  if(start_time + 5000 > millis() ) { return; }

  // Otherwise, snag current average range and store it to EEPROM
  int range_ave = calc_ave_range();
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("Set");
  display.println(name_dist);
  display.println("Dist:");
  display.print(range_ave);
  display.println("mm");
  display.display();

  *save_dist = range_ave;
  eeprom_save();

  delay(1000);
}


// *************************************************************************************************** //
void loop() {

  bool got_range = take_range_sample();
  update_screen(got_range);

  if( digitalRead(UPPER_BUTTON_PIN) == LOW ) {
    set_spool_distance(UPPER_BUTTON_PIN, &empty_spool_range, "Empty");
  }
  else if( digitalRead(LOWER_BUTTON_PIN) == LOW ) {
    set_spool_distance(LOWER_BUTTON_PIN, &full_spool_range, "Full");
  }

  delay(100);
}
