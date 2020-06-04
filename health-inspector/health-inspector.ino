#define USE_ARDUINO_INTERRUPTS true    // Set-up low-level interrupts for most acurate BPM math.

#include <I2Cdev.h>
#include <MPU6050.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <PulseSensorPlayground.h>
#include<SoftwareSerial.h>

#define PULSE_PIN 2
#define PULSE_THRESHOLD 700

#define TEMP_PIN 0

// pins used for LCD
#define LCD_RS 3
#define LCD_EN 4
#define LCD_D4 5
#define LCD_D5 6
#define LCD_D6 7
#define LCD_D7 8

#define FALL_TIME 25               // time to detect a fall after a high acceleration in milliseconds
#define FALL_HIGH_THRESHOLD 25000  // high acceleration threshold to detect a fall
#define FALL_LOW_THRESHOLD 3000    // low acceleration threshold to detect a fall

// offsets found by holding the accelerometer still
#define ACCEL_X_OFFSET -16000
#define ACCEL_Y_OFFSET 150
#define ACCEL_Z_OFFSET 8000

// offset found by comparing LM35 with a working thermometer
#define TEMP_OFFSET -6.0

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
PulseSensorPlayground pulseSensor;
SoftwareSerial client(12, 11); //RX, TX

MPU6050 accelgyro;

// acceleration values from accelerometer
int16_t ax, ay, az;
bool negX = false;

//int count = 0;

// uncomment "OUTPUT_READABLE_ACCELGYRO" if you want to see a tab-separated
// list of the accel X/Y/Z and then gyro X/Y/Z values in decimal. Easy to read,
// not so easy to parse, and slow(er) over UART.
//#define OUTPUT_READABLE_ACCELGYRO

float tempC;
float tempF;
float avgReading;
float avgTemp;
int tempRawRead;
int tempCount;
int myBPM;

unsigned long currTime;
unsigned long prevTime;

unsigned long fallTimer = 0;

int i = 0, k = 0;
String readString;
int x = 0;
////////////////////////////////////////////////////
//////////PARAMETERS HERE///////////////////////////
////////////////////////////////////////////////////
String temp = "0";
String heartrate = "0";
int fall = 0;

////////////////////////////////////////////////////
////////////////////////////////////////////////////
boolean No_IP = false;
String IP = "";
char temp1 = '0';

String name = "<p>Circuit Digest</p>"; //22
String dat = "<p>Data Received Successfully.....</p>";   //21

void fallDetection();
void getTempPulse();

void check4IP(int t1)
{
  int t2 = millis();
  while (t2 + t1 > millis())
  {
    while (client.available() > 0)
    {
      if (client.find("WIFI GOT IP"))
      {
        No_IP = true;
      }
    }
  }
}

void get_ip()
{
  IP = "";
  char ch = 0;
  while (1)
  {
    client.println("AT+CIFSR");
    while (client.available() > 0)
    {
      if (client.find("STAIP,"))
      {
        delay(1000);
        Serial.print("IP Address:");
        while (client.available() > 0)
        {
          ch = client.read();
          if (ch == '+')
            break;
          IP += ch;
        }
      }
      if (ch == '+')
        break;
    }
    if (ch == '+')
      break;
    delay(1000);
  }
  Serial.print(IP);
  Serial.print("Port:");
  Serial.println(80);
}

void connect_wifi(String cmd, int t)
{
  int temp = 0, i = 0;
  while (1)
  {
    Serial.println(cmd);
    client.println(cmd);
    while (client.available())
    {
      if (client.find("OK"))
        i = 8;
    }
    delay(t);
    if (i > 5)
      break;
    i++;
  }
  if (i == 8)
    Serial.println("OK");
  else
    Serial.println("Error");
}

void wifi_init()
{
  connect_wifi("AT", 100);
  connect_wifi("AT+CWMODE=3", 100);
  connect_wifi("AT+CWQAP", 100);
  connect_wifi("AT+RST", 5000);
  check4IP(5000);
  if (!No_IP)
  {
    Serial.println("Connecting Wifi....");
    connect_wifi("AT+CWJAP=\"Frontier_2.4\",\"0935985578\"", 7000);        //provide your WiFi username and password here
    // connect_wifi("AT+CWJAP=\"vpn address\",\"wireless network\"",7000);
  }
  else
  {
  }
  Serial.println("Wifi Connected");
  get_ip();
  connect_wifi("AT+CIPMUX=1", 100);
  connect_wifi("AT+CIPSERVER=1,80", 100);
}

void sendwebdata(String webPage)
{
  int ii = 0;
  while (1)
  {
    unsigned int l = webPage.length();
    Serial.print("AT+CIPSEND=0,");
    client.print("AT+CIPSEND=0,");
    Serial.println(l + 2);
    client.println(l + 2);
    delay(100);
    Serial.println(webPage);
    client.println(webPage);
    while (client.available())
    {
      //Serial.print(Serial.read());
      if (client.find("OK"))
      {
        ii = 11;
        break;
      }
    }
    if (ii == 11)
      break;
    delay(100);
  }
}

void setup() {
  Wire.begin();
  Serial.begin(9600);
  client.begin(115200);
  wifi_init();
  Serial.println("Wifi Ready..");

  // initialize device
  //Serial.println("Initializing I2C devices...");
  accelgyro.initialize();

  // verify connection
  //Serial.println("Testing device connections...");
  //Serial.println(accelgyro.testConnection() ? "MPU6050 connection successful" : "MPU6050 connection failed");
  pulseSensor.analogInput(PULSE_PIN);
  pulseSensor.setThreshold(PULSE_THRESHOLD);
  pulseSensor.begin();

  lcd.begin(16, 2);

  prevTime = 0;
}

void loop() {
  currTime = millis();

  fallDetection();

  if (currTime - prevTime > 20) {
    getTempPulse();
  }

  // update LCD about every 1 second
  if (currTime - prevTime > 1000) {
    prevTime = currTime;
    tempC = (avgTemp * 500.0) / 1023.0 + TEMP_OFFSET;
    temp = tempC;
    //temp = tempC;
    tempF = tempC * 1.8 + 32;
    avgReading = 0;
    tempCount = 0;

    // temperatures are on first row, BPM is on second
    lcd.setCursor(0, 0);
    lcd.print(tempC, 1);
    lcd.print((char)223); // degree symbol
    lcd.print("C ");
    lcd.print(tempF, 1);
    lcd.print((char)223); // degree symbol
    lcd.print("F   ");

    if (pulseSensor.sawStartOfBeat()) {
      //heartrate = myBPM;
      lcd.setCursor(0, 1);
      lcd.print(myBPM);
      lcd.print(" BPM  ");
    }
  }
  
  heartrate = myBPM;
  Serial.println("Please Refresh your Page");
  delay(1000);
  while (client.available()) {
    if (client.find("0,CONNECT"))
    {
      Serial.println("Start Printing");
      Send();
      Serial.println("Done Printing");
      delay(1000);
    }
  }
}

void Send()
{
  String webpage = "";
  webpage = "<h1>Temperature is " + temp + " C</h1><body bgcolor=f0f0f0>";
    sendwebdata(webpage);
    webpage = "<h1>Heartrate is " + heartrate + " BPM</h1><body bgcolor=f0f0f0>";
    sendwebdata(webpage);
  if (fall == 1) { //There is a fall detected
    webpage = "<h1>Fall DETECTED</h1><body bgcolor=f0f0f0>";
    sendwebdata(webpage);
  } else {
    webpage = "<h1>Fall not detected</h1><body bgcolor=f0f0f0>";
    sendwebdata(webpage);
  }
  delay(1000);
  client.println("AT+CIPCLOSE=0");
}

/*
   detects if a fall happened
   prints "Fall Detected" to first line of LCD if a fall is detected
*/
void fallDetection() {
  // read raw accel/gyro measurements from device
  //accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  negX = false;
  accelgyro.getAcceleration(&ax, &ay, &az);
  ax = ax + ACCEL_X_OFFSET;
  ay = ay + ACCEL_Y_OFFSET;
  az = az + ACCEL_Z_OFFSET;
  unsigned long accel = sqrt((long)ax * ax + (long)ay * ay + (long)az * az);

  if (accel > FALL_HIGH_THRESHOLD) {
    fallTimer = millis();
    //Serial.print("past high ");
    //Serial.println(count);
    //count++;

    // check for low acceleration after breaking high threshold for FALL_TIME
    while (millis() - FALL_TIME < fallTimer) {
      // debugging prints
#ifdef OUTPUT_READABLE_ACCELGYRO
      //Serial.print("after high:\t");
      Serial.print(ax); Serial.print("\t");
      Serial.print(ay); Serial.print("\t");
      Serial.print(az); Serial.print("\t");
      Serial.println(accel);
#endif
      accelgyro.getAcceleration(&ax, &ay, &az);
      accel = sqrt((long)ax * ax + (long)ay * ay + (long)az * az);
      if (!negX && ax < 0) {
        negX = true;
      }
      // if low acceleration is passed, a fall is detected
      if ((accel != 0) && (accel < FALL_LOW_THRESHOLD) && negX) {
        lcd.setCursor(0, 0);
        lcd.print("Fall detected");
        delay(7000);
        break;
      }
    }
  }
}

/* measures temperature and pulse rate
   temperature is measured from raw readings - it will be converted to C and F later
   temperature is measured using an average of 10 readings
   pulse is measured in BPM
*/
void getTempPulse() {
#ifdef DEBUG_PRINT
  Serial.print(myBPM);
  Serial.println(" BPM");
  Serial.print(tempC, 1);
  Serial.print(" C\t");
  Serial.print(tempF, 1);
  Serial.println(" F");
#endif

  myBPM = pulseSensor.getBeatsPerMinute();
  //Serial.println(analogRead(PULSE_PIN));;
  //Serial.println(myBPM);

  // disable interrupts when reading temp to prevent bad reads
  noInterrupts();
  if (tempCount < 10) {
    tempRawRead = analogRead(TEMP_PIN);
    //Serial.println(tempRawRead);
    avgReading += tempRawRead;
    tempCount++;
  }
  interrupts();

  avgTemp = avgReading / tempCount;
}
