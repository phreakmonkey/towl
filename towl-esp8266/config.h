// Telemetry over Opportunistic WiFi Links (T.O.W.L.)
// http://phreakmonkey.com/projects/towl

// --- Digistump Oak Board ---
// The following enables Particle.connect() and OTA flashing for Digistump Oak.
// HOMESSID must be set the same as in your Oak firmware for OTA to work.
// Comment these out for most other generic ESP8266 boards.
#define OAK
#define HOMESSID "Linksys"
// --- End Digistump Oak Config ---

//  The queries are sent in the form of S-{BASE32}.DEVICE_ID.SUBDOMAIN
//  #define DEVICE_ID and SUBDOMAIN below:
#define DEVICE_ID "a01"
#define SUBDOMAIN "foobar.example.com"

// LED pin (currently just indicates GPS signal status via PWM)
// Hint: DigiStump Oak = 1, Adafruit Huzzah = 0, others = ??
#define LED 1

// NMEA GPS Serial baudrate
// Note this will also be the bitrate of debug output messages on TX pin
#define GPS_BAUD 115200

// TSTORE_SZ = max number of telemetry entries to backlog (16 bytes RAM each)
#define TSTORE_SZ 200
// MAX_INTERVAL : Highest interval to track in 10 sec increments. (6 = 1 min)
#define MAX_INTERVAL 18

// --- end CONFIGURATION section ---
