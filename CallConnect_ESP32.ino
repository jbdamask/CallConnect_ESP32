/*
 * Project: CallConnect_ESP32
 * Description: Uses a button to change NeoPixel animation and publish a message.
 * Author: John B Damask
 * Date: January 30, 2019
 * Notes: Good first pass. Issues with NeoPixels
 */

#include "WiFiManager.h" // https://github.com/tzapu/WiFiManager
#include <WiFiClientSecure.h>
#include <MQTTClient.h>   //you need to install this library: https://github.com/256dpi/arduino-mqtt
#include "Adafruit_NeoPixel.h"
#include <ArduinoJson.h>
#include "AceButton.h"
#include "Config.h"
using namespace ace_button;

/* Pins -----*/
#define PIN_SOFTAP    13    // Button pin for reconfiguring WiFiManager
#define PIN_STATE     27    // Button pin for changing state
#define PIN_NEOPIXEL   12    // pin connected to the small NeoPixels strip

/* AWS IOT -----*/
int tick=0,msgCount=0,msgReceived = 0;
int status = WL_IDLE_STATUS;    // Connection status
char payload[512];  // Payload array to store thing shadow JSON document
char rcvdPayload[512];
int counter = 0;    // Counter for iteration
WiFiClientSecure net;
MQTTClient client;

/* JSON -----*/
#define JSON_BUFFER_SIZE  512

/* NeoPixel stuff -----*/
#define NUMPIXELS1      14 // number of LEDs on ring
#define BRIGHTNESS      30 // Max brightness of NeoPixels
unsigned long patternInterval = 20 ; // time between steps in the pattern
unsigned long animationSpeed [] = { 100, 50, 2, 2 } ; // speed for each animation (order counts!)
#define ANIMATIONS sizeof(animationSpeed) / sizeof(animationSpeed[0])
// Colors for sparkle
uint8_t myFavoriteColors[][3] = {{200,   0, 200},   // purple
                                 {200,   0,   0},   // red 
                                 {200, 200, 200},   // white
                               };
#define FAVCOLORS sizeof(myFavoriteColors) / 3
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUMPIXELS1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);
bool isOff = true;  // NeoPixel on/off toggle

/* States (1,2,3) ---*/
uint8_t state = 0, previousState = 0;
unsigned long lastUpdate = 0, idleTimer = 0; // for millis() when last update occurred

/* Timing stuff -----*/
long countDown = 0;  // Counts down a certain number of seconds before taking action (for certain states)
#define IDLE_TIMEOUT    5000   // Milliseconds that there can be no touch or ble input before reverting to idle state

/* WiFi -----*/
WiFiManager wm; // global wm instance
bool res;       // Boolean letting us know if we can connect to saved WiFi

/* Button stuff -----*/
ButtonConfig buttonStateConfig;
AceButton buttonState(&buttonStateConfig);
ButtonConfig buttonAPConfig;
AceButton buttonAP(&buttonAPConfig);
void handleStateEvent(AceButton*, uint8_t, uint8_t); // function prototype for state button
void handleAPEvent(AceButton*, uint8_t, uint8_t); // function prototype for access point button
bool isTouched = false;
bool previouslyTouched = false;
bool makingCall = false;    // Keep track of who calls and who receives



void setup() {
    WiFi.disconnect(true);
    delay(5000);
    Serial.begin(115200);
    Serial.println("\n Starting");

  // Configs for the buttons. Need Released event to change the state,
  // and LongPressed to go into SoftAP mode. Don't need Clicked.
    buttonStateConfig.setEventHandler(handleStateEvent);
    buttonStateConfig.setClickDelay(75);
    buttonStateConfig.setFeature(ButtonConfig::kFeatureClick);
    buttonStateConfig.setFeature(ButtonConfig::kFeatureRepeatPress);
    // These suppressions not really necessary but cleaner.
    buttonStateConfig.setFeature(ButtonConfig::kFeatureSuppressAfterClick);
    buttonStateConfig.setFeature(ButtonConfig::kFeatureSuppressAfterRepeatPress);
   
    buttonAPConfig.setEventHandler(handleAPEvent);
    buttonAPConfig.setFeature(ButtonConfig::kFeatureLongPress);
    buttonAPConfig.setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);  

    pinMode(PIN_SOFTAP, INPUT_PULLUP);    // Use built in pullup resistor
    pinMode(PIN_STATE, INPUT_PULLUP);     // Use built in pullup resistor
    buttonState.init(PIN_STATE, HIGH, 0 /* id */);
    buttonAP.init(PIN_SOFTAP, HIGH, 1 /* id */);

    // Initialize NeoPixels
    strip.begin(); // This initializes the NeoPixel library.
    resetBrightness();// These things are bright!
    updatePattern(state);

    Serial.println("Trying to reconnect to wifi...");
    // Initiatize WiFiManager
    res = wm.autoConnect(CLIENT_ID,"password"); // password protected ap
    wm.setConfigPortalTimeout(30); // auto close configportal after n seconds

    // Seeing if we can re-connect to known WiFi
    if(!res) {
        Serial.println("Failed to connect or hit timeout");
        //wm.resetSettings(); // Uncomment for debugging
        //ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected to wifi :)");
    }

    awsConnect();
    mqttTopicSubscribe();
}

void loop(){
  bool static toldUs = false; // When in state 1, we're either making or receiving a call
  isTouched = false;  // Reset unless there's a touch event
  buttonAP.check();
  buttonState.check();

  /* Act on state change */
  if(previousState != state) {
      //Serial.print("New state: "); Serial.println(state);
      wipe();
      resetBrightness();
      patternInterval = animationSpeed[state]; // set speed for this animation
      previousState = state;
      // if(state != 0) isOff = false;
      isOff = (state == 0) ? true : false;
  }

  // The various cases we can face
  switch (state){
    case 0: // idle 
      if(isTouched){
        state = 1;
        publish(String(state));   // TODO - make sure String is the right type for state in the payload
        previouslyTouched = true;
        makingCall = true;
        Serial.println("Calling...");
        idleTimer = millis();
      }
      break;
    case 1: // calling
      if(makingCall){
          if(!toldUs) { // This is used to print once to the console
            toldUs = true;
          }
          if(millis() - idleTimer > IDLE_TIMEOUT){
            resetState();       // If no answer, we reset
            Serial.println("No one answered :-(");
          }
      } else if(isTouched){  // If we're receiving a call, are now are touching the local device, then we're connected
          state = 2;
          publish(String(state));
          previouslyTouched = true;
      }
      break;
    case 2: // connected
      if(isTouched){    // Touch again to disconnect
          Serial.println("State 2. Button pushed. Moving to State 3");
          state = 3;
          publish(String(state));
          previouslyTouched = false;
      }
      if(state == 3) {
          Serial.println("Disconnecting. Starting count down timer.");
          countDown = millis();   // Start the timer
      }      
      break;
    case 3: // Disconnecting
      if(millis() - countDown > IDLE_TIMEOUT) {
          Serial.println("State 3. Timed out. Moving to State 0");
          resetState();
          previousState = 0;
      }
      if(isTouched && previouslyTouched == false){  // If we took our hand off but put it back on in under the time limit, re-connect
          state = 2;
          publish("2");
          previouslyTouched = true;
      }    
      break;
    default:
      resetState();
      break;
  }

  // Update animation frame
  if(millis() - lastUpdate > patternInterval) { 
    updatePattern(state);
  }

  //gotNewMessage = false;      // Reset
}

// Clean house
void resetState(){
  state = 0;
  //previousMQTTState = 0;
  previouslyTouched = false;
  makingCall = false;
  publish(String(state));
}

// The event handler for the state change button.
void handleStateEvent(AceButton* /* button */, uint8_t eventType,
    uint8_t buttonState) {
  switch (eventType) {
    case AceButton::kEventReleased:
      Serial.println("single click");
      isTouched = true;     
      //state = 1;
      break;
  }
}

// The event handler for the WiFi Access Point button.
void handleAPEvent(AceButton* /* button */, uint8_t eventType,
    uint8_t buttonState) {
  switch (eventType) {
    case AceButton::kEventLongPressed:
      Serial.println("Button Held");
      Serial.println("Erasing Config, restarting");
      //isTouched = true;     
      //state = 0;        // TODO - what should the state be in this case?
      res = false;  // Reset WiFi to false since we want AP
      break;
  }
}

// Update the animation
void  updatePattern(int pat){ 
  switch(pat) {
    case 0:
      if(!isOff){               
        Serial.println("turn off");
        wipe();
        strip.show();
        if(!res){   // If we went to state 0 as a result of WiFi button, then this will be false. Otherwise it's true
          if(resetWiFi()){
            if(awsConnect){
              if(!mqttTopicSubscribe){
                Serial.println("CRITICAL ERROR: Couldn't connect to MQTT topic");
              }
            }else {
              Serial.println("CRITICAL ERROR: Couldn't connect to AWS");
            }        
            }else{
              Serial.println("CRITICAL ERROR: Couldn't connect to wifi");
            }     
        }
        isOff = true;
      }
      break;
    case 1: 
      wipe();
      sparkle(3);
      break;     
    case 2:
      breathe(1); // Breathe blue
      break;
    case 3:
      breathe(2); // Breathe red
      break;
    default:
      // donada
      break;
  }  
}

// LED breathing. 
void breathe(int x) {
  float SpeedFactor = 0.008;
  static int i = 0;
  static int r,g,b;
  switch(x){
    case 1:
      r = 0; g = 127; b = 127;
      break;
    case 2:
      r = 255; g = 0; b = 0;
      break;
  }
  // Make the lights breathe
  float intensity = BRIGHTNESS /2.0 * (1.0 + sin(SpeedFactor * i));
  for (int j=0; j<strip.numPixels(); j++) {
    strip.setPixelColor(j, strip.Color(r, g, b)); // Use with WS2812B
    // strip.setPixelColor(j, strip.Color(g, r, b));   // Use with SK6812RGBW
  }
  strip.setBrightness(intensity);
  strip.show();
  i++;
  if(i >= 65535){
    i = 0;
  }
  lastUpdate = millis();
}

// LED sparkling. 
void sparkle(uint8_t howmany) {
  static int x = 0;
  static bool goingUp = true;

  for(uint16_t i=0; i<howmany; i++) {
    // pick a random favorite color!
    int c = random(FAVCOLORS);
    int red = myFavoriteColors[c][0];
    int green = myFavoriteColors[c][1];
    int blue = myFavoriteColors[c][2];

    // get a random pixel from the list
    int j = random(strip.numPixels());

    // now we will 'fade' it in 5 steps
    if(goingUp){
      if(x < 5) {
        x++;
      } else {
        goingUp = false;
      }
    } else {
      if(x>0){
        x--;
      } else {
        goingUp = true;
      }
    }

    int r = red * (x+1); r /= 5;
    int g = green * (x+1); g /= 5;
    int b = blue * (x+1); b /= 5;
    strip.setPixelColor(j, strip.Color(r,g,b));
    strip.show();
  }
  lastUpdate = millis();
}

// clear all LEDs
void wipe(){
   for(int i=0;i<=strip.numPixels();i++){
     strip.setPixelColor(i, strip.Color(0,0,0));
   }
}

// set brightness back to default
void resetBrightness(){
    strip.setBrightness(BRIGHTNESS);
}

bool resetWiFi(){
  wm.resetSettings();
  ESP.restart();
  // start portal w delay
  Serial.println("Starting config portal");
  wm.setConfigPortalTimeout(120);      
  if (!wm.startConfigPortal("OnDemandAP","password")) {
    Serial.println("failed to connect or hit timeout");
    delay(3000);
    return false;
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    return true;
  } 
}

bool awsConnect(){
  net.setCACert(rootCABuff);
  net.setCertificate(certificateBuff);
  net.setPrivateKey(privateKeyBuff);
  client.begin(awsEndPoint, 8883, net);

  Serial.print("\nConnecting to AWS");
  while (!client.connect(CLIENT_ID)) {
    Serial.print(".");
    delay(100);
  }

  if(client.connected()) {
    Serial.println("Connected to AWS"); 
  } else {
    Serial.println("Failed to connect to AWS.");
    Serial.println(client.returnCode());
  }
}

bool mqttTopicSubscribe(){
  client.subscribe(subscribeTopic);
  client.onMessage(messageReceived);
}

void publish(String state){
  char msg[50];
  static int value = 0;

  int NUM_RETRIES = 10;
  int cnt = 0;
  Serial.println("Function: publish()");
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["thing_name"] = String(CLIENT_ID);
  root["state"] = state;
  //Serial.println("    Created the object. Now print to json");
  String sJson = "";
  root.printTo(sJson);
  char* cJson = &sJson[0u];
 
  if(!client.publish(publishTopic, cJson)){
   Serial.println("Publish failed");
  }

}

void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);
    StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload);
    const char* d = root["thing_name"];
    const char* s = root["state"];    
    Serial.println(String(d));
    if(strcmp(d, CLIENT_ID)==0) return; // If we're receiving our own message, ignore
    Serial.println("    Message is from another device. Printing...");
    //Serial.print("State value: "); Serial.println(s);

    if(strcmp(s,"0")==0){  
        state = 0;
    }else if(strcmp(s,"1")==0){
        state = 1;
    }else if(strcmp(s,"2")==0){
        state = 2;
    }else if(strcmp(s,"3")==0){
        state = 3;
        countDown = millis();   // Set the timer so that the device receiving the countdown message shows the animation for the right amount of time
    }    
}

// AWS MQTT callback handler
void mySubCallBackHandler (char *topicName, int payloadLen, char *payLoad){
    Serial.println("Function: mySubCallBackHandler()");

    StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payLoad);
    const char* d = root["thing_name"];
    const char* s = root["state"];    
    Serial.println(String(d));
    if(strcmp(d, CLIENT_ID)==0) return; // If we're receiving our own message, ignore

    Serial.println("    Message is from another device. Printing...");
    //Serial.print("State value: "); Serial.println(s);

    if(strcmp(s,"0")==0){  
        state = 0;
    }else if(strcmp(s,"1")==0){
        state = 1;
    }else if(strcmp(s,"2")==0){
        state = 2;
    }else if(strcmp(s,"3")==0){
        state = 3;
        countDown = millis();   // Set the timer so that the device receiving the countdown message shows the animation for the right amount of time
    }
}