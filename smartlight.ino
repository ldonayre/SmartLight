//---------------------------------------------------------------------
// Lou note: esp8266-12F use "Wemos R2 or mini board" board type.
//           Press "flash button" on device after blue light blinks.
//           Then "reset" after flash.
//           Connects fine to Blynk and Phone app sees it.
//---------------------------------------------------------------------

#include <Arduino.h>
#include <Stream.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

// strtok
#include <string.h>
#define TOKEN_DELIMITER "\""

//AWS
#include "sha256.h"
#include "Utils.h"

//WEBSockets
#include <Hash.h>
#include <WebSocketsClient.h>

//MQTT PAHO
#include <SPI.h>
#include <IPStack.h>
#include <Countdown.h>
#include <MQTTClient.h>

// Lou: SMD5050 control code derived from FirstLight example.
#include "FastLED.h"
#define NUM_LEDS 24      // Number of LEDs in the strip
#define DATA_PIN 5       // Data pin that led data will be written out over - GPIO 5 on esp8266-12F
#define NUM_VIRT_BUTTONS 3  // number of Blynk Widget buttons (in order from zero).

CRGBArray<NUM_LEDS> leds;

int lightStatus = 0;  // corresponds to the virtualpin selected.  0 off/none.
#define ON_OFF_VPIN 7  // Blynk Virtual Pin for the On Off Button Widget 
#define COOL_HUE_LOW  140
#define COOL_HUE_HIGH 240
#define WARM_HUE_LOW  50
#define WARM_HUE_HIGH 95

uint8_t v_red, v_green, v_blue;  // pulled from user prefs saved on EEPROM
int brightness = 32;  // pulled from flash per user defined preference - default set here. 
int color_play_variance = 0; // default zero no variance. 
unsigned long color_play_time = 0; // default zero - unlimted play time. 
unsigned long prevTime; // used in conjunction with color_play_time.  Set by horiz. slider.
uint8_t hue = 0; 
uint8_t hue_direction = 1;  // colow sway increaseing or decreasing 

// BLYNK code
// #define BLYNK_PRINT Serial   // TODO : Comment this out to disable prints and save space 

// PIR Sensor - mini PIR - HC-SR505. Note: during the first minute of booting the HC-SR505 
// will go pin high 0-3 times before settling down.  During this first minute if must be ignored.
int PIR_SENSOR = 14;     //the digital pin connected to the PIR sensor's output, interrupt driven.
int motion_detected = 0;  // 0 = no motion/reset, 1 = motion detected. 
int pir_enabled = 0;      // boolean default disable (0). to enable/disable the PIR sensor.

#include <EEPROM.h>
#define EEPROM_SIZE 32  // up to 1024 on esp8266-12 - Note: EEPROM.length() returns 0 so hardcoding.
/* EEPROM Layout
   addr : string 
    0-2 : brightness "000"-"255"
    3-3 : pir_enabled "0"-"1"
    4-6 : red level "000"-"255"
    7-9 : green level "000"-"255"
  10-12 : blue level "000"-"255"
  13-14 : color_play_variance "00-10"
  15-16 : color_play_time "00-60"
*/

#include <BlynkSimpleEsp8266.h>
char auth[] = "......................."; // Blynk Auth token 


//AWS MQTT Websocket
#include "Client.h"
#include "AWSWebSocketClient.h"
#include "CircularByteBuffer.h"

// TODO: delete this before publishing publicly
char wifi_ssid[]       = "......";
char wifi_password[]   = "......";
char aws_endpoint[]    = ".............iot.us-east-1.amazonaws.com";
char aws_key[]         = "AKIA...............";
char aws_secret[]      = "....................................";
char aws_region[]      = "us-east-1";

const char* aws_topic  = "SmartLight";

int port = 443;

//MQTT config
const int maxMQTTpackageSize = 512;
const int maxMQTTMessageHandlers = 1;

ESP8266WiFiMulti WiFiMulti;

AWSWebSocketClient awsWSclient(1000);

IPStack ipstack(awsWSclient);
MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers> *client = NULL;


//callback to handle mqtt messages
void messageArrived(MQTT::MessageData& md)
{
  char *k, *v;
  MQTT::Message &message = md.message;

  char* msg = new char[message.payloadlen+1]();
  memcpy (msg,message.payload,message.payloadlen);
 
  (void *) strtok(msg, TOKEN_DELIMITER);          
  k = strtok((char *)NULL, TOKEN_DELIMITER);
  (void *) strtok((char *)NULL, TOKEN_DELIMITER);  
  v = strtok((char *)NULL, TOKEN_DELIMITER);

  if (strncmp(v,"on", 2) == 0) {
     for(int i = 0; i < NUM_LEDS; i++)
        leds[i] = CRGB::White;
     FastLED.show(brightness); // TODO consider setting full white/bright here.
  }
  else if (strncmp(v,"off", 3) == 0) {
      allOtherButtonWidgetsOff (ON_OFF_VPIN); // simulate "Off" button press.
      FastLED.show(0);  // needed
      lightStatus = 0; // NoOp
  }
  else if (strncmp(v,"sunrise", 7) == 0) {
         sceneSunrise();
  }
  else if (strncmp(v,"sunset", 6) == 0) {
         sceneSunset();
  }
  else if (strncmp(k,"brightness", 10) == 0) {
     int val = map(atoi(v), 0, 100, 0, 255);   // percentage is 0-100, LED vals are 1 to 255

     switch (val) {
        case -1 :  // dimmer code
           if (brightness > 25) 
              brightness-=25;
           else
              brightness=1;
           break;
        case 999:  // brighter code
           if (brightness < 75) 
              brightness+=25;
           else
              brightness=100;
           break;
        default :  // else explicit brightness
           brightness = val;
           break;
     }
     Blynk.virtualWrite(V11, brightness);  // brightness rt. verticle slider set app widget
     FastLED.setBrightness( brightness );
     FastLED.show();
  }

  delete msg;
}


//connects to websocket layer and mqtt layer
bool connect () {

   if (client == NULL) {
      client = new MQTT::Client<IPStack, Countdown, maxMQTTpackageSize, maxMQTTMessageHandlers>(ipstack);
   } 
 
   int rc = ipstack.connect(aws_endpoint, port);
   if (rc != 1)
   {
      Serial.println("Connection failure");
      return false;
   } 
   else {
      Serial.println("Connected");
   }

   Serial.println("Connecting to AWS MQTT");
   MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
   data.MQTTVersion = 3;
   
   char clientID[17];
   String s = WiFi.macAddress();

   for (int i=0; i<17; i++) 
      clientID[i] = (char) s[i];
   
   data.clientID.cstring = clientID;
   rc = client->connect(data);

   if (rc != 0)
   {
      Serial.print("MQTT connect error");
      Serial.println(rc);
      return false;
   }
   Serial.println("MQTT connected");
   return true;
}


//subscribe to a mqtt topic
void subscribe () {
   int rc = client->subscribe(aws_topic, MQTT::QOS0, messageArrived);
   if (rc != 0) {
      Serial.print("Error in MQTT subscription :");
      Serial.println(rc);
      return;
   }
   Serial.println("MQTT subscribed");  
}


// interrupt driven call to detect motion.
void pirInterrupt() {  // CHANGE so triggers for RISING or FALLING
   // toggle
   if (motion_detected)  {
      motion_detected = 0;
      Serial.println ("Motion Detected.");
      allOtherButtonWidgetsOff (V3);  // simulate a "Custom" button press, but don't light custom button.  Note: the "On" button will be lit.
      lightStatus = 3;   // simulate a "Custom" button press.
   }
   else  { 
      motion_detected = 1;
      Serial.println ("Motion reset.");    
      allOtherButtonWidgetsOff (ON_OFF_VPIN);  // simulate on "off" button press.
      FastLED.show(0);  // needed
      lightStatus = 0; // NoOp 
   }
}


void writeEEPROM(int addr, int ln, char *value) {
   int i, j=0, len = (ln < EEPROM_SIZE ? ln : EEPROM_SIZE);
 
   for (i=addr; i < len+addr; i++) {
     EEPROM.write(i, value[j++]);
   }
   EEPROM.commit();
}


void readEEPROM(int addr, int ln, char *value) {
   int i, j=0, len = (ln < EEPROM_SIZE ? ln : EEPROM_SIZE);

   for (i=addr; i < len+addr; i++) {
      value[j++] = EEPROM.read(i);
   }
   value[j] = '\0';  // add null terminator
}


void onOffButton(int val) {
   uint8_t onOffButton = ON_OFF_VPIN;
   if (val) {
      Blynk.virtualWrite(onOffButton, 1);
   }
   else {
      Blynk.virtualWrite(onOffButton, 0);   
      FastLED.show(0);
   }
}

// this handles cases where another light button is pressed, so turn off other light widgets.
void allOtherButtonWidgetsOff (int calling_button) {
   // Virtual pins are uint8_t
   for (uint8_t i = 1; i<1+NUM_VIRT_BUTTONS; i++) {
      if (i != calling_button)
         Blynk.virtualWrite(i, 0);
   }
   
   if (calling_button != ON_OFF_VPIN) {  // i.e. another light button was pressed.
      onOffButton (1);
      prevTime = millis();  // set in case timer selected 
   } 
}


void sceneSunrise() {  // V2 - blue : (140,2 - 240,2)
   if (hue < COOL_HUE_LOW) {
      hue = COOL_HUE_LOW;
      hue_direction = 1;
   }
   else if (hue > COOL_HUE_HIGH) {
      hue = COOL_HUE_HIGH;
      hue_direction = 0;
   }
   
   if (hue_direction)
      hue+=color_play_variance;
   else
      hue-=color_play_variance;

   fill_rainbow( leds, NUM_LEDS, hue, color_play_variance);  // 85, 2 nice hot
   LEDS.show();
}


void sceneSunset() {   // V3 - red : (50,2 - 95,2)
   if (hue < WARM_HUE_LOW) {
      hue = WARM_HUE_LOW;
      hue_direction = 1;
   }
   else if (hue > WARM_HUE_HIGH) {
      hue = WARM_HUE_HIGH;
      hue_direction = 0;
   }

   if (hue_direction)
      hue+=color_play_variance;
   else
      hue-=color_play_variance;
        
   fill_rainbow( leds, NUM_LEDS, hue, color_play_variance);
   LEDS.show();
}


void sceneCustom() {   // V3 
CRGB rgb;
CHSV hsv;  //three one-byte data members - .hue, .sat. ,val

   rgb.r = v_green;  // not a typo.  I have to switch G and R or else not correct.
   rgb.g = v_red;    // not a typo.  see above.
   rgb.b = v_blue;
   hsv = rgb2hsv_approximate( rgb );


   if (hue < WARM_HUE_LOW) {
      hue = WARM_HUE_LOW;
      hue_direction = 1;
   }
   else if (hue > WARM_HUE_HIGH) {
      hue = WARM_HUE_HIGH;
      hue_direction = 0;
   }

   if (hue_direction)
      hue+=color_play_variance;
   else
      hue-=color_play_variance;

   for (int i=0; i<NUM_LEDS; i++) 
      fill_rainbow( leds, NUM_LEDS, hue, color_play_variance);
   
   LEDS.show();
}


// === BLYNK ===
BLYNK_WRITE(V1) //Button Widget is writing to pin V1 - sunset
{
   if (param.asInt()) {
      lightStatus = 1; // set codes for each scene based on vPin.
      allOtherButtonWidgetsOff (1);
   }
   else {
      onOffButton (0);
      lightStatus = 0; // NoOp 
   }
}


BLYNK_WRITE(V2) //Button Widget is writing to pin V2 - sunrise.
{
   if (param.asInt()) {
      lightStatus = 2; // set codes for each scene based on vPin.
      allOtherButtonWidgetsOff (2);
   }
   else {
      onOffButton (0);
      lightStatus = 0; // NoOp 
   }
}

// scene - Custom - user defined
BLYNK_WRITE(V3) 
{
   if (param.asInt()) {
      lightStatus = 3; // set codes for each scene based on vPin.
      allOtherButtonWidgetsOff (3);
   } else { 
      onOffButton (0);
      lightStatus = 0; // NoOp 
   }
}


BLYNK_WRITE(V5) // write/save settings  
{
   if (param.asInt()) {
      char* temp = new char[4]();

      sprintf(temp, "%03d", brightness);
      writeEEPROM(0, 3, temp);

      sprintf(temp, "%d", pir_enabled);
      writeEEPROM(3, 1, temp);  // start at addr 3, write 1 char

      sprintf(temp, "%d", v_green);  // Not a type.  Must switch Red and Green
      writeEEPROM(4, 3, temp);  // start at addr 4, write 3 chars

      sprintf(temp, "%d", v_red);  // Not a type.  Must switch Red and Green
      writeEEPROM(7, 3, temp);  // start at addr 7, write 3 chars

      sprintf(temp, "%d", v_blue);
      writeEEPROM(10, 3, temp);  // start at addr 10, write 3 chars

      sprintf(temp, "%d", color_play_variance);
      writeEEPROM(13, 2, temp);  // start at addr 10, write 2 chars

      sprintf(temp, "%d", color_play_time);
      writeEEPROM(15, 2, temp);  // start at addr 10, write 2 chars

      delete temp;
   }
}

// Enable/Disable PIR
BLYNK_WRITE(V6)
{
   if (param.asInt()) {
      pir_enabled = 1;
      attachInterrupt(PIR_SENSOR, pirInterrupt, CHANGE);  // attach interrupt to PIR Sensor for either rising or falling.
      interrupts();  // activate Interrupts
      Serial.println ("PIR Enabled");
   }
   else {
      pir_enabled = 0;
      noInterrupts();  // deactivate Interrupts
      detachInterrupt(PIR_SENSOR);
      Serial.println ("PIR Disabled");
      //ledControl(0);  // turn off the LED
   }
}


// Alexa call : "On" / "Off" - 0/1 to act as the "Light on/off" control.  
BLYNK_WRITE(V7) // On/Off Button Widget - ignores timer
{
   if (param.asInt())  {
      // FastLED.clear();

      for(int i = 0; i < NUM_LEDS; i++)  
         leds[i] = CRGB::White; 

      FastLED.show(brightness);
   }
   else {
      allOtherButtonWidgetsOff (ON_OFF_VPIN);
      FastLED.show(0);  // needed
      lightStatus = 0; // NoOp 
   }
}

BLYNK_WRITE(V10) // Color picker Widget "zeRGBa" (horse of a different color)
{
   allOtherButtonWidgetsOff (10);

   // Note GRB (0,1,2)  This works and is correct.
   v_green = param[0].asInt();
   v_red   = param[1].asInt();
   v_blue  = param[2].asInt();

   for (int i = 0; i < NUM_LEDS; i++) {
      // second val is "Whiteness"
      leds[i].r = v_red;
      leds[i].g = v_green;
      leds[i].b = v_blue;
   }
   allOtherButtonWidgetsOff (10); 
   lightStatus = 10; // this Vpin
   FastLED.show();
}

// We limit the low end to 1. - Alexa call : "brighter"/"dimmer"
BLYNK_WRITE(V11) // Right Verticle slider - brightness control
{
   brightness = param.asInt(); // assigning incoming value from pin V1 to a variable
   FastLED.setBrightness( brightness );
   FastLED.show();
}

BLYNK_WRITE(V12) // Color Variation Left Verticle Slider Widget
{
   color_play_variance = param.asInt(); 
}

BLYNK_WRITE(V13) // Time play Horizontal Slider Widget
{
   color_play_time = 60000 * param.asInt();  // minutes
   prevTime = millis();
}

// This function will run every time Blynk connection is established
BLYNK_CONNECTED() {
   // Request Blynk server to re-send latest values for all pins FROM app widgets.
   //Blynk.syncAll();

   Blynk.virtualWrite(V6, pir_enabled);  // PIR mode buton
   Blynk.virtualWrite(V11, brightness);  // brightness rt. verticle slider 
   Blynk.virtualWrite(V12, color_play_variance);  // color variation slider to zero.  
   Blynk.virtualWrite(V13, color_play_time);  // reset horizontal time slider to zero.

   // Reset app/widget buttons.
   allOtherButtonWidgetsOff (ON_OFF_VPIN);  // simulate an "off" button press to turn off both the "stobe" and "on/off" button widgets.
   onOffButton (0);  // special case where the on/off button is on an a connection to Blynk was lost.
}


void setup() {
   // Debug console
   Serial.begin (115200);
   delay (2000); // sanity check delay 

   EEPROM.begin(EEPROM_SIZE);

   // read in saved defaults
   char* temp = new char[4]();  // +1 for terminating null

   readEEPROM(0, 3, temp);
   brightness = atoi(temp);    

   readEEPROM(3, 1, temp);
   pir_enabled = atoi(temp);    

   readEEPROM(4, 3, temp);
   v_red = atoi(temp);   

   readEEPROM(7, 3, temp);
   v_green = atoi(temp);  

   readEEPROM(10, 3, temp);
   v_blue = atoi(temp);  

   readEEPROM(13, 2, temp);
   color_play_variance = atoi(temp); 

   readEEPROM(15, 2, temp);
   color_play_time = atoi(temp);    

   delete temp;

   
   //pinMode(PIR_SENSOR, INPUT_PULLUP); 
   //digitalWrite(PIR_SENSOR, HIGH);
   // too sensitive.
   pinMode(PIR_SENSOR, INPUT); 
   digitalWrite(PIR_SENSOR, LOW);
   if (pir_enabled) {
      attachInterrupt(PIR_SENSOR, pirInterrupt, CHANGE);  // attach interrupt to PIR Sensor for either rising or falling.
      interrupts();  // activate Interrupts
   }

   //pinMode(LED_BUILTIN, OUTPUT);     // Debug only : Lou: Initialize the LED_BUILTIN pin as an output - LED_BUILTIN is pre-defined
   //digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED OFF by making the (resistance) voltage HIGH

   FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS);  // -L
   FastLED.clear();
 
   Blynk.begin(auth, wifi_ssid, wifi_password);  

   
   //AWS parameters    
   awsWSclient.setAWSRegion(aws_region);
   awsWSclient.setAWSDomain(aws_endpoint);
   awsWSclient.setAWSKeyID(aws_key);
   awsWSclient.setAWSSecretKey(aws_secret);
   awsWSclient.setUseSSL(true);

   if (connect ()){
      subscribe ();
   }
}


void loop() {
   int i, rev = 0;
   
   Blynk.run(); // Blink run async  
   
   //keep the mqtt up and running
   if (awsWSclient.connected ()) {    
      client->yield();
   } else {
      //handle reconnection
      if (connect ()){
         subscribe ();      
      }
   }

   if (color_play_time  && lightStatus)  {
      if (millis() > color_play_time + prevTime) {
         allOtherButtonWidgetsOff (ON_OFF_VPIN);  // simulate an "off" button press to turn off both the "stobe" and "on/off" button widgets.
         onOffButton (0);  // now also turn off the on/off widget  
         Blynk.virtualWrite(V13, 0);  // reset horizontal time slider to zero.
         lightStatus = 0;
         color_play_time = 0;
      }
   }

   switch (lightStatus) {
      case 0:  // No OP - idle
         break;

      case 1:  // sunset
         FastLED.clear();
         sceneSunset();
         break;

      case 2:  // sunrise
         FastLED.clear();
         sceneSunrise();
         break;

      case 3:
         FastLED.clear();
         sceneCustom();
         break;
   }
}

