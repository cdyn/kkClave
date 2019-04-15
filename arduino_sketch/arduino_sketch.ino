#include <SPI.h>
#include <Wire.h>
#include <Ethernet.h>
#include <SD.h>

//pin addresses
const int t0_a = A0;
const int t1_a = A1;
const int c0_a = A2;
const int r1_d = 5;
const int r2_d = 6;
const int r3_d = 7;
const int s0_d = 8;
const int s1_d = 9;
//state variables
int t0 = 0;
int t1 = 0;
int c0 = 0;
int s0 = 0;//start
int s1 = 0;//mode
bool r1 = 0;//light
bool r2 = 0;//fan
bool r3 = 0;//heater
bool start = 0;
float temp = 0;
float set_temp = 122;
float threshold = 3;
int start_time = 0;
int end_time = 0;

void setup() {
  // Digital pin mode
  pinMode(r1_d, OUTPUT);
  pinMode(r2_d, OUTPUT);
  pinMode(r3_d, OUTPUT);
  pinMode(s0_d, INPUT);
  pinMode(s1_d, INPUT);
  //start serial
  Serial.begin(9600);
}

void stop() {
  //cycle reset code and cron handler
  if (start){ 
    if (millis() > end_time) { start = 0;}
  }
  else {
    
  }
  delay(1000); //main program delay
}

void poll() {//524 == 84 deg f 603 == 93 : 79/9 (target 122) ~8.78 per deg f
  //read sensors
  t0 =  analogRead(t0_a);
  t1 =  analogRead(t1_a);
  c0 =  analogRead(c0_a);
  s0 =  digitalRead(s0_d);
  s1 =  digitalRead(s1_d);
  //compute resultant data
  temp = ((t0 + t1)/2.0)/8.78;
}

void relay() {
  if (! start){//prograum unstart
    r1 = 0;
    r2 = 0;
    r3 = 0;
  }
  else if (temp + threshold < set_temp) { // heater on condition
    r1 = 1;
    r2 = 1;
    r3 = 1;
  }
  else {
    r1 = 0;
    r2 = 1;
    r3 = 1;
  }
  //set relays
  if(r1) { digitalWrite(r1_d, HIGH); } else { digitalWrite(r1_d, LOW); }
  if(r2) { digitalWrite(r2_d, HIGH); } else { digitalWrite(r2_d, LOW); }
  if(r3) { digitalWrite(r3_d, HIGH); } else { digitalWrite(r3_d, LOW); }
}

void printState() {
  //print state to serial
  Serial.print("t0:");
  Serial.print (t0);
  Serial.print(" t1:");
  Serial.print (t1);
  Serial.print(" c0:");
  Serial.println (c0);
  Serial.print("s0:");
  Serial.print (s0);
  Serial.print(" s1: ");
  Serial.println (s1);
  Serial.print("r1:");
  Serial.print (r1);
  Serial.print(" r2:");
  Serial.print (r2);
  Serial.print(" r3:");
  Serial.println (r3);
  Serial.print("temp:");
  Serial.print(temp);
  Serial.print(" set_temp:");
  Serial.println(set_temp);
  Serial.print("time:");
  Serial.print(millis());
  Serial.print(" end_time:");
  Serial.println(end_time);  
  //print to display
}

void loop() {
  //main program loop
  stop();
  poll();
  relay();
  printState();
}
