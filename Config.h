// Stuff for AWS IoT 


const char *CLIENT_ID= "<name>";
const char *awsEndPoint = "<iotendpoint>";
const char *subscribeTopic = "<topic>"; // Can be changed to shadow topics...
const char *publishTopic = "<topic>";
const long RESET_AFTER = 21600000;  // 6 hours
const long LONG_PRESS_DURATION = 5000; // Used for long button push to restart the ESP chip
#define NUMPIXELS1      16 // number of LEDs 

