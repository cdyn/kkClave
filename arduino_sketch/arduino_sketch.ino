/*
 * kkClave 
 *    Upcycled Yogurt incubator: assumes arduino mega 2560, relay 
 *    shield, ethernet shield, Adafruit_SSD1306 on i2c
 * https://github.com/cdyn/kkClave
 * 
 * Jean Runnells
 * https://www.customdyn.com
 * 
 * TODO:
 *  - implement ethernet
 *  - add cooling cycle features
 *  - handle clock overflow
 *  - add runtime max
 */

#include <SPI.h>
#include <Wire.h>
#include <Ethernet.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <thermistor.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     22 // Reset pin # (4)(or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define NUMFLAKES     10 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16
static const unsigned char PROGMEM logo_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000 };
  
//pin addresses
static const int PROGMEM t0_a = A13;
static const int PROGMEM t1_a = A14;
static const int PROGMEM c0_a = A15;
static const int PROGMEM r1_d = 5;
static const int PROGMEM r2_d = 6;
static const int PROGMEM r3_d = 7;
static const int PROGMEM s0_d = 24;
static const int PROGMEM s1_d = 25;
static const int PROGMEM s2_d = 26;
static const int PROGMEM s3_d = 27;

//IO states
THERMISTOR t0(t0_a,        // Analog pin
              10000,       // Nominal resistance at 25 ºC
              3950,        // thermistor's beta coefficient
              10000);      // Value of the series resistor
THERMISTOR t1(t1_a,        // Analog pin
              10000,       // Nominal resistance at 25 ºC
              3950,        // thermistor's beta coefficient
              10000);      // Value of the series resistor
int _t0 = 0; //themistor0 1/10 ºC
int _t1 = 0; //themistor1 1/10 ºC
int c0 = 0; //potentiometer (unused)
int s0 = 0; //start
int s1 = 0; //mode
int s2 = 0; //add
int s3 = 0; //dec
bool r1 = 0; //light
bool r2 = 0; //fan
bool r3 = 0; //heater

//LOGIC vars
int start = 0; // cycle state: 0=ProgSet 1=ProgRun 2=ProgFin
int mode = 0; // input state: 0=TempSet 1=TimeSet
float temp = 0; // ºC computed temp
float set_temp = 50; // ºC default set temp
float heat_thresh = 2; // ºC +- threshold for heater start/stop i.e. 'swing'
float fan_thresh = 6; // ºC thermometer diff threshold for fan start/stop 
unsigned long run_time = 21600000; // default runtime (6 hr)
unsigned long inc = 300000; // inc/dec minimum time increment (5 min)
unsigned long start_time = 0; // cycle start time
unsigned long end_time = 0; // cycle end time
unsigned long stby = millis(); // last press for screensaver and race, if 0 device is in standby
unsigned long race_hold = 500; // inc/dec race delay

void setup() {  
  //start serial
  Serial.begin(9600);
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(2000); // Pause for 2 seconds
  // Digital pin mode
  pinMode(r1_d, OUTPUT);
  pinMode(r2_d, OUTPUT);
  pinMode(r3_d, OUTPUT);
  pinMode(s0_d, INPUT_PULLUP);
  pinMode(s1_d, INPUT_PULLUP);
  pinMode(s2_d, INPUT_PULLUP);
  pinMode(s3_d, INPUT_PULLUP);
  // Clear the buffer
  display.clearDisplay();
  //testanimate(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT);
} 

void testdrawchar(void) {
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  // Not all the characters will fit on the display. This is normal.
  // Library will draw what it can and the rest will be clipped.
  for(int16_t i=0; i<256; i++) {
    if(i == '\n') display.write(' ');
    else          display.write(i);
  }

  display.display();
  delay(2000);
}

void stop() { // Cycle set code and cron handler
  if ( start == 0 ) { // cycle start: launch screen saver after delay
    if(millis() > stby + 300000){ // screen saver delay (5 min)
      stby = 0;
      relay();
      testanimate(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT);
    } 
  } else if ( start == 1 ){ // cycle run:  run until end_time
    if (millis() > end_time) { start = 2; }
  } else if (start == 2){ // cycle fin: lanunch screensaver after cooldown
    if(millis() > end_time + 900000 && millis() > stby + 300000){ // screen saver after cooldown (15 min)
      stby = 0;
      relay();
      testanimate(logo_bmp, LOGO_WIDTH, LOGO_HEIGHT);
    } 
  }
  else {
    
  }
  //delay(100); //main program delay
}

void poll() {
  //read sensors
  c0 =  analogRead(c0_a);
  s0 =  digitalRead(s0_d);
  s1 =  digitalRead(s1_d);
  s2 =  digitalRead(s2_d);
  s3 =  digitalRead(s3_d);
  _t0 = t0.read();
  _t1 = t1.read();
  temp = (_t0 + _t1) / 20.0;
  //button handlers
  if(s0 == LOW){//Start button
    if(stby == 0){//wake from standby
      stby = millis();
      while(digitalRead(s0_d) == LOW){}
      return;
    }
    if(start == 0) {
      start = 1;
      start_time = millis();
      end_time = millis() + run_time;
    } else if(start == 1) {
      start = 2;
      end_time = millis();
    }
    else if(start == 2) {
      start = 0;
      end_time = 0;
      start_time = 0;
    }
    else if(start == 3) {
      start = 0;
    }
    else {
      start = 0;
      end_time = 0;
      start_time = 0;
    }
    printState();
    stby = millis();
    while(digitalRead(s0_d) == LOW){}
  }
  if(s1 == LOW){//mode button
    if(stby == 0){//wake from standby
      stby = millis();
      while(digitalRead(s1_d) == LOW){}
      return;
    }
    int mset = 0;
    if (mode == 0) {
      mset = 1;
    } else if (mode == 1){
      mset = 0;
    } else {mset = 0;}
    mode = mset;
    printState();
    stby = millis();
    while(digitalRead(s1_d) == LOW){}
  }
  if(s2 == LOW){//increment with hold down race
    if(stby == 0){//wake from standby
      stby = millis();
      while(digitalRead(s2_d) == LOW){}
      return;
    }
    if(mode == 0) {
      set_temp += 1;
    } else if(mode == 1){
      run_time += inc;
      if (start == 1){end_time += inc;}
    }
    printState();
    stby = millis();
    while(digitalRead(s2_d) == LOW){//race on hold
      if(mode == 1 && millis() - stby > race_hold){
        stby = millis();
        run_time += inc * 3;
        if (start == 1){end_time += inc * 3;}
        printState();
      }
    }
  }
  if(s3 == LOW){//decrement with hold down race
    if(stby == 0){//wake from standby
      stby = millis();
      while(digitalRead(s0_d) == LOW){}
      return;
    }
    if(mode == 0) {
      set_temp -= 1;
    } else if(mode == 1){
      run_time -= inc;
      if (start == 1){end_time -= inc;}
    }
    printState();
    stby = millis();
    while(digitalRead(s3_d) == LOW){//race on hold
      if(mode == 1 && millis() - stby > race_hold){
        stby = millis();
        run_time -= inc * 3;
        if (start == 1){end_time -= inc * 3;}
        printState();
      }
    }
  }
}

void relay() { // Update relay states
  float diff = 10.0 * (_t0 - _t1); //thermocouple temp difference in ºC
  if (start == 0){ //program unstart
    r1 = 0; // heater
    r2 = 0; // fan
    //light
    if (stby == 0){
      r3 = 0;
    } else {
      r3 = 1;
    }
  }
  else if (start == 1) { //program running
    // heater on/off with 'swing'
    if(r1 == 0 && temp < set_temp - heat_thresh){ r1 == 1; }
    else if(r1 == 1 && temp > set_temp + heat_thresh){ r1 = 0; }
    // fan on/off with temp balance threshold
    if(abs(diff) > fan_thresh ){ r2 = 1; }
    else { r2 = 0; }
    r3 = 1; // light
  }
  else if (start == 2){ //program finished
    r1 == 0;
    if (stby == 0){ //cooldwon elapsed, standby
      r2 = 0;
      r3 = 0;
    } else { //temp balance with fans during cooldown
      if(abs(diff) > fan_thresh ){ r2 = 1; }
      else { r2 = 0; }
      r3 = 1;
    }
  }
  else { // FAULT! saftey shut off ...
    r1 = 0;
    r2 = 0;
    r3 = 0;
  }
  
  //set relays - if relays get mixed around change assigment here
  if(r1) { digitalWrite(r1_d, HIGH); } else { digitalWrite(r1_d, LOW); }
  if(r2) { digitalWrite(r2_d, HIGH); } else { digitalWrite(r2_d, LOW); }
  if(r3) { digitalWrite(r3_d, HIGH); } else { digitalWrite(r3_d, LOW); }
}

void printState() {
  //print state to serial
  Serial.println("c0:"+(String)c0);
  Serial.println("s0:"+(String)s0+" s1:"+(String)s1+" s2:"+(String)s2+" s3:"+(String)s3);
  Serial.println("r1:"+(String)r1+" r2:"+(String)r2+" r3:"+(String)r3);
  Serial.println("temp:"+(String)temp+" set_temp:"+(String)set_temp);
  Serial.println("time:"+(String)millis()+" end_time"+(String)end_time+" run_time:"+timeString(run_time));
  Serial.println("Status:"+statusString()+" Mode:"+ modeString());  
  //print to display
  display.clearDisplay();
  display.setTextSize(2);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.print((String)(int)temp + "/" + (String)(int)set_temp + " " + (char)248 + "C ");
  if(mode == 0) {display.print("*");}
  display.print("\n");
  if (start == 0 || start == 2) {
    display.print((String)"00:00 RUN\n" + timeString(run_time) + " SET");
  }
  else if (start == 1) {
    display.print(timeString(end_time-millis()) + " RUN\n" + timeString(run_time) + " SET");
  }
  if(mode == 1) {display.print("*");}
  display.print("\n");
  display.setTextSize(2);
  display.print(statusString() + "  " + modeString() + "\n");
  display.display();
}

String modeString() {
    if(start == 0) {
      if(mode == 0) {
        return "sTemp";
      } else if(mode == 1) {
        return "sTime";
      }
      else {return "ERR";}
  }
  else if(start == 1) {
    if(mode == 0) {
      return "aTemp";
    } else if(mode == 1) {
      return "aTime";
    }
    else {return "ERR";}
  }
  else if (start == 2) {
    return timeString(end_time-start_time);
  }
  else {return "ERR";}
}

String statusString() {
  if(start == 0) {
    return "PRG";
  }
  else if(start == 1) {
    return "RUN";
  }
  else if (start == 2) {
    return "FIN";
  }
  else {return "ERR"; }
}

String timeString(unsigned long t) {
  unsigned long sec = t/1000;
  unsigned long _hour = sec / 3600;
  unsigned long _min = (sec % 3600)/60;
  //Serial.println((String)t+">"+(String)_hour+":"+(String)_min);
  char str[6];
  str[0] = '0' + _hour / 10;
  str[1] = '0' + _hour % 10;
  str[2] = ':'; 
  str[3] = '0' + _min / 10;
  str[4] = '0' + _min % 10;
  str[5] = '\0';
  return (String)str;
}

#define XPOS   0 // Indexes into the 'icons' array in function below
#define YPOS   1
#define DELTAY 2
void testanimate(const uint8_t *bitmap, uint8_t w, uint8_t h) {
  int8_t f, icons[NUMFLAKES][3];

  // Initialize 'snowflake' positions
  for(f=0; f< NUMFLAKES; f++) {
    icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
    icons[f][YPOS]   = -LOGO_HEIGHT;
    icons[f][DELTAY] = random(1, 6);
    Serial.print(F("x: "));
    Serial.print(icons[f][XPOS], DEC);
    Serial.print(F(" y: "));
    Serial.print(icons[f][YPOS], DEC);
    Serial.print(F(" dy: "));
    Serial.println(icons[f][DELTAY], DEC);
  }

  while(stby == 0) { // Loop until standby wake
    poll();
    display.clearDisplay(); // Clear the display buffer

    // Draw each snowflake:
    for(f=0; f< NUMFLAKES; f++) {
      display.drawBitmap(icons[f][XPOS], icons[f][YPOS], bitmap, w, h, WHITE);
    }

    display.display(); // Show the display buffer on the screen
    delay(200);        // Pause for 1/10 second

    // Then update coordinates of each flake...
    for(f=0; f< NUMFLAKES; f++) {
      icons[f][YPOS] += icons[f][DELTAY];
      // If snowflake is off the bottom of the screen...
      if (icons[f][YPOS] >= display.height()) {
        // Reinitialize to a random position, just off the top
        icons[f][XPOS]   = random(1 - LOGO_WIDTH, display.width());
        icons[f][YPOS]   = -LOGO_HEIGHT;
        icons[f][DELTAY] = random(1, 6);
      }
    }
  }
}

void loop() {
  //main program loop
  stop();
  poll();
  relay();
  printState();
}
