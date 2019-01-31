#include "WiFiManager.h" // https://github.com/tzapu/WiFiManager
#include "AWS_IOT.h"
#include "Adafruit_NeoPixel.h"
#include "AceButton.h"
using namespace ace_button;

/* Pins -----*/
#define PIN_SOFTAP    13    // Button pin for reconfiguring WiFiManager
#define PIN_NEOPIXEL   12    // pin connected to the small NeoPixels strip

/* AWS IOT -----*/
AWS_IOT hornbill;
int tick=0,msgCount=0,msgReceived = 0;
int status = WL_IDLE_STATUS;    // Connection status
char payload[512];  // Payload array to store thing shadow JSON document
char rcvdPayload[512];
int counter = 0;    // Counter for iteration
char HOST_ADDRESS[]="...";
char CLIENT_ID[]= "...";
char TOPIC_NAME[]= "...";

/* NeoPixel stuff -----*/
#define NUMPIXELS1      14 // number of LEDs on ring
#define BRIGHTNESS      30 // Max brightness of NeoPixels
unsigned long patternInterval = 20 ; // time between steps in the pattern
unsigned long animationSpeed [] = { 100, 50, 2 } ; // speed for each animation (order counts!)
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
unsigned long lastUpdate = 0;

/* Timing stuff -----*/
long countDown = 0;  // Counts down a certain number of seconds before taking action (for certain states)

/* WiFi -----*/
WiFiManager wm; // global wm instance
bool res;       // Boolean letting us know if we can connect to saved WiFi

/* Button stuff -----*/
AceButton button(PIN_SOFTAP);
void handleEvent(AceButton*, uint8_t, uint8_t);
bool isTouched = false;
bool previouslyTouched = false;
void handleEvent(AceButton*, uint8_t, uint8_t); // function prototype

/* MQTT callback ----*/
void mySubCallBackHandler (char *topicName, int payloadLen, char *payLoad)
{
    strncpy(rcvdPayload,payLoad,payloadLen);
    rcvdPayload[payloadLen] = 0;
    msgReceived = 1;
}

void setup() {
    WiFi.disconnect(true);
    Serial.begin(115200);
    Serial.println("\n Starting");
    pinMode(PIN_SOFTAP, INPUT_PULLUP);

    // Configure the ButtonConfig with the event handler, and enable all higher
    // level events.
    ButtonConfig* buttonConfig = button.getButtonConfig();
    buttonConfig->setEventHandler(handleEvent);
    buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
    buttonConfig->setFeature(ButtonConfig::kFeatureSuppressClickBeforeDoubleClick);
    buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterClick);
    buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);
    buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);
    buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);

    // Initialize NeoPixels
    strip.begin(); // This initializes the NeoPixel library.
    resetBrightness();// These things are bright!
    updatePattern(state);

    Serial.println("Trying to reconnect to wifi...");
    // Initiatize WiFiManager
    res = wm.autoConnect("AutoConnectAP","password"); // password protected ap
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

}

void loop(){
  button.check();

  /* Act on state change */
  if(previousState != state) {
      Serial.print("New state: ");
      Serial.println(state);
      wipe();
      resetBrightness();
      patternInterval = animationSpeed[state]; // set speed for this animation
      previousState = state;
      if(state != 0) isOff = false;
  }

  // Update animation frame
  if(millis() - lastUpdate > patternInterval) { 
    updatePattern(state);
  }
}

// The event handler for the button.
void handleEvent(AceButton* /* button */, uint8_t eventType,
    uint8_t buttonState) {
  switch (eventType) {
    case AceButton::kEventClicked:
    case AceButton::kEventReleased:
      Serial.println("single click");
      state = 1;
      break;
    case AceButton::kEventDoubleClicked:
      Serial.println("double click");
      state = 2;
      break;
    case AceButton::kEventLongPressed:
      Serial.println("Button Held");
      Serial.println("Erasing Config, restarting");     
      state = 0;
      res = false;  // Reset WiFi to false since we want AP
      break;
  }
}

// Update the animation
void  updatePattern(int pat){ 
  switch(pat) {
    case 0:
      if(!isOff && !res){
        Serial.println("turn off");
        wipe();
        strip.show();
        wm.resetSettings();
        ESP.restart();      
        // start portal w delay
        Serial.println("Starting config portal");
        wm.setConfigPortalTimeout(120);      
        if (!wm.startConfigPortal("OnDemandAP","password")) {
          Serial.println("failed to connect or hit timeout");
          delay(3000);
          // ESP.restart();
        } else {
          //if you get here you have connected to the WiFi
          Serial.println("connected...yeey :)");
        }         
        isOff = true;
      }
      break;
    case 1: 
      wipe();
      sparkle(3);
      break;     
    case 2:
      breathe(1); // Breath blue
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

void resetWiFi(){
  wm.resetSettings();
  ESP.restart();
  // start portal w delay
  Serial.println("Starting config portal");
  wm.setConfigPortalTimeout(120);      
  if (!wm.startConfigPortal("OnDemandAP","password")) {
    Serial.println("failed to connect or hit timeout");
    delay(3000);
    // ESP.restart();
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
  } 
}