// This code controls the pupation station, specifically it allows the user to empty, fill, or manually operate the valves in the system. 

#include <Wire.h>   // 
#include <SPI.h>    // 
#include <LiquidCrystal_I2C.h>    // display library
 
// pinout
int valve_pins[] = {2,3,4,5,6,7}; // valve pins. ordered from top to bottom, pin 2 is the top valve. 
#define fill_pin 8
#define REout1 10  // Rotary encoder pins(1 is on right side)
#define REswitch 11
#define REout3 12

// rotary encoder variables
int previousState;    // tracks what the rotary encoder was set to last
int currentState;   // what the rotary encoder is currently
int knob_output;    // variable for the interpretation of the rotary encoder

// filling and draining variables
int pan_x = 1;
bool fill_open = false;
int drain_to_time[] = {350, 450, 550, 650, 750}; // these are the time points at which to close valves during filling.
#define number_of_pans 6
#define s_per_gal 8   // seconds per gallon
#define empty_time 300    // in s
#define rinse_fill_delay 120   // in s
#define rinse_volume 6   // in gallons
#define rinses 6
#define open_valve HIGH
#define close_valve LOW
#define button_delay 300
#define drain_time 50

// Menu stuff
String menu_options[] = {" EMPTY", " FILL", "SUB MENU"};
int menu_length = (sizeof(menu_options) / sizeof(String)) - 1;
String task_options[] = {"   OPEN VALVE", "  CLOSE VALVE", "   ADD WATER", "OPEN ALL VALVES", "CLOSE ALL VALVES", "   MAIN MENU"};
int task_length = (sizeof(task_options) / sizeof(String)) - 1;
String menu_results[] = {" EMPTYING", " FILLING", " CLEANING"};
int menu_pos = 0;
String paused_text[] = {"", "", "", ""};
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 20, 4);

void setup() {
  Serial.begin(9600);
  Serial.println("starting");

  pinMode(REout1, INPUT_PULLUP);
  pinMode(REout3, INPUT_PULLUP);
  pinMode(REswitch, INPUT_PULLUP);  
  pinMode(valve_pins[0], OUTPUT);
  pinMode(valve_pins[1], OUTPUT);
  pinMode(valve_pins[2], OUTPUT);
  pinMode(valve_pins[3], OUTPUT);
  pinMode(valve_pins[4], OUTPUT);
  pinMode(valve_pins[5], OUTPUT);
  pinMode(fill_pin, OUTPUT);

  lcd.init();   // Set up lcd
  lcd.backlight();

  reset_system();
}

void loop() {
  delay(button_delay);
  choose_option(menu());
  
}

// ---------------------------------   MAIN MENU FUNCTIONS   ---------------------------------

int menu() {    // this function cycles through the main menu and allows users to select options
  menu_pos = 0;
  while(true) {
    lcd_text(false, "", "  Select Function:", "      " + menu_options[menu_pos], "");
    knob_output = read_knob();
    if (knob_output == 0) {
      return(menu_pos);
    }
    menu_pos += knob_output;
    
    if (menu_pos < 0) {
      menu_pos = menu_length;
    }
    else if(menu_pos > menu_length) {
      menu_pos = 0;
    }
    delay(250);
  }
}

int select_pans(String do_to_pan) { // menu for the user to select how many pans to fill
  pan_x = 1;
  delay(button_delay); // time to release button
  while (true) {
    lcd_text(false, "", do_to_pan + String(pan_x), "", "");
    knob_output = read_knob();
    if (knob_output == 0) {
      return(pan_x); 
    }
    pan_x += knob_output;    
    if (pan_x < 1) {
      pan_x = number_of_pans;
    }
    if(pan_x > number_of_pans) {
      pan_x = 1;
    }
    delay(250);
  }
}

void choose_option(int choice) {    // function for choosing a menu option
  if (choice == 0) {
    lcd_text(false, menu_results[choice],"","","");
    empty_pans();
  }
  if (choice == 1) {
    fill(select_pans("  FILL: "));
  }
  if (choice == 2) {
    choose_sub_option(task());
  }
}

void lcd_text(bool pausing, String text_line_1, String text_line_2, String text_line_3, String text_line_4){    // function for printing an array of text to the display
  lcd.clear();
  String text_to_print[] = {text_line_1, text_line_2, text_line_3, text_line_4};
  for(int i = 0; i < 4; i++) {
    lcd.setCursor(0, i);
    lcd.print(text_to_print[i]);
    if (!pausing) {
      paused_text[i] = text_to_print[i];    // saves the printed text incase of a pause
    }
  }
      
}

// ---------------------------------   TASK MENU   -----------------------------------

int task() {    // menu function for sub tasks aka more granular tasks such as opening a specific valve
  menu_pos = 0;
  delay(button_delay);
  while(true) {
    lcd_text(false, "", "    Select Task:", "  " + task_options[menu_pos], "");
    knob_output = read_knob();
    if (knob_output == 0) {
      return(menu_pos);
    }
    menu_pos += knob_output;
    if (menu_pos < 0) {
      menu_pos = task_length;
    }
    if(menu_pos > task_length) {
      menu_pos = 0;
    }
    delay(250);
  }
}

void choose_sub_option(int choice) {    // function for choosing a subtask
  if (choice == 0) {
    operate_valve(select_pans(" Open Valve: "), open_valve);
  }
  if (choice == 1) {
    operate_valve(select_pans(" Close Valve: "), close_valve);
  }
  if (choice == 2) {
    digitalWrite(fill_pin, HIGH);
    lcd_text(false, "", "   FILLING", "PRESS TO STOP", "");
    delay(button_delay);
    while (read_knob() != 0) {
      delay(100);
    }
    digitalWrite(fill_pin, LOW);
  }
  if (choice == 3) {
    lcd_text(false, "", "OPENING VALVES", "", "");
    open_all_valves();
  }
  if (choice == 4) {
    lcd_text(false, "", "CLOSING VALVES", "", "");
    close_all_valves();
  }
  if (choice == 5) { // goes back to main menu
    delay(button_delay);
    loop();
  }
  choose_sub_option(task()); // after one of the options go back to sub menu
}

// ---------------------------------   HIGH LEVEL FUNCTIONS   ---------------------------------

void empty_pans() {  // emptys the pans and rinses out any remaining larvae or pupa
  open_all_valves();    // initial draining
  delay(empty_time);
  for (int i = 0; i < rinses; i++) {
    rinse();
  }
}

void fill(int pans_to_fill) {   // fills the pans with water one at a time from bottom to top
  lcd_text(false, "", "   FILLING", "", "");
  int filling_time = 0;
  open_all_valves();
  add_water_bool(true);
  operate_valve(6, close_valve);
  while (filling_time < drain_to_time[pans_to_fill - 2]) {
    for (int i = 0; i < (pans_to_fill - 2); i++) {    // the minus one is to not close the valve for the top pan0
      if (filling_time == drain_to_time[i]) {
        operate_valve(5 - i, close_valve); //closes valves 5,4,3,2,1 
        add_water_bool(false);
        delay(40000); //what does the delay do? 
        filling_time += 40;
        add_water_bool(true);
        
      }
    }
    delay(1000);
    filling_time += 1;
    if(filling_time == (100 * pans_to_fill)) {
      add_water_bool(false);
    }
  }  
  operate_valve(1, close_valve);
  add_water_bool(false);
}

void rinse() { // fills and then emptys each pan sequentially
  add_water(rinse_volume);
  delay(rinse_fill_delay);
}

void open_all_valves() {
  for(int i = 1; i <= 6; i++) { 
    operate_valve(i, open_valve);
  }
}

void close_all_valves() {
  for(int i = 1; i <= 6; i++) {
    operate_valve(i, close_valve);
  }
}

// -----------------------------------   BASE FUNCTIONS   -----------------------------------

void operate_valve(int valve_to_open, bool do_i_open) {   // opens or closes a valve. 1 for the valve on pan 1. 
  digitalWrite(valve_pins[valve_to_open - 1], do_i_open);
  delay(500);
}

void add_water(int volume) {
  digitalWrite(fill_pin, HIGH);
  delay(s_per_gal * volume);
  digitalWrite(fill_pin, LOW);
}

void add_water_bool(bool adding_water) {
  digitalWrite(fill_pin, adding_water);
}

// Waits until rotary encoder is moved. Returns 1 for ccw, 2 for cw, and 3 for press. 
int read_knob() {
  delay(button_delay);
  previousState = digitalRead(REout1);
  while(true){
    currentState = digitalRead(REout1); // Read current state of rotary encoder
    if(currentState != previousState){ // Checks if rotary encoder has turned, and in which direction
      previousState = currentState; 
      if(digitalRead(REout3) == currentState){
        return(-1); // Counter clockwise
      }
      else{
        return(1); // Clockwise
      }
    }
    if(digitalRead(REswitch) == 0){     // Checks for a button press
      return(0);
    } 
  }
}

// Checks if rotary encoder is moved during a second. Returns 1 for ccw, 2 for cw, and 3 for press. 
int sec_read_knob() {
  previousState = digitalRead(REout1);
  for (int j = 0; j < 1000; j += 100) {
    currentState = digitalRead(REout1); // Read current state of rotary encoder
    if(currentState != previousState){ // Checks if rotary encoder has turned, and in which direction
      previousState = currentState; 
      if(digitalRead(REout3) == currentState){
        return(-1); // Counter clockwise
      }
      else{
        return(1); // Clockwise
      }
    }
    if(digitalRead(REswitch) == 0){     // Checks for a button press
      return(0);
    }
    delay(100); 
  }
}

void reset_system() {   // shuts off all valves and returns to main menu
  digitalWrite(fill_pin, close_valve);
  digitalWrite(valve_pins[0], close_valve);
  digitalWrite(valve_pins[1], close_valve);
  digitalWrite(valve_pins[2], close_valve);
  digitalWrite(valve_pins[3], close_valve);
  digitalWrite(valve_pins[4], close_valve);
  digitalWrite(valve_pins[5], close_valve);
  choose_option(menu());
}
