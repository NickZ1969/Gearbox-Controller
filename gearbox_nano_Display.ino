//Transmission controller
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <U8g2lib.h>

//U8G2_SH1106_128X64_NONAME_F_4W_HW_SPI u8g2_0(U8G2_R0, /* cs=*/ 7, /* dc=*/ 6, /* reset=*/ 8);
volatile int active;
//4th gear is safest to start in if all else fails. Engine can't overrev in 4th
volatile int currentGear = 4;
//Set lockup status to engaged to force unlock at first change unless later read otherwise, for safety
volatile int lockupStatus = 0;
const int solenoid1 = 2;
const int solenoid2 = 3;
const int LUsolenoid = 4;
const int S1read = 42;
const int S2read = 44;
const int LUread = 12;
const int masterSwitch = 14;
const int upButton = 15;
const int downButton = 16;
const int overrun = 10;
const int linepressure = 5;
const int lineresistor = 9;
int counter = 0;
int counter1 = 0;
unsigned long disarmTimer;
unsigned long sensitivity = 500UL;
int buttonsArmed = 0;
int firstRun = 1;
int firstInactiveRun = 1;

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

void setup()
{
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  cbi(ADCSRA, ADPS0);
  
 Serial.begin(9600);
//  u8g2_0.begin();

  //Initialize serial
  //Initialize relays in OFF position
  digitalWrite(overrun, LOW);
  digitalWrite(linepressure, LOW);
  digitalWrite(lineresistor, LOW);
  digitalWrite(solenoid1, LOW);
  digitalWrite(solenoid2, LOW);
  digitalWrite(LUsolenoid, LOW);
  //Set pins to output mode
  pinMode(overrun, OUTPUT);
  pinMode(linepressure, OUTPUT);
  pinMode(lineresistor, OUTPUT); 
  pinMode(solenoid1, OUTPUT);
  pinMode(solenoid2, OUTPUT);
  pinMode(LUsolenoid, OUTPUT);  
  //Pins for switches
  pinMode(masterSwitch, INPUT);
  pinMode(upButton, INPUT);
  pinMode(downButton, INPUT);
  //Pins for gear sensing
  pinMode(S1read, INPUT);
  pinMode(S2read, INPUT);
  pinMode(LUread, INPUT);
  pinMode(6, OUTPUT);
  pinMode(7, OUTPUT);
  pinMode(8, OUTPUT);
  pinMode(11, OUTPUT);
                                                                                        
}

void loop()
{
  //Serial counter to watch loop times
Serial.println("looping");

  //Master switch goes between auto and manual
  if (digitalRead(masterSwitch) == 1) {
    active = 1;
  }
  else{
    active = 0;
  }

    //Active 0 is automatic/factory passthru mode
    if (active == 0) {
      if (firstInactiveRun == 1) {
          //Ensure all relays are off on first loop since arduino is still connected
          digitalWrite(solenoid1, LOW);
          digitalWrite(solenoid2, LOW);
          digitalWrite(LUsolenoid, LOW);
          firstInactiveRun = 0;
          //FirstRun set to 1 syncs manual mode to the current gear when initially switching to manual
          firstRun = 1;
  
      }
      //Take input and discard to avoid queueing changes
      char ser = Serial.read();
      //Monitor shift solenoids for gear and lockup clutch
      determineGear();
      determineLockup();
      digitalWrite(6, LOW);
      digitalWrite(7, LOW);
      digitalWrite(8, LOW);
      digitalWrite(11, LOW);
      //Display gear position
      //u8g2_0.firstPage();
      //u8g2_0.setFont(u8g2_font_ncenB10_tr);
      //u8g2_0.drawStr(20,25,"Deactivated");
      //u8g2_0.sendBuffer();

    }

    //Active 1 is paddle shift manual mode
    if (active == 1) {
      if (firstRun == 1) {
        //Activate relays to match what automatic/passthru mode was doing on the previous loop
        if (lockupStatus == 1) {
          lockupEngage(1);
        }
        else {
          lockupDisengage(1);
        }
        callGear(currentGear);
        //FirstInactiveRun forces all relays off when initally switched to auto mode
        firstInactiveRun = 1;
        firstRun = 0;
      }
      
counter1++;
  if (counter1 >= 100) {
    counter1 = 0;
  }
      if (counter1 == 50) {
        //Geardisplay();
    }

      //This is critical. A normal IF statement will process one gearchange only, iterate through the loop, then process the next.
      //The WHILE loop will parse gearchanges for as many serial reads are queued up. It's the difference between a fast 
      //sequence and 'perfect' synchronization.
      while (Serial.available()) {
        char ser = Serial.read();
        //Convert from ASCII to int. Hack, but serial wont normally be used.
        ser = ser - 48;
        callGear(ser);
      }

      //ButtonsArmed is a millis-based debounce system
      if (buttonsArmed == 1) {          
        if (digitalRead(upButton) == 1) {
          upShift();
          buttonsArmed = 0;
          disarmTimer = millis();
        }
        if (digitalRead(downButton) == 1) {
          downShift();
          buttonsArmed = 0;
          disarmTimer = millis();
        }
      }
      else{
        //If not armed, check disarmTimer to re-arm
        if ((millis() - disarmTimer) >= sensitivity) {
          buttonsArmed = 1;
        }
      }
    }

//Counter only functions as performance metric. Loops at 1000 to prevent overflow
  counter++;
  if (counter >= 1000) {
    counter = 0;
  }
  //delay(800);
}

//The following subs activate specific solenoids to shift to the desired gear. There are 2 shift solenoids and 1 lockup clutch solenoid
void firstGear()
{
  digitalWrite(solenoid1, HIGH);
  digitalWrite(solenoid2, HIGH);
  currentGear = 1;
  digitalWrite(6, LOW);
      digitalWrite(7, LOW);
      digitalWrite(8, HIGH);
      digitalWrite(11, LOW);
    //Geardisplay();
}
void secondGear()
{
  digitalWrite(solenoid1, LOW);
  digitalWrite(solenoid2, HIGH);
  currentGear = 2;
  digitalWrite(6, LOW);
      digitalWrite(7, LOW);
      digitalWrite(8, LOW);
      digitalWrite(11, HIGH);
  //Geardisplay();
}
void thirdGear()
{
  digitalWrite(solenoid1, LOW);
  digitalWrite(solenoid2, LOW);
  currentGear = 3;
   digitalWrite(6, LOW);
      digitalWrite(7, LOW);
      digitalWrite(8, HIGH);
      digitalWrite(11, HIGH);
  //Geardisplay();
}
void fourthGear()
{
  digitalWrite(solenoid1, HIGH);
  digitalWrite(solenoid2, LOW);
  currentGear = 4;
  digitalWrite(6, LOW);
      digitalWrite(7, HIGH);
      digitalWrite(8, LOW);
      digitalWrite(11, LOW);
  //Geardisplay(); 
}

//Lockup clutch needed a timer to prevent shifting while the torque converter is locked (a bad thing).
//If called with NoDelay, it performs just like the gear changes.
//This is useful for FirstRun conditions to sync the relays with passthru mode for seamless transitions between auto/manual
void lockupEngage(int noDelay)
{
  //Delay before continuing in case of pending gear change
  if (noDelay != 1) {
    delay(750);
  }
  digitalWrite(LUsolenoid, HIGH);
  lockupStatus = 1;
}
void lockupDisengage(int noDelay)
{
  digitalWrite(LUsolenoid, LOW);
  lockupStatus = 0;
  Geardisplay();
    
  //Delay before continuing in case of pending gear change
  if (noDelay != 1) {
    delay(750);
  }
}

//upShift and downShift are called by the shifter buttons for sequential gearchanges. 
//Handles TC lockup, prevents shifting above 4th or below 1st
void upShift()
{
  if (currentGear == 4) {
    lockupEngage(0);
  }
  if (currentGear < 4) {
    currentGear++;
    if (lockupStatus == 1) {
      lockupDisengage(0);
    }
  }

  switch (currentGear) {
   {
    case 2:
      secondGear();
   } 
   break;
   {
    case 3:
      thirdGear();
   } 
   break;
   {
    case 4:
      fourthGear();
   } 
   break;
  }
}

void downShift()
{
  if ((currentGear == 4) && (lockupStatus == 1)) {
    lockupDisengage(0);
  }
  else {
    if (currentGear > 1) {
      currentGear--;
      if (lockupStatus == 1) {
        lockupDisengage(0);
      }
    }
    switch (currentGear) {
     {
      case 1:
        firstGear();
     } 
     break;
     {
      case 2:
        secondGear();
     } 
     break;
     {
      case 3:
        thirdGear();
     } 
     break;
    }
  }
}

//Code to call specific gear, not necessarily sequentially.
//Currently used for serial testing
void callGear(int option){
  switch (option) {
    {
    case 1:
      firstGear();
    }
    break;
    {
    case 2:
      secondGear();
    }
    break;
    {
    case 3:
      thirdGear();
    }
    break;
    {
    case 4:
      fourthGear();
    }
    break;
    {
    case 7:
      upShift();
    }
    break;
    {
    case 8:
      downShift();
    }
    break;
    {
    case 5:
      lockupEngage(0);
    }
    break;
    {
    case 6:
      lockupDisengage(0);
    }
    break;
  }
}

//Monitor voltage of solenoid 1 and 2 wires to see what the factory computer is doing
//2 solenoids, 2 states each = 4 gears
void determineGear() {
  if (digitalRead(S1read) == 0) {
    if (digitalRead(S2read) == 0) {
      currentGear = 1;
    }
    else {
      currentGear = 4;
    }
  }
  else {
    if (digitalRead(S2read) == 0) {
      currentGear = 2;
    }
    else {
      currentGear = 3;
    }
  }
}

//Monitor voltage of lockup clutch solenoid
void determineLockup() {
  if (digitalRead(LUread) == 1) {
    lockupStatus = 1;
  }
 else {
   lockupStatus = 0;
 }
}
// Gear Display
void Geardisplay() {
  //u8g2_0.firstPage();
  //u8g2_0.setFont(u8g2_font_logisoso58_tr);
  //u8g2_0.setCursor(0,60);
  //u8g2_0.print(currentGear);
  //u8g2_0.sendBuffer(); 
  
  //if (lockupStatus == 1) {
   //u8g2_0.setFont(u8g2_font_ncenB10_tr);
   //u8g2_0.drawStr(40,50,"TC");
    //u8g2_0.sendBuffer();
   }

//}
