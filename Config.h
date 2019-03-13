// Stuff for AWS IoT 


const char *CLIENT_ID= "<name>";
const char *awsEndPoint = "<iotendpoint>";
const char *subscribeTopic = "<topic>"; // Can be changed to shadow topics...
const char *publishTopic = "<topic>";
#define NUMPIXELS1      16 // number of LEDs 


// Paste the file contents into the certificate_pem_crt array in the following fashion. Make sure you follow these four rules when doing so:

// Do not add any extra whitespaces
// End your lines with: \n\
// End the last line with: \n
// Put the array in quotes: ""

const char rootCABuff[] = {"-----BEGIN CERTIFICATE-----\n\
example\n\
-----END CERTIFICATE-----\n"};

const char certificateBuff[] = {"-----BEGIN CERTIFICATE-----\n\
example\n\
-----END CERTIFICATE-----\n"};

const char privateKeyBuff[] = {"-----BEGIN CERTIFICATE-----\n\
example\n\
-----END CERTIFICATE-----\n"};
