  #include <Wire.h>                 // This library provides I2C (Two-Wire) communication for devices like the LCD
  #include <LiquidCrystal_I2C.h>    // This library allows control of an I2C-based LCD display
  #include <Stepper.h>              // This library supports basic stepper motor control
  #include <SPI.h>
  #include <WiFiNINA.h>
  #include "arduino_secrets.h"
  //libraries needed for Rev2 ISR
  #include <avr/io.h>
  #include <avr/interrupt.h>

  int state = 0;                     // Tracks the current state of the app connection
  int hallSensorPin = 2;             // Digital pin connected to the Hall effect sensor
  int hall_state = 0;                // Stores the current reading from the Hall sensor
  int connection_status = 0;

  // Define the pins used by the stepper motor driver
  #define stepPin 3                 // Pin that sends step pulses to the stepper driver
  #define dirPin 4                  // Pin that sets the rotation direction of the motor
  #define delaytime 12             // Delay (ms) between steps; ~8 ms is the maximum reliable speed
  #define counts_per_pill  1       // Number of dispensing cycles per pill (1 ≈12.5s rotation)
                                    // 2 makes the wheel rotate for 24 seconds
                                    // 0 doesn't make the wheel rotate at all
  // EEPROM addresses for storing current clock time
  #define hours   0                // EEPROM index for current hours value
  #define minutes 1                // EEPROM index for current minutes value
  #define AM_PM   2                // EEPROM index for AM/PM flag (0=AM, 1=PM)
  // Constants for AM and PM
  #define AM      0
  #define PM      1
  // Commands received from the Android app over serial connection
  #define connected    'C'       // App has established a connection
  #define disconnecting 'D'       // App is disconnecting
  #define cancel        'X'       // User cancels an action
  #define set           'S'       // User confirms a pill setting
  #define Exit          'e'       // Marker for end of data transmission

  //(None of the EEPROM functionality actually works)
  // Pill identifiers and their corresponding EEPROM offsets
  #define pill_1    'a'           // Command character for pill slot 1
  #define P1_state   5            // EEPROM index for pill 1 "set" flag
  #define P1_hour    6            // EEPROM index for pill 1 dispensing hour
  #define P1_minute  7            // EEPROM index for pill 1 dispensing minute
  #define P1_AM_PM   8            // EEPROM index for pill 1 AM/PM flag
  // ... definitions continue similarly up to pill_14
  #define pill_2    'b'
  #define P2_state  10
  #define P2_hour   11
  #define P2_minute 12
  #define P2_AM_PM  13
  #define pill_3    'c'
  #define P3_state  15
  #define P3_hour   16
  #define P3_minute 17
  #define P3_AM_PM  18
  #define pill_4    'd'
  #define P4_state  20
  #define P4_hour   21
  #define P4_minute 22
  #define P4_AM_PM  23
  #define pill_5    'e'
  #define P5_state  25
  #define P5_hour   26
  #define P5_minute 27
  #define P5_AM_PM  28
  #define pill_6    'f'
  #define P6_state  30
  #define P6_hour   31
  #define P6_minute 32
  #define P6_AM_PM  33
  #define pill_7    'g'
  #define P7_state  35
  #define P7_hour   36
  #define P7_minute 37
  #define P7_AM_PM  38
  #define pill_8    'h'
  #define P8_state  40
  #define P8_hour   41
  #define P8_minute 42
  #define P8_AM_PM  43
  #define pill_9    'k'
  #define P9_state  45
  #define P9_hour   46
  #define P9_minute 47
  #define P9_AM_PM  48
  #define pill_10   'l'
  #define P10_state  50
  #define P10_hour   51
  #define P10_minute 52
  #define P10_AM_PM  53
  #define pill_11   'm'
  #define P11_state  55
  #define P11_hour   56
  #define P11_minute 57
  #define P11_AM_PM  58
  #define pill_12   'n'
  #define P12_state  60
  #define P12_hour   61
  #define P12_minute 62
  #define P12_AM_PM  63
  #define pill_13   'o'
  #define P13_state  65
  #define P13_hour   66
  #define P13_minute 67
  #define P13_AM_PM  68
  #define pill_14   'p'
  #define P14_state  70
  #define P14_hour   71
  #define P14_minute 72
  #define P14_AM_PM  73

  #define empty   0                // Value indicating a pill slot is empty
  #define set  1                // Value indicating a pill slot has been set


  LiquidCrystal_I2C lcd(0x27,16,2); //create object named lcd
                                    //first argument is the address of lcd
                                    //second argument is the number of columns
                                    //third argument is the number of rows

  //----Function prototypes for clarity and modular structure----//

  void welcome_message();            // Display the startup welcome message
  void wait_app_connecting();        // Prompt the user to connect the device to the app
  void wait_setting_time();          // Receive and store the current time from the app
  void start_time_counting();        // Configure Timer2 for 0.01s interrupts for clock functionality
  void device_ready_to_work();       // Notify the user that pill scheduling can begin
  void time_showing();               // Continuously update LCD with current time and next pill time
  void time_setting(char pill_loc);  // Receive and store scheduling data for a specific pill
  int app_command();                 // Read the latest incoming command from the app
  char check_pill_state(char pill);  // Check whether a pill slot is already configured
  void stepper_moving();             // Dispense a pill by rotating the motor until cup is detected
  void forward();                    // Execute the minimal pulse sequence to step the motor

  volatile int counter = 0;          // Counts Timer2 overflows to track real time
  volatile char Minutes, Hours, carry, ampm; // Used in ISR for intermediate time calculations
  volatile byte memory[100] = {0};   // Simulated EEPROM space for clock and pill schedules
  char app_state = 'C';       // Current state of the Bluetooth app connection

  void setup() {

      // Sets the two motor pins as outputs
    pinMode(stepPin, OUTPUT);        // Set step pin as output
    pinMode(dirPin, OUTPUT);         // Set direction pin as output
    connection_status = WiFi.begin(SECRET_SSID, SECRET_PASS);
    lcd.init();                     //initialize the lcd
                                    //Note: This .init() method also starts the I2C bus, i.e. there does not need to be
                                    //a separate "Wire.begin();" statement in the setup.
    lcd.backlight();                //turn on the backlight of LCD

    Serial.begin(9600);            //sets the baudrate of the UART communication to 9600

    while (!Serial) {
      ; // wait for serial port to connect. Needed for native USB port only
    }
    // check for the WiFi module:
    if (WiFi.status() == WL_NO_MODULE) {
      Serial.println("Communication with WiFi module failed!");
      // don't continue
      while (true);
    }


    // attempt to connect to WiFi network:
    while (connection_status != WL_CONNECTED) {
      Serial.print("Attempting to connect to WiFi: ");
      Serial.println(SECRET_SSID);
      // Connect to WPA/WPA2 network:

      // wait 10 seconds for connection:
      delay(10000);
    }
    // you're connected now, so print out the data:
    Serial.print("You're connected to the network");
    printCurrentNet();
    printWifiData();
    welcome_message();            //shows the welcome message
    wait_app_connecting();        //Wait for user to connect via app
    wait_setting_time();            // Receive current time from app and store it
    start_time_counting();          // Begin hardware timer for clock operation
    device_ready_to_work();         // Inform user that pill scheduling can start

  }

  void loop() {

    // check the network connection once every 10 seconds:
    delay(10000);
    printCurrentNet();

    int state, command;
    command = app_command();
    hall_state = digitalRead(hallSensorPin); // Read Hall sensor (cup presence)
    state = app_state;


    // Continue processing as long as the app remains connected
    while(state == 'C'){
      if( command == pill_1){
      // Show current setting or empty status
        check_pill_state(P1_state);
      // Wait for the user to either set, cancel, or disconnect
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing(); // Return to time display if cancelled
        }else if( command == set){
          time_setting(P1_hour); // Receive and store time for pill 1
        }
        // same logic for pills 2 through 14
      }else if( command == pill_2){
        if(check_pill_state(P2_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P2_hour);
        }
      }else if( command == pill_3){
        if(check_pill_state(P3_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P3_hour);
        }
      }else if( command == pill_4){
        if(check_pill_state(P4_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P4_hour);
        }
      }else if( command == pill_5){
        if(check_pill_state(P5_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P5_hour);
        }
      }else if( command == pill_6){
        if(check_pill_state(P6_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P6_hour);
        }
      }else if( command == pill_7){
        if(check_pill_state(P7_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P7_hour);
        }
      }else if( command == pill_8){
        if(check_pill_state(P8_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P8_hour);
        }
      }else if( command == pill_9){
        if(check_pill_state(P9_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P9_hour);
        }
      }else if( command == pill_10){
        if(check_pill_state(P10_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P10_hour);
        }
      }else if( command == pill_11){
        if(check_pill_state(P11_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P11_hour);
        }
      }else if( command == pill_12){
        if(check_pill_state(P12_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P12_hour);
        }
      }else if( command == pill_13){
        if(check_pill_state(P13_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P13_hour);
        }
      }else if( command == pill_14){
        if(check_pill_state(P14_state))
        do{
          command = app_command();
          state = app_state;
        }while(!(command == cancel || command==set || state==disconnecting));
        if( command ==  cancel || command == disconnecting){
          time_showing();
        }else if( command == set){
          time_setting(P14_hour);
        }
      }else{
        time_showing();
        delay(100);
      }
      command = app_command();
      state = app_state;
    }
    time_showing();
    delay(100);

  } //end of main loop

    void printWifiData() {
    // print your board's IP address:
    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);
    Serial.println(ip);

    // print your MAC address:
    byte mac[6];
    WiFi.macAddress(mac);
    Serial.print("MAC address: ");
    printMacAddress(mac);
  }

  void printCurrentNet() {
    // print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    // print the MAC address of the router you're attached to:
    byte bssid[6];
    WiFi.BSSID(bssid);
    Serial.print("BSSID: ");
    printMacAddress(bssid);

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.println(rssi);

    // print the encryption type:
    byte encryption = WiFi.encryptionType();
    Serial.print("Encryption Type:");
    Serial.println(encryption, HEX);
    Serial.println();
  }

  void printMacAddress(byte mac[]) {
    for (int i = 5; i >= 0; i--) {
      if (mac[i] < 16) {
        Serial.print("0");
      }
      Serial.print(mac[i], HEX);
      if (i > 0) {
        Serial.print(":");
      }
    }
    Serial.println();
  }

  void start_time_counting(void){

    cli();  // disable interrupts during configuration
    TCA0.SINGLE.CTRLA = 0; //stop the timer
    TCA0.SINGLE.CTRLB = 0; //normal mode
    TCA0.SINGLE.CNT = 0; //clear the counter
    // Set the period for 0.01 seconds (100 Hz)
    // Formula: TOP = (F_CPU / (Prescaler * Desired Frequency)) - 1
    // For 16 MHz, Prescaler = 1024, Desired Frequency = 100 Hz:
    // TOP = (16,000,000 / (1024 * 100)) - 1 = 155
    TCA0.SINGLE.PER = 155; // set period register
    // Configure the prescaler (1024)
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc | TCA_SINGLE_ENABLE_bm;
    // Enable overflow interrupt
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_OVF_bm;
    //re-enable interrutps
    sei();
  }

  /*
  *  implmentation of "device ready to work" function.
  *  This function is used to show messages to users on LCD to
      notify them that the pill dispenser is ready to work.
  */
  void device_ready_to_work(void){

    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("The dispenser is");
    lcd.setCursor(2,1);
    lcd.print("ready for the");
    lcd.setCursor(2,0);
    lcd.print("individual pill");
    lcd.setCursor(2,1);
    lcd.print("time settings.");
    delay(2000);
  }

  /*
  * implmentation of welcome message function.
  * this function used to show welcome message and play sound track for users.
  */
  void welcome_message(void){
    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("Welcome to the");
    lcd.setCursor(1,1);
    lcd.print("pill dispenser.");
    delay(4000);

  }

  /*
  * implmentation of wait app connecting function.
  * this function to notify users to connect pill dispenser divce with android app to could set time for device and pills
  */
  void wait_app_connecting(void){
    char reading=0;
    lcd.clear();
    lcd.setCursor(1,0);
    lcd.print("Please connect");
    lcd.setCursor(3,1);
    lcd.print("to the app");
    delay(4000);
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("to start the");
    lcd.setCursor(2,1);
    lcd.print("time setting.");
    delay(4000);
  }

  /*
  * implmentation of wait setting time.
  * this function used to receive currently time from user.
  */
  void wait_setting_time(void){
    char reading=0,counter=0;                     //one of these two variables used to save coming data temporary on it and the another used as counter.
    int data_array[7];                            //create this array to save received time data on it.
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("(Waiting for");
    lcd.setCursor(0,1);
    lcd.print("time setting...)");
    do{                                         //this do while loop used to receive data from android app through using bluetooth module
      if(Serial.available()){                   //then save this data on data array to reuse it later.
        reading = Serial.read();
        data_array[counter]=reading-'0';
        counter=(counter+1)%7;
      }

    }while(reading != Exit);                     //read data until device receive 'e',indicator for exit command, from user
    memory[hours]= data_array[0]*10 + data_array[1];      //the following three lines to manipulate the received data which saved in data array to save it as time
    memory[minutes]=data_array[3]*10 + data_array[4];     //in the coreponding location in array.
    memory[AM_PM]=(data_array[5] == ('A'-'0'))?AM:PM;
    lcd.setCursor(4,1);
    if(memory[hours]<10)
      lcd.print(0);
    lcd.print(memory[hours]);
    lcd.print(":");
    if(memory[minutes]<10)
      lcd.print(0);
    lcd.print(memory[minutes]);
    lcd.print(" ");
    if(memory[AM_PM] == AM){
      lcd.print("AM");
    }else{
      lcd.print("PM");
    }
    delay(2000);
  }

  /*
  * implmentation of time showing function.
  * this function have two roles:
  * first, it's used to show (on the LCD) current time and the time that the next pill is ready to be dispensed if it's been set
  * second, it compares the current time to that of the next pill that's ready, so that if it's time then the stepper moving function
  * is called to rotate the stepper motor to dispense the pill.
  */
  void time_showing(void){
    static char counter=5;    //this counter variable is used to know which pill is ready to be dispensed.
    lcd.clear();              //the next 16 line related to showing currently time on LCD.
    lcd.setCursor(0,0);
    lcd.print("Time ");
    lcd.setCursor(8,0);
    if(memory[hours]<10)
      lcd.print(0);
    lcd.print(memory[hours]);
    lcd.print(":");
    if(memory[minutes]<10)
      lcd.print(0);
    lcd.print(memory[minutes]);
    lcd.print(" ");
    if(memory[AM_PM] == AM){
      lcd.print("AM");
    }else{
      lcd.print("PM");
    }
    if(memory[counter] == 1){           //this if conditions is used to check if there pill set before and still on pill dispenser or not

      lcd.setCursor(0,1);               //the next 15 lines is for showing the time of next pill that's ready to be dispensed on LCD.
      lcd.print("next");
      lcd.setCursor(8,1);
      if(memory[counter+1]<10)
        lcd.print(0);
      lcd.print(memory[counter+1]);
      lcd.print(":");
      if(memory[counter+2]<10)
        lcd.print(0);
      lcd.print(memory[counter+2]);
      lcd.print(" ");
      if(memory[counter+3] == AM){
        lcd.print("AM");
      }else{
      lcd.print("PM");
      }

      if(memory[hours] == memory[counter+1]){         //the following three if conditions to compare currently time 'hours, minutes and AM or PM' to
        if(memory[minutes] == memory[counter+2]){     //the next pill that's ready to be dispensed
          if( memory[AM_PM] == memory[counter+3]){
            if(counter>=10)
              memory[counter-5]=0;
            lcd.clear();                             //the following lines to show message for user on LCD to notify about ready pill for outting.
            lcd.print("Outputting...");
            lcd.setCursor(3,1);
            // trying [counter+1]<9 plus (counter <= 9); original value is 10: "You need to set pill 1 first"
            if(memory[counter+1]<10)
            lcd.print(0);
            lcd.print(memory[counter+1]);
            lcd.print(":");
            if(memory[counter+2]<10)
            lcd.print(0);
            lcd.print(memory[counter+2]);
            lcd.print(" ");
            if(memory[counter+3] == AM){
            lcd.print("AM");
            }else{
            lcd.print("PM");
            }
            stepper_moving();                        //calling stepper moving function to rotate the motor.
            counter = (counter+5)%75;                //this line to increament counter value by 5 until 70
            if(counter == 0)                         //this if condition used to reset counter value to 5
              counter = 5;
          }
        }
      }

    }else{                                          //this else statements used to show -- incase of no pill seted on pill dispenser device
      lcd.setCursor(0,1);
      lcd.print("next");
      lcd.setCursor(8,1);
      lcd.print("--:-- --");
    }

  }

  /*
  * implementation of time setting function.
  * this function get hour memory location of pill which required to set it as argument
  * to save its which users will set in the predetermined locations of time.
  */

  void time_setting(char pill_hour_location){
    char reading=0,counter=0;
    int data_array[7];
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("Time setting:");
    do{                                           //this do while loop to receive time from user and save in in data array
      if(Serial.available()){
        reading = Serial.read();
        data_array[counter]=reading-'0';
        counter=(counter+1)%7;
      }

    }while(!(reading == Exit || reading == cancel));
    if(reading == Exit){                                              //this if condition to know if user enter time or not.
      memory[pill_hour_location-1]=1;                                   //this line to set state of this pill by one
      memory[pill_hour_location]= data_array[0]*10 + data_array[1];     //the following three lines to manipulate the received data which saved in data array to save
      memory[pill_hour_location+1]=data_array[3]*10 + data_array[4];    //it as time in the coresponding locations
      memory[pill_hour_location+2]=(data_array[5] == ('A'-'0'))?AM:PM;
      lcd.setCursor(4,1);                                               //the following 14 lines are for showing time on the LCD
      if(memory[pill_hour_location]<10)
        lcd.print(0);
      lcd.print(memory[pill_hour_location]);
      lcd.print(":");
      if(memory[pill_hour_location+1]<10)
        lcd.print(0);
      lcd.print(memory[pill_hour_location+1]);
      lcd.print(" ");
      if(memory[pill_hour_location+2] == AM){
        lcd.print("AM");
      }else{
        lcd.print("PM");
      }
      delay(2000);
    }
  }

  /*
  * implmentation of app command function.
  * this function used to get commands from user through android app and determine the state of bluetooth
  * if it connecting or disconnecting
  */

  int app_command(void){

    char reading=0;
    if(Serial.available()){
      reading = Serial.read();
      if(reading == disconnecting){
        app_state = disconnecting;
      }else if(app_state == disconnecting){
        app_state = 'C';
      }
    }
    return reading;
  }

  /*
  * implmentation of check pill state function.
  * this function get pill state location as argument to check if this pill set before or not.
  * if it set before LCD will show messages for users to notify them this pill was set before
  * and if not set before also will show messages for users to notify them the cell is empty.
  */

  char check_pill_state(char pill){
    char reading = memory[pill];
    char state=0;
    if(reading == empty){
      lcd.clear();
      lcd.setCursor(2,0);
      lcd.print("this cell is");
      lcd.setCursor(4,1);
      lcd.print("empty");
      delay(2000);
      state=1;
    }else{
      lcd.clear();
      lcd.print("set to");
      lcd.setCursor(2,1);
      if(memory[pill+1]<10)
        lcd.print(0);
      lcd.print(memory[pill+1]);
      lcd.print(":");
      if(memory[pill+2]<10)
        lcd.print(0);
      lcd.print(memory[pill+2]);
      lcd.print(" ");
      if(memory[pill+3] == AM){
        lcd.print("AM");
      }else{
        lcd.print("PM");
      }
      delay(2000);
    }
    return state;
  }

  /*
  * the following 5 functions related to motion of stepper motor.
  */

  // The stepper_moving function will incorporate the Hall sensor's state.
  void stepper_moving(void){
        while (digitalRead(hallSensorPin) == LOW) {
              lcd.clear();
              lcd.setCursor(1, 0);
              lcd.print("Please set the");
              lcd.setCursor(3, 1);
              lcd.print("cup first.");
              delay(1000);
        }
        lcd.clear();
        lcd.setCursor(3, 0);
        lcd.print("Dispensing...");
        for (int counter = 0; counter < counts_per_pill; counter++) {
            forward();
        }
        lcd.clear();
        lcd.print("Done!");
        delay(1000);
      }

  void forward(){//one tooth forward
      digitalWrite(dirPin,HIGH); // Enables the motor to move in a particular direction
      // Makes 200 pulses to rotate the shaft and wheel by one full slot
      for(int x = 0; x < 200; x++) {
        digitalWrite(stepPin,HIGH);
        delayMicroseconds(500);    // by changing this time delay between the steps we can change the rotation speed
        digitalWrite(stepPin,LOW);
        delayMicroseconds(500);
    }
  }
  /*
  * interrupt service rotuine of timer2 over flow interrupt.
  */
  ISR(TIMER2_OVF_vect){
    TCA0.SINGLE.CNT = 0;       //clear the counter
    counter++;
    if(counter == 6000){             //if counter value reach to 6000 this mean one minute passed so statements of this if condition to update
      counter =0;                    //time and save the updated time numbers in their specified location in memory array.
      Minutes = memory[minutes];
      Hours = memory[hours];
      ampm = memory[AM_PM];
      carry = (Minutes+1)/60;

      Minutes = (Minutes+1)%60;
      if(carry){
        Hours=Hours+1;
        if(Hours==12)
          memory[AM_PM]=ampm ^ 1;
        if(Hours==13)
          Hours=1;
      }
      memory[minutes]=Minutes;
      memory[hours]=Hours;
    }
  }
