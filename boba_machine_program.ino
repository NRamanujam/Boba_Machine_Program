/*
 * Title: Boba Machine Program
 * Author(s): The Boba Factory
 * March 2020
 * Rev 1
 * Controls Stepper Motors (including Archimedes Screw), Contactors, and Solenoids
 * Monitors RTDs and Load Cells
 * 
 * Notes: Someone pls add a stall detect feature for the motors if needed, I'm not sure how
 *        If anyone has experience with multithreading, can you help me understand the TeensyThreads library?
 *        Let me know if I should use a function for controlling all 3 cooking pots (used here) or each pot individually
 *        Placeholder values are indicated w/ capitalized comments, should be altered by anyone who's doing testing
 *        Not a fan of how I'm checking for boiling water
 *        I'm following the PJRC website documentation and DualMotorShield AccelStepper example for stepper motor control thru drivers
 *        Contact nikhita.ramanujam2@gmail.com if something is weird, incorrectly used, or doesn't work, which it probably won't the first time 
 */


#include <Wire.h>               //Used for I2C
#include <TeensyThreads.h>      //Used for multi-tasking on Teensy
#include <AccelStepper.h>       //Used for stepper motor control    
#include <Adafruit_MAX31865.h>  //Used for RTDs
#include "Adafruit_MCP23017.h"  //Used for GPIO Expander
#include <Adafruit_ADS1015.h>   //Used for load cells


//Teensy 3.6 Pins
#define SEN1-LC1-DAT        2
#define SEN1-LC1-CLK        3
#define MTR1-STEP1-ENABLE   4
#define MTR1-STEP1-STEP     5
#define MTR1-STEP1-DIR      6
#define XBEE_TX             7
#define XBEE_RX             8
#define SEN2-RTD1-CS        9
#define SEN3-RTD2-CS        10
#define SPI_MOSI            11
#define SPI_MISO            12
#define SPI_SCK             13
#define MTR2-STEP2-ENABLE   14
#define MTR2-STEP2-STEP     15
#define MTR2-STEP2-DIR      16
#define MTR3-STEP3-ENABLE   17
#define SDA                 18
#define SCL                 19
#define SEN4-RTD3-CS        21
#define TC_DO               22
#define TC_SCK              23
#define MTR3-STEP3-STEP     24
#define MTR3-STEP3-DIR      25
#define MTR4-STEP4-ENABLE   26
#define MTR4-STEP4-STEP     27
#define LED6-STATUS1        28
#define LED7-STATUS2        29
#define MTR4-STEP4-DIR      30
#define MTR5-STEP5-ENABLE   31
#define MTR5-STEP5-STEP     32
#define MTR5-STEP5-DIR      33
#define MTR6-STEP6-ENABLE   34
#define MTR6-STEP6-STEP     35
#define MTR6-STEP6-DIR      36
#define MTR7-STEP7-ENABLE   37
#define MTR7-STEP7-STEP     38
#define MTR7-STEP7-DIR      39


//GPIO-EXP1 Pins
#define SOL1-WSV1-IN-S+     0
#define SOL2-WSV1-OUT-S+    1   
#define SOL3-WSV2-IN-S+     2
#define SOL4-WSV2-OUT-S+    3
#define SOL5-WSV3-IN-S+     4
#define SOL6-WSV3-OUT-S+    5
#define SOL7-WSV4-IN-S+     6
#define SOL8-SSV1-IN-S+     7
#define SOL9-SSV1-OUT-S+    8
#define CON1-HP1-S+         9
#define CON2-HP2-S+         10
#define CON3-HP3-S+         11


//For use with RTDs
#define RREF      430.0     //Value of Rref resistor (PT100: 430, PT1000: 4300)
#define RNOMINAL  100.0     //0-degrees-C resistance of sensor  (PT100: 100, PT1000: 1000)


//Instantiate Class for Steppers
AccelStepper step1(1, MTR1-STEP1-STEP, MTR1-STEP1-DIR);
AccelStepper step2(1, MTR2-STEP2-STEP, MTR2-STEP2-DIR);
AccelStepper step3(1, MTR3-STEP3-STEP, MTR3-STEP3-DIR);
AccelStepper step4(1, MTR4-STEP4-STEP, MTR4-STEP4-DIR);
AccelStepper step5(1, MTR5-STEP5-STEP, MTR5-STEP5-DIR);
AccelStepper step6(1, MTR6-STEP6-STEP, MTR6-STEP6-DIR);
AccelStepper step7(1, MTR7-STEP7-STEP, MTR7-STEP7-DIR);


//Instantiate Class for RTDs
Adafruit_MAX31865 rtd1 = Adafruit_MAX31865(SEN2-RTD1-CS, SPI_MOSI, SPI_MISO, SPI_SCK);
Adafruit_MAX31865 rtd2 = Adafruit_MAX31865(SEN3-RTD2-CS, SPI_MOSI, SPI_MISO, SPI_SCK);
Adafruit_MAX31865 rtd3 = Adafruit_MAX31865(SEN4-RTD3-CS, SPI_MOSI, SPI_MISO, SPI_SCK);


//Instantiate Class for GPIO Expander
Adafruit_MCP23017 gpio1;


//Instantiate Class for Load Cells/ ADC
Adafruit_ADS1015 myScale;


//Other variables
uint8_t numRefills;                 //Number of times steps in bobaInit() runs to refill boba resovoir
uint8_t numOrders;                  //Number of times steps in myBoba() runs to fulfill customer orders
const uint32_t cookWater = 1000;    //Time for water to pour into cooking vessels -- THIS IS A PLACEHOLDER VALUE!
const uint32_t sugarWater = 1000;   //Time for water to pour into sugar soaking chamber -- THIS IS A PLACEHOLDER VALUE!
const uint32_t sugarSol = 1000;     //Time for sugar solution to pour into sugar soaking chamber -- THIS IS A PLACEHOLDER VALUE!
const uint32_t drainWater = 1000;   //Time for water to be drained from cooking vessels -- THIS IS A PLACEHOLDER VALUE!


void setup() {
  //***put your setup code here, to run once:
  Serial.begin(9600);

  //Starting parameters for bobaInit() and myBoba(), assuming that there is no boba cooked and no orders placed after hard reset
  numRefills = 7;
  numOrders = 0;

  //Initialize GPIO Expander Pins
  gpio1.begin();
  gpio1.pinMode(SOL1-WSV1-IN-S+, OUTPUT);
  gpio1.pinMode(SOL2-WSV1-OUT-S+, OUTPUT);
  gpio1.pinMode(SOL3-WSV2-IN-S+, OUTPUT);
  gpio1.pinMode(SOL4-WSV2-OUT-S+, OUTPUT);
  gpio1.pinMode(SOL5-WSV3-IN-S+, OUTPUT);
  gpio1.pinMode(SOL6-WSV3-OUT-S+, OUTPUT);
  gpio1.pinMode(SOL7-WSV4-IN-S+, OUTPUT);
  gpio1.pinMode(SOL8-SSV1-IN-S+, OUTPUT);
  gpio1.pinMode(SOL9-SSV1-OUT-S+, OUTPUT);
  gpio1.pinMode(CON1-HP1-S+, OUTPUT);
  gpio1.pinMode(CON2-HP2-S+, OUTPUT);
  gpio1.pinMode(CON3-HP3-S+, OUTPUT);

  //Initialize Load Cells
  myScale.begin();
  
  //Initialize Steppers
  step1.setMaxSpeed(200.0);           //Maximum speed of motor -- THIS IS A PLACEHOLDER VALUE!
  step1.setAcceleration(200.0);       //Acceleration of motor until it reaches max. speed and deccelerates -- THIS IS A PLACEHOLDER VALUE!
  
  step2.setMaxSpeed(200.0);           //THIS USES A PLACEHOLDER VALUE!
  step2.setAcceleration(200.0);       //THIS USES A PLACEHOLDER VALUE!
  
  step3.setMaxSpeed(200.0);           //THIS USES A PLACEHOLDER VALUE!
  step3.setAcceleration(200.0);       //THIS USES A PLACEHOLDER VALUE!
  
  step4.setMaxSpeed(200.0);           //THIS USES A PLACEHOLDER VALUE!
  step4.setAcceleration(200.0);       //THIS USES A PLACEHOLDER VALUE!
  
  step5.setMaxSpeed(200.0);           //THIS USES A PLACEHOLDER VALUE!
  step5.setAcceleration(200.0);       //THIS USES A PLACEHOLDER VALUE!
  
  step6.setMaxSpeed(200.0);           //THIS USES A PLACEHOLDER VALUE!
  step6.setAcceleration(200.0);       //THIS USES A PLACEHOLDER VALUE!
  
  step7.setMaxSpeed(200.0);           //THIS USES A PLACEHOLDER VALUE!
  step7.setAcceleration(200.0);       //THIS USES A PLACEHOLDER VALUE!
  step7.moveTo(0);                    //THIS USES A PLACEHOLDER VALUE!

  //Initialize RTDs
  rtd1.begin(MAX31865_3WIRE);
  rtd2.begin(MAX31865_3WIRE);
  rtd3.begin(MAX31865_3WIRE);

  //Calls bobaInit() to make 7 servings of boba, under the assumption that each call yields a serving of prepared boba
  bobaInit(numRefills);
  
  while(step7.distanceToGo() != 0){
    step7.run();                         
  }
}


void loop() {
  //***put your main code here, to run repeatedly:

  //Insert condition to update number of orders

  //Update number of required refills, based on number of orders

  if(numRefills <= 5){
    
  }

}


void bobaInit(uint8_t refills){
  //***Makes boba and fills/re-fills resovoir
  
  while(refills > 0){
    
    //Steppers move to starting positions
    step1.moveTo(0);                    //Absolute starting position of the motor -- THIS IS A PLACEHOLDER VALUE!
    step2.moveTo(0);                    //THIS USES A PLACEHOLDER VALUE!
    step3.moveTo(0);                    //THIS USES A PLACEHOLDER VALUE!
    step4.moveTo(0);                    //THIS USES A PLACEHOLDER VALUE!
    step5.moveTo(0);                    //THIS USES A PLACEHOLDER VALUE!
    step6.moveTo(0);                    //THIS USES A PLACEHOLDER VALUE!
    
    while(step1.distanceToGo() != 0){
      step1.run();                         
    }
    while(step2.distanceToGo() != 0){
      step2.run();
    }
    while(step3.distanceToGo() != 0){
      step3.run();
    }
    while(step4.distanceToGo() != 0){
      step4.run();                          
    }
    while(step5.distanceToGo() != 0){
      step5.run();
    }
    while(step6.distanceToGo() != 0){
      step6.run();
    }
  
    //Steppers 1-3 rotate cups of raw boba into cooking vessels
    step1.moveTo(100);                      //Position of motor when boba is rotated into cook pot -- THIS IS A PLACEHOLDER VALUE!
    step2.moveTo(100);                      //THIS USES A PLACEHOLDER VALUE!
    step3.moveTo(100);                      //THIS USES A PLACEHOLDER VALUE!
    
    while(step1.distanceToGo() != 0){
      step1.run();                          //Rotates boba into cooking vessels
    }
    while(step2.distanceToGo() != 0){
      step2.run();
    }
    while(step3.distanceToGo() != 0){
      step3.run();
    }
  
    //Turn on Solenoids 1,3,5 to pour water into cooking vessels for "cookWater" milliseconds
    gpio1.digitalWrite(SOL1-WSV1-IN-S+, HIGH);
    gpio1.digitalWrite(SOL3-WSV2-IN-S+, HIGH);
    gpio1.digitalWrite(SOL5-WSV3-IN-S+, HIGH);
    delay(cookWater);
    gpio1.digitalWrite(SOL1-WSV1-IN-S+, LOW);
    gpio1.digitalWrite(SOL3-WSV2-IN-S+, LOW);
    gpio1.digitalWrite(SOL5-WSV3-IN-S+, LOW);
  
    //Turn on Contactors 1-3 to turn on hot plates to heat cooking vessels
    gpio1.digitalWrite(CON1-HP1-S+, HIGH);
    gpio1.digitalWrite(CON2-HP2-S+, HIGH);
    gpio1.digitalWrite(CON3-HP3-S+, HIGH);
  
    //Monitor RTDs 1-3 until boiling (100-degrees-C)
    //I'm monitoring the RTDs every 3 seconds
    while(1){
      if ((rtd1.temperature(RNOMINAL, RREF) >= 100)){
        if ((rtd2.temperature(RNOMINAL, RREF) >= 100)){
          if ((rtd3.temperature(RNOMINAL, RREF) >= 100)){
            break;
          }
        }
      }
      delay(3000);
    }
  
    //Maintain RTD values for 7 minutes at boiling temperature
    for(long j = 420000; j >= 0; j -= 20000){
      if(rtd1.temperature(RNOMINAL, RREF) >= 100)      //Checks if temperatures are higher than/equal to boiling
        gpio1.digitalWrite(CON1-HP1-S+, LOW);          //Turn off hot plate if too hot
      if(rtd2.temperature(RNOMINAL, RREF) >= 100)
        gpio1.digitalWrite(CON2-HP2-S+, LOW);
      if(rtd3.temperature(RNOMINAL, RREF) >= 100)
        gpio1.digitalWrite(CON3-HP3-S+, LOW);    
      delay(10000);
      
      if(rtd1.temperature(RNOMINAL, RREF) < 100)      //Checks if temperatures are lower than boiling
        gpio1.digitalWrite(CON1-HP1-S+, HIGH);        //Turn on hot plate if too cold
      if(rtd2.temperature(RNOMINAL, RREF) < 100)
        gpio1.digitalWrite(CON2-HP2-S+, HIGH);
      if(rtd3.temperature(RNOMINAL, RREF) < 100)
        gpio1.digitalWrite(CON3-HP3-S+, HIGH);
      delay(10000);
    }
    gpio1.digitalWrite(CON1-HP1-S+, LOW);
    gpio1.digitalWrite(CON2-HP2-S+, LOW);
    gpio1.digitalWrite(CON3-HP3-S+, LOW);
  
    //Turn on Solenoid 7 for "sugarWater" milliseconds to add water to sugar-soaking chamber
    gpio1.digitalWrite(SOL7-WSV4-IN-S+, HIGH);
    delay(sugarWater);
    gpio1.digitalWrite(SOL7-WSV4-IN-S+, LOW);
  
    //Turn on Solenoid 8 for "sugarSol" milliseconds to add sugar solution to sugar-soaking chamber
    gpio1.digitalWrite(SOL8-SSV1-IN-S+, HIGH);
    delay(sugarSol);
    gpio1.digitalWrite(SOL8-SSV1-IN-S+, LOW);
  
    //Steppers 4-6 rotate cooked boba into sugar chamber
    step4.moveTo(100);                      //Position of motor once boba is rotated into sugar-soaker -- THIS IS A PLACEHOLDER VALUE!
    step5.moveTo(100);                      //THIS USES A PLACEHOLDER VALUE!
    step6.moveTo(100);                      //THIS USES A PLACEHOLDER VALUE!
    
    while(step4.distanceToGo() != 0){
      step4.run();                          //Rotates cooked boba into sugar-soaking container
    }
    while(step5.distanceToGo() != 0){
      step5.run();
    }
    while(step6.distanceToGo() != 0){
      step6.run();
    }
  
    //Wait until water is at 65-degrees-C, then use Solenoids 2,4,6 to drain used cooking water for "drainWater" milliseconds
    while(1){
      if ((rtd1.temperature(RNOMINAL, RREF) <= 65)){
        if ((rtd2.temperature(RNOMINAL, RREF) <= 65)){
          if ((rtd3.temperature(RNOMINAL, RREF) <= 65)){
            break;
          }
        }
      }
      delay(3000);
    }
    gpio1.digitalWrite(SOL2-WSV1-OUT-S+, HIGH);
    gpio1.digitalWrite(SOL4-WSV2-OUT-S+, HIGH);
    gpio1.digitalWrite(SOL6-WSV3-OUT-S+, HIGH);
    delay(drainWater);
    gpio1.digitalWrite(SOL2-WSV1-OUT-S+, LOW);
    gpio1.digitalWrite(SOL4-WSV2-OUT-S+, LOW);
    gpio1.digitalWrite(SOL6-WSV3-OUT-S+, LOW);
      
    refills--;
  }
}


void myBoba(uint8_t orders){
  //***Fills a cup with boba
  while(orders > 0){
    step7.moveTo(100);                              //THIS USES A PLACEHOLDER VALUE!

    int myTurns = 0;
    while(1){
      step7.run();
      if (myScale.readADC_SingleEnded(0) >= 1){     //THESE USE PLACEHOLDER VALUES!
       if (myScale.readADC_SingleEnded(1) >= 1){
        if (myScale.readADC_SingleEnded(2) >= 1){
         if (myScale.readADC_SingleEnded(3) >= 1){
          break;
        }
       }
      }
     }
    }
    orders--;
  }
}
