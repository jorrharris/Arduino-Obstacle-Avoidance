#include <UCMotor.h>
#include <UCNEC.h>

// UCTRONICS Smart Robot Car demo (C)2018 uctronics
// Web: http://www.uctronics.com
// UCTRONICS release K0072 robot which is update from K0070.
// This demo support bluetooth control and IR remote control.
// This demo support Graphical programming
// Step1: Upload the Smart_Robot_Car_K0070_Scratch firmware
// Step2: Download the mblock software from http://www.mblock.cc/software/mblock/mblock3/
// Step3: Install The UCTRONICS_Smart_Robot_Car extension
// Step4: Enjoy your Graphical programming.

// Mainly work:
//After power on, it will come in automatic avoidance obstacle mode.
// At the same time, you also can control it by IR controller and bluetooth controller.
//If you use IR controller or bluetooth controller to control it, it will automatic leave
//automatic avoidance obstacle mode.
//By using IR controller, you can control the the robot car run all directions 
//By using bluetooth controller,you can control the robot car run all directons and make it come 
//in automatic avoiddance obstacle mode and come tracking line mode.



#include <UCMotor.h>
#include <Servo.h>
#include "UCNEC.h"
typedef struct MeModule
{
  int device;
  int port;
  int slot;
  int pin;
  int index;
  float values[3];
} MeModule;
union {
  byte byteVal[4];
  float floatVal;
  long longVal;
} val;

union {
  byte byteVal[8];
  double doubleVal;
} valDouble;

union {
  byte byteVal[2];
  short shortVal;
} valShort;


String mVersion = "0d.01.105";
boolean isAvailable = false;
boolean isBluetooth = false;
boolean isDetecte = false;
int len = 52;
char buffer[52];
char bufferBt[52];
byte index = 0;
byte dataLen;
boolean isStart = false;
char serialRead;
// define the device ID
#define ROBOTCAR 54
#define ULTRASONIC_SENSOR 55
#define SERVO 56

#define GET 1
#define RUN 2
#define RESET 4
#define START 5
unsigned char prevc = 0;
double lastTime = 0.0;
uint8_t keyPressed = 0;
uint8_t command_index = 0;

//CHANGED smart mode to track mode
bool isSmartMode  = false;
bool isIrMode  = false;
bool isTrackMode = true;
unsigned int S;
unsigned int Sleft;
unsigned int Sright;

uint32_t irValue = 0;

//CHANGED: new variable
int obstaclesAvoided = 0;

//CHANGED: originally 40
#define TURN_DIST 20

#define TRIG_PIN A2
#define ECHO_PIN A3
#define SERVO_PIN 10

#define leftSensor    A0
#define middleSensor  A1
#define rightSensor   13

//The center Angle of two steering engines.
byte servoXCenterPoint = 88;
//The accuracy of servo
byte servoStep = 4;
//The current Angle of the two steering engines is used for retransmission
byte servoXPoint = 0;

UC_DCMotor leftMotor1(3, MOTOR34_64KHZ);
UC_DCMotor rightMotor1(4, MOTOR34_64KHZ);
UC_DCMotor leftMotor2(1, MOTOR34_64KHZ);
UC_DCMotor rightMotor2(2, MOTOR34_64KHZ);

Servo neckControllerServoMotor;
UCNEC myIR(2);
void setup() {
  pinMode(ECHO_PIN, INPUT); //Set the connection pin output mode Echo pin
  pinMode(TRIG_PIN, OUTPUT);//Set the connection pin output mode trog pin
  neckControllerServoMotor.attach(SERVO_PIN);
  neckControllerServoMotor.write(90);
  delay(2000);
  neckControllerServoMotor.detach();
  delay(100);
  myIR.begin();
  Serial.begin(115200);
  Serial.print("Version: ");
  Serial.println(mVersion);
}
void loop() {
  readSerial();
  if (isAvailable) {
    unsigned char c = serialRead & 0xff;
    if (c == 0x55 && isStart == false) {
      if (prevc == 0xff) {
        index = 1;
        isStart = true; isSmartMode = false;
      }
    } else {
      prevc = c;
      if (isStart) {
        if (index == 2) {
          dataLen = c;
        } else if (index > 2) {
          dataLen--;
        }
        writeBuffer(index, c);
      }
    }
    index++;
    if (index > 51) {
      index = 0;
      isStart = false;
    }
    if (isStart && dataLen == 0 && index > 3) {
      isStart = false;
      parseData();
      index = 0;
    } else if (!isStart) {
      if (serialRead >= 1 && serialRead <= 5) { //0x01->forward  0x02->backward  0x03->left  0x04-> right  0x05->stop
        if (serialRead == 1)  {
          isDetecte = true;
        }
        else  {
          isDetecte = false;
        }
        leftMotor1.run(serialRead); rightMotor1.run(serialRead);
        leftMotor2.run(serialRead); rightMotor2.run(serialRead);
        leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
        leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
        neckControllerServoMotor.detach();
        delay(100);
        isSmartMode = false;
        myIR.begin();
      }
      if (serialRead == 0x06) { //automatic obstacle avoidance
        neckControllerServoMotor.attach(SERVO_PIN);
        neckControllerServoMotor.write(90);
        isSmartMode = true;
      }
      if (serialRead == 0x07) { //tracking line mode enable
        neckControllerServoMotor.attach(SERVO_PIN);
        neckControllerServoMotor.write(90);
        delay(100);
        neckControllerServoMotor.detach();
        delay(100);
        isSmartMode = false;
        myIR.begin();
        isTrackMode = true; isDetecte = false;
      }
      if (serialRead == 0x08) {
        servo_left();
      }
      if (serialRead == 0x09) {
        servo_right();
      }
      if (serialRead == 0x11) {
        Serial.write(0xAA);
        Serial.print("{\"version\":\"K0072\"}");
        Serial.write(0xAB);
      }

      if (serialRead == 0x10) { //connect to bluetooth
        neckControllerServoMotor.attach(SERVO_PIN);
        neckControllerServoMotor.write(90);
        delay(100);
        neckControllerServoMotor.detach();
        delay(100);
        isSmartMode = false;
        myIR.begin();
        leftMotor1.run(0x05); rightMotor1.run(0x05);
        leftMotor2.run(0x05); rightMotor2.run(0x05);
      }
    }
  }
  if (isSmartMode) {
    S = readPing();
    if (S <= TURN_DIST ) {
      isIrMode = false;
      neckControllerServoMotor.attach(SERVO_PIN);
      neckControllerServoMotor.write(90);
      turn();
    } else if (S > TURN_DIST) {
      leftMotor1.run(1); rightMotor1.run(1);//1-> forward
      leftMotor2.run(1); rightMotor2.run(1);
      leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
      leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
      if (isIrMode == false) {
        isIrMode = true;
        neckControllerServoMotor.detach();
        delay(100);
        myIR.begin();
      }
    }
  }
  if (isTrackMode) {
    moveTrack();
  }
}
unsigned char readBuffer(int index) {
  return isBluetooth ? bufferBt[index] : buffer[index];
}
void writeBuffer(int index, unsigned char c) {
  if (isBluetooth) {
    bufferBt[index] = c;
  } else {
    buffer[index] = c;
  }
}
void writeHead() {
  writeSerial(0xff);
  writeSerial(0x55);
}
void writeEnd() {
  Serial.println();
#if defined(__AVR_ATmega32U4__)
  Serial1.println();
#endif
}
void writeSerial(unsigned char c) {
  Serial.write(c);
#if defined(__AVR_ATmega32U4__)
  Serial1.write(c);
#endif
}
void readSerial() {
  isAvailable = false;
  if (Serial.available() > 0) {
    isAvailable = true;
    isBluetooth = false;
    serialRead = Serial.read();
  }
  if (isDetecte) {
    S = readPing();
    if (S <= TURN_DIST ) {
      leftMotor1.run(5); rightMotor1.run(5);//5-> stop
      leftMotor2.run(5); rightMotor2.run(5);//5-> stop
      leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
    }
  }
  while (myIR.available())
  {
    irValue =  myIR.read();
  }
  if (irValue == 0xFF46B9)  //forward
  {

    irValue = 0; isSmartMode = false;
    isDetecte = true;

    leftMotor1.run(1); rightMotor1.run(1);//1-> forward
    leftMotor2.run(1); rightMotor2.run(1);
    leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
    leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
  } else if (irValue == 0xFF15EA) { //backward

    irValue = 0; isSmartMode = false;
    isDetecte = false;

    leftMotor1.run(2); rightMotor1.run(2);//2-> backward
    leftMotor2.run(2); rightMotor2.run(2);
    leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
    leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
  } else if (irValue == 0xFF44BB) { // left

    irValue = 0; isSmartMode = false;
    isDetecte = false;

    leftMotor1.run(3); rightMotor1.run(3);//3-> left
    leftMotor2.run(3); rightMotor2.run(3);
    leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
    leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
  } else if (irValue == 0xFF43BC) { //right

    irValue = 0; isSmartMode = false;
    isDetecte = false;

    leftMotor1.run(4); rightMotor1.run(4);//4-> right
    leftMotor2.run(4); rightMotor2.run(4);
    leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
    leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
  } else if (irValue == 0xFF40BF) {  //stop

    irValue = 0; isSmartMode = false;
    isDetecte = false;

    leftMotor1.run(5); rightMotor1.run(5);//5-> stop
    leftMotor2.run(5); rightMotor2.run(5);
    leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
    leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
  }
}
/*
  ff 55 len idx action device port  slot  data a
  0  1  2   3   4      5      6     7     8
*/
void parseData() {
  isStart = false;
  int idx = readBuffer(3);
  command_index = (uint8_t)idx;
  int action = readBuffer(4);
  int device = readBuffer(5);
  switch (action) {
    case GET: {
        writeHead();
        writeSerial(idx);
        readSensor(device);
        writeEnd();
      }
      break;
    case RUN: {
        runModule(device);
        callOK();
      }
      break;
    case RESET: {
        //reset
        leftMotor1.run(5); rightMotor1.run(5);
        leftMotor2.run(5); rightMotor2.run(5);
        leftMotor1.setSpeed(0); rightMotor1.setSpeed(0);
        leftMotor2.setSpeed(0); rightMotor2.setSpeed(0);
        neckControllerServoMotor.write(90);
        delay(100);
        neckControllerServoMotor.detach();
        delay(100);
        isSmartMode = false;
        myIR.begin();
        callOK();
      }
      break;
    case START: {
        //start
        callOK();
      }
      break;
  }
}
void callOK() {
  writeSerial(0xff);
  writeSerial(0x55);
  writeEnd();
}
void sendByte(char c) {
  writeSerial(1);
  writeSerial(c);
}
void sendString(String s) {
  int l = s.length();
  writeSerial(4);
  writeSerial(l);
  for (int i = 0; i < l; i++) {
    writeSerial(s.charAt(i));
  }
}
void sendFloat(float value) {
  writeSerial(0x2);
  val.floatVal = value;
  writeSerial(val.byteVal[0]);
  writeSerial(val.byteVal[1]);
  writeSerial(val.byteVal[2]);
  writeSerial(val.byteVal[3]);
}
void sendShort(double value) {
  writeSerial(3);
  valShort.shortVal = value;
  writeSerial(valShort.byteVal[0]);
  writeSerial(valShort.byteVal[1]);
}
void sendDouble(double value) {
  writeSerial(2);
  valDouble.doubleVal = value;
  writeSerial(valDouble.byteVal[0]);
  writeSerial(valDouble.byteVal[1]);
  writeSerial(valDouble.byteVal[2]);
  writeSerial(valDouble.byteVal[3]);
}
short readShort(int idx) {
  valShort.byteVal[0] = readBuffer(idx);
  valShort.byteVal[1] = readBuffer(idx + 1);
  return valShort.shortVal;
}
float readFloat(int idx) {
  val.byteVal[0] = readBuffer(idx);
  val.byteVal[1] = readBuffer(idx + 1);
  val.byteVal[2] = readBuffer(idx + 2);
  val.byteVal[3] = readBuffer(idx + 3);
  return val.floatVal;
}
long readLong(int idx) {
  val.byteVal[0] = readBuffer(idx);
  val.byteVal[1] = readBuffer(idx + 1);
  val.byteVal[2] = readBuffer(idx + 2);
  val.byteVal[3] = readBuffer(idx + 3);
  return val.longVal;
}
int readPing()
{
  // establish variables for duration of the ping,
  // and the distance result in inches and centimeters:
  long duration, cm;
  // The PING))) is triggered by a HIGH pulse of 2 or more microseconds.
  // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
  pinMode(TRIG_PIN, OUTPUT);
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(5);
  digitalWrite(TRIG_PIN, LOW);
  pinMode(ECHO_PIN, INPUT);
  duration = pulseIn(ECHO_PIN, HIGH);
  // convert the time into a distance
  cm = duration / 29 / 2;;
  return cm ;
}

void turn() {
  leftMotor1.run(5); rightMotor1.run(5);//5-> stop
  leftMotor2.run(5); rightMotor2.run(5);
  leftMotor1.setSpeed(0); rightMotor1.setSpeed(0);
  leftMotor2.setSpeed(0); rightMotor2.setSpeed(0);
  neckControllerServoMotor.write(150);
  delay(500);
  S = readPing();
  Sleft = S;
  neckControllerServoMotor.write(90);
  delay(500);
  neckControllerServoMotor.write(30);
  delay(500);
  S = readPing();
  Sright = S;
  neckControllerServoMotor.write(90);
  delay(500);
  if (Sleft <= TURN_DIST && Sright <= TURN_DIST) {
    leftMotor1.run(2); rightMotor1.run(2);//2-> backward
    leftMotor2.run(2); rightMotor2.run(2);
    leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
    leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
    delay(500);
    int x = random(1);
    if (x = 0) {
      leftMotor1.run(4); rightMotor1.run(4);//4-> right
      leftMotor2.run(4); rightMotor2.run(4);//4-> right
      leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
      leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
    }
    else {
      leftMotor1.run(3); rightMotor1.run(3);//3-> left
      leftMotor2.run(3); rightMotor2.run(3);
      leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
      leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
    }
    delay(500);
  } else {
    if (Sleft >= Sright) {
      leftMotor1.run(3); rightMotor1.run(3);//3-> left
      leftMotor2.run(3); rightMotor2.run(3);
      leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
      leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
    } else {
      leftMotor1.run(4); rightMotor1.run(4);//4-> right
      leftMotor2.run(4); rightMotor2.run(4);//4-> right
      leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
      leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
    }
    delay(500);
  }
}
void runModule(int device) {
  //0xff 0x55 0x6 0x0 0x1 0xa 0x9 0x0 0x0 0xa
  int port = readBuffer(6);
  int pin = port;
  switch (device) {
    case SERVO: {
        int angle = readBuffer(6);
        if (angle >= 0 && angle <= 180)
        {
          neckControllerServoMotor.attach(SERVO_PIN);
          neckControllerServoMotor.write(angle);
        }
      }
      break;
    case ROBOTCAR: {
        int directionMode = readBuffer(6);
        int motorSpeed = readBuffer(8);
        leftMotor1.run(directionMode);   rightMotor1.run(directionMode);
        leftMotor2.run(directionMode);   rightMotor2.run(directionMode);
        leftMotor1.setSpeed(motorSpeed); rightMotor1.setSpeed(motorSpeed);
        leftMotor2.setSpeed(motorSpeed); rightMotor2.setSpeed(motorSpeed);
      }
      break;
  }
}

void readSensor(int device) {
  /**************************************************
      ff 55 len idx action device port slot data a
       0  1  2   3   4      5      6    7    8
  ***************************************************/
  float value = 0.0;
  int port, slot, pin;
  port = readBuffer(6);
  pin = port;
  switch (device) {
    case ULTRASONIC_SENSOR: {
        value = readPing();
        sendFloat(value);
      }
      break;
  }
}

void avoidObstacles(void) {
  int spd = 90;
  S = readPing();
  if (S <= TURN_DIST) {
    neckControllerServoMotor.attach(SERVO_PIN);
    neckControllerServoMotor.write(90);

    //stop car
    leftMotor1.run(5); rightMotor1.run(5);//5-> stop
    leftMotor2.run(5); rightMotor2.run(5);
    leftMotor1.setSpeed(0); rightMotor1.setSpeed(0);
    leftMotor2.setSpeed(0); rightMotor2.setSpeed(0);

    //turn right
    leftMotor1.run(4); rightMotor1.run(4);//4-> right
    leftMotor2.run(4); rightMotor2.run(4);//4-> right
    leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
    leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);

    delay(250);

    //stop car
    leftMotor1.run(5); rightMotor1.run(5);//5-> stop
    leftMotor2.run(5); rightMotor2.run(5);
    leftMotor1.setSpeed(0); rightMotor1.setSpeed(0);
    leftMotor2.setSpeed(0); rightMotor2.setSpeed(0);

    //move forward a bit
    leftMotor1.run(1); rightMotor1.run(1);//1-> forward
    leftMotor2.run(1); rightMotor2.run(1);
    leftMotor1.setSpeed(spd); rightMotor1.setSpeed(spd);
    leftMotor2.setSpeed(spd); rightMotor2.setSpeed(spd);

    delay(1200);

    //stop car
    leftMotor1.run(5); rightMotor1.run(5);//5-> stop
    leftMotor2.run(5); rightMotor2.run(5);
    leftMotor1.setSpeed(0); rightMotor1.setSpeed(0);
    leftMotor2.setSpeed(0); rightMotor2.setSpeed(0);
    delay(50);

    //turn left
    leftMotor1.run(3); rightMotor1.run(3);//3-> left
    leftMotor2.run(3); rightMotor2.run(3);
    leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
    leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
    delay(460);
    
    //stop car
    leftMotor1.run(5); rightMotor1.run(5);//5-> stop
    leftMotor2.run(5); rightMotor2.run(5);
    leftMotor1.setSpeed(0); rightMotor1.setSpeed(0);
    leftMotor2.setSpeed(0); rightMotor2.setSpeed(0);
    delay(50);

    //move forward a bit
    leftMotor1.run(1); rightMotor1.run(1);//1-> forward
    leftMotor2.run(1); rightMotor2.run(1);
    leftMotor1.setSpeed(spd); rightMotor1.setSpeed(spd);
    leftMotor2.setSpeed(spd); rightMotor2.setSpeed(spd);

    delay(500);

    //stop car
    leftMotor1.run(5); rightMotor1.run(5);//5-> stop
    leftMotor2.run(5); rightMotor2.run(5);
    leftMotor1.setSpeed(0); rightMotor1.setSpeed(0);
    leftMotor2.setSpeed(0); rightMotor2.setSpeed(0);
    delay(50);

    /*
    //turn right a tiny bit, to line up beter
    leftMotor1.run(4); rightMotor1.run(4);//4-> right
    leftMotor2.run(4); rightMotor2.run(4);//4-> right
    leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
    leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);

    delay(70);

    
    //stop car
    leftMotor1.run(5); rightMotor1.run(5);//5-> stop
    leftMotor2.run(5); rightMotor2.run(5);
    leftMotor1.setSpeed(0); rightMotor1.setSpeed(0);
    leftMotor2.setSpeed(0); rightMotor2.setSpeed(0);
    delay(50);

    */

    //move forward a bit
    /*leftMotor1.run(1); rightMotor1.run(1);//1-> forward
    leftMotor2.run(1); rightMotor2.run(1);
    leftMotor1.setSpeed(spd); rightMotor1.setSpeed(spd);
    leftMotor2.setSpeed(spd); rightMotor2.setSpeed(spd);
    delay(1200);

    //stop car
    leftMotor1.run(5); rightMotor1.run(5);//5-> stop
    leftMotor2.run(5); rightMotor2.run(5);
    leftMotor1.setSpeed(0); rightMotor1.setSpeed(0);
    leftMotor2.setSpeed(0); rightMotor2.setSpeed(0);
    delay(50);

    //turn left just a bit more
    leftMotor1.run(3); rightMotor1.run(3);//3-> left
    leftMotor2.run(3); rightMotor2.run(3);
    leftMotor1.setSpeed(200); rightMotor1.setSpeed(200);
    leftMotor2.setSpeed(200); rightMotor2.setSpeed(200);
    delay(250);

    */
    //CHANGED: uhhh maybe?
    //return;

    //go forward until we see the line
    int temp = 0, num1 = 0, num2 = 0, num3 = 0;
    while (1) {
      num1 = digitalRead(leftSensor);
      num2 = digitalRead(middleSensor);
      num3 = digitalRead(rightSensor);
      if ((num2 == 1) && (num1 == 1) && (num3 == 1)) {
        //move forward a bit
        leftMotor1.run(1); rightMotor1.run(1);//1-> forward
        leftMotor2.run(1); rightMotor2.run(1);
        leftMotor1.setSpeed(spd / 1.6); rightMotor1.setSpeed(spd / 1.6);
        leftMotor2.setSpeed(spd / 1.6); rightMotor2.setSpeed(spd / 1.6);
        //delay(5);
      } else {
        //CHANGED
        obstaclesAvoided = obstaclesAvoided + 1;
        //return;
        //stop car
        leftMotor1.run(5); rightMotor1.run(5);//5-> stop
        leftMotor2.run(5); rightMotor2.run(5);
        leftMotor1.setSpeed(0); rightMotor1.setSpeed(0);
        leftMotor2.setSpeed(0); rightMotor2.setSpeed(0);
        delay(100);

        while ((num2 == 0) && (num1 == 0) && (num3 == 0)) {
          //turn right
          leftMotor1.run(4); rightMotor1.run(4);//4-> right
          leftMotor2.run(4); rightMotor2.run(4);//4-> right
          leftMotor1.setSpeed(60); rightMotor1.setSpeed(60);
          leftMotor2.setSpeed(60); rightMotor2.setSpeed(60);
        }
        
        return;
      }
      
    }
  }
}

void moveTrack(void)
{
  int temp = 0, num1 = 0, num2 = 0, num3 = 0;
  int spd = 90; //CHANGED from 180, 90 seems to work well
  int turnspd = 90;
  while (1) {
    if ((Serial.available() > 0))
    {
      isTrackMode = false;
      break;
    }

    avoidObstacles();
    
    num1 = digitalRead(leftSensor);
    num2 = digitalRead(middleSensor);
    num3 = digitalRead(rightSensor);
    if ((num2 == 0) && (num1 == 0) && (num3 == 0)) {
      leftMotor1.run(5); rightMotor1.run(5);//5-> stop
      leftMotor2.run(5); rightMotor2.run(5);
      leftMotor1.setSpeed(0); rightMotor1.setSpeed(0);
      leftMotor2.setSpeed(0); rightMotor2.setSpeed(0);
      continue;
    } else if ((num2 == 1) && (num1 == 1) && (num3 == 1) && (obstaclesAvoided >= 2)) { //CHANGED: this is new!!
      leftMotor1.run(5); rightMotor1.run(5);//5-> stop
      leftMotor2.run(5); rightMotor2.run(5);
      leftMotor1.setSpeed(0); rightMotor1.setSpeed(0);
      leftMotor2.setSpeed(0); rightMotor2.setSpeed(0);
      while (1) { //loop forever to stop the car lol
        delay(1000);
      }
      continue; 
    } else if ( (num1 == 0) && num3 == 1) { //go to right
      leftMotor1.run(3); rightMotor1.run(3);//3-> left
      leftMotor2.run(3); rightMotor2.run(3);
      leftMotor1.setSpeed(0); rightMotor1.setSpeed(turnspd); //speed for left motors changed from 0
      leftMotor2.setSpeed(0); rightMotor2.setSpeed(turnspd);
      while (1) {
        if ((Serial.available() > 0))
        {
          isTrackMode = false;
          break;
        }

        avoidObstacles();
        
        num2 = digitalRead(middleSensor);
        if (num2) {
          leftMotor1.run(3); rightMotor1.run(3);//3-> left
          leftMotor2.run(3); rightMotor2.run(3);
          leftMotor1.setSpeed(0); rightMotor1.setSpeed(turnspd);
          leftMotor2.setSpeed(0); rightMotor2.setSpeed(turnspd);
          continue;
        }
        else
          break;
      }
    } else if ((num3 == 0) && (num1 == 1)) { // go to left
      leftMotor1.run(4); rightMotor1.run(4);//4-> right
      leftMotor2.run(4); rightMotor2.run(4);
      leftMotor1.setSpeed(turnspd); rightMotor1.setSpeed(0);
      leftMotor2.setSpeed(turnspd); rightMotor2.setSpeed(0);
      while (1) {
        if ((Serial.available() > 0))
        {
          isTrackMode = false;
          break;
        }

        avoidObstacles();
        
        num2 = digitalRead(middleSensor);
        if (num2) {
          leftMotor1.run(4); rightMotor1.run(4);//4-> right
          leftMotor2.run(4); rightMotor2.run(4);
          leftMotor1.setSpeed(turnspd); rightMotor1.setSpeed(0);
          leftMotor2.setSpeed(turnspd); rightMotor2.setSpeed(0);
          continue;
        }
        else
          break;
      }
    }
    else
    {
      leftMotor1.run(1); rightMotor1.run(1);//1-> forward
      leftMotor2.run(1); rightMotor2.run(1);
      leftMotor1.setSpeed(spd); rightMotor1.setSpeed(spd);
      leftMotor2.setSpeed(spd); rightMotor2.setSpeed(spd);
    }
  }
}

void servo_right(void)
{
  neckControllerServoMotor.attach(10); delay(10);
  int servotemp = neckControllerServoMotor.read();
  servotemp -= servoStep;
  servo_Horizontal(servotemp);
  delay(50);
  neckControllerServoMotor.detach();
  delay(50);
  myIR.begin();

}
void servo_left(void)
{
  neckControllerServoMotor.attach(10); delay(10);
  int servotemp = neckControllerServoMotor.read();
  servotemp += servoStep;
  servo_Horizontal(servotemp);
  delay(50);
  neckControllerServoMotor.detach();
  delay(50);
  myIR.begin();
}

void servo_Horizontal(int corner)
{
  int i = 0;
  byte cornerX = neckControllerServoMotor.read();
  if (cornerX > corner) {
    for (i = cornerX; i > corner; i = i - servoStep) {
      \
      neckControllerServoMotor.write(i);
      servoXPoint = i;
      delay(50);
    }
  }
  else {
    for (i = cornerX; i < corner; i = i + servoStep) {
      neckControllerServoMotor.write(i);
      servoXPoint = i;
      delay(50);
    }
  }
  neckControllerServoMotor.write(corner);
  servoXPoint = corner;
}
