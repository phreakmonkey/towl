// Telemetry over Opportunistic WiFi Links (T.O.W.L.)
// http://phreakmonkey.com/projects/towl

// !! Be sure to check configuration settings in config.h before compiling !!

#include "config.h"

#ifdef OAK
SYSTEM_MODE(SEMI_AUTOMATIC);
#endif

#include <TimeLib.h>
#include <Time.h>

#include <TinyGPS.h>
#include <Base32.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

// Globals:
Base32 base32;
TinyGPS gps;

struct telem {
  uint32_t tstamp;
  int32_t lat;
  int32_t lon;
  uint8_t spd;
  uint8_t sats;
  uint8_t id;
  uint8_t mode;
};  // 16 bytes total

struct telem tstore[TSTORE_SZ];
uint8_t query_id = 0;
uint32_t last_rec = 0; // Last record time
uint32_t last_rep = 0; // Last report time
uint8_t startup = 3;

// Function prototypes:
void parseGPS(void);
void setGPSTime(void);
struct telem * getTelem(void);
uint16_t findSlot(uint8_t);
void storeTelem(struct telem *);
uint8_t sendStoredTelem(void);
int connectAP(void);
uint8_t sendDNSTelem(struct telem *);
void serDelay(unsigned long);
void setup(void);
void loop(void);
#ifdef OAK
void homeConnect();
#endif


void setup() {
  Serial.begin(GPS_BAUD);
  delay(100);
  Serial.println("Startup sequence.");
  pinMode(LED, OUTPUT);
  analogWrite(LED, 0);
  randomSeed(analogRead(0));
  memset(tstore, 0, sizeof(tstore));
#ifdef OAK
  homeConnect();
#else
  WiFi.mode(WIFI_STA);
#endif
}

void loop() {
  telem *currentpos;
  uint8_t res;
  parseGPS();

  if (gps.satellites() != 255) {
    res = 0;
    analogWrite(LED, gps.satellites() * (1024 / 11));
    if (timeStatus() == timeNotSet) setGPSTime();
    else {
      currentpos = getTelem();
      if (connectAP() == 1) res = sendDNSTelem(currentpos);
      if (res == 1) {
        last_rep = millis();
        last_rec = millis();
      }
      else if (startup > 0 || millis() - last_rec > 10000) storeTelem(currentpos);
      delete currentpos;
      while (res == 1) {
        res = sendStoredTelem();
        parseGPS();
      }
    }
  }
}

#ifdef OAK
void homeConnect() {
  int numNets = WiFi.scanNetworks();
  uint16_t thisNet;
  for (thisNet = 0; thisNet < numNets; thisNet++)
    if (strcmp(WiFi.SSID(thisNet).c_str(), HOMESSID) == 0) break;
  if (thisNet < numNets) {
    Particle.connect();
    for (uint8_t i=0; i<70; i++) {
      analogWrite(LED, 1023);
      delay(100);
      analogWrite(LED, 0);
      delay(100);
      if (Particle.connected() == false) Particle.connect();
    }
    Particle.disconnect();
    WiFi.disconnect();
  }
  return;
}
#endif

uint16_t findSlot(uint8_t pmode) {
  // Find a memory slot that is empty or at a higher time
  // resolution than the current object.
  uint16_t i;
  for(i=0; i < TSTORE_SZ; i++) {
    if (tstore[i].tstamp == 0) return i;  // Empty slot
  }
  if (pmode == 0) return i;
  
  // Buffer full, is there a higher res one we can overwrite?
  for(uint8_t j=0; j < pmode; j++)
    for(i=0; i < TSTORE_SZ; i++) {    
      if (tstore[i].mode == j) return i;
    }
  return i;
}

void storeTelem(struct telem *tdata) {
  uint16_t slot;
  uint8_t thisMode = ((millis() - last_rep) / 10000) - 1;
  if (thisMode >= MAX_INTERVAL) {
    last_rep = millis();  // Reset counter at MAX_INTERVAL
    thisMode = 0; 
  }

  slot = findSlot(thisMode);
  if (slot == TSTORE_SZ) return; // Buffer full at current res.
    
  tstore[slot].tstamp = tdata->tstamp;
  tstore[slot].lat = tdata->lat;
  tstore[slot].lon = tdata->lon;
  tstore[slot].spd = tdata->spd;
  tstore[slot].sats = tdata->sats;
  tstore[slot].id = tdata->id;
  if (tdata->mode == 255) tstore[slot].mode = 255;  // Preserve startup marker
  else tstore[slot].mode = thisMode;
  last_rec = millis();

  Serial.print("Telem stored in slot ");
  Serial.println(slot);
  return;
}

uint8_t sendStoredTelem() {
  uint8_t i, res = 0;
  for(i=0; i < TSTORE_SZ; i++) {
    if (tstore[i].tstamp != 0) {
      res = sendDNSTelem(&tstore[i]);
      if (res == 1) tstore[i].tstamp = 0;
      return res;
    }
  }
  return 0;
}

int connectAP() {
  uint16_t numNets;
  uint16_t numOpen = 0;
  int16_t bestcandidate[2] = {-1, -1};
  int16_t bestsignal[2] = {-255, -255};
  int wstatus = WL_IDLE_STATUS;
  char wSSID[64];
  wSSID[63] = 0;

  Serial.println("Scanning WiFi");
  numNets = WiFi.scanNetworks();
  for (uint16_t thisNet = 0; thisNet < numNets; thisNet++) {
    if (WiFi.encryptionType(thisNet) == ENC_TYPE_NONE) {
      numOpen++;
      Serial.print("OPEN SSID: ");
      Serial.print(WiFi.SSID(thisNet));
      Serial.print(" RSSI: ");
      Serial.println(WiFi.RSSI(thisNet));
      for (uint8 i=0; i < 2; i++) {
        if (WiFi.RSSI(thisNet) > bestsignal[i]) {
          bestcandidate[i] = thisNet;
          bestsignal[i] = WiFi.RSSI(thisNet);
          break;
        }
      }
    }
  }

  if (bestcandidate[0] > -1) {
    uint8_t choice = 0;
    if (bestcandidate[1] > -1) choice = random(2);
    strncpy(wSSID, WiFi.SSID(bestcandidate[choice]).c_str(), sizeof(wSSID)-1);
    Serial.print("Attempting connect via ");
    Serial.print(wSSID);
    WiFi.disconnect();
    WiFi.persistent(false);
#ifdef OAK
      wstatus = WiFi.begin_internal(wSSID, NULL, 0, NULL);
#else 
      wstatus = WiFi.begin(wSSID, NULL, 0, NULL, true);
#endif
    for (uint8_t i=0; i < 65; i++) {
      if (wstatus == WL_CONNECTED) {
        Serial.print(". connected. ");
        Serial.println(i * 100);
        return 1;
      }
      if (wstatus == WL_CONNECT_FAILED) {
        Serial.println(". connect failed.");
        return 0;
      }
      serDelay(100);
      wstatus = WiFi.status();
    }
    Serial.println(". timeout.");
  }
  return 0;
}

struct telem *getTelem() {
  struct telem *tdata = new struct telem;
  unsigned long age;
  float flat, flon;
  tdata->sats = gps.satellites();
  if (tdata->sats < 3 || tdata->sats > 20) {
    tdata->sats = 255;
    return tdata;  // Invalid GPS Signal
  }
  tdata->tstamp = now();
  gps.f_get_position(&flat, &flon, &age);
  tdata->lat = flat * 1000000;
  tdata->lon = flon * 1000000;
  tdata->spd = (uint16_t)gps.f_speed_mph();
  tdata->id = query_id;
  if (startup > 0) {
    tdata->mode = 255; // Startup marker
    startup--;
  } else tdata->mode = 254; // Live telem marker
  if (query_id < 255) query_id++;
  else query_id = 0;
  return tdata;
}

uint8_t sendDNSTelem(struct telem *tdata) {
  IPAddress qresponse = {0,0,0,0};
  char query[127];
  unsigned int outlen;
  byte *b32string;

  memset(query, '\0', sizeof(query));
  outlen = base32.toBase32((byte*)tdata, sizeof(struct telem), b32string, false);
  strcat(query, "S-");
  strncpy(query+2, (char*)b32string, outlen);
  strcat(query, ".");
  strcat(query, DEVICE_ID);
  strcat(query, ".");
  strcat(query, SUBDOMAIN);
  free(b32string);  // Got malloc()'d inside base32.toBase32()

  WiFi.hostByName(query, qresponse);
  if (qresponse[0] == 10 && qresponse[3] == tdata->id) {
    Serial.println("Telemetry ACK");
    return 1;
  }
  Serial.println("No confirm.");
  return 0;
}

void parseGPS() {
  while(Serial.available())
    gps.encode(Serial.read());
}

void serDelay(unsigned long ms) {
  unsigned long start = millis();
  do {
    parseGPS();
  } while (millis() - start < ms);
}

void setGPSTime() {
  uint8_t sats = gps.satellites();
  if (sats < 3 || sats > 20) return;

  unsigned long age;
  int Year;
  byte Month, Day, Hour, Minute, Second;

  gps.crack_datetime(&Year, &Month, &Day, &Hour, &Minute, &Second, NULL, &age);
  if (age > 500) return;
  if (Year < 2016) return;  // Weirdness with MTK-3329 during initialization
  setTime(Hour, Minute, Second, Day, Month, Year);
  char timestr[64];
  sprintf(timestr, "GPS Time: %d-%02d-%02d %02d:%02d:%02d", Year, Month, Day, Hour, Minute, Second);
  Serial.println(timestr);
  return;
}
