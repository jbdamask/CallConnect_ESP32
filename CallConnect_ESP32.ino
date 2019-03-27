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
#include <NeoPixelBrightnessBus.h>
#include <NeoPixelAnimator.h>
#include <ArduinoJson.h>
#include "AceButton.h"
#include "Config.h"
#include "certificates.h"
using namespace ace_button;

/* Pins -----*/
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
// Number of pixels in Config.h
#define BRIGHTNESS      255 // Max brightness of NeoPixels
#define SPARKLE_SPEED   85 // Speed at which sparkles animate
unsigned long patternInterval = 20 ; // time between steps in the pattern
unsigned long animationSpeed [] = { SPARKLE_SPEED } ; // speed for each animation (order counts!)
#define ANIMATIONS sizeof(animationSpeed) / sizeof(animationSpeed[0])
// Colors for sparkle
uint8_t myFavoriteColors[][3] = {{200,   0, 200},   // purple
                                 {200,   0,   0},   // red 
                                 {200, 200, 200},   // white
                               };
#define FAVCOLORS sizeof(myFavoriteColors) / 3
bool isOff = true;  // NeoPixel on/off toggle
// Stuff for light ring effects
const uint16_t AnimCount = 1; // we only need one
const uint16_t TailLength = 6; // length of the tail, must be shorter than PixelCount
const float MaxLightness = 0.4f; // max lightness at the head of the tail (0.5f is full bright)
NeoGamma<NeoGammaTableMethod> colorGamma; // for any fade animations, best to correct gamma
// Let er rip
NeoPixelBrightnessBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUMPIXELS1, PIN_NEOPIXEL);
const uint8_t AnimationChannels = 1; // we only need one as all the pixels are animated at once for breathing
NeoPixelAnimator animations(AnimationChannels); // NeoPixel animation management object

NeoPixelAnimator ringAnimation(AnimationChannels); // NeoPixel animation management object

boolean fadeToColor = true;  // general purpose variable used to store effect state
// what is stored for state is specific to the need, in this case, the colors.
// basically what ever you need inside the animation update function
struct MyAnimationState
{
    RgbColor StartingColor;
    RgbColor EndingColor;
};
// one entry per pixel to match the animation timing manager
MyAnimationState animationState[AnimationChannels];

/* States (1,2,3) ---*/
uint8_t state = 0, previousState = 0;
unsigned long lastUpdate = 0, idleTimer = 0, resetTimer = 0; // for millis() when last update occurred

/* Timing stuff -----*/
long countDown = 0;  // Counts down a certain number of seconds before taking action (for certain states)
#define IDLE_TIMEOUT    5000   // Milliseconds that there can be no touch or ble input before reverting to idle state

/* WiFi -----*/
WiFiManager wm; // global wm instance
bool res;       // Boolean letting us know if we can connect to saved WiFi

/* Button stuff -----*/
ButtonConfig buttonStateConfig;
AceButton buttonState(&buttonStateConfig);
void handleButtonPush(AceButton*, uint8_t, uint8_t); // function prototype for state button

bool isTouched = false;
bool previouslyTouched = false;
bool makingCall = false;    // Keep track of who calls and who receives

/* Clean house -----*/
void resetState(){
  state = 0;
  previouslyTouched = false;
  makingCall = false;
  publish(String(state));
}

/*  ====================================================================  *
*                               BUTTON                                    *
*   ====================================================================  */

void buttonSetup(){
  // Configs for the buttons. Need Released event to change the state,
  // and LongPressed to go into SoftAP mode. Don't need Clicked.
    buttonStateConfig.setEventHandler(handleButtonPush);
    buttonStateConfig.setClickDelay(75);
    buttonStateConfig.setFeature(ButtonConfig::kFeatureClick);
    buttonStateConfig.setFeature(ButtonConfig::kFeatureRepeatPress);
    buttonStateConfig.setFeature(ButtonConfig::kFeatureLongPress);
    buttonStateConfig.setLongPressDelay(LONG_PRESS_DURATION);
    // These suppressions not really necessary but cleaner.
    buttonStateConfig.setFeature(ButtonConfig::kFeatureSuppressAfterClick);
    buttonStateConfig.setFeature(ButtonConfig::kFeatureSuppressAfterRepeatPress);
    buttonStateConfig.setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);  
    pinMode(PIN_STATE, INPUT_PULLUP);     // Use built in pullup resistor
    buttonState.init(PIN_STATE, HIGH, 0 /* id */);
}

// The event handler for the state change button.
void handleButtonPush(AceButton* /* button */, uint8_t eventType,
    uint8_t buttonState) {
  switch (eventType) {
    case AceButton::kEventLongPressed:
      Serial.println("Button Held");
      Serial.println("Restarting chip");
      isOff = true;   // Turn off NeoPixels   
      restartAndConnect();
      break;    
    case AceButton::kEventReleased:
      Serial.println("single click");
      isTouched = true;     
      break;
  }
}

/*  ====================================================================  *
*                               ANIMATIONS                                *
*   ====================================================================  */

// simple blend function
void BlendAnimUpdate(const AnimationParam& param){
    // this gets called for each animation on every time step
    // progress will start at 0.0 and end at 1.0
    // we use the blend function on the RgbColor to mix
    // color based on the progress given to us in the animation
    RgbColor updatedColor = RgbColor::LinearBlend(
        animationState[param.index].StartingColor,
        animationState[param.index].EndingColor,
        param.progress);

    // apply the color to the strip
    for (uint16_t pixel = 0; pixel < NUMPIXELS1; pixel++)
    {
        strip.SetPixelColor(pixel, updatedColor);
    }
}

void FadeInFadeOutRinseRepeat(RgbColor myColor){
    if (fadeToColor)
    {
        // Fade upto a random color
        // we use HslColor object as it allows us to easily pick a hue
        // with the same saturation and luminance so the colors picked
        // will have similiar overall brightness
        RgbColor target = HslColor(myColor);
        uint16_t time = random(800, 2000);

        animationState[0].StartingColor = strip.GetPixelColor(0);
        animationState[0].EndingColor = target;

        animations.StartAnimation(0, time, BlendAnimUpdate);
    }
    else 
    {
        // fade to black
        uint16_t time = random(600, 700);
        animationState[0].StartingColor = strip.GetPixelColor(0);
        animationState[0].EndingColor = RgbColor(0);
        animations.StartAnimation(0, time, BlendAnimUpdate);
    }

    // toggle to the next effect state
    fadeToColor = !fadeToColor;
}

// LED breathing. 
void breathe(int breatheColor){
  Serial.print("Function: breathe(). Color code is: "); Serial.println(breatheColor);
  static RgbColor c;
  switch(breatheColor){
    case 1:
      c = RgbColor(0, 127, 127);  // Light blue
      break;
    case 2:
      c = RgbColor(255, 0, 0);    // Red
      break;
    case 3: 
      c = RgbColor(255, 215, 0);  // Gold
      break;
    case 4:
      c = RgbColor(0, 200, 0); // Greenish
      break;
    if (animations.IsAnimating())
    {
        // the normal loop just needs these two to run the active animations
        animations.UpdateAnimations();
        strip.Show();
    }
    else
    {
        FadeInFadeOutRinseRepeat(c);
    }
  }
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
    int j = random(NUMPIXELS1);

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
    strip.SetPixelColor(j, RgbColor(r,g,b));
    strip.Show();
  }
  lastUpdate = millis();
}

// LED ring
void roundy(float hue){
    // Initialize if we're not already animating
    if(!animations.IsAnimating()){
      DrawTailPixels(hue);
      ringAnimation.StartAnimation(0,66, LoopAnimUpdate);
    } 
}

// Used by Roundy
void DrawTailPixels(float hue){
    // using Hsl as it makes it easy to pick from similiar saturated colors
    for (uint16_t index = 0; index < strip.PixelCount() && index <= TailLength; index++)
    {
        float lightness = index * MaxLightness / TailLength;
        RgbColor color = HslColor(hue, 1.0f, lightness);
        strip.SetPixelColor(index, colorGamma.Correct(color));
    }
}

// Used by Roundy
void LoopAnimUpdate(const AnimationParam& param){
    // wait for this animation to complete,
    // we are using it as a timer of sorts
    if (param.state == AnimationState_Completed)
    {
        // done, time to restart this position tracking animation/timer
        ringAnimation.RestartAnimation(param.index);
        // rotate the complete strip one pixel to the right on every update
        strip.RotateRight(1);
    }
}

// clear all LEDs
void wipe(){
  for(int i = 0; i <= NUMPIXELS1; i++){
    strip.SetPixelColor(i, RgbColor(0,0,0));
  }
}

// set brightness back to default
void resetBrightness(){
    strip.SetBrightness(BRIGHTNESS);
}

// Update the animation
void  updatePattern(int pat){ 
  switch(pat) {
    case 0:
      if(isOff){
        wipe();
        strip.Show();
      }
      break;
    case 1: 
      wipe();
      sparkle(3);
      break;     
    case 2:
      Serial.println("State 2");
      breathe(1); // Breathe blue
      break;
    case 3:
      Serial.println("State 3");
      breathe(2); // Breathe red
      break;
    case 4:
      Serial.println("State 4");
      breathe(3); // Breathe gold
      break;
    case 5:
      Serial.println("State 5");
      breathe(4); // Breathe greenish
    default:
      // donada
      break;
  }  
}

/*  ====================================================================  *
*                               NETWORKING                                *
*   ====================================================================  */

// Clears memory of any previously joined networks and starts access point
bool resetWiFi(){
  wm.resetSettings();
  ESP.restart();
  // start portal w delay
  Serial.println("Starting config portal");
  wm.setConfigPortalTimeout(120);      
  if (!wm.startConfigPortal(CLIENT_ID, AP_PASSWORD)) {
    Serial.println("failed to connect or hit timeout");
    delay(3000);
    return false;
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    res = true;
    return true;
  } 
}

// Restarts the WiFi chip and reconnects to AWS
// This can be used when device moves to new network or when gremlins strike
void restartAndConnect(){
  client.disconnect();
  ESP.restart();
  connectWiFI();
  connect();
}

// Connect to AWS IoT service and subscribe to MQTT topic
void connect(){
    if(!awsConnect()){ // Return immediately if we can't connect to AWS
      return;
    } 
    state = 5;  // Animation tied to this state shows user we're good to go
    countDown = millis();   // Set the timer so we show the animation for the right amount of time
}

bool awsConnect(){
  static int awsConnectTimer = 0;
  int awsConnectTimout = 10000;
  net.setCACert(rootCABuff);
  net.setCertificate(certificateBuff);
  net.setPrivateKey(privateKeyBuff);
  // Changes the keep alive interval to match aws's SDK default (https://docs.aws.amazon.com/general/latest/gr/aws_service_limits.html#limits_iot)
  // I'm hoping this reduces the number of disconnects I'm seeing
  client.setOptions(1200, true, 1000);  
  client.begin(awsEndPoint, 8883, net);
  Serial.print("\nConnecting to AWS MQTT broker");
  while (!client.connect(CLIENT_ID)) {
    if(awsConnectTimer == 0) awsConnectTimer = millis();
    if (millis() - awsConnectTimer > awsConnectTimout) {
      Serial.println("CONNECTION FAILURE: Could not connect to aws");
      return false;
    }
    Serial.print(".");
    delay(100);
  }
  Serial.println("Connected to AWS"); 
  awsConnectTimer = 0;
  // Now subscribe to MQTT topic
  if(!client.subscribe(mqttTopic)) {
    Serial.println("CONNECTION FAILURE: Could not subscribe to MQTT topic");
    return false;
  }
  client.onMessage(messageReceived);
  return true;
}

void publish(String state){ // Isn't state global? If so, no need to pass
  char msg[50];
  static int value = 0;
  int NUM_RETRIES = 10;
  int cnt = 0;
  Serial.println("Function: publish()");
  StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["thing_name"] = String(CLIENT_ID);
  root["state"] = state;
  String sJson = "";
  root.printTo(sJson);
  char* cJson = &sJson[0u];
  if(!client.connected()){
    Serial.println("PUBLISH ERROR: Client not connected");
  }
  if(!client.publish(mqttTopic, cJson)){    
   Serial.println("PUBLISH ERROR: Publish failed");
   Serial.print("  Topic: "); Serial.println(mqttTopic);
   Serial.print("  Message: "); Serial.println(cJson);
  }
}

// AWS MQTT callback handler
void messageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);
    StaticJsonBuffer<JSON_BUFFER_SIZE> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload);
    const char* d = root["thing_name"];
    const char* s = root["state"];    
    Serial.println(String(d));
    if(strcmp(d, CLIENT_ID)==0) return; // If we're receiving our own message, ignore
    Serial.println("    Message is from another device. Printing...");
    Serial.print("State value: "); Serial.println(s);

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

// Connects to known WiFi or launches access point if none available
void connectWiFI(){
  WiFi.disconnect(true); // Just in case we're connected
  Serial.println("Trying to reconnect to known wifi...");
  // Set this in order for loop() to keep running
  wm.setConfigPortalBlocking(false);
  // Initiatize WiFiManager
  res = wm.autoConnect(CLIENT_ID, AP_PASSWORD); // password protected ap
  wm.setConfigPortalTimeout(30); // auto close configportal after n seconds
  // Seeing if we can re-connect to known WiFi
  if(!res) {
      Serial.println("Failed to connect or hit timeout");
      state = 4;  // Show SoftAP animation
  } 
  else {
      //if you get here you have connected to the WiFi    
      Serial.println("connected to wifi :)");
  }

}

void setup() {
    // @TODO
    // Consider setting MQTT options (https://github.com/256dpi/arduino-mqtt):
    // void setOptions(int keepAlive, bool cleanSession, int timeout);

    delay(5000);
    Serial.begin(115200);
    Serial.println("\n Starting");

    buttonSetup();

    // Initialize NeoPixels
    strip.Begin();
    resetBrightness();// These things are bright!
    updatePattern(state);

    // Connect to stuff
    connectWiFI();
    connect();

    // Start the reset countdown
    resetTimer = millis();  
}

void loop(){
  wm.process(); // Needed for loop to run when WiFiManager is in SoftAP mode
  client.loop();
  // The MQTTClient library docs say to put a delay(10) here but I don't want to block!
  
  // Make sure we're still connected
  if(!client.connected()){
    connect();
  }
  
  bool static toldUs = false; // When in state 1, we're either making or receiving a call
  isTouched = false;  // Reset unless there's a touch event
  buttonState.check();

  /* Act on state change */
  if(previousState != state) {
      Serial.print("New state: "); Serial.println(state);
      wipe();
      if(state != 0 ) resetBrightness();      
      strip.Show();
      previousState = state;
      isOff = (state == 0) ? true : false;
      resetTimer = millis();  // If state change is registered, things are working. Reset the timer
  }

  // The various cases we can face
  switch (state){
    case 0: // idle 
      if(isTouched && state != 4){ // Only register touch event if we're not in SoftAP mode
        state = 1;
        publish(String(state));   // TODO - make sure String is the right type for state in the payload
        previouslyTouched = true;
        makingCall = true;
        Serial.println("Calling...");
        idleTimer = millis();
      } else {  // If nothing going on, and nothing has gone on for a while, proactively restart the chip
        if(millis() - resetTimer > RESET_AFTER){
          ESP.restart();
        }
      }
      break;
    case 1: // calling
      if(millis() - lastUpdate > SPARKLE_SPEED) { 
        wipe();
        sparkle(3);
      }  
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
      if (animations.IsAnimating()){
          // the normal loop just needs these two to run the active animations
          animations.UpdateAnimations();
          strip.Show();
      } else {
          Serial.println("Starting to breathe cyan");
          FadeInFadeOutRinseRepeat(RgbColor(0,127,127));
      }
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
      if (animations.IsAnimating()){
          // the normal loop just needs these two to run the active animations
          animations.UpdateAnimations();
          strip.Show();
      } else {
          Serial.println("Starting to breathe red");
          FadeInFadeOutRinseRepeat(RgbColor(255,0,0));
      }    
      if(millis() - countDown > IDLE_TIMEOUT) {
          Serial.println("State 3. Timed out. Moving to State 0");
          resetState();
      }
      if(isTouched && previouslyTouched == false){  // If we took our hand off but put it back on in under the time limit, re-connect
          state = 2;
          publish("2");
          previouslyTouched = true;
      }    
      break;
    case 4: // SoftAP
      if(ringAnimation.IsAnimating()){
          // the normal loop just needs these two to run the active animations
          ringAnimation.UpdateAnimations();
          strip.Show();
      } else {
        //FadeInFadeOutRinseRepeat(RgbColor(255,215,0));
        float hue = 51/360.0f;
        roundy(hue); // Hue of 120 is green
      }  
      break;
    case 5: // Connection success
      if(ringAnimation.IsAnimating()){
        // the normal loop just needs these two to run the active animations
        ringAnimation.UpdateAnimations();
        strip.Show();
      } else {
        float hue = 120/360.0f;
        roundy(hue); // Hue of 120 is green
      }
      if(millis() - countDown > (IDLE_TIMEOUT-2)) { // Show we're connected for just a few seconds
        Serial.println("State 5. Timed out. Moving to State 0");
        resetState();
      }
      break;
    default:
      resetState();
      break;
  }

}

