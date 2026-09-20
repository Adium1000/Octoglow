                                                                                
//    ▄▄▄▄                                            ▄▄▄▄                         
//   ██▀▀██               ██                          ▀▀██                         
//  ██    ██   ▄█████▄  ███████    ▄████▄    ▄███▄██    ██       ▄████▄  ██      ██
//  ██    ██  ██▀    ▀    ██      ██▀  ▀██  ██▀  ▀██    ██      ██▀  ▀██ ▀█  ██  █▀
//  ██    ██  ██          ██      ██    ██  ██    ██    ██      ██    ██  ██▄██▄██ 
//   ██▄▄██   ▀██▄▄▄▄█    ██▄▄▄   ▀██▄▄██▀  ▀██▄▄███    ██▄▄▄   ▀██▄▄██▀  ▀██  ██▀ 
//    ▀▀▀▀      ▀▀▀▀▀      ▀▀▀▀     ▀▀▀▀     ▄▀▀▀ ██     ▀▀▀▀     ▀▀▀▀     ▀▀  ▀▀  
//                                          ▀████▀▀                                                                                                          
#include <WiFi.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <StreamString.h>
#include <esp_sntp.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include "mbedtls/sha256.h"
#include "mbedtls/md.h"

// FIRMWARE VERSION
#define FW_VERSION "0.3"

// FORWARD DECLARATIONS
extern bool provisionMode;
void stopProvisionMode();
bool connectToWiFi(const char* ssid, const char* pass);
void startProvisionMode();
void handleStartAp();
void handleApState();
void handleApSett();
void handleTimeSett();
void handleApPass();
void handleStopAp();
String getApSsid();
String getApPass();
String jsonEscape(const String& in);
void saveSettings();
void applyBrightness();
// AUTH
void handleAuthState();
void handleAuthSetup();
void handleLogin();
void handleLogout();
void handleUpdateAccount();
void handleDashboard();
bool checkAuth();
// SOFTWARE UPDATE (OTA)
void handleSwUpdateUpload();
void handleSwUpdateUploadDone();
void handleBackupRestore();
void handleFactoryReset();
void loadSettings();
void advanceSlot();
void retreatSlot();
int  prevActiveSlot(int from);
void enterSlot(bool playTransition = true);
void drawCircuitTileFrame();
void resumeCircuitTileFromPriority();
void beginC2PCapture();
void finishC2PTransition(uint8_t tileTrans = 0, uint8_t tileSpd = 0);
void beginP2CCapture();
void finishP2CTransition();
void registerTouchTap();
void checkPendingTap();
void executeTouchAction(uint8_t action);
void toggleScreenPower();
bool ipTick();
void startIpShowFromTouch();
void npInit();
void rebuildNowPlayingBuf();
int  nextActiveSlot(int from);
void sortItems();
bool higherPriorityTileActive(uint8_t id);
void rebuildPriorityOrderFromEts2Order();
String priorityOrderToString();
bool npPriorityTick();
void weatherInit();
void weatherFetch();
void weatherDrawAtPos(int pos);
void currencyInit();
void currencyFetch();
void currencyDrawAtPos(int pos);
void drawCanvas();
String canvasBitmapToHex();
bool   canvasBitmapFromHex(const String& hex);
void   ssInit();
void   ssTick();
void   handleScreensaverSett();
String urlEncode(const String& s);

// STOPWATCH
void swInit();
bool swPriorityTick();
void handleSwStart();
void handleSwStop();
void handleSwState();

// TIMER
void timerInit();
bool timerPriorityTick();
void timerCheckExpiry();
void handleTimerSett();
void handleTimerStart();
void handleTimerPause();
void handleTimerState();

// POWER
void handleWifiInfo();
void handlePower();
void noteActivity();
void sleepWake();
void sleepEnter();
bool powerOffNow();

// ALARM
void alarmInit();
bool alarmPriorityTick();
void alarmStopRinging();
void handleAlarmSett();
void handleAlarmStop();
void handleAlarmState();

// DISPLAY
#define CLK_PIN      3
#define DATA_PIN     1
#define CS_PIN       2
#define MAX_DEVICES  4
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define BRIGHTNESS   4

// Brightness / Dim

uint8_t  curBrightness = BRIGHTNESS;
bool     dimAutoOn     = false;
uint8_t  dimFromH = 22, dimFromM = 0;
uint8_t  dimToH   =  7, dimToM   = 0;
uint8_t  dimLevel  = 1;

// BMP280
#define BMP_SDA 5
#define BMP_SCL 4

// TOUCH
#define TOUCH_PIN     6
#define TOUCH_HOLD_MS 2000
#define DOUBLE_TAP_WINDOW_MS 500

// BUZZER
#define BUZZER_PIN 12
// Which volume a sound answers to. NONE is the main volume, heard only in the
// main slider's own preview; the other four can be set apart.
#define BZ_CAT_NONE  0
#define BZ_CAT_NOTIF 1
#define BZ_CAT_AUTO  2
#define BZ_CAT_ALARM 3
#define BZ_CAT_TOUCH 4

// AP CONFIG
#define AP_SSID "Adrian's Octoglow"
#define AP_PASS ""

// TIME

// TIMEZONE TABLE

struct TzEntry { const char* iana; const char* posix; };
static const TzEntry TZ_TABLE[] PROGMEM = {
  {"Africa/Abidjan", "GMT0"},
  {"Africa/Accra", "GMT0"},
  {"Africa/Addis_Ababa", "EAT-3"},
  {"Africa/Algiers", "CET-1"},
  {"Africa/Cairo", "EET-2"},
  {"Africa/Casablanca", "WET0"},
  {"Africa/Johannesburg", "SAST-2"},
  {"Africa/Lagos", "WAT-1"},
  {"Africa/Nairobi", "EAT-3"},
  {"Africa/Tripoli", "EET-2"},
  {"Africa/Tunis", "CET-1"},
  {"America/Anchorage", "AKST9AKDT,M3.2.0,M11.1.0"},
  {"America/Argentina/Buenos_Aires", "ART3"},
  {"America/Bogota", "COT5"},
  {"America/Caracas", "VET4"},
  {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
  {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
  {"America/Halifax", "AST4ADT,M3.2.0,M11.1.0"},
  {"America/Lima", "PET5"},
  {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
  {"America/Mexico_City", "CST6CDT,M4.1.0,M10.5.0"},
  {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
  {"America/Phoenix", "MST7"},
  {"America/Santiago", "CLT4CLST,M10.2.6/24,M3.2.6/24"},
  {"America/Sao_Paulo", "BRT3BRST,M10.3.0/0,M2.3.0/0"},
  {"America/St_Johns", "NST3:30NDT,M3.2.0,M11.1.0"},
  {"America/Toronto", "EST5EDT,M3.2.0,M11.1.0"},
  {"America/Vancouver", "PST8PDT,M3.2.0,M11.1.0"},
  {"America/Winnipeg", "CST6CDT,M3.2.0,M11.1.0"},
  {"Asia/Almaty", "ALMT-6"},
  {"Asia/Amman", "EET-2EEST,M3.5.4/0,M10.5.5/1"},
  {"Asia/Baghdad", "AST-3"},
  {"Asia/Baku", "AZT-4"},
  {"Asia/Bangkok", "ICT-7"},
  {"Asia/Beirut", "EET-2EEST,M3.5.0/0,M10.5.0/0"},
  {"Asia/Colombo", "IST-5:30"},
  {"Asia/Dhaka", "BST-6"},
  {"Asia/Dubai", "GST-4"},
  {"Asia/Ho_Chi_Minh", "ICT-7"},
  {"Asia/Hong_Kong", "HKT-8"},
  {"Asia/Irkutsk", "IRKT-8"},
  {"Asia/Jakarta", "WIB-7"},
  {"Asia/Jerusalem", "IST-2IDT,M3.4.4/26,M10.5.0"},
  {"Asia/Kabul", "AFT-4:30"},
  {"Asia/Karachi", "PKT-5"},
  {"Asia/Kathmandu", "NPT-5:45"},
  {"Asia/Kolkata", "IST-5:30"},
  {"Asia/Krasnoyarsk", "KRAT-7"},
  {"Asia/Kuala_Lumpur", "MYT-8"},
  {"Asia/Kuwait", "AST-3"},
  {"Asia/Magadan", "MAGT-11"},
  {"Asia/Manila", "PHT-8"},
  {"Asia/Muscat", "GST-4"},
  {"Asia/Nicosia", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Asia/Novosibirsk", "NOVT-7"},
  {"Asia/Omsk", "OMST-6"},
  {"Asia/Qatar", "AST-3"},
  {"Asia/Riyadh", "AST-3"},
  {"Asia/Seoul", "KST-9"},
  {"Asia/Shanghai", "CST-8"},
  {"Asia/Singapore", "SGT-8"},
  {"Asia/Taipei", "CST-8"},
  {"Asia/Tashkent", "UZT-5"},
  {"Asia/Tbilisi", "GET-4"},
  {"Asia/Tehran", "IRST-3:30IRDT,80/0,264/0"},
  {"Asia/Tokyo", "JST-9"},
  {"Asia/Ulaanbaatar", "ULAT-8"},
  {"Asia/Vladivostok", "VLAT-10"},
  {"Asia/Yakutsk", "YAKT-9"},
  {"Asia/Yekaterinburg", "YEKT-5"},
  {"Asia/Yerevan", "AMT-4"},
  {"Atlantic/Azores", "AZOT1AZOST,M3.5.0/0,M10.5.0/1"},
  {"Atlantic/Cape_Verde", "CVT1"},
  {"Australia/Adelaide", "ACST-9:30ACDT,M10.1.0,M4.1.0/3"},
  {"Australia/Brisbane", "AEST-10"},
  {"Australia/Darwin", "ACST-9:30"},
  {"Australia/Hobart", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"Australia/Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"Australia/Perth", "AWST-8"},
  {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
  {"Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Belgrade", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Brussels", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Bucharest", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Budapest", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Copenhagen", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Dublin", "GMT0IST,M3.5.0/1,M10.5.0"},
  {"Europe/Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Istanbul", "TRT-3"},
  {"Europe/Kiev", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Kyiv", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Lisbon", "WET0WEST,M3.5.0/1,M10.5.0"},
  {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
  {"Europe/Luxembourg", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Minsk", "FET-3"},
  {"Europe/Moscow", "MSK-3"},
  {"Europe/Oslo", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Prague", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Riga", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Samara", "SAMT-4"},
  {"Europe/Sofia", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Stockholm", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Tallinn", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Vienna", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Vilnius", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"Europe/Warsaw", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Europe/Zurich", "CET-1CEST,M3.5.0,M10.5.0/3"},
  {"Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
  {"Pacific/Fiji", "FJT-12"},
  {"Pacific/Guam", "ChST-10"},
  {"Pacific/Honolulu", "HST10"},
  {"Pacific/Noumea", "NCT-11"},
  {"Pacific/Port_Moresby", "PGT-10"},
  {"UTC", "UTC0"},
  {nullptr, nullptr}
};
static const int TZ_TABLE_SIZE = 119;

const char* ianaToPostfix(const char* iana) {
  for (int i = 0; i < TZ_TABLE_SIZE; i++) {
    if (strcmp(iana, TZ_TABLE[i].iana) == 0)
      return TZ_TABLE[i].posix;
  }
  return nullptr;
}

void applyUtcOffsetFallback(int offset_sec) {
  int absOff = abs(offset_sec);
  int h = absOff / 3600;
  int m = (absOff % 3600) / 60;
  char posix[24];

  if (m == 0) {
    snprintf(posix, sizeof(posix), "UTC%s%d", offset_sec >= 0 ? "-" : "+", h);
  } else {
    snprintf(posix, sizeof(posix), "UTC%s%d:%02d", offset_sec >= 0 ? "-" : "+", h, m);
  }
  setenv("TZ", posix, 1);
  tzset();
  Serial.printf("[TZ] Fallback POSIX: %s\n", posix);
}

void autoDetectTimezone() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;

  http.begin("http://ip-api.com/json/?fields=timezone,offset");
  http.setTimeout(5000);
  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    StaticJsonDocument<128> doc;
    if (!deserializeJson(doc, body)) {
      const char* iana = doc["timezone"] | "";
      int offset_sec   = doc["offset"]   | 0;
      Serial.printf("[TZ] IP timezone: %s (offset %d)\n", iana, offset_sec);
      const char* posix = ianaToPostfix(iana);
      if (posix) {
        setenv("TZ", posix, 1);
        tzset();
        Serial.printf("[TZ] POSIX: %s\n", posix);
      } else {

        applyUtcOffsetFallback(offset_sec);
      }
    }
  } else {
    Serial.printf("[TZ] ip-api.com eroare %d - ramane TZ anterior\n", code);
  }
  http.end();
}

// NOW PLAYING

#define NOW_PLAYING_TIMEOUT_MS 20000

// CYCLE ITEMS
#define ITEM_HOUR        0
#define ITEM_DATE        1
#define ITEM_TEMP        2
#define ITEM_NOW_PLAYING 3
#define ITEM_WEATHER     4
#define ITEM_MEMENTO     5
#define ITEM_CANVAS      6
#define ITEM_NOTIF       7
#define ITEM_PRESSURE    8
#define ITEM_SCREENSAVER 9
#define ITEM_HOURWEEK    10
#define ITEM_CURRENCY    11
#define ITEM_YOUTUBE     12
#define NUM_ITEMS        12
#define CANVAS_COLS      32

// SCREEN SAVER 
#define SS_ANIM_RANDOM      0
#define SS_ANIM_PONG        1
#define SS_ANIM_FIREWORKS   2
#define SS_ANIM_EQ          3
#define SS_ANIM_COUNT       3

struct CycleItem {
  uint8_t  id;
  bool     enabled;
  uint16_t durationSec;
  uint8_t  order;
};

// GLOBALS
MD_Parola         P  = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
MD_MAX72XX        mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
bool gSuppressHwFlash = false;
void screenStreamTick();
static inline void mxCommit() {
  mx.update(gSuppressHwFlash ? MD_MAX72XX::OFF : MD_MAX72XX::ON);
  // The live copy for a subscribed Octoglow Connect; nothing when nobody is.
  screenStreamTick();
}

// The level the panel is running at right now, 0 meaning shut down. /getscreen
// reads it: a panel in shutdown keeps its frame in the buffer but lights none of it.
uint8_t mxPanelLevel = 0;

void applyMaxBrightness(uint8_t uiLevel) {
  mxPanelLevel = uiLevel;
  if (uiLevel == 0) {
    mx.control(MD_MAX72XX::SHUTDOWN, true);
  } else {
    mx.control(MD_MAX72XX::SHUTDOWN, false);
    uint8_t hwIntensity = uiLevel - 1;
    P.setIntensity(hwIntensity);
    mx.control(MD_MAX72XX::INTENSITY, hwIntensity);
  }
}
Adafruit_BMP280   bmp;
WebServer         server(80);
Preferences       prefs;

// AUTH GLOBALS
static char authUser[33]       = "";
static char authPassHash[65]   = "";
static bool authConfigured     = false;

// Cat timp ramane valabil cookie-ul de sesiune (secunde). 30 zile.
#define SESSION_TTL_SECONDS (30UL * 24UL * 3600UL)
// Prag folosit ca sa stim daca ceasul (NTP) e deja sincronizat la bord.
// (orice data reala de dupa 2023 e mai mare decat asta; time(nullptr) inainte
// de sincronizare NTP intoarce valori foarte mici, aproape de epoch 0)
#define TIME_LOOKS_VALID(t) ((t) > 1700000000UL)

static void sha256hex(const char* input, char outHex[65]) {
  uint8_t digest[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const unsigned char*)input, strlen(input));
  mbedtls_sha256_finish(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  for (int i = 0; i < 32; i++) sprintf(outHex + i * 2, "%02x", digest[i]);
  outHex[64] = '\0';
}

// HMAC-SHA256(payload) folosind authPassHash-ul curent ca si cheie.
// Nu se stocheaza nimic in plus pe ESP - cheia e deja cea salvata la login.
static void hmacHex(const char* payload, char outHex[65]) {
  uint8_t digest[32];
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, info, 1 /* HMAC */);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)authPassHash, strlen(authPassHash));
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)payload, strlen(payload));
  mbedtls_md_hmac_finish(&ctx, digest);
  mbedtls_md_free(&ctx);
  for (int i = 0; i < 32; i++) sprintf(outHex + i * 2, "%02x", digest[i]);
  outHex[64] = '\0';
}

// Comparatie in timp constant, ca sa nu scurgem informatie prin timing.
static bool constTimeEq(const char* a, const char* b) {
  size_t la = strlen(a), lb = strlen(b);
  if (la != lb) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < la; i++) diff |= (uint8_t)(a[i] ^ b[i]);
  return diff == 0;
}

static void userToHex(const char* user, char* out) {
  size_t n = strlen(user);
  for (size_t i = 0; i < n; i++) sprintf(out + i * 2, "%02x", (uint8_t)user[i]);
  out[n * 2] = '\0';
}

// Construieste un token de sesiune STATELESS: userHex.expiry.hmac
// ESP-ul nu retine nimic despre sesiuni active - orice token se poate
// re-verifica oricand (inclusiv dupa un reboot) doar cu authPassHash-ul
// curent, care e deja incarcat din NVS la boot in loadAuth().
static String makeSessionToken(const char* user) {
  char userHex[70];
  userToHex(user, userHex);
  time_t now = time(nullptr);
  unsigned long expiry = (unsigned long)now + SESSION_TTL_SECONDS;
  char payload[140];
  snprintf(payload, sizeof(payload), "%s.%lu", userHex, expiry);
  char sig[65];
  hmacHex(payload, sig);
  return String(payload) + "." + String(sig);
}

// Verifica un token din cookie fara nicio stare pe ESP.
// Intoarce numele userului daca e valid, altfel sir gol.
static String verifySessionToken(const String& tok) {
  if (!authConfigured || tok.length() == 0) return "";

  int d1 = tok.indexOf('.');
  int d2 = (d1 < 0) ? -1 : tok.indexOf('.', d1 + 1);
  if (d1 < 0 || d2 < 0) return "";

  String userHex   = tok.substring(0, d1);
  String expiryStr = tok.substring(d1 + 1, d2);
  String sig        = tok.substring(d2 + 1);
  if (userHex.length() == 0 || expiryStr.length() == 0 || sig.length() != 64) return "";

  // Fara ceas real (NTP nesincronizat) nu putem verifica expirarea in siguranta.
  time_t now = time(nullptr);
  if (!TIME_LOOKS_VALID(now)) return "";

  unsigned long expiry = strtoul(expiryStr.c_str(), nullptr, 10);
  if ((unsigned long)now > expiry) return "";

  char payload[140];
  snprintf(payload, sizeof(payload), "%s.%s", userHex.c_str(), expiryStr.c_str());
  char expectedSig[65];
  hmacHex(payload, expectedSig);
  if (!constTimeEq(sig.c_str(), expectedSig)) return "";

  // Decodifica userHex -> text si confirma ca e chiar contul configurat acum
  // (daca userul/parola s-au schimbat intre timp, cheia HMAC s-a schimbat si
  // deja am fi picat mai sus la verificarea semnaturii - asta e doar un dublu-check).
  String user;
  for (int i = 0; i + 1 < (int)userHex.length(); i += 2) {
    char byteStr[3] = { userHex[i], userHex[i + 1], 0 };
    user += (char)strtol(byteStr, nullptr, 16);
  }
  if (user != String(authUser)) return "";
  return user;
}

// AP CONFIG 

String getApSsid() {
  prefs.begin("apcfg", true);
  String s = prefs.getString("ssid", "");
  prefs.end();
  if (s.length() == 0) s = AP_SSID;
  return s;
}

String getApPass() {
  prefs.begin("apcfg", true);
  bool hasKey = prefs.isKey("pass");
  String p = hasKey ? prefs.getString("pass", "") : String(AP_PASS);
  prefs.end();
  return p;
}

static void loadAuth() {
  prefs.begin("auth", true);
  String u = prefs.getString("user", "");
  String h = prefs.getString("hash", "");
  prefs.end();
  if (u.length() > 0 && h.length() == 64) {
    strncpy(authUser,     u.c_str(), 32); authUser[32]   = '\0';
    strncpy(authPassHash, h.c_str(), 64); authPassHash[64] = '\0';
    authConfigured = true;
  }
}

static void saveAuth(const char* user, const char* passHash) {
  prefs.begin("auth", false);
  prefs.putString("user", user);
  prefs.putString("hash", passHash);
  prefs.end();
  strncpy(authUser,     user,     32); authUser[32]   = '\0';
  strncpy(authPassHash, passHash, 64); authPassHash[64] = '\0';
  authConfigured = true;
}

static String extractCookieToken() {
  String cookieHdr = server.header("Cookie");
  int idx = cookieHdr.indexOf("sc_tok=");
  if (idx < 0) return "";
  String tok = cookieHdr.substring(idx + 7);
  int end = tok.indexOf(';');
  if (end >= 0) tok = tok.substring(0, end);
  tok.trim();
  return tok;
}

static String getTokenUser() {
  String tok = extractCookieToken();
  if (tok.length() == 0) return "";
  return verifySessionToken(tok);
}

bool checkAuth() {
  if (provisionMode) return true;
  if (getTokenUser().length() == 0) {
    server.send(401, "text/plain", "Neautentificat");
    return false;
  }
  return true;
}

CycleItem items[NUM_ITEMS] = {
  { ITEM_HOUR,        true,  5,  0 },
  { ITEM_DATE,        true,  4,  1 },
  { ITEM_TEMP,        true,  4,  2 },
  { ITEM_NOW_PLAYING, false, 8,  3 },
  { ITEM_WEATHER,     false, 8,  4 },
  { ITEM_MEMENTO,     false, 8,  5 },
  { ITEM_CANVAS,      false, 8,  6 },
  { ITEM_PRESSURE,    false, 8,  7 },
  { ITEM_SCREENSAVER, false, 15,  8 },
  { ITEM_CURRENCY,    false, 8,  9 },
  { ITEM_YOUTUBE,     false, 8, 10 },
  { ITEM_HOURWEEK,    false, 5, 11 },
};

// TILES REMOVED FROM THE MANAGER
//
// One bit per tile: set means the row is not in the Tile Manager, and the tile
// can only come back through the "add" picker. tileHiddenMask is indexed by
// circuit tile id (ITEM_*), prioHiddenMask by PRIORITY_ID_*.
//
// PRIORITY_ID_NOWPLAYING's bit is deliberately never used: Now Playing is one
// tile that lives in either list, so its presence is the ITEM_NOW_PLAYING bit
// in tileHiddenMask no matter which list is showing it, and npIsPriority only
// says which.
uint16_t tileHiddenMask = 0;
uint8_t  prioHiddenMask = 0;

// Whether a circuit tile is in the Tile Manager at all - the same thing as "not
// offered by the add picker". A tile that is not there has nothing to draw on,
// so nothing needs fetching for it.
static inline bool tileInManager(uint8_t id) {
  return !(tileHiddenMask & (1u << id));
}

// A removed tile has no switch left to turn it off with, so removing one has to
// turn it off too. Kept here rather than only in the browser so a restored
// backup, or a client that predates this, cannot leave a tile cycling on the
// panel with no row to stop it from.
static void applyTileHiddenMask() {
  bool anyVisible = false;
  for (int i = 0; i < NUM_ITEMS; i++) {
    if (!(tileHiddenMask & (1u << items[i].id))) { anyVisible = true; break; }
  }
  // Emptying the circuit list entirely would leave the panel with nothing to
  // show and the interface with nothing to put back, so that mask is refused.
  if (!anyVisible) tileHiddenMask = 0;
  for (int i = 0; i < NUM_ITEMS; i++) {
    if (tileHiddenMask & (1u << items[i].id)) items[i].enabled = false;
  }
}

// CANVAS TILE

uint8_t canvasBitmap[CANVAS_COLS] = {0};

uint8_t       currentSlot      = 0;
unsigned long slotStartMs      = 0;

// What the panel is showing at this instant, for the Tile Manager's live
// marker. gLiveTileId is a circuit tile id (ITEM_*) or -1; gLivePrio is the
// wire name of the priority tile that took the frame, or "" when none did.
// Both are set from loop(), where the decision is actually made.
int8_t        gLiveTileId      = -1;
const char*   gLivePrio        = "";
bool          inScrollAnim     = false;
bool          inFadeOut        = false;
bool          inFadeIn         = false;
// Sentinel for "the BMP280 has never given us a usable reading".
#define TEMP_NO_READING (-999)
int           lastTemp         = TEMP_NO_READING;
unsigned long lastTempSampleMs = 0;
#define TEMP_SAMPLE_INTERVAL_MS 5000UL

// Value currently rendered on the temperature tile, so loop() can tell the
// "--" placeholder apart from a real number and swap it out in place.
int           gTempShownValue  = TEMP_NO_READING;

// BMP280 HEALTH
//
// The sensor used to be initialised exactly once in setup() and never checked
// again: if bmp.begin() lost the race at power-up, or the I2C bus later got
// wedged by a slave holding SDA low, every read failed forever and only a
// reboot brought the temperature back. These track the failure state so the
// sensor can be recovered while the clock keeps running.
bool          bmpOk              = false;
uint8_t       bmpFailStreak      = 0;
unsigned long bmpNextRetryMs     = 0;
unsigned long bmpRetryBackoffMs  = 3000UL;
unsigned long lastPressureReadMs = 0;
#define BMP_FAIL_LIMIT       3        // consecutive bad reads before re-init
#define BMP_RETRY_MIN_MS     3000UL
#define BMP_RETRY_MAX_MS     60000UL
#define BMP_READ_INTERVAL_MS 5000UL

unsigned long gLastStaticDrawMs = 0;
bool          gStaticDrawDone   = false;

// HARDWARE MONITOR - main loop instrumentation
//
// The ESP32 exposes no readable CPU-usage counter without enabling the FreeRTOS
// run-time-stats sdkconfig options (off by default in the Arduino core), so no
// usage percentage is reported - inventing one from the loop rate would only
// look precise. What is measured instead is genuinely useful on its own:
// loop() here is a non-blocking state machine, so its iteration rate and, more
// importantly, its worst iteration period tell you directly when something is
// blocking the main loop - which is exactly what makes the web UI sluggish.
unsigned long hwLastLoopUs   = 0;   // timestamp of the previous loop() entry
unsigned long hwLoopCount    = 0;   // iterations in the window being measured
unsigned long hwWindowStart  = 0;
unsigned long hwLoopMaxUs    = 0;   // worst iteration period in the current window
uint32_t      hwLoopsPerSec  = 0;   // last completed window
uint32_t      hwLoopMaxUsLast = 0;

// HTTP TIMING (diagnostic)
//
// server.handleClient() runs the whole request inline, so however long a
// response takes to push is time loop() is not animating the matrix. These
// record the worst offender since boot so it can be read back from /hwstate.
uint32_t      hwHttpMaxUs     = 0;    // longest single handleClient() call
char          hwHttpMaxUri[28] = "";  // which request that was
uint32_t      hwLoopMaxUsEver = 0;    // worst loop iteration since boot
uint32_t      hwPageFull      = 0;    // dashboard sent in full
uint32_t      hwPage304       = 0;    // dashboard answered with a 304
uint32_t      hwPageUs        = 0;    // longest page send
uint32_t      hwStateBuildUs  = 0;    // /state: time spent building + flushing

uint32_t      hwStateUs       = 0;    // longest /state build+send
bool          hwPeaksCleared  = false;

// The configured network name, kept in RAM. /state used to re-open the NVS
// "wifi" namespace on every request just to read this back, which is by far the
// most expensive thing that endpoint did.
String        gWifiSsidCached;

void refreshWifiSsidCache() {
  prefs.begin("wifi", true);
  gWifiSsidCached = prefs.getString("ssid", "");
  prefs.end();
}

// Boot is legitimately busy - WiFi association, NTP, the first weather and
// currency fetches - and those spikes swamped the numbers we actually care
// about. Drop everything recorded in the first 30 seconds, once.
static void hwClearPeaksAfterBoot() {
  if (hwPeaksCleared || millis() < 30000UL) return;
  hwPeaksCleared  = true;
  hwHttpMaxUs     = 0;
  hwLoopMaxUsEver = 0;
  hwPageUs        = 0;
  hwStateUs       = 0;
  hwStateBuildUs  = 0;
  hwHttpMaxUri[0] = '\0';
}

// The peaks above are worst-since-boot, which is the wrong window for chasing
// a stall that happens now and then: one bad moment hours ago hides every later
// one. This lets the interface start a fresh window right before reproducing it.
static void hwResetPeaks() {
  hwHttpMaxUs     = 0;
  hwLoopMaxUsEver = 0;
  hwPageUs        = 0;
  hwStateUs       = 0;
  hwStateBuildUs  = 0;
  hwHttpMaxUri[0] = '\0';
}

// Wraps every handleClient() call in loop() so the cost is attributed.
static void serviceHttp() {
  uint32_t t0 = micros();
  server.handleClient();
  uint32_t dt = micros() - t0;
  if (dt > hwHttpMaxUs) {
    hwHttpMaxUs = dt;
    String u = server.uri();
    // Left empty when there is no URI to name: the interface words that itself,
    // in the language it is showing, where "(fara cerere)" went out as-is.
    strncpy(hwHttpMaxUri, u.c_str(), sizeof(hwHttpMaxUri) - 1);
    hwHttpMaxUri[sizeof(hwHttpMaxUri) - 1] = '\0';
  }
}

// Called at the top of every loop() iteration; a few microseconds of work.
inline void hwMonitorTick() {
  unsigned long nowUs = micros();
  if (hwLastLoopUs) {
    unsigned long dt = nowUs - hwLastLoopUs;
    if (dt > hwLoopMaxUs) hwLoopMaxUs = dt;
    if (dt > hwLoopMaxUsEver) hwLoopMaxUsEver = (uint32_t)dt;
  }
  hwLastLoopUs = nowUs;
  hwLoopCount++;

  unsigned long nowMs = millis();
  if (nowMs - hwWindowStart >= 1000) {
    hwLoopsPerSec   = hwLoopCount;
    hwLoopMaxUsLast = hwLoopMaxUs;
    hwLoopCount   = 0;
    hwLoopMaxUs   = 0;
    hwWindowStart = nowMs;
  }
}



char     nowPlayingBuf[300]    = "";  // "artist - title", and both can be long
char     nowArtist[128]        = "";
char     nowTitle[160]         = "";
unsigned long lastNowPlayingMs = 0;
bool     nowPlayingActive      = false;

uint8_t  npDisplayMode         = 0;

// Adaptive icon: shows a "video" glyph instead of the music note when the
// detected playback source is a video player (set by Octoglow Connect).
bool     npAdaptiveIcon        = true;
bool     npIsVideoSource       = false;

// NOTIFICATIONS 
char     notifBuf[204]         = "";
bool     notifActive           = false;
// When set, this replaces the notification tile's usual glyph for the current
// message only, and is cleared as soon as that message ends.
const uint8_t* notifIconOverride = nullptr;
// True only while the "web interface accessed" alert is the message on screen.
bool     webAccessAlertActive  = false;
bool     hideTileIcons         = false;
bool     notifEnabled          = true;

bool     hideIconDate          = false;
bool     hideIconTemp          = false;
bool     hideIconReminder      = false;
bool     hideIconWeather       = false;
bool     hideIconNotif         = false;
bool     hideIconNowPlaying    = false;
bool     hideIconPressure      = false;
bool     hideIconCurrency      = false;
bool     hideIconYoutube       = false;
bool     hideIconWebAccess     = false;
bool     hideIconIp            = false;

// TILE ICON OVERRIDES (0 = Auto/Default -> tile's own built-in icon)
// Values 1..N select an entry from ICON_CATALOG (see below).
uint8_t  iconSelDate           = 0;
uint8_t  iconSelTemp           = 0;
uint8_t  iconSelReminder       = 0;
uint8_t  iconSelNotif          = 0;
uint8_t  iconSelNpMusic        = 0;
uint8_t  iconSelNpVideo        = 0;
uint8_t  iconSelPressure       = 0;
uint8_t  iconSelCurrency       = 0;
uint8_t  iconSelIp             = 0;
// Weather has several built-in icons (one per condition); each can be
// overridden independently since the tile switches icon automatically.
uint8_t  iconWxSunny           = 0;
uint8_t  iconWxCloud           = 0;
uint8_t  iconWxRain            = 0;
uint8_t  iconWxStorm           = 0;
uint8_t  iconWxSnow            = 0;
uint8_t  iconWxWind            = 0;
uint8_t  iconWxNight           = 0;

uint8_t  scrollType            = 0;

uint8_t  scrollTypeDate         = 0;
uint8_t  scrollTypeTemp         = 0;
uint8_t  scrollTypeReminder     = 0;
uint8_t  scrollTypeWeather      = 0;
uint8_t  scrollTypeNotif        = 0;
uint8_t  scrollTypeNowPlaying   = 0;
uint8_t  scrollTypePressure     = 0;
uint8_t  scrollTypeCurrency     = 0;
uint8_t  scrollTypeYoutube      = 0;
uint8_t  scrollTypeWebAccess    = 0;
uint8_t  scrollTypeStopwatch    = 0;
uint8_t  scrollTypeTimer        = 0;
uint8_t  scrollTypeIp           = 0;

// FONT TYPE (0 = Marymba/default, 1 = Tiko 3x7, 2 = Mako 4x7)
uint8_t  fontType               = 0;

uint8_t  fontTypeDate           = 0;
uint8_t  fontTypeTemp           = 0;
uint8_t  fontTypeReminder       = 0;
uint8_t  fontTypeWeather        = 0;
uint8_t  fontTypeNotif          = 0;
uint8_t  fontTypeNowPlaying     = 0;
uint8_t  fontTypePressure       = 0;
uint8_t  fontTypeCurrency       = 0;
uint8_t  fontTypeYoutube        = 0;
uint8_t  fontTypeWebAccess      = 0;
uint8_t  fontTypeStopwatch      = 0;
uint8_t  fontTypeTimer          = 0;
uint8_t  fontTypeIp             = 0;

// TILE TRANSITION 
uint8_t  tileTransGlobal       = 0;
uint8_t  tileTransHour         = 0;
uint8_t  tileTransDate         = 0;
uint8_t  tileTransTemp         = 0;
uint8_t  tileTransNp           = 0;
uint8_t  tileTransWx           = 0;
uint8_t  tileTransRem          = 0;
uint8_t  tileTransCanvas       = 0;
uint8_t  tileTransPress        = 0;
uint8_t  tileTransSs           = 0;
uint8_t  tileTransCurr         = 0;
uint8_t  tileTransYt           = 0;
uint8_t  tileTransHw           = 0;
uint8_t  tileTransWeb          = 0;
uint8_t  tileTransNotif        = 0;
uint8_t  tileTransEts2         = 0;
uint8_t  tileTransSw           = 0;
uint8_t  tileTransTmr          = 0;
uint8_t  tileTransAlarm        = 0;
uint8_t  tileTransIp           = 0;
uint8_t  tileTransC2P          = 0; // Circuit Tile -> Priority Tile
uint8_t  tileTransP2C          = 0; // Priority Tile -> Circuit Tile

// TILE TRANSITION SPEED
//
// How fast a transition plays, in percent of the pace its effect was drawn
// at: 100 is unchanged, 200 twice as fast, 50 half as fast. Every tileTrans*
// setting above has its own, so whichever setting picked an effect also sets
// its pace; the global slider re-applies one value to all of them. Kept in one
// NVS blob ("trSpeeds") rather than 21 more keys.
#define TRSPD_DEFAULT  100
#define TRSPD_MIN       50
#define TRSPD_MAX      250
#define TRSPD_HOUR      0
#define TRSPD_DATE      1
#define TRSPD_TEMP      2
#define TRSPD_NP        3
#define TRSPD_WX        4
#define TRSPD_REM       5
#define TRSPD_CANVAS    6
#define TRSPD_PRESS     7
#define TRSPD_SS        8
#define TRSPD_CURR      9
#define TRSPD_YT       10
#define TRSPD_HW       11
#define TRSPD_WEB      12
#define TRSPD_NOTIF    13
#define TRSPD_ETS2     14
#define TRSPD_SW       15
#define TRSPD_TMR      16
#define TRSPD_ALARM    17
#define TRSPD_IP       18
#define TRSPD_C2P      19
#define TRSPD_P2C      20
#define TRSPD_COUNT    21

// Suffix of the /state field and the POST argument, "tileTransSpd" + key - the
// same names the effects carry after "tileTrans".
static const char* const TRSPD_KEYS[TRSPD_COUNT] = {
  "Hour", "Date", "Temp", "Np", "Wx", "Rem", "Canvas", "Press", "Ss", "Curr",
  "Yt", "Hw", "Web", "Notif", "Ets2", "Sw", "Tmr", "Alarm", "Ip", "C2P", "P2C"
};

uint8_t  tileTransSpeed        = TRSPD_DEFAULT;  // the global slider
uint8_t  tileTransSpd[TRSPD_COUNT];              // one per setting, filled by loadSettings()

static inline bool scrollIconInBuffer(uint8_t st) { return st == 2 || st == 3; }
static inline bool scrollIsWrap(uint8_t st)       { return st == 1 || st == 3; }

// Key, drawn for the "web interface accessed" alert.
// SCROLL SPEED
//
// Milliseconds between one column step and the next - lower is faster. Every
// tile that can scroll has its own, defaulting to the constant above so nothing
// changes until the user moves a slider.
#define SCROLL_SPEED_DEFAULT 40
#define SCROLL_SPEED_MIN     10
#define SCROLL_SPEED_MAX     120
#define SPD_DATE        0
#define SPD_TEMP        1
#define SPD_REMINDER    2
#define SPD_WEATHER     3
#define SPD_NOTIF       4
#define SPD_NOWPLAYING  5
#define SPD_PRESSURE    6
#define SPD_CURRENCY    7
#define SPD_YOUTUBE     8
#define SPD_WEBACCESS   9
#define SPD_STOPWATCH  10
#define SPD_IP         11
#define SPD_TIMER      12
#define SCROLL_SPEED_COUNT 13

// Suffix used for the NVS key, the /state field and the POST argument, so the
// three stay in step by construction instead of by hand.
static const char* const SPD_KEYS[SCROLL_SPEED_COUNT] = {
  "Date", "Temp", "Reminder", "Weather", "Notif", "NowPlaying", "Pressure",
  "Currency", "Youtube", "WebAccess", "Stopwatch", "Ip", "Timer"
};

uint8_t scrollSpeed = SCROLL_SPEED_DEFAULT;                 // the global slider
uint8_t scrollSpeedTile[SCROLL_SPEED_COUNT] = {
  SCROLL_SPEED_DEFAULT, SCROLL_SPEED_DEFAULT, SCROLL_SPEED_DEFAULT, SCROLL_SPEED_DEFAULT,
  SCROLL_SPEED_DEFAULT, SCROLL_SPEED_DEFAULT, SCROLL_SPEED_DEFAULT, SCROLL_SPEED_DEFAULT,
  SCROLL_SPEED_DEFAULT, SCROLL_SPEED_DEFAULT, SCROLL_SPEED_DEFAULT, SCROLL_SPEED_DEFAULT,
  SCROLL_SPEED_DEFAULT
};

static inline unsigned long spd(uint8_t i) { return (unsigned long)scrollSpeedTile[i]; }

// The alert borrows the notification tile's renderer but has its own look, so
// these pick which set of settings applies to the message currently on screen.
static inline uint8_t notifScrollType() {
  return webAccessAlertActive ? scrollTypeWebAccess : scrollTypeNotif;
}
static inline uint8_t notifFontType() {
  return webAccessAlertActive ? fontTypeWebAccess : fontTypeNotif;
}
static inline bool notifHideIcon() {
  return webAccessAlertActive ? hideIconWebAccess : hideIconNotif;
}
static inline unsigned long notifScrollSpeed() {
  return spd(webAccessAlertActive ? SPD_WEBACCESS : SPD_NOTIF);
}

const uint8_t keyIcon[8] = {
  0b00000000,
  0b01100000,
  0b10010000,
  0b10011111,
  0b10010011,
  0b01100010,
  0b00000000,
  0b00000000
};

const uint8_t notifIcon[8] = {
  0b00000000,
  0b01111110,
  0b11000011,
  0b10100101,
  0b10011001,
  0b10000001,
  0b01111110,
  0b00000000
};

// SHOW IP ADDRESS (Touch Sensor Action)
const uint8_t ipIcon[8] = {
  0b00000001,
  0b00000011,
  0b00001011,
  0b00011011,
  0b01011011,
  0b11011011,
  0b11011011,
  0b00000000
};

// EURO TRUCK SIMULATOR 2

bool     ets2Active            = false;
bool     ets2Enabled           = true;
bool     ets2OrderFirst        = false;

// PRIORITY TILES generic precedence order

#define PRIORITY_ID_NOTIF      0
#define PRIORITY_ID_ETS2       1
#define PRIORITY_ID_NOWPLAYING 2
#define PRIORITY_ID_STOPWATCH  3
#define PRIORITY_ID_TIMER      4
#define PRIORITY_ID_WEB        5
#define PRIORITY_ID_ALARM      6
#define NUM_PRIORITY_IDS       7
// The alarm starts out at the top: it is the one tile that goes off on its own,
// while you are somewhere else, and an alarm ringing behind another tile is an
// alarm you can hear but cannot read. The order stays the user's to change.
uint8_t  priorityOrder[NUM_PRIORITY_IDS] = { PRIORITY_ID_ALARM, PRIORITY_ID_NOTIF, PRIORITY_ID_ETS2, PRIORITY_ID_NOWPLAYING, PRIORITY_ID_STOPWATCH, PRIORITY_ID_TIMER, PRIORITY_ID_WEB };

// ALARM
//
// Three independent alarms. Each has a time, the weekdays it repeats on, a tone
// and a switch. One rings when the clock enters its minute on one of its days,
// and goes on ringing until the touch sensor or the interface stops it.
#define ALARM_COUNT 3

// Days are one bit each, bit 0 = Monday through bit 6 = Sunday. struct tm counts
// from Sunday, so the two are bridged in exactly one place: ALARM_WDAY_BIT.
#define ALARM_WDAY_BIT(tm_wday) ((uint8_t)(1 << (((tm_wday) + 6) % 7)))
#define ALARM_ALL_DAYS 0x7F

// Ringing forever would hold the panel and the buzzer for the rest of the day
// when nobody is there to stop it. After this it gives up on its own; the alarm
// still rings at its next slot.
#define ALARM_MAX_RING_MS 900000UL
// Silence between one pass of the tone and the next.
#define ALARM_TONE_GAP_MS 700UL

struct AlarmCfg {
  uint8_t hour;
  uint8_t minute;
  uint8_t days;
  bool    enabled;
  // Whether this slot holds an alarm at all. Absent is not the same as switched
  // off: an alarm that is off is one you keep and will want again, an absent
  // one is a card that never appears.
  bool    present;
  char    tone[16];
};

AlarmCfg alarms[ALARM_COUNT] = {
  { 7, 30, 0x1F,           false, false, "classic" },
  { 8,  0, 0x60,           false, false, "chimes"  },
  { 9,  0, ALARM_ALL_DAYS, false, false, "morning" },
};

bool          alarmRinging     = false;
int8_t        alarmRingingIdx  = -1;
unsigned long alarmRingStartMs = 0;
bool          alarmWasActive   = false;
unsigned long alarmToneNextMs  = 0;

// One latch per alarm, held for as long as the clock is inside that alarm's
// minute. loop() runs thousands of times a second, so without it an alarm would
// re-fire on every pass; clearing it the moment the minute passes is also what
// lets the same alarm ring again tomorrow, and again next week.
bool          alarmFired[ALARM_COUNT] = { false, false, false };
bool     webAccessEnabled = true;
bool     nowPlayingIsPriority  = false;

bool     npPriorityWasActive   = false;
bool     timerWasActive        = false;

int      ets2Speed             = 0;
unsigned long lastEts2Ms       = 0;
#define ETS2_TIMEOUT_MS 5000

static uint8_t  ets2ColBuf[64];
static int      ets2ColCount  = 0;
static int      ets2LastSpeed = -1;

// STOPWATCH  variables
bool          swRunning       = false;
bool          swWasActive     = false;
unsigned long swStartMs       = 0;
unsigned long swElapsedMs     = 0;

// TIMER variables
char          eventSoundTimer[16] = "calm";

uint32_t      timerDurationSec      = 300;
uint32_t      timerRemainingSnapMs  = 300000UL;
bool          timerRunning          = false;
bool          timerFinished         = false;
bool          timerBlinkOn          = true;
unsigned long timerBlinkLastMs      = 0;
unsigned long timerEndMs            = 0;
uint8_t       timerPreset           = 0;

void timerStart() {
  if (timerRunning || timerFinished) return;
  if (timerRemainingSnapMs == 0) timerRemainingSnapMs = (unsigned long)timerDurationSec * 1000UL;
  timerEndMs  = millis() + timerRemainingSnapMs;
  timerRunning = true;
}

void timerPause() {
  if (!timerRunning) return;
  long rem = (long)(timerEndMs - millis());
  timerRemainingSnapMs = (rem > 0) ? (unsigned long)rem : 0;
  timerRunning = false;
}

void timerDismiss() {
  timerFinished = false;
  timerRemainingSnapMs = (unsigned long)timerDurationSec * 1000UL;
}

uint32_t timerGetRemainingSec() {
  if (timerRunning) {
    long rem = (long)(timerEndMs - millis());
    if (rem < 0) rem = 0;
    return (uint32_t)(rem / 1000);
  }
  return (uint32_t)(timerRemainingSnapMs / 1000);
}

void timerCheckExpiry() {
  if (!timerRunning) return;
  if (timerGetRemainingSec() == 0) {
    timerRunning = false;
    timerRemainingSnapMs = 0;
    timerFinished = true;
    timerBlinkOn = true;
    timerBlinkLastMs = millis();
    nbPlayPreset(eventSoundTimer, BZ_CAT_AUTO);
  }
}

void timerFormatText(char* out, size_t outSize) {
  uint32_t rem = timerGetRemainingSec();
  uint32_t h = rem / 3600, m = (rem % 3600) / 60, s = rem % 60;
  if (h > 0) snprintf(out, outSize, "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
  else       snprintf(out, outSize, "%02u:%02u", (unsigned)m, (unsigned)s);
}

unsigned long swGetElapsedMs() {
  return swRunning ? (swElapsedMs + (millis() - swStartMs)) : swElapsedMs;
}

void swFormatFull(char* out, size_t outSize, unsigned long ms) {
  unsigned long h  = ms / 3600000UL;
  unsigned long m  = (ms / 60000UL) % 60UL;
  unsigned long s  = (ms / 1000UL) % 60UL;
  unsigned long cs = (ms / 10UL) % 100UL;
  snprintf(out, outSize, "%02lu:%02lu:%02lu:%02lu", h, m, s, cs);
}

void swFormatAdaptive(char* out, size_t outSize, unsigned long ms) {
  unsigned long h = ms / 3600000UL;
  unsigned long m = (ms / 60000UL) % 60UL;
  unsigned long s = (ms / 1000UL) % 60UL;
  if (h > 0) {
    snprintf(out, outSize, "%02lu:%02lu:%02lu", h, m, s);
  } else {
    snprintf(out, outSize, "%02lu:%02lu", m, s);
  }
}

// WEATHER
char     weatherCity[64]       = "Baia Mare";
char     weatherApiKey[64]     = "";

// OpenWeather keys are 32 hex characters. Rather than pin it to that and break
// the day they change format, this only insists on something a key could be:
// printable ASCII, no spaces, and short enough to store whole. That is already
// enough to reject the bullet characters a masked field used to post back.
static bool wxKeyLooksValid(const String& k) {
  if (k.length() < 8 || k.length() >= sizeof(weatherApiKey)) return false;
  for (unsigned int i = 0; i < k.length(); i++) {
    char c = k[i];
    if (c <= ' ' || c > '~') return false;
  }
  return true;
}
char     weatherLang[8]        = "en";
float    weatherLat            = 47.6575f;
float    weatherLon            = 23.5689f;
char     weatherBuf[128]       = "";
// Which parts of the reading the tile draws. Stored as flags rather than an
// index so the seven combinations need no lookup table, and so the default (all
// three) is the same value the tile behaved as before this existed.
#define WX_SHOW_TEMP 0x01
#define WX_SHOW_HUM  0x02
#define WX_SHOW_DESC 0x04
#define WX_SHOW_ALL  (WX_SHOW_TEMP | WX_SHOW_HUM | WX_SHOW_DESC)
uint8_t  wxPreset              = WX_SHOW_ALL;

float    weatherTempC          = -999;
int      weatherHumidity       = -1;
char     weatherDesc[48]       = "";
unsigned long lastWeatherFetch = 0;
bool     weatherValid          = false;
#define WEATHER_FETCH_INTERVAL_MS  600000UL

const uint8_t sunnyIcon[8] = {
  0b00011000,
  0b01000010,
  0b00011000,
  0b10111101,
  0b10111101,
  0b00011000,
  0b01000010,
  0b00011000
};

const uint8_t cloudIcon[8] = {
  0b00000000,
  0b00111000,
  0b01000110,
  0b10000001,
  0b10000001,
  0b01111110,
  0b00000000,
  0b00000000
};

const uint8_t cloudRainIcon[8] = {
  0b00000000,
  0b00111000,
  0b01000110,
  0b10000001,
  0b10000001,
  0b01111110,
  0b01010100,
  0b00101010
};

const uint8_t cloudLightningIcon[8] = {
  0b00111000,
  0b01000110,
  0b10000001,
  0b10000001,
  0b01111110,
  0b00100100,
  0b01101100,
  0b01001000
};

const uint8_t cloudSnowIcon[8] = {
  0b00111000,
  0b01000110,
  0b10000001,
  0b10010001,
  0b00111000,
  0b01010010,
  0b11100111,
  0b01000010
};

const uint8_t windIcon[8] = {
  0b00001100,
  0b00010010,
  0b00000010,
  0b11111100,
  0b00000000,
  0b11111000,
  0b00000100,
  0b00001000
};

char     weatherCondition[32]  = "";

const uint8_t moonIcon[8] = {
  0b00111100,
  0b01110000,
  0b11100000,
  0b11100000,
  0b11100000,
  0b11100000,
  0b01110000,
  0b00111100
};

// THE CLOCK, READ SAFELY
//
// getLocalTime(info, 0) is not a reliable way to ask what time it is. The core
// writes it as
//
//     uint32_t start = millis();
//     while ((millis() - start) <= ms) { ... return true; }
//     return false;
//
// so with ms == 0 the body runs only if the millisecond counter has not ticked
// between those two millis() calls. That gap is a couple of microseconds out of
// every thousand, which makes roughly one call in several hundred report failure
// on a clock that is perfectly set - and callers that bail on failure then skip
// a draw for no reason at all.
//
// Giving it a non-zero timeout would trade that for a different problem: on a
// clock that has never been set it spends a delay(10) per attempt, and these run
// inside loop(), where the display advances one column per iteration.
//
// This is the same test with neither behaviour: read the clock once, and decide
// from the year whether it has been set.
static bool readLocalTime(struct tm& out) {
  time_t t = time(nullptr);
  localtime_r(&t, &out);
  return out.tm_year > (2016 - 1900);
}

static bool isNight() {
  struct tm ti;
  if (!readLocalTime(ti)) return false;
  int h = ti.tm_hour;
  return (h >= 21 || h < 6);
}

const uint8_t* getWeatherIcon() {
  if (strcasecmp(weatherCondition, "Clear") == 0)
    return isNight() ? resolveTileIcon(iconWxNight, moonIcon) : resolveTileIcon(iconWxSunny, sunnyIcon);
  if (strcasecmp(weatherCondition, "Clouds") == 0)
    return resolveTileIcon(iconWxCloud, cloudIcon);
  if (strcasecmp(weatherCondition, "Rain") == 0 ||
      strcasecmp(weatherCondition, "Drizzle") == 0)
    return resolveTileIcon(iconWxRain, cloudRainIcon);
  if (strcasecmp(weatherCondition, "Thunderstorm") == 0)
    return resolveTileIcon(iconWxStorm, cloudLightningIcon);
  if (strcasecmp(weatherCondition, "Snow") == 0)
    return resolveTileIcon(iconWxSnow, cloudSnowIcon);
  if (strcasecmp(weatherCondition, "Wind") == 0   ||
      strcasecmp(weatherCondition, "Mist") == 0   ||
      strcasecmp(weatherCondition, "Fog") == 0    ||
      strcasecmp(weatherCondition, "Haze") == 0   ||
      strcasecmp(weatherCondition, "Smoke") == 0  ||
      strcasecmp(weatherCondition, "Dust") == 0   ||
      strcasecmp(weatherCondition, "Sand") == 0   ||
      strcasecmp(weatherCondition, "Ash") == 0    ||
      strcasecmp(weatherCondition, "Squall") == 0 ||
      strcasecmp(weatherCondition, "Tornado") == 0)
    return resolveTileIcon(iconWxWind, windIcon);

  return resolveTileIcon(iconWxCloud, cloudIcon);
}

uint8_t  tempUnit              = 0;

uint8_t  hourFormat            = 0;
bool     hourLeadingZero       = false;   // 2:30 -> 02:30

// HOUR AND WEEKDAY TILE
//
// Its own copy of the hour options, so it can run 12h next to a 24h Hour tile,
// plus the one-row bar that goes with the weekday.
#define HW_BAR_STATIC    0   // a fixed underline
#define HW_BAR_SECONDS   1   // fills up across the minute
#define HW_BAR_BELOW     0
#define HW_BAR_ABOVE     1
uint8_t  hwFormat              = 0;       // 0 = 24h, 1 = 12h
bool     hwLeadZero            = false;
uint8_t  hwBarMode             = HW_BAR_SECONDS;
uint8_t  hwBarPos              = HW_BAR_BELOW;
bool     hwSwap                = false;   // day on the left, time on the right

// DEFAULT START MODE
//
// Which radio the clock comes up on after a power cut. Choosing AP also keeps
// the tile loop running there: the access point stops being a repair hatch and
// becomes how the clock normally lives, for anyone with no router to put it on.
// Purely an interface preference: with it off, the web interface leaves out the
// rows it would otherwise show dimmed - a tile that needs the internet while the
// access point is up, an event sound whose tile is switched off, the update
// check in AP mode. Kept on the device so it travels with the user rather than
// with the browser.
bool     showGrayedContent     = true;

// Marks the row the panel is showing right now, in the Tile Manager.
bool     liveTileHighlight     = true;

// Dark mode and the interface language. Neither changes anything the device
// does - they sit next to accentColor purely so the interface looks the same
// from whichever browser the user opens it in. The browser keeps its own
// localStorage copy as well, to paint the right theme before /state arrives.
bool     webUiDark             = true;
char     webUiLang[3]          = "ro";
// Forma ramelor de iconita din interfata: 0 cerc, 1 patrat rotunjit, 2 dala,
// 3 squircle.
uint8_t  webUiShape            = 0;
// Custom interface colours as "bgDark,cardDark,bgLight,cardLight": each slot is
// empty for the theme's own colour, or #rrggbb. Stored as sent - the browser
// works the rest of the surfaces out from them.
char     webUiColors[32]       = "";

#define START_MODE_WIFI 0
#define START_MODE_AP   1
uint8_t  defaultStartMode      = START_MODE_WIFI;

// True while the panel belongs to the AP badge rather than to the tiles. Every
// early return in loop() goes through this, so the one exception - AP picked as
// the start mode - is stated once instead of thirty-four times.
static inline bool tileLoopSuspended() {
  return provisionMode && defaultStartMode != START_MODE_AP;
}

// NETWORK TIME
//
// On by default: NTP keeps the clock right on its own. Switched off, nothing
// writes the RTC but the user - and since there is no battery behind it, the
// time is genuinely lost at every restart and has to be typed in again.
bool     netTimeSync           = true;

// With no valid clock verifySessionToken() refuses to judge a cookie's expiry,
// so every session is rejected and the page that sets the time is the one page
// nobody can reach. Boot manual mode on a plausible date instead of 1970.
#define MANUAL_TIME_FALLBACK 1767225600UL   // 2026-01-01 00:00:00

// SNTP would otherwise walk over a hand-set clock at its next poll.
static void stopNtp() {
  if (esp_sntp_enabled()) esp_sntp_stop();
}

static void seedManualClock() {
  if (TIME_LOOKS_VALID(time(nullptr))) return;
  struct timeval tv;
  tv.tv_sec  = (time_t)MANUAL_TIME_FALLBACK;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}

// mktime() reads the fields as local time in whatever TZ is active and
// localtime_r() turns them back into the same fields, so what the user types is
// exactly what the tiles show - no offset arithmetic on either side.
static bool applyManualTime(int y, int mo, int d, int h, int mi, int s) {
  if (y < 2024 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31 ||
      h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59) return false;
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_year  = y - 1900;
  t.tm_mon   = mo - 1;
  t.tm_mday  = d;
  t.tm_hour  = h;
  t.tm_min   = mi;
  t.tm_sec   = s;
  t.tm_isdst = -1;
  time_t e = mktime(&t);
  if (e <= 0) return false;
  // mktime normalises out-of-range days (32 Jan becomes 1 Feb); reject that
  // rather than silently storing a date the user did not ask for.
  if (t.tm_mday != d || t.tm_mon != mo - 1) return false;
  struct timeval tv;
  tv.tv_sec  = e;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  return true;
}

uint8_t  dateFormat            = 2;
uint8_t  dateLang              = 0;  // 0 = English, 1 = Romanian (used only by dateFormat 4 "Mon 11")
char     customDateFmt[24]     = "DD/MM/YYYY";  // used only by dateFormat 5 (custom)

enum NpState {
  NP_SHOW_START,
  NP_PAUSE_BEFORE_RIGHT,
  NP_SCROLL_RIGHT,
  NP_PAUSE_AFTER_RIGHT,
  NP_SCROLL_LEFT,
  NP_PAUSE_BEFORE_NEXT,
  NP_PAUSE_SHORT,
  NP_SCROLL_WRAP
};
NpState       npState         = NP_SHOW_START;
unsigned long npPauseStartMs  = 0;
#define NP_PAUSE_MS 1000

#define SCROLL_WRAP_PASSES 2
static int npWrapPass = 0;

NpState       wxState         = NP_SHOW_START;
unsigned long wxPauseStartMs  = 0;
static int    wxWrapPass      = 0;

NpState       notifState       = NP_SHOW_START;
unsigned long notifPauseMs     = 0;
// Room for the whole text in the widest font (6 columns a letter with the
// gap) plus the icon. At 512 anything past ~85 characters was cut off.
#define NOTIF_COL_MAX 1250
static uint8_t  notifColBuf[NOTIF_COL_MAX];
static int      notifColCount  = 0;
static int      notifScrollPos = 0;
static unsigned long notifLastScrollMs = 0;
static int      notifWrapPass  = 0;
static uint8_t  wxColBuf[512];
static int      wxColCount  = 0;
static int      wxScrollPos = 0;
static unsigned long wxLastScrollMs = 0;

// SHOW IP ADDRESS (Touch Sensor Action)
char          ipBuf[24]          = "";
bool          ipShowActive       = false;
NpState       ipState            = NP_SHOW_START;
unsigned long ipPauseMs          = 0;
static uint8_t  ipColBuf[128];
static int      ipColCount       = 0;
static int      ipScrollPos      = 0;
static unsigned long ipLastScrollMs = 0;
static int      ipWrapPass       = 0;

bool          provisionMode    = false;

// POWER
//
// sleepActive is the panel being dark while everything else carries on: the
// web server answers, Octoglow Connect still gets through, and anything that
// arrives counts as a reason to light back up. Power Off is a different thing
// entirely and does not live in a flag - it is a deep sleep the device only
// comes out of by being held.
bool          sleepActive      = false;
bool          autoSleepOn      = false;
uint32_t      autoSleepSec     = 1800;
unsigned long lastActivityMs   = 0;
// Long enough that a hand passing the pad is not a power button, short enough
// that nobody thinks the clock is broken.
#define POWER_ON_HOLD_MS 1500

bool          buzzerOn         = true;
uint8_t       buzzerVolume     = 80;
// A level per category. The main volume sets all four at once, the way the
// global transition speed sets every tile; they can then be moved apart.
uint8_t       buzzVolNotif     = 80;
uint8_t       buzzVolAuto      = 80;
uint8_t       buzzVolAlarm     = 80;
uint8_t       buzzVolTouch     = 80;
char          buzzerPreset[16] = "calm";
char          eventSoundTile[16]  = "calm";
char          eventSoundWifi[16]  = "urgent";
char          eventSoundNotif[16] = "soft";
char          eventSoundWeb[16]   = "soft";
char          eventSoundEts2[16]  = "urgent";
char          eventSoundTouch[16] = "soft";
char          accentColor[8]      = "#d0bcff";
#define ETS2_SPEED_LIMIT 130
bool          ets2SpeedAlertFired = false;
bool          wifiWasConnected    = true;
bool          touchActive      = false;
unsigned long touchStartMs     = 0;
bool          touchShortFired  = false;
uint8_t       touchTapAction       = 8; // default: Show IP Address
uint8_t       touchDoubleTapAction = 0;
bool          tapPending           = false;
unsigned long lastTapReleaseMs     = 0;
bool          touchScreenOff       = false;

char displayBuf[32];

// BUZZER OUTPUT
//
// The notes are played by the RMT peripheral, from a task of their own.
//
// Timing: a note has to end on time, and the next one start on time, even when
// loop() is busy. A tile transition, a slow request or a WiFi reconnect holds
// loop() for hundreds of milliseconds; a note that only loop() could stop
// droned on through all of it, and a tune that only loop() could advance fell
// apart. buzzTask() walks the queue on its own clock, and each note is a
// counted run of periods that the hardware stops by itself.
//
// Volume: a passive buzzer has no volume line, and narrowing the pulse to make
// it quieter pushes the sound into its upper harmonics, so everything played
// quietly came out at a different pitch. Instead the note keeps its
// half-and-half square shape and its high half is chopped by a 40 kHz carrier.
// The buzzer cannot follow the carrier, only its average, so the level scales
// with the carrier duty and the pitch stays where it was.
//
// Shape: rewriting that duty while a note sounds lets it die away like a
// struck bell or a plucked string instead of stopping dead, which is most of
// the difference between a chime and a beep.
#include "soc/rmt_struct.h"
#include "soc/gpio_struct.h"
#include "soc/gpio_sig_map.h"

#define BUZZ_TICK_HZ       1000000UL  // symbol durations in microseconds
#define BUZZ_CARRIER_HZ    40000UL
#define BUZZ_CARRIER_TICKS 2000       // one carrier period at the 80 MHz RMT group clock
// Never 0 ticks high or low: the register reads 0 as 65536, and the HAL asserts.
#define BUZZ_TICKS_MIN     4
#define BUZZ_TICKS_MAX     1960

// How a note's level moves while it sounds.
#define BZ_ENV_FLAT   0  // on, then off: the original beeps
#define BZ_ENV_PLUCK  1  // struck and quickly gone
#define BZ_ENV_BELL   2  // struck and left to ring
#define BZ_ENV_SWELL  3  // rises, then fades

static portMUX_TYPE nbMux = portMUX_INITIALIZER_UNLOCKED;
static bool    buzzReady        = false;
static int     buzzCarrierTicks = -2;  // -1 carrier off, -2 not known
static int8_t  buzzRmtCh        = -1;  // the RMT channel on the pin, -1 unknown

static uint8_t buzzCatVolume(uint8_t cat) {
  uint8_t v;
  switch (cat) {
    case BZ_CAT_NOTIF: v = buzzVolNotif; break;
    case BZ_CAT_AUTO:  v = buzzVolAuto;  break;
    case BZ_CAT_ALARM: v = buzzVolAlarm; break;
    case BZ_CAT_TOUCH: v = buzzVolTouch; break;
    default:           v = buzzerVolume; break;
  }
  return v > 100 ? 100 : v;
}

static bool buzzBegin() {
  if (buzzReady) return true;
  if (!rmtInit(BUZZER_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, BUZZ_TICK_HZ)) return false;
  buzzReady        = true;
  buzzCarrierTicks = -2;
  // The Arduino layer does not say which channel it took; the GPIO matrix does.
  uint32_t sel = GPIO.func_out_sel_cfg[BUZZER_PIN].func_sel;
  buzzRmtCh = (sel >= RMT_SIG_OUT0_IDX && sel <= RMT_SIG_OUT3_IDX) ? (int8_t)(sel - RMT_SIG_OUT0_IDX) : -1;
  return true;
}

// Between notes, through the driver. It reaches the pin when the next note starts.
static void buzzCarrierSet(int ticks) {
  if (ticks == buzzCarrierTicks) return;
  bool ok;
  if (ticks < 0) {
    ok = rmtSetCarrier(BUZZER_PIN, false, false, 0, 0.5f);
  } else {
    // The third argument is polarity_active_low: false puts the carrier on the
    // high half of the note. The half tick keeps the driver's truncation from
    // landing one below.
    ok = rmtSetCarrier(BUZZER_PIN, true, false, BUZZ_CARRIER_HZ, (ticks + 0.5f) / BUZZ_CARRIER_TICKS);
  }
  buzzCarrierTicks = ok ? ticks : -2;
}

// While a note sounds. The driver has no call for this, so the duty goes
// straight into the register, followed by the channel's synchronisation bit
// that carries configuration across to the RMT clock domain - the same bit the
// driver sets to start or stop a transmission. The critical section keeps the
// RMT interrupt, which lives on this core, off CONF0 while it is written.
static void buzzCarrierLive(int ticks) {
  if (ticks == buzzCarrierTicks) return;
  if (buzzRmtCh < 0 || buzzCarrierTicks < 0) { buzzCarrierSet(ticks); return; }
  portENTER_CRITICAL(&nbMux);
  RMT.chncarrier_duty[buzzRmtCh].val = ((uint32_t)ticks << 16) | (uint32_t)(BUZZ_CARRIER_TICKS - ticks);
  RMT.chnconf0[buzzRmtCh].conf_update_chn = 1;
  portEXIT_CRITICAL(&nbMux);
  buzzCarrierTicks = ticks;
}

static int buzzTicksFor(float amp) {
  int t = (int)(amp * BUZZ_CARRIER_TICKS + 0.5f);
  if (t < BUZZ_TICKS_MIN) t = BUZZ_TICKS_MIN;
  if (t > BUZZ_TICKS_MAX) t = BUZZ_TICKS_MAX;
  return t;
}

// The level t ms into a note dur ms long, 1 being the note's full volume.
static float buzzEnvelope(uint8_t env, uint32_t t, uint16_t dur) {
  float x = dur ? (float)t / dur : 1.0f;
  if (x > 1.0f) x = 1.0f;
  switch (env) {
    case BZ_ENV_PLUCK: return expf(-3.5f * x);  // ends near 3 %
    case BZ_ENV_BELL:  return expf(-1.8f * x);  // still ringing at 16 %
    case BZ_ENV_SWELL: return x < 0.35f ? 0.4f + 0.6f * x / 0.35f : 1.0f - 0.85f * (x - 0.35f) / 0.65f;
    default:           return 1.0f;
  }
}

// An empty looping write cuts off whatever is sounding; the pin drops to its
// idle level, low.
static void buzzSilence() {
  if (buzzReady) rmtWriteLooping(BUZZER_PIN, NULL, 0);
}

// Starts a note. Returns its full-volume carrier amplitude, or a negative
// number when nothing is sounding: a rest, or a category turned down to 0.
static float buzzStartNote(uint16_t freq, uint16_t durMs, uint8_t env, uint8_t cat) {
  uint8_t vol = buzzCatVolume(cat);
  if (freq == 0 || durMs == 0 || vol == 0) { buzzSilence(); return -1.0f; }
  if (!buzzBegin()) return -1.0f;
  // Loudness falls away far more slowly than the number does, so the setting
  // is squared: 50 drives the buzzer at a quarter, about -12 dB.
  float peak = (vol / 100.0f) * (vol / 100.0f);
  // A flat note at full volume needs no carrier: that is the plain square wave
  // tone() used to play. A shaped note keeps it on so its level can move.
  if (env == BZ_ENV_FLAT && vol >= 100) buzzCarrierSet(-1);
  else                                  buzzCarrierSet(buzzTicksFor(peak * buzzEnvelope(env, 0, durMs)));
  if (freq < 20)    freq = 20;     // half a period has to fit in 15 bits of µs
  if (freq > 20000) freq = 20000;
  uint32_t period = (BUZZ_TICK_HZ + freq / 2) / freq;
  static rmt_data_t sym;
  sym.level0 = 1; sym.duration0 = period / 2;
  sym.level1 = 0; sym.duration1 = period - period / 2;
  uint32_t cycles = ((uint32_t)durMs * 1000UL + period / 2) / period;
  // One cycle would go out as a one-shot write, and the next note could not
  // cut that short. Starting a repeated write cancels the one before it.
  if (cycles < 2) cycles = 2;
  rmtWriteRepeated(BUZZER_PIN, &sym, 1, cycles);
  return peak;
}

// BUZZER QUEUE
//
// loop() and the web handlers queue notes, buzzTask() plays them, and nbMux
// covers everything the two share. Each note carries the category whose volume
// it plays at; nbEnqCat is the one being queued, and only loop() touches it.
#define NB_QUEUE_MAX 64
struct NbNote { uint16_t freq; uint16_t dur; uint8_t env; uint8_t cat; };
static NbNote  nbQueue[NB_QUEUE_MAX];
static uint8_t nbHead     = 0;
static uint8_t nbTail     = 0;
static bool    nbStopReq  = false;
static bool    nbNoteOn   = false;  // a note, or the gap after it, is under way
static bool    nbShutReq  = false;
static volatile bool nbShutDone = false;
static uint8_t nbEnqCat   = BZ_CAT_NONE;
static TaskHandle_t buzzTaskH = NULL;

static void buzzWake() {
  if (buzzTaskH) xTaskNotifyGive(buzzTaskH);
}

static void nbEnqueueEnv(uint16_t freq, uint16_t dur, uint8_t env) {
  portENTER_CRITICAL(&nbMux);
  uint8_t next = (nbTail + 1) % NB_QUEUE_MAX;
  if (next != nbHead) {
    nbQueue[nbTail] = { freq, dur, env, nbEnqCat };
    nbTail = next;
  }
  portEXIT_CRITICAL(&nbMux);
  buzzWake();
}

static void nbEnqueue(uint16_t freq, uint16_t dur) {
  nbEnqueueEnv(freq, dur, BZ_ENV_FLAT);
}

// Empties the queue and cuts off whatever is sounding.
static void nbStop() {
  portENTER_CRITICAL(&nbMux);
  nbHead = nbTail = 0;
  nbStopReq = true;
  portEXIT_CRITICAL(&nbMux);
  buzzWake();
}

// Anything queued, sounding, or about to be cut off.
static bool nbBusy() {
  portENTER_CRITICAL(&nbMux);
  bool busy = nbHead != nbTail || nbNoteOn || nbStopReq;
  portEXIT_CRITICAL(&nbMux);
  return busy;
}

// A note holds the queue for its length plus a 5 ms gap. A shaped note is
// revisited every 3 ms to move its level; otherwise the task sleeps until the
// note is over or something new is queued.
static void buzzTask(void*) {
  unsigned long start = 0;
  uint16_t dur = 0, span = 0;
  uint8_t  env = BZ_ENV_FLAT;
  float    peak = -1.0f;
  for (;;) {
    unsigned long now = millis();
    NbNote n = { 0, 0, 0, 0 };
    bool shut, stop = false, have = false;
    portENTER_CRITICAL(&nbMux);
    shut = nbShutReq;
    if (nbStopReq) { nbStopReq = false; nbNoteOn = false; stop = true; }
    if (nbNoteOn && now - start >= span) nbNoteOn = false;
    if (!shut && !nbNoteOn && nbHead != nbTail) {
      n = nbQueue[nbHead];
      nbHead = (nbHead + 1) % NB_QUEUE_MAX;
      nbNoteOn = have = true;
    }
    bool idle = !nbNoteOn;
    portEXIT_CRITICAL(&nbMux);

    if (shut) {
      buzzSilence();
      if (buzzReady) { rmtDeinit(BUZZER_PIN); buzzReady = false; }
      nbShutDone = true;
      vTaskSuspend(NULL);
    }
    if (have) {
      start = now; dur = n.dur; span = n.dur + 5; env = n.env;
      peak = buzzStartNote(n.freq, n.dur, n.env, n.cat);
    } else if (stop) {
      peak = -1.0f;
      buzzSilence();
    }
    bool shaping = peak >= 0 && env != BZ_ENV_FLAT && now - start < dur;
    if (shaping) buzzCarrierLive(buzzTicksFor(peak * buzzEnvelope(env, now - start, dur)));

    TickType_t wait = portMAX_DELAY;
    if (shaping) {
      wait = pdMS_TO_TICKS(3);
    } else if (!idle) {
      unsigned long el = millis() - start;
      wait = el < span ? pdMS_TO_TICKS(span - el) : 1;
      if (wait == 0) wait = 1;
    }
    ulTaskNotifyTake(pdTRUE, wait);
  }
}

// Silent and released: for boot and for sleep, where the pin must sit low.
static void buzzOff() {
  if (buzzTaskH) {
    portENTER_CRITICAL(&nbMux);
    nbHead = nbTail = 0;
    nbShutReq = true;
    portEXIT_CRITICAL(&nbMux);
    buzzWake();
    for (int i = 0; i < 50 && !nbShutDone; i++) delay(2);
  }
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

// Above loop() in priority and on loop()'s core: a busy pass cannot hold a
// note back, and the RMT interrupt the task installs lands on the core that
// buzzCarrierLive() shuts it out of.
static void buzzStartTask() {
  if (buzzTaskH) return;
  xTaskCreatePinnedToCore(buzzTask, "buzzer", 5120, NULL, 2, &buzzTaskH, xPortGetCoreID());
}

// cat is whose volume the preset plays at: BZ_CAT_NOTIF, BZ_CAT_AUTO or
// BZ_CAT_TOUCH.
static void nbPlayPreset(const char* id, uint8_t cat) {
  if (!buzzerOn) return;
  if (strcmp(id, "none") == 0) return;
  nbStop();
  nbEnqCat = cat;
  if (strcmp(id, "calm") == 0) {
    nbEnqueue(1000, 80);
  } else if (strcmp(id, "loud") == 0) {
    nbEnqueue(1500, 130);
  } else if (strcmp(id, "urgent") == 0) {
    for (int i = 0; i < 3; i++) { nbEnqueue(1800, 60); nbEnqueue(0, 15); }
  } else if (strcmp(id, "soft") == 0) {
    nbEnqueue(650, 60);
  } else if (strcmp(id, "double") == 0) {
    nbEnqueue(1200, 70); nbEnqueue(0, 20); nbEnqueue(1200, 70);
  } else if (strcmp(id, "triple") == 0) {
    for (int i = 0; i < 3; i++) { nbEnqueue(1200, 60); nbEnqueue(0, 20); }
  } else if (strcmp(id, "chime") == 0) {
    nbEnqueue(523, 90); nbEnqueue(0, 5); nbEnqueue(659, 90); nbEnqueue(0, 5); nbEnqueue(784, 130);
  } else if (strcmp(id, "bell") == 0) {
    nbEnqueue(1568, 60); nbEnqueue(0, 10); nbEnqueue(1318, 70); nbEnqueue(0, 10); nbEnqueue(1046, 160);
  } else if (strcmp(id, "doorbell") == 0) {
    nbEnqueue(784, 160); nbEnqueue(0, 10); nbEnqueue(659, 220);
  } else if (strcmp(id, "xylophone") == 0) {
    int notes[5] = {659, 784, 880, 988, 1175};
    for (int i = 0; i < 5; i++) { nbEnqueue(notes[i], 55); nbEnqueue(0, 5); }
  } else if (strcmp(id, "harp") == 0) {
    for (int f = 1400; f >= 500; f -= 90) { nbEnqueue(f, 22); nbEnqueue(0, 5); }
  } else if (strcmp(id, "marimba") == 0) {
    nbEnqueue(440, 110); nbEnqueue(0, 5); nbEnqueue(220, 160);
  } else if (strcmp(id, "crystal") == 0) {
    nbEnqueue(1568, 45); nbEnqueue(0, 8); nbEnqueue(1976, 45); nbEnqueue(0, 8); nbEnqueue(2349, 90);
  } else if (strcmp(id, "wave") == 0) {
    for (int f = 400; f <= 1000; f += 60) { nbEnqueue(f, 15); }
    for (int f = 1000; f >= 400; f -= 60) { nbEnqueue(f, 15); }
  } else if (strcmp(id, "lullaby") == 0) {
    int notes[4] = {784, 659, 587, 523};
    for (int i = 0; i < 4; i++) { nbEnqueue(notes[i], 140); nbEnqueue(0, 10); }
  } else if (strcmp(id, "pingpong") == 0) {
    for (int i = 0; i < 4; i++) { nbEnqueue(i % 2 == 0 ? 1200 : 800, 55); nbEnqueue(0, 10); }
  } else if (strcmp(id, "sos") == 0) {
    for (int i = 0; i < 3; i++) { nbEnqueue(1500, 60); nbEnqueue(0, 60); }
    nbEnqueue(0, 80);
    for (int i = 0; i < 3; i++) { nbEnqueue(1500, 180); nbEnqueue(0, 60); }
    nbEnqueue(0, 80);
    for (int i = 0; i < 3; i++) { nbEnqueue(1500, 60); nbEnqueue(0, 60); }
  } else if (strcmp(id, "siren") == 0) {
    for (int f = 400; f <= 1800; f += 70) nbEnqueue(f, 18);
    for (int f = 1800; f >= 400; f -= 70) nbEnqueue(f, 18);
  } else if (strcmp(id, "klaxon") == 0) {
    for (int i = 0; i < 6; i++) { nbEnqueue(i % 2 == 0 ? 1000 : 1400, 45); nbEnqueue(0, 10); }
  } else if (strcmp(id, "laser") == 0) {
    for (int f = 2500; f >= 300; f -= 100) nbEnqueue(f, 10);
  } else if (strcmp(id, "robot") == 0) {
    int notes[5] = {900, 1300, 700, 1600, 1000};
    for (int i = 0; i < 5; i++) { nbEnqueue(notes[i], 40); nbEnqueue(0, 15); }
  } else if (strcmp(id, "fanfare") == 0) {
    nbEnqueue(523, 90);  nbEnqueue(0, 10);
    nbEnqueue(659, 90);  nbEnqueue(0, 10);
    nbEnqueue(784, 90);  nbEnqueue(0, 10);
    nbEnqueue(1046, 220);
  } else if (strcmp(id, "powerdown") == 0) {
    for (int f = 1200; f >= 200; f -= 50) nbEnqueue(f, 14);
  } else if (strcmp(id, "powerup") == 0) {
    for (int f = 200; f <= 1200; f += 50) nbEnqueue(f, 14);
  } else if (strcmp(id, "heartbeat") == 0) {
    nbEnqueue(150, 80); nbEnqueue(0, 40); nbEnqueue(150, 60); nbEnqueue(0, 220);
    nbEnqueue(150, 80); nbEnqueue(0, 40); nbEnqueue(150, 60);
  } else if (strcmp(id, "scifi") == 0) {
    for (int i = 0; i < 2; i++) {
      for (int f = 600; f <= 1600; f += 80) nbEnqueue(f, 12);
      for (int f = 1600; f >= 600; f -= 80) nbEnqueue(f, 12);
    }
  } else if (strcmp(id, "arcade") == 0) {
    int notes[4] = {800, 1000, 1200, 1600};
    for (int i = 0; i < 4; i++) { nbEnqueue(notes[i], 45); nbEnqueue(0, 8); }
  } else if (strcmp(id, "zen") == 0) {
    nbEnqueue(220, 400);
  } else if (strcmp(id, "bubble") == 0) {
    int notes[5] = {600, 900, 1200, 1500, 1800};
    for (int i = 0; i < 5; i++) { nbEnqueue(notes[i], 30); nbEnqueue(0, 10); }
  } else if (strcmp(id, "whistle") == 0) {
    for (int f = 800; f <= 2000; f += 40) nbEnqueue(f, 12);
  // The shaped ones. Each note is struck and left to fade (PLUCK, BELL) or
  // swells and falls away (SWELL), and they keep to the pentatonic and major
  // intervals, in the octave the buzzer sings best in.
  } else if (strcmp(id, "glass") == 0) {
    nbEnqueueEnv(2637, 120, BZ_ENV_BELL); nbEnqueueEnv(1976, 650, BZ_ENV_BELL);
  } else if (strcmp(id, "musicbox") == 0) {
    nbEnqueueEnv(1047, 110, BZ_ENV_PLUCK); nbEnqueueEnv(1319, 110, BZ_ENV_PLUCK);
    nbEnqueueEnv(1568, 110, BZ_ENV_PLUCK); nbEnqueueEnv(2093, 480, BZ_ENV_BELL);
  } else if (strcmp(id, "kalimba") == 0) {
    nbEnqueueEnv(1568, 150, BZ_ENV_PLUCK); nbEnqueueEnv(1319, 150, BZ_ENV_PLUCK);
    nbEnqueueEnv(1047, 380, BZ_ENV_PLUCK);
  } else if (strcmp(id, "dingdong") == 0) {
    nbEnqueueEnv(1319, 420, BZ_ENV_BELL); nbEnqueueEnv(1047, 750, BZ_ENV_BELL);
  } else if (strcmp(id, "droplet") == 0) {
    nbEnqueueEnv(1568, 50, BZ_ENV_PLUCK); nbEnqueueEnv(2349, 300, BZ_ENV_PLUCK);
  } else if (strcmp(id, "success") == 0) {
    nbEnqueueEnv(784, 90, BZ_ENV_PLUCK);   nbEnqueueEnv(1047, 90, BZ_ENV_PLUCK);
    nbEnqueueEnv(1319, 90, BZ_ENV_PLUCK);  nbEnqueueEnv(1568, 450, BZ_ENV_BELL);
  } else if (strcmp(id, "softping") == 0) {
    nbEnqueueEnv(1760, 800, BZ_ENV_BELL);
  } else if (strcmp(id, "aurora") == 0) {
    nbEnqueueEnv(1047, 280, BZ_ENV_SWELL); nbEnqueueEnv(1568, 560, BZ_ENV_BELL);
  } else if (strcmp(id, "harmony") == 0) {
    nbEnqueueEnv(880, 80, BZ_ENV_PLUCK);   nbEnqueueEnv(1109, 80, BZ_ENV_PLUCK);
    nbEnqueueEnv(1319, 80, BZ_ENV_PLUCK);  nbEnqueueEnv(1760, 550, BZ_ENV_BELL);
  } else if (strcmp(id, "twinkle") == 0) {
    nbEnqueueEnv(2637, 70, BZ_ENV_PLUCK);  nbEnqueueEnv(2093, 70, BZ_ENV_PLUCK);
    nbEnqueueEnv(1568, 70, BZ_ENV_PLUCK);  nbEnqueueEnv(2637, 380, BZ_ENV_BELL);
  } else if (strcmp(id, "boldalert") == 0) {
    for (int i = 0; i < 4; i++) { nbEnqueue(300, 70); nbEnqueue(0, 10); nbEnqueue(1200, 70); nbEnqueue(0, 10); }
  } else {
    nbEnqueue(1000, 80);
  }
}

// ALARM TONES
//
// The presets above are single notifications - a note or two and done. An alarm
// has to carry on until somebody stops it, so each tone here is a phrase that
// alarmToneTick() re-queues for as long as the alarm rings. They go through the
// same nbEnqueue()/buzzTask() pair as the presets; only the repeat is new.
// NB_QUEUE_MAX leaves room for 63 entries, rests included, and no phrase below
// goes past 30.
static void alarmEnqueueTone(const char* id) {
  nbEnqCat = BZ_CAT_ALARM;
  if (strcmp(id, "chimes") == 0) {
    // A four-note chime figure, twice over.
    static const uint16_t n[4] = { 659, 784, 587, 392 };
    for (int r = 0; r < 2; r++)
      for (int i = 0; i < 4; i++) { nbEnqueue(n[i], 260); nbEnqueue(0, 40); }
  } else if (strcmp(id, "morning") == 0) {
    static const uint16_t n[6] = { 523, 659, 784, 659, 784, 1046 };
    for (int r = 0; r < 2; r++)
      for (int i = 0; i < 6; i++) { nbEnqueue(n[i], 190); nbEnqueue(0, 30); }
  } else if (strcmp(id, "radar") == 0) {
    for (int r = 0; r < 4; r++) {
      nbEnqueue(880, 90);   nbEnqueue(0, 30);
      nbEnqueue(1046, 90);  nbEnqueue(0, 30);
      nbEnqueue(1318, 150); nbEnqueue(0, 200);
    }
  } else if (strcmp(id, "insistent") == 0) {
    for (int r = 0; r < 6; r++) { nbEnqueue(2000, 90); nbEnqueue(0, 60); nbEnqueue(1600, 90); nbEnqueue(0, 160); }
  } else if (strcmp(id, "siren") == 0) {
    for (int f = 500; f <= 1500; f += 125) nbEnqueue(f, 60);
    for (int f = 1500; f >= 500; f -= 125) nbEnqueue(f, 60);
  } else if (strcmp(id, "melody") == 0) {
    static const uint16_t n[8] = { 523, 587, 659, 523, 523, 587, 659, 523 };
    for (int i = 0; i < 8; i++) { nbEnqueue(n[i], 220); nbEnqueue(0, 30); }
  } else if (strcmp(id, "sunrise") == 0) {
    // A slow climb up the pentatonic scale, every step left to ring.
    static const uint16_t n[6] = { 1047, 1175, 1319, 1568, 1760, 2093 };
    for (int i = 0; i < 6; i++) nbEnqueueEnv(n[i], i == 5 ? 1100 : 420, BZ_ENV_BELL);
  } else if (strcmp(id, "musicbox") == 0) {
    static const uint16_t n[16] = { 1319, 1568, 2093, 1568, 1760, 1568, 1319, 1047,
                                    1175, 1319, 1568, 1319, 1175, 1047, 1175, 1047 };
    for (int i = 0; i < 15; i++) nbEnqueueEnv(n[i], 230, BZ_ENV_PLUCK);
    nbEnqueueEnv(n[15], 800, BZ_ENV_BELL);
  } else if (strcmp(id, "arpeggio") == 0) {
    static const uint16_t n[8] = { 1047, 1319, 1568, 2093, 2637, 2093, 1568, 1319 };
    for (int r = 0; r < 2; r++)
      for (int i = 0; i < 8; i++) nbEnqueueEnv(n[i], 140, BZ_ENV_PLUCK);
    nbEnqueueEnv(1047, 700, BZ_ENV_BELL);
  } else if (strcmp(id, "birds") == 0) {
    // Short plucked chirps with room between them - a trill, a call, a flutter.
    static const uint16_t a[3] = { 2093, 2349, 2637 };
    static const uint16_t b[4] = { 1976, 2349, 2794, 2349 };
    for (int r = 0; r < 2; r++) {
      for (int i = 0; i < 3; i++) nbEnqueueEnv(a[i], 20, BZ_ENV_PLUCK);
      nbEnqueue(0, 140);
      nbEnqueueEnv(2637, 24, BZ_ENV_PLUCK); nbEnqueueEnv(2349, 40, BZ_ENV_PLUCK);
      nbEnqueue(0, 320);
      for (int i = 0; i < 4; i++) nbEnqueueEnv(b[i], 18, BZ_ENV_PLUCK);
      nbEnqueue(0, 480);
    }
  } else if (strcmp(id, "zen") == 0) {
    nbEnqueueEnv(880, 1000, BZ_ENV_SWELL);  nbEnqueue(0, 120);
    nbEnqueueEnv(1319, 1000, BZ_ENV_SWELL); nbEnqueue(0, 120);
    nbEnqueueEnv(1175, 1000, BZ_ENV_SWELL); nbEnqueue(0, 120);
    nbEnqueueEnv(880, 1400, BZ_ENV_SWELL);
  } else if (strcmp(id, "windchime") == 0) {
    nbEnqueueEnv(2637, 420, BZ_ENV_BELL); nbEnqueueEnv(2093, 320, BZ_ENV_BELL); nbEnqueue(0, 120);
    nbEnqueueEnv(2349, 520, BZ_ENV_BELL); nbEnqueueEnv(1760, 360, BZ_ENV_BELL); nbEnqueue(0, 200);
    nbEnqueueEnv(1568, 620, BZ_ENV_BELL); nbEnqueueEnv(2637, 260, BZ_ENV_BELL);
    nbEnqueueEnv(2093, 800, BZ_ENV_BELL);
  } else if (strcmp(id, "gentle") == 0) {
    // Three soft four-note figures, each a step higher, then one bell.
    static const uint16_t n[3][4] = { { 1047, 1319, 1568, 1319 },
                                      { 1175, 1397, 1760, 1397 },
                                      { 1319, 1568, 2093, 1568 } };
    for (int p = 0; p < 3; p++) {
      for (int i = 0; i < 4; i++) nbEnqueueEnv(n[p][i], 260, BZ_ENV_PLUCK);
      nbEnqueue(0, 200);
    }
    nbEnqueueEnv(2093, 900, BZ_ENV_BELL);
  } else {
    // "classic" and anything unrecognised: the two-tone alarm everybody knows.
    for (int r = 0; r < 7; r++) { nbEnqueue(1200, 130); nbEnqueue(0, 60); nbEnqueue(900, 130); nbEnqueue(0, 60); }
  }
}

// Minutes from now until the next alarm that is switched on, or -1 if there is
// none. It walks forward one day at a time instead of reasoning about the
// calendar, so midnight, the end of the week and "already gone for today" are
// all the same case. Eight steps rather than seven: an alarm set for one day a
// week is exactly a week away when today is that day and the time has passed.
static int alarmMinutesUntilNext(int* outIdx) {
  if (outIdx) *outIdx = -1;
  struct tm ti;
  if (!readLocalTime(ti)) return -1;
  int nowMin = ti.tm_hour * 60 + ti.tm_min;
  int best = -1, bestIdx = -1;
  for (int i = 0; i < ALARM_COUNT; i++) {
    if (!alarms[i].present || !alarms[i].enabled || alarms[i].days == 0) continue;
    int at = alarms[i].hour * 60 + alarms[i].minute;
    for (int d = 0; d <= 7; d++) {
      if (!(alarms[i].days & ALARM_WDAY_BIT((ti.tm_wday + d) % 7))) continue;
      int delta = d * 1440 + at - nowMin;
      if (delta < 0) continue;
      // The minute it is ringing in, or has already rung in, is behind us.
      if (delta == 0 && alarmFired[i]) continue;
      if (best < 0 || delta < best) { best = delta; bestIdx = i; }
      break;
    }
  }
  if (outIdx) *outIdx = bestIdx;
  return best;
}

static void alarmStartRinging(int idx) {
  // An alarm nobody can see is half an alarm.
  sleepWake();
  alarmRinging     = true;
  alarmRingingIdx  = (int8_t)idx;
  alarmRingStartMs = millis();
  alarmToneNextMs  = 0;
  if (buzzerOn) {
    nbStop();
    alarmEnqueueTone(alarms[idx].tone);
  }
}

void alarmStopRinging() {
  if (!alarmRinging) return;
  alarmRinging    = false;
  alarmRingingIdx = -1;
  alarmToneNextMs = 0;
  nbStop();
}

// Feeds the buzzer another pass of the tone once the last one has drained. The
// gap is measured from the moment the queue empties, not from when it started,
// so a long tone and a short one both get the same pause between repeats.
static void alarmToneTick() {
  if (!alarmRinging || !buzzerOn) return;
  if (nbBusy()) { alarmToneNextMs = 0; return; }
  unsigned long now = millis();
  if (alarmToneNextMs == 0) { alarmToneNextMs = now + ALARM_TONE_GAP_MS; return; }
  if ((long)(now - alarmToneNextMs) < 0) return;
  alarmToneNextMs = 0;
  if (alarmRingingIdx >= 0) alarmEnqueueTone(alarms[alarmRingingIdx].tone);
}

// Called every pass through loop(). Everything it decides is a function of the
// wall clock, so a reboot changes nothing: the alarms come back from NVS and the
// first tick after the clock is set picks up where it left off.
static void alarmTick() {
  // Taken out of the Tile Manager means switched off, the same as it does for
  // every other priority tile - and a tile that is gone cannot be silenced.
  if (prioHiddenMask & (1u << PRIORITY_ID_ALARM)) {
    if (alarmRinging) alarmStopRinging();
    return;
  }
  if (alarmRinging && (millis() - alarmRingStartMs >= ALARM_MAX_RING_MS)) alarmStopRinging();

  // Four times a second is ample for something that acts on whole minutes, and
  // it keeps even the cheap clock read off the critical path.
  static unsigned long lastCheckMs = 0;
  unsigned long nowMs = millis();
  if (nowMs - lastCheckMs < 250) return;
  lastCheckMs = nowMs;

  struct tm ti;
  if (!readLocalTime(ti)) return;

  uint8_t todayBit = ALARM_WDAY_BIT(ti.tm_wday);
  for (int i = 0; i < ALARM_COUNT; i++) {
    bool match = alarms[i].present &&
                 alarms[i].enabled &&
                 (alarms[i].days & todayBit) &&
                 ti.tm_hour == alarms[i].hour &&
                 ti.tm_min  == alarms[i].minute;
    if (!match)        { alarmFired[i] = false; continue; }
    if (alarmFired[i]) continue;
    // Two alarms set to the same minute both latch here, but only the first in
    // index order starts ringing: the second would otherwise queue up behind it
    // and ask to be stopped a second time for the same minute.
    alarmFired[i] = true;
    if (!alarmRinging) alarmStartRinging(i);
  }
}

// BUZZER BLOCKING (for non-touch events)

void beepSwitch() {
  if (!buzzerOn) return;
  nbPlayPreset(eventSoundTile, BZ_CAT_AUTO);
}

void beepTouch() {
  if (!buzzerOn) return;
  nbPlayPreset(eventSoundTouch, BZ_CAT_TOUCH);
}

// CUSTOM FONT
const uint8_t FONT[][5] = {
  {0x3E, 0x51, 0x49, 0x45, 0x3E},
  {0x00, 0x42, 0x7F, 0x40, 0x00},
  {0x42, 0x61, 0x51, 0x49, 0x46},
  {0x21, 0x41, 0x45, 0x4B, 0x31},
  {0x18, 0x14, 0x12, 0x7F, 0x10},
  {0x27, 0x45, 0x45, 0x45, 0x39},
  {0x3C, 0x4A, 0x49, 0x49, 0x30},
  {0x01, 0x71, 0x09, 0x05, 0x03},
  {0x36, 0x49, 0x49, 0x49, 0x36},
  {0x06, 0x49, 0x49, 0x29, 0x1E},
};
const uint8_t CHAR_C[5]     = {0x3E, 0x41, 0x41, 0x41, 0x22};
const uint8_t CHAR_F[5]     = {0x7F, 0x09, 0x09, 0x01, 0x01};
const uint8_t CHAR_DEG[5]   = {0x06, 0x09, 0x09, 0x06, 0x00};
const uint8_t CHAR_SPACE[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
const uint8_t CHAR_COLON[2] = {0x36, 0x36};
const uint8_t CHAR_DOT[1]   = {0x40};
// Font mode constants (moved up from below so they're visible to appendGlyphAuto)
#define FONT_MODE_DEFAULT 0
#define FONT_MODE_TIKO    1
#define FONT_MODE_MAKO    2
#define FONT_MODE_COUNT   3
#define TIKO_MAX_COLS 5

// Tiko and Mako are both drawn from a glyph table of our own rather than the
// library font, so every place that asked "is it Tiko?" really means "is it
// one of ours?" - and then has to ask the right table.
uint8_t tikoGetChar(char ch, uint8_t size, uint8_t* buf);
uint8_t makoGetChar(char ch, uint8_t size, uint8_t* buf);
static inline bool isPixelFont(uint8_t m) {
  return m == FONT_MODE_TIKO || m == FONT_MODE_MAKO;
}
static inline uint8_t pixGetChar(uint8_t m, char ch, uint8_t size, uint8_t* buf) {
  return (m == FONT_MODE_MAKO) ? makoGetChar(ch, size, buf) : tikoGetChar(ch, size, buf);
}

static void appendGlyphAuto(uint8_t* buf, int& count, int maxCount, char ch, uint8_t fontMode = FONT_MODE_DEFAULT) {
  if (isPixelFont(fontMode)) {
    uint8_t tmp[TIKO_MAX_COLS];
    uint8_t n = pixGetChar(fontMode, ch, sizeof(tmp), tmp);
    if (n == 0) return;
    for (int i = 0; i < n && count < maxCount; i++) buf[count++] = tmp[i];
    if (count < maxCount) buf[count++] = 0x00;
    return;
  }
  if (ch >= '0' && ch <= '9') {
    const uint8_t* g = FONT[ch - '0'];
    for (int i = 0; i < 5 && count < maxCount; i++) buf[count++] = g[i];
    if (count < maxCount) buf[count++] = 0x00;
  } else if (ch == ':') {
    for (int i = 0; i < 2 && count < maxCount; i++) buf[count++] = CHAR_COLON[i];
    if (count < maxCount) buf[count++] = 0x00;
  } else if (ch == '.') {
    if (count < maxCount) buf[count++] = CHAR_DOT[0];
    if (count < maxCount) buf[count++] = 0x00;
  } else {
    uint8_t tmp[8];
    uint8_t n = mx.getChar(ch, sizeof(tmp), tmp);
    if (n == 0) return;
    for (int i = 0; i < n && count < maxCount; i++) buf[count++] = tmp[i];
    if (count < maxCount) buf[count++] = 0x00;
  }
}


// ============================================================
// TIKO FONT (3x7 custom pixel font) — alternative to the
// library-default "Marymba" font used across all tiles.
// Each glyph is stored as N columns (N=3 for almost every
// character, N=5 only for 'W'); each column byte packs 7 rows,
// bit r (r=0..6, r=0 is the TOP row) -> same column-encoding
// convention as the existing digit FONT[][5] table above, so
// it can be dropped in anywhere getChar()-style output is used.
// ============================================================
struct TikoGlyph {
  char    ch;
  uint8_t cols;
  uint8_t data[TIKO_MAX_COLS];
};

const TikoGlyph TIKO_FONT[] = {
  { '1', 3, { 0x42, 0x7F, 0x40 } },
  { '0', 3, { 0x3E, 0x41, 0x3E } },
  { '2', 3, { 0x62, 0x51, 0x4E } },
  { '3', 3, { 0x41, 0x49, 0x3E } },
  { '4', 3, { 0x0F, 0x08, 0x7F } },
  { '5', 3, { 0x4F, 0x49, 0x31 } },
  { '6', 3, { 0x3E, 0x49, 0x30 } },
  { '7', 3, { 0x01, 0x71, 0x0F } },
  { '8', 3, { 0x3E, 0x49, 0x3E } },
  { '9', 3, { 0x06, 0x49, 0x3E } },
  { 'A', 3, { 0x7E, 0x09, 0x7E } },
  { 'B', 3, { 0x7F, 0x49, 0x3E } },
  { 'C', 3, { 0x3E, 0x41, 0x41 } },
  { 'D', 3, { 0x7F, 0x41, 0x3E } },
  { 'E', 3, { 0x7F, 0x49, 0x41 } },
  { 'F', 3, { 0x7F, 0x09, 0x01 } },
  { 'H', 3, { 0x7F, 0x08, 0x7F } },
  { 'I', 3, { 0x00, 0x7F, 0x00 } },
  { 'J', 3, { 0x40, 0x3F, 0x00 } },
  { 'K', 3, { 0x7F, 0x10, 0x6C } },
  { 'L', 3, { 0x3F, 0x40, 0x40 } },
  { 'G', 3, { 0x3E, 0x41, 0x70 } },
  { 'M', 3, { 0x7F, 0x02, 0x7F } },
  { 'N', 3, { 0x7F, 0x01, 0x7E } },
  { 'O', 3, { 0x3E, 0x41, 0x3E } },
  { 'P', 3, { 0x7F, 0x09, 0x06 } },
  { 'Q', 3, { 0x1E, 0x21, 0x5E } },
  { 'R', 3, { 0x7F, 0x09, 0x76 } },
  { 'S', 3, { 0x4E, 0x49, 0x31 } },
  { 'T', 3, { 0x01, 0x7F, 0x01 } },
  { 'U', 3, { 0x3F, 0x40, 0x3F } },
  { 'V', 3, { 0x3F, 0x60, 0x3F } },
  { 'W', 5, { 0x3F, 0x40, 0x38, 0x40, 0x3F } },
  { 'X', 3, { 0x67, 0x18, 0x67 } },
  { 'Y', 3, { 0x07, 0x78, 0x07 } },
  { ':', 3, { 0x00, 0x36, 0x00 } },
  { ',', 3, { 0x00, 0x60, 0x00 } },
  { '.', 3, { 0x00, 0x20, 0x00 } },
  { '?', 3, { 0x02, 0x51, 0x0E } },
  { '!', 3, { 0x00, 0x5F, 0x00 } },
  { '%', 3, { 0x71, 0x08, 0x47 } },
  { '^', 3, { 0x04, 0x03, 0x04 } },
  { '(', 3, { 0x3E, 0x41, 0x00 } },
  { ')', 3, { 0x00, 0x41, 0x3E } },
  { '[', 3, { 0x7F, 0x41, 0x00 } },
  { ']', 3, { 0x00, 0x41, 0x7F } },
  { '<', 3, { 0x08, 0x14, 0x00 } },
  { '>', 3, { 0x00, 0x14, 0x08 } },
  { '\'', 3, { 0x00, 0x03, 0x00 } },
  { ';', 3, { 0x40, 0x26, 0x00 } },
  { '/', 3, { 0x70, 0x1C, 0x07 } },
  { '\\', 3, { 0x07, 0x1C, 0x70 } },
  { '|', 3, { 0x00, 0x7F, 0x00 } },
  { '"', 3, { 0x03, 0x00, 0x03 } },
  { '`', 3, { 0x01, 0x02, 0x00 } },
  { '~', 3, { 0x0C, 0x08, 0x18 } },
  { 'Z', 3, { 0x47, 0x4D, 0x71 } },
};
const uint8_t TIKO_FONT_COUNT = sizeof(TIKO_FONT) / sizeof(TIKO_FONT[0]);

// Characters that exist in the pixel-art source but are not part
// of the printable glyph table above (used by the temp/pressure
// builders instead of going through tikoGetChar):
const uint8_t TIKO_DEG[3]   = { 0x02, 0x05, 0x02 };  // °
const uint8_t TIKO_MINUS[3] = { 0x08, 0x08, 0x08 };  // -
const uint8_t TIKO_SPACE[2] = { 0x00, 0x00 };        // ' ' (no glyph in source font)

// Drop-in replacement for mx.getChar(ch, size, buf): looks the
// character up in TIKO_FONT (case-insensitive — the source font
// only defines uppercase letters), falls back to a blank space
// glyph for anything unmapped (accented letters, '@', '-', '+',
// '=', '&', '*', '#', '_', '$', etc. are not present in the
// supplied pixel art and are not guessed at here).
uint8_t tikoGetChar(char ch, uint8_t size, uint8_t* buf) {
  if (ch == ' ') {
    uint8_t n = (2 <= size) ? 2 : size;
    for (uint8_t i = 0; i < n; i++) buf[i] = TIKO_SPACE[i];
    return n;
  }
  if (ch == '-') {
    uint8_t n = (3 <= size) ? 3 : size;
    for (uint8_t i = 0; i < n; i++) buf[i] = TIKO_MINUS[i];
    return n;
  }
  char up = (ch >= 'a' && ch <= 'z') ? (ch - 32) : ch;
  for (uint8_t gi = 0; gi < TIKO_FONT_COUNT; gi++) {
    if (TIKO_FONT[gi].ch == up) {
      uint8_t n = (TIKO_FONT[gi].cols <= size) ? TIKO_FONT[gi].cols : size;
      for (uint8_t i = 0; i < n; i++) buf[i] = TIKO_FONT[gi].data[i];
      return n;
    }
  }
  // Unmapped character: fall back to a blank 3-col space so text
  // keeps flowing instead of stalling the buffer.
  uint8_t n = (3 <= size) ? 3 : size;
  for (uint8_t i = 0; i < n; i++) buf[i] = 0x00;
  return n;
}

// ============================================================
// MICRO FONT (3x5) - only for the Hour and Weekday tile, where the
// weekday has to sit next to a full Tiko time. Same column layout
// as TIKO_FONT (bit 0 = top row); W is the only 5-column letter.
// Holds just the capitals the RO/EN weekday names use.
// ============================================================
const TikoGlyph MICRO_FONT[] = {
  { 'A', 3, { 0x1E, 0x05, 0x1E } },
  { 'D', 3, { 0x1F, 0x11, 0x0E } },
  { 'E', 3, { 0x1F, 0x15, 0x11 } },
  { 'F', 3, { 0x1F, 0x05, 0x01 } },
  { 'H', 3, { 0x1F, 0x04, 0x1F } },
  { 'I', 3, { 0x11, 0x1F, 0x11 } },
  { 'J', 3, { 0x08, 0x10, 0x0F } },
  { 'L', 3, { 0x1F, 0x10, 0x10 } },
  { 'M', 3, { 0x1F, 0x02, 0x1F } },
  { 'N', 3, { 0x1F, 0x01, 0x1E } },
  { 'O', 3, { 0x0E, 0x11, 0x0E } },
  { 'R', 3, { 0x1F, 0x05, 0x1A } },
  { 'S', 3, { 0x12, 0x15, 0x09 } },
  { 'T', 3, { 0x01, 0x1F, 0x01 } },
  { 'U', 3, { 0x1F, 0x10, 0x1F } },
  { 'V', 3, { 0x0F, 0x10, 0x0F } },
  { 'W', 5, { 0x1F, 0x08, 0x04, 0x08, 0x1F } },
};
const uint8_t MICRO_FONT_COUNT = sizeof(MICRO_FONT) / sizeof(MICRO_FONT[0]);

// Case-insensitive like tikoGetChar(); returns 0 for a letter it lacks.
// buf must hold TIKO_MAX_COLS bytes.
static uint8_t microGetChar(char ch, uint8_t* buf) {
  char up = (ch >= 'a' && ch <= 'z') ? (ch - 32) : ch;
  for (uint8_t gi = 0; gi < MICRO_FONT_COUNT; gi++) {
    if (MICRO_FONT[gi].ch == up) {
      memcpy(buf, MICRO_FONT[gi].data, MICRO_FONT[gi].cols);
      return MICRO_FONT[gi].cols;
    }
  }
  return 0;
}

// ============================================================
// MAKO FONT (4x7 custom pixel font) - the third option, between
// Tiko's 3-wide capitals and Marymba's 5-wide library look.
// Rounded bowls and cut corners; real lowercase with a 5-row
// x-height and descenders on row 7. Digits are all exactly 4
// columns so the clock centres the same whatever the time.
// Same column encoding as Tiko: bit r is row r, r=0 is the top.
// Generated from the ASCII drawings in mako_font.py.
// ============================================================
const TikoGlyph MAKO_FONT[] = {
  { '0', 4, { 0x3E, 0x41, 0x41, 0x3E } },
  { '1', 4, { 0x00, 0x42, 0x7F, 0x40 } },
  { '2', 4, { 0x62, 0x51, 0x49, 0x46 } },
  { '3', 4, { 0x41, 0x49, 0x49, 0x36 } },
  { '4', 4, { 0x0C, 0x0A, 0x09, 0x7F } },
  { '5', 4, { 0x27, 0x45, 0x45, 0x39 } },
  { '6', 4, { 0x3E, 0x49, 0x49, 0x30 } },
  { '7', 4, { 0x01, 0x71, 0x09, 0x07 } },
  { '8', 4, { 0x36, 0x49, 0x49, 0x36 } },
  { '9', 4, { 0x06, 0x49, 0x49, 0x3E } },
  { 'A', 4, { 0x7E, 0x09, 0x09, 0x7E } },
  { 'B', 4, { 0x7F, 0x49, 0x49, 0x36 } },
  { 'C', 4, { 0x3E, 0x41, 0x41, 0x22 } },
  { 'D', 4, { 0x7F, 0x41, 0x41, 0x3E } },
  { 'E', 4, { 0x7F, 0x49, 0x49, 0x41 } },
  { 'F', 4, { 0x7F, 0x09, 0x09, 0x01 } },
  { 'G', 4, { 0x3E, 0x41, 0x49, 0x7A } },
  { 'H', 4, { 0x7F, 0x08, 0x08, 0x7F } },
  { 'I', 3, { 0x41, 0x7F, 0x41 } },
  { 'J', 4, { 0x30, 0x40, 0x41, 0x3F } },
  { 'K', 4, { 0x7F, 0x08, 0x14, 0x63 } },
  { 'L', 4, { 0x7F, 0x40, 0x40, 0x40 } },
  { 'M', 5, { 0x7F, 0x02, 0x0C, 0x02, 0x7F } },
  { 'N', 4, { 0x7F, 0x06, 0x18, 0x7F } },
  { 'O', 4, { 0x3E, 0x41, 0x41, 0x3E } },
  { 'P', 4, { 0x7F, 0x09, 0x09, 0x06 } },
  { 'Q', 4, { 0x3E, 0x41, 0x21, 0x5E } },
  { 'R', 4, { 0x7F, 0x09, 0x19, 0x66 } },
  { 'S', 4, { 0x46, 0x49, 0x49, 0x31 } },
  { 'T', 5, { 0x01, 0x01, 0x7F, 0x01, 0x01 } },
  { 'U', 4, { 0x3F, 0x40, 0x40, 0x3F } },
  { 'V', 4, { 0x1F, 0x60, 0x60, 0x1F } },
  { 'W', 5, { 0x7F, 0x20, 0x18, 0x20, 0x7F } },
  { 'X', 4, { 0x63, 0x1C, 0x1C, 0x63 } },
  { 'Y', 4, { 0x07, 0x78, 0x78, 0x07 } },
  { 'Z', 4, { 0x61, 0x51, 0x4D, 0x43 } },
  { 'a', 4, { 0x20, 0x54, 0x54, 0x78 } },
  { 'b', 4, { 0x7F, 0x44, 0x44, 0x38 } },
  { 'c', 4, { 0x38, 0x44, 0x44, 0x44 } },
  { 'd', 4, { 0x38, 0x44, 0x44, 0x7F } },
  { 'e', 4, { 0x38, 0x54, 0x54, 0x58 } },
  { 'f', 3, { 0x7E, 0x05, 0x05 } },
  { 'g', 4, { 0x98, 0xA4, 0xA4, 0x7C } },
  { 'h', 4, { 0x7F, 0x04, 0x04, 0x78 } },
  { 'i', 1, { 0x7D } },
  { 'j', 2, { 0x80, 0x7D } },
  { 'k', 4, { 0x7F, 0x10, 0x28, 0x44 } },
  { 'l', 2, { 0x3F, 0x40 } },
  { 'm', 5, { 0x7C, 0x04, 0x78, 0x04, 0x78 } },
  { 'n', 4, { 0x7C, 0x04, 0x04, 0x78 } },
  { 'o', 4, { 0x38, 0x44, 0x44, 0x38 } },
  { 'p', 4, { 0xFC, 0x24, 0x24, 0x18 } },
  { 'q', 4, { 0x18, 0x24, 0x24, 0xFC } },
  { 'r', 3, { 0x7C, 0x08, 0x04 } },
  { 's', 4, { 0x48, 0x54, 0x54, 0x24 } },
  { 't', 3, { 0x04, 0x3F, 0x44 } },
  { 'u', 4, { 0x3C, 0x40, 0x40, 0x7C } },
  { 'v', 4, { 0x1C, 0x60, 0x60, 0x1C } },
  { 'w', 5, { 0x3C, 0x40, 0x30, 0x40, 0x3C } },
  { 'x', 4, { 0x6C, 0x10, 0x10, 0x6C } },
  { 'y', 4, { 0x1C, 0xA0, 0xA0, 0x7C } },
  { 'z', 4, { 0x64, 0x54, 0x4C, 0x44 } },
  { '.', 1, { 0x40 } },
  { ',', 2, { 0x80, 0x60 } },
  { ':', 1, { 0x36 } },
  { ';', 2, { 0x40, 0x36 } },
  { '!', 1, { 0x5F } },
  { '?', 4, { 0x02, 0x51, 0x09, 0x06 } },
  { '-', 3, { 0x08, 0x08, 0x08 } },
  { '+', 3, { 0x08, 0x1C, 0x08 } },
  { '=', 3, { 0x14, 0x14, 0x14 } },
  { '/', 3, { 0x60, 0x1C, 0x03 } },
  { '\\', 3, { 0x03, 0x1C, 0x60 } },
  { '(', 2, { 0x3E, 0x41 } },
  { ')', 2, { 0x41, 0x3E } },
  { '[', 2, { 0x7F, 0x41 } },
  { ']', 2, { 0x41, 0x7F } },
  { '\'', 1, { 0x03 } },
  { '"', 3, { 0x03, 0x00, 0x03 } },
  { '%', 4, { 0x13, 0x0B, 0x34, 0x32 } },
  { '*', 3, { 0x0A, 0x04, 0x0A } },
  { '#', 5, { 0x12, 0x3F, 0x12, 0x3F, 0x12 } },
  { '_', 4, { 0x80, 0x80, 0x80, 0x80 } },
  { '<', 3, { 0x08, 0x14, 0x22 } },
  { '>', 3, { 0x22, 0x14, 0x08 } },
  { '^', 3, { 0x02, 0x01, 0x02 } },
  { '~', 4, { 0x10, 0x08, 0x10, 0x08 } },
  { '`', 2, { 0x01, 0x02 } },
  { '|', 1, { 0x7F } },
};
const uint8_t MAKO_FONT_COUNT = sizeof(MAKO_FONT) / sizeof(MAKO_FONT[0]);
const uint8_t MAKO_DEG[3] = { 0x02, 0x05, 0x02 };  // 3-wide ring, the same shape Tiko draws

// Like tikoGetChar, but case-sensitive - Mako has its own lowercase - and
// anything it does not draw (@, &, $...) falls back to the Marymba glyph
// instead of a gap, so a notification never silently loses a character.
uint8_t makoGetChar(char ch, uint8_t size, uint8_t* buf) {
  if (ch == ' ') {
    uint8_t n = (2 <= size) ? 2 : size;
    for (uint8_t i = 0; i < n; i++) buf[i] = 0x00;
    return n;
  }
  for (uint8_t gi = 0; gi < MAKO_FONT_COUNT; gi++) {
    if (MAKO_FONT[gi].ch == ch) {
      uint8_t n = (MAKO_FONT[gi].cols <= size) ? MAKO_FONT[gi].cols : size;
      for (uint8_t i = 0; i < n; i++) buf[i] = MAKO_FONT[gi].data[i];
      return n;
    }
  }
  return mx.getChar(ch, size, buf);
}

// The degree sign both of our fonts draw next to a temperature (3 columns).
static inline const uint8_t* pixDeg(uint8_t m) {
  return (m == FONT_MODE_MAKO) ? MAKO_DEG : TIKO_DEG;
}

// FONT_MODE_DEFAULT = 0 ("Marymba", the existing library/FONT[] look)
// FONT_MODE_TIKO    = 1 (3x7 custom font)
// FONT_MODE_MAKO    = 2 (4x7 custom font with lowercase)
// (defined earlier, near appendGlyphAuto, so they're visible where first used)

const uint8_t wifiLogo[8] = {
  0b00000010, 0b00001001, 0b00000101, 0b00110101,
  0b00110101, 0b00000101, 0b00001001, 0b00000010
};

const uint8_t apIcon[8] = {
  0b00011000,
  0b01000010,
  0b00011000,
  0b10111101,
  0b10111101,
  0b00011000,
  0b01000010,
  0b00011000
};

void drawApScreen() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);

  for (int col = 0; col < 8; col++) {
    uint8_t colVal = 0;
    for (int row = 0; row < 8; row++) {
      if (ipIcon[row] & (0x80 >> col)) colVal |= (1 << row);
    }
    mx.setColumn(31 - col, colVal);
  }

  mx.setColumn(31 - 8, 0x00);

  // In the font picked under Tile Manager -> Font Type, like the tiles.
  uint8_t lbl[23]; int lblN = 0;
  appendGlyphAuto(lbl, lblN, sizeof(lbl), 'A', fontType);
  appendGlyphAuto(lbl, lblN, sizeof(lbl), 'P', fontType);
  for (int i = 0; i < lblN; i++) mx.setColumn(31 - (9 + i), lbl[i]);

  mxCommit();
}

// LOADING ANIMATION
//
// A gap of LOAD_GAP pixels running clockwise around the 24-pixel perimeter of an
// 8x8 rounded box. 24 perimeter positions means 24 frames and exactly one full
// turn, with every position visited once - so the loop is seamless and needs no
// stored frame table: 24 frames x 8 rows would be 192 bytes of flash holding
// data that is fully derivable from the frame index.
#define LOAD_FRAMES   24
#define LOAD_GAP       3
#define LOAD_FRAME_MS 55

// Wi-Fi connect window, and how much of it the Wi-Fi logo keeps to itself
// before the spinner takes over - cutting to the spinner immediately made the
// logo look like it had been yanked off the panel.
#define WIFI_CONNECT_ATTEMPTS 20   // x500ms -> 10s ceiling on the wait
#define LOAD_LOGO_HOLD_PCT    20   // logo holds for the first fifth of it

// The AP badge gets the same treatment on the way in: long enough to read that
// the mode changed, then the spinner while the access point comes up.
#define AP_SCREEN_HOLD_MS 2000

// Maps a perimeter position (0..23, clockwise starting at the top-left) to the
// pixel it lights.
static inline void loadPerimeterPixel(uint8_t idx, uint8_t& row, uint8_t& col) {
  idx %= LOAD_FRAMES;
  if (idx < 6)       { row = 0;        col = idx + 1;  }   // top,    left  -> right
  else if (idx < 12) { row = idx - 5;  col = 7;        }   // right,  top   -> bottom
  else if (idx < 18) { row = 7;        col = 18 - idx; }   // bottom, right -> left
  else               { row = 24 - idx; col = 0;        }   // left,   bottom-> top
}

// Builds one frame as 8 row bytes (MSB = leftmost column), the same layout the
// static icons above use.
static void buildLoadFrame(uint8_t frame, uint8_t rows[8]) {
  for (uint8_t i = 0; i < 8; i++) rows[i] = 0;
  uint8_t gapStart = (uint8_t)((frame + 3) % LOAD_FRAMES);
  for (uint8_t k = 0; k < LOAD_FRAMES; k++) {
    if ((uint8_t)((k + LOAD_FRAMES - gapStart) % LOAD_FRAMES) < LOAD_GAP) continue;
    uint8_t r, c;
    loadPerimeterPixel(k, r, c);
    rows[r] |= (uint8_t)(0x80 >> c);
  }
}

// Spinner on the left, "Load" next to it.
void drawLoadingScreen(uint8_t frame) {
  uint8_t rows[8];
  buildLoadFrame(frame, rows);

  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);

  for (int col = 0; col < 8; col++) {
    uint8_t colVal = 0;
    for (int row = 0; row < 8; row++) {
      if (rows[row] & (0x80 >> col)) colVal |= (1 << row);
    }
    mx.setColumn(31 - col, colVal);
  }

  // Build the text into a column buffer first so its width is known and it can
  // be centred in the space left of the icon instead of risking a clipped tail.
  uint8_t txtCols[24];
  int nTxt = 0;
  const char* txt = "Load";
  for (const char* p = txt; *p; p++) {
    uint8_t tmp[8];
    uint8_t n = mx.getChar(*p, sizeof(tmp), tmp);
    for (uint8_t i = 0; i < n && nTxt < (int)sizeof(txtCols); i++) txtCols[nTxt++] = tmp[i];
    if (p[1] && nTxt < (int)sizeof(txtCols)) txtCols[nTxt++] = 0x00;  // letter spacing
  }

  const int textStart = 9;                     // one blank column after the icon
  int avail = 32 - textStart;
  int at = textStart + (nTxt < avail ? (avail - nTxt) / 2 : 0);
  for (int i = 0; i < nTxt && at < 32; i++) mx.setColumn(31 - at++, txtCols[i]);

  mxCommit();
}

// Keeps the spinner turning for `ms` milliseconds instead of blocking on a bare
// delay(). The frame counter is static so a wait split across several calls
// carries on from where the previous one stopped - the animation never restarts
// mid-spin, however many times it is called.
static uint8_t loadAnimFrame = 0;

static void loadAnimWait(uint16_t ms) {
  uint32_t start = millis();
  do {
    drawLoadingScreen(loadAnimFrame);
    loadAnimFrame = (uint8_t)((loadAnimFrame + 1) % LOAD_FRAMES);
    delay(LOAD_FRAME_MS);
  } while ((uint32_t)(millis() - start) < ms);
}

// autoDetectTimezone() blocks on an HTTP request for up to 5s, which would
// otherwise freeze the spinner mid-turn. Give it the same treatment the
// weather and currency fetches already get - run it on core 0 and keep
// animating on this one until it reports back. The guard is well past the 5s
// HTTP timeout, so a wedged request can never hold up boot indefinitely.
static volatile bool tzDetectBusy = false;

static void tzDetectTask(void* pv) {
  autoDetectTimezone();
  tzDetectBusy = false;
  vTaskDelete(nullptr);
}

// Same task, but nobody waits for it - safe to call from an HTTP handler,
// where blocking for the 5s lookup would stall the whole loop.
static void tzDetectAsync() {
  if (tzDetectBusy) return;
  tzDetectBusy = true;
  if (xTaskCreatePinnedToCore(tzDetectTask, "tzDetect", 8192, nullptr, 1, nullptr, 0) != pdPASS)
    tzDetectBusy = false;
}

static void autoDetectTimezoneAnimated() {
  tzDetectBusy = true;
  if (xTaskCreatePinnedToCore(tzDetectTask, "tzDetect", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
    tzDetectBusy = false;
    autoDetectTimezone();          // could not spawn the task - run it inline
    return;
  }
  uint32_t start = millis();
  while (tzDetectBusy && (uint32_t)(millis() - start) < 12000) loadAnimWait(LOAD_FRAME_MS);
}

const uint8_t musicIcon[8] = {
  0b11100000,
  0b10011100,
  0b10000100,
  0b10000100,
  0b11000100,
  0b11100110,
  0b11000111,
  0b00000110
};

const uint8_t videoIcon[8] = {
  0b00000000,
  0b01111110,
  0b11101111,
  0b11100111,
  0b11100111,
  0b11101111,
  0b01111110,
  0b00000000
};

const uint8_t mementoIcon[8] = {
  0b00100100,
  0b01011010,
  0b00111100,
  0b00111100,
  0b00111100,
  0b01111110,
  0b00011000,
  0b00000000
};

// MEMENTO
char     mementoBuf[128]        = "";
NpState  mementoState           = NP_SHOW_START;
unsigned long mementoPauseMs    = 0;
// Room for the whole text in the widest font (6 columns a letter with the
// gap) plus the icon. At 512 anything past ~85 characters was cut off.
#define MEMENTO_COL_MAX 800
static uint8_t  mementoColBuf[MEMENTO_COL_MAX];
static int      mementoColCount = 0;
static int      mementoScrollPos = 0;
static unsigned long mementoLastScrollMs = 0;
static int      mementoWrapPass = 0;
#define NP_ICON_COLS   8
#define NP_SEP_COLS    1
#define NP_TEXT_COLS   (NP_DISPLAY_COLS - NP_ICON_COLS - NP_SEP_COLS)

const uint8_t tempIcon[8] = {
  0b00011000,
  0b00011000,
  0b00011000,
  0b00011000,
  0b00011000,
  0b00101100,
  0b00100100,
  0b00011000
};
#define TEMP_ICON_COLS  8
#define TEMP_SEP_COLS   1
#define TEMP_TEXT_COLS  (32 - TEMP_ICON_COLS - TEMP_SEP_COLS)

const uint8_t dateIcon[8] = {
  0b01111110,
  0b11111111,
  0b11111111,
  0b10000001,
  0b10000001,
  0b10000001,
  0b10000001,
  0b01111110
};
#define DATE_ICON_COLS  8
#define DATE_SEP_COLS   1
#define DATE_TEXT_COLS  (32 - DATE_ICON_COLS - DATE_SEP_COLS)

const uint8_t pressureIconUp[8] = {
  0b00011000,
  0b00111100,
  0b01111110,
  0b11011011,
  0b10011001,
  0b00011000,
  0b00011000,
  0b00011000
};
const uint8_t pressureIconDown[8] = {
  0b00011000,
  0b00011000,
  0b00011000,
  0b10011001,
  0b11011011,
  0b01111110,
  0b00111100,
  0b00011000
};
const uint8_t pressureIconFlat[8] = {
  0b00000000,
  0b00000000,
  0b00000000,
  0b01111110,
  0b01111110,
  0b00000000,
  0b00000000,
  0b00000000
};
#define PRESSURE_ICON_COLS  8
#define PRESSURE_SEP_COLS   1
#define PRESSURE_TEXT_COLS  (32 - PRESSURE_ICON_COLS - PRESSURE_SEP_COLS)

// ============================================================
// CUSTOM TILE ICONS (extra icons offered in the web UI picker,
// in addition to every icon the tiles already use natively)
// ============================================================
const uint8_t exclamationIcon[8] = {
  0b00011000,
  0b00011000,
  0b00011000,
  0b00011000,
  0b00011000,
  0b00000000,
  0b00011000,
  0b00011000
};

const uint8_t pauseIcon[8] = {
  0b00000000,
  0b01100110,
  0b01100110,
  0b01100110,
  0b01100110,
  0b01100110,
  0b01100110,
  0b00000000
};

const uint8_t euroIcon[8] = {
  0b00011000,
  0b00100100,
  0b01110000,
  0b00100000,
  0b01110000,
  0b00100100,
  0b00011000,
  0b00000000
};

const uint8_t bluetoothIcon[8] = {
  0b00001000,
  0b00101100,
  0b00011010,
  0b00001100,
  0b00001100,
  0b00011010,
  0b00101100,
  0b00001000
};

const uint8_t blockedIcon[8] = {
  0b00000000,
  0b00111100,
  0b01000110,
  0b01001010,
  0b01010010,
  0b01100010,
  0b00111100,
  0b00000000
};

// Master catalog of every icon selectable in the "Icon Settings" picker
// (all pre-existing tile icons + the new ones above). The index used by
// iconSel*/iconWx* settings is (position in this array + 1); 0 means
// "Auto / Default" i.e. keep the tile's own built-in icon logic.
struct IconCatalogEntry { const char* id; const uint8_t* bmp; };
const IconCatalogEntry ICON_CATALOG[] = {
  { "date",      dateIcon },
  { "temp",      tempIcon },
  { "reminder",  mementoIcon },
  { "music",     musicIcon },
  { "notif",     notifIcon },
  { "pressup",   pressureIconUp },
  { "pressdown", pressureIconDown },
  { "pressflat", pressureIconFlat },
  { "sunny",     sunnyIcon },
  { "cloud",     cloudIcon },
  { "rain",      cloudRainIcon },
  { "storm",     cloudLightningIcon },
  { "snow",      cloudSnowIcon },
  { "wind",      windIcon },
  { "moon",      moonIcon },
  { "wifi",      ipIcon },
  { "ap",        apIcon },
  { "exclaim",   exclamationIcon },
  { "pause",     pauseIcon },
  { "euro",      euroIcon },
  { "bluetooth", bluetoothIcon },
  { "blocked",   blockedIcon },
  { "wifilogo",  wifiLogo },
  { "video",     videoIcon }
};
#define ICON_CATALOG_COUNT (sizeof(ICON_CATALOG) / sizeof(ICON_CATALOG[0]))

static inline const uint8_t* resolveTileIcon(uint8_t sel, const uint8_t* fallback) {
  if (sel > 0 && sel <= ICON_CATALOG_COUNT) return ICON_CATALOG[sel - 1].bmp;
  return fallback;
}

// Now Playing's default (un-overridden) icon: the music note, unless
// Adaptive Icon is on and the source app was reported as a video player.
static inline const uint8_t* npFallbackIcon() {
  return (npAdaptiveIcon && npIsVideoSource) ? videoIcon : musicIcon;
}

// Resolves the icon actually shown on the Now Playing tile: uses the
// Music override or the Video override depending on the currently
// detected source, falling back to npFallbackIcon() (which respects
// Adaptive Icon) when the relevant override is left on Auto.
static inline const uint8_t* npResolvedIcon() {
  uint8_t sel = npIsVideoSource ? iconSelNpVideo : iconSelNpMusic;
  return resolveTileIcon(sel, npFallbackIcon());
}

// DISPLAY HELPERS

void writeCharN(const uint8_t* ch, int n, int logicalCol) {
  for (int i = 0; i < n; i++) {
    int physCol = 31 - (logicalCol + i);
    if (physCol >= 0 && physCol < 32) mx.setColumn(physCol, ch[i]);
  }
}

void writeChar(const uint8_t* ch, int logicalCol) {
  writeCharN(ch, 5, logicalCol);
}

void writeGap(int logicalCol) {
  int physCol = 31 - logicalCol;
  if (physCol >= 0 && physCol < 32) mx.setColumn(physCol, 0x00);
}
static uint8_t  tempColBuf[128];
static int      tempColCount = 0;
static int      tempScrollPos = 0;
static unsigned long tempLastScrollMs = 0;
static NpState  tempState    = NP_SHOW_START;
static unsigned long tempPauseMs = 0;
static bool     tempNeedsScroll = false;
static int      tempWrapPass = 0;

static uint8_t  dateColBuf[128];
static int      dateColCount = 0;
static int      dateScrollPos = 0;
static unsigned long dateLastScrollMs = 0;
static NpState  dateState    = NP_SHOW_START;
static unsigned long datePauseMs = 0;
static bool     dateNeedsScroll = false;
static int      dateWrapPass = 0;

static void tempAddGlyphTo(uint8_t* buf, int& count, const uint8_t* glyph, int width = 5) {
  for (int i = 0; i < width && count < 62; i++)
    buf[count++] = glyph[i];
  if (count < 63) buf[count++] = 0x00;
}

static void tempAddGlyph(const uint8_t* glyph, int width = 5) {
  for (int i = 0; i < width && tempColCount < 126; i++)
    tempColBuf[tempColCount++] = glyph[i];
  if (tempColCount < 127) tempColBuf[tempColCount++] = 0x00;
}

// Shared helper: returns a pointer-friendly digit glyph (0-9) for
// the classic 5-wide FONT[] table, 3-wide Tiko or 4-wide Mako,
// and reports its column width via 'width'. 'tmp' must be
// at least TIKO_MAX_COLS bytes and stays valid until reused.
static const uint8_t* digitGlyph(uint8_t fontMode, int d, uint8_t* tmp, int& width) {
  if (isPixelFont(fontMode)) {
    width = pixGetChar(fontMode, (char)('0' + d), TIKO_MAX_COLS, tmp);
    return tmp;
  }
  width = 5;
  return FONT[d];
}

static void dateAddDot() {
  if (isPixelFont(fontTypeDate)) {
    uint8_t tmp[TIKO_MAX_COLS];
    uint8_t n = pixGetChar(fontTypeDate, '.', TIKO_MAX_COLS, tmp);
    for (int i = 0; i < n && dateColCount < 126; i++) dateColBuf[dateColCount++] = tmp[i];
    if (dateColCount < 127) dateColBuf[dateColCount++] = 0x00;
    return;
  }
  if (dateColCount < 126) dateColBuf[dateColCount++] = CHAR_DOT[0];
  if (dateColCount < 127) dateColBuf[dateColCount++] = 0x00;
}

// SCROLL TYPE

static void scrollBufPrependIcon(uint8_t* buf, int& count, int maxCount, const uint8_t icon[8], int iconCols) {
  for (int col = 0; col < iconCols && count < maxCount; col++) {
    uint8_t colVal = 0;
    for (int row = 0; row < 8; row++) {
      if (icon[row] & (0x80 >> col)) colVal |= (1 << row);
    }
    buf[count++] = colVal;
  }
  if (count < maxCount) buf[count++] = 0x00;
}

void tempBuildBuffer(int tempC) {
  int temp = (tempC == TEMP_NO_READING) ? 0
           : (tempUnit == 1) ? (tempC * 9 / 5 + 32) : tempC;
  tempColCount = 0;
  if (scrollIconInBuffer(scrollTypeTemp) && !hideIconTemp) {
    scrollBufPrependIcon(tempColBuf, tempColCount, 127, resolveTileIcon(iconSelTemp, tempIcon), TEMP_ICON_COLS);
  }

  bool tiko = isPixelFont(fontTypeTemp);   // Tiko or Mako

  if (tempC == TEMP_NO_READING) {
    // No usable reading yet. Draw "--" followed by the unit rather than
    // leaving the buffer empty: an empty buffer meant tempDrawAtPos() wrote 32
    // blank columns and the matrix stayed black for the tile's whole slot,
    // which reads as a broken clock instead of a quiet sensor. This also
    // matches the "--" the web dashboard already shows for the same state.
    for (int rep = 0; rep < 2; rep++) {
      for (int i = 0; i < 3 && tempColCount < 126; i++)
        tempColBuf[tempColCount++] = tiko ? TIKO_MINUS[i] : 0x08;
      if (tempColCount < 127) tempColBuf[tempColCount++] = 0x00;
    }
  } else {
    bool negative = (temp < 0);
    int  absTemp  = abs(temp);

    if (negative) {
      const uint8_t* minusGlyph = tiko ? TIKO_MINUS : nullptr;
      int minusW = tiko ? 3 : 3;
      for (int i = 0; i < minusW && tempColCount < 126; i++)
        tempColBuf[tempColCount++] = tiko ? minusGlyph[i] : 0x08;
      if (tempColCount < 127) tempColBuf[tempColCount++] = 0x00;
    }

    uint8_t dtmp[TIKO_MAX_COLS]; int dw;
    if (absTemp >= 100) {
      tempAddGlyph(digitGlyph(fontTypeTemp, absTemp / 100, dtmp, dw), dw);
      tempAddGlyph(digitGlyph(fontTypeTemp, (absTemp / 10) % 10, dtmp, dw), dw);
      tempAddGlyph(digitGlyph(fontTypeTemp, absTemp % 10, dtmp, dw), dw);
    } else if (absTemp >= 10) {
      tempAddGlyph(digitGlyph(fontTypeTemp, absTemp / 10, dtmp, dw), dw);
      tempAddGlyph(digitGlyph(fontTypeTemp, absTemp % 10, dtmp, dw), dw);
    } else {
      tempAddGlyph(digitGlyph(fontTypeTemp, absTemp, dtmp, dw), dw);
    }
  }

  if (tiko) {
    tempAddGlyph(pixDeg(fontTypeTemp), 3);
    uint8_t utmp[TIKO_MAX_COLS];
    uint8_t uw = pixGetChar(fontTypeTemp, (tempUnit == 1) ? 'F' : 'C', TIKO_MAX_COLS, utmp);
    tempAddGlyph(utmp, uw);
  } else {
    tempAddGlyph(CHAR_DEG);
    tempAddGlyph((tempUnit == 1) ? CHAR_F : CHAR_C);
  }

  if (tempColCount > 0) tempColCount--;
}

void tempDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);

  if (!hideIconTemp && !scrollIconInBuffer(scrollTypeTemp)) {

    // Resolved once rather than 64 times inside the loop below.
    const uint8_t* tempIconBmp = resolveTileIcon(iconSelTemp, tempIcon);
    for (int col = 0; col < TEMP_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (tempIconBmp[row] & (0x80 >> col)) colVal |= (1 << row);
      }
      mx.setColumn(31 - col, colVal);
    }
    mx.setColumn(31 - TEMP_ICON_COLS, 0x00);
  }

  bool fullWidth = hideIconTemp || scrollIconInBuffer(scrollTypeTemp);
  int textOffset = fullWidth ? 0 : (TEMP_ICON_COLS + TEMP_SEP_COLS);
  int textCols   = fullWidth ? 32 : TEMP_TEXT_COLS;
  for (int col = 0; col < textCols; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < tempColCount) ? tempColBuf[src] : 0x00;
    mx.setColumn(31 - (textOffset + col), val);
  }

  mxCommit();
}

void tempInit(int tempC) {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mxCommit();

  tempBuildBuffer(tempC);
  int tempEffTextCols = (hideIconTemp || scrollIconInBuffer(scrollTypeTemp)) ? 32 : TEMP_TEXT_COLS;
  tempNeedsScroll = (tempColCount > tempEffTextCols);

  if (tempNeedsScroll) {
    tempScrollPos = 0;
    tempState     = NP_PAUSE_BEFORE_RIGHT;
    tempPauseMs   = millis();
    tempDrawAtPos(0);
  } else {

    int offset = (tempEffTextCols - tempColCount) / 2;
    if (offset < 0) offset = 0;
    tempDrawAtPos(-offset);
  }
}

// ATMOSPHERIC PRESSURE 

#define PRESSURE_HISTORY_LEN      6
#define PRESSURE_SAMPLE_MS        (30UL * 60UL * 1000UL)
#define PRESSURE_TREND_THRESH_HPA 1.0f
float         pressureHistory[PRESSURE_HISTORY_LEN] = {NAN, NAN, NAN, NAN, NAN, NAN};
uint8_t       pressureHistCount     = 0;
unsigned long lastPressureSampleMs  = 0;
float         lastPressureHpa       = 1013.25f;
int8_t        pressureTrend         = 0;

static inline const uint8_t* pressureIconForTrend() {
  const uint8_t* base = pressureIconFlat;
  if (pressureTrend > 0) base = pressureIconUp;
  else if (pressureTrend < 0) base = pressureIconDown;
  return resolveTileIcon(iconSelPressure, base);
}

// Brings up the BMP280 at either of its two possible addresses. Also used as
// the recovery path, so it must be safe to call repeatedly.
static bool bmpInitSensor() {
  if (!(bmp.begin(0x76) || bmp.begin(0x77))) {
    bmpOk = false;
    return false;
  }
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);
  bmpOk = true;
  return true;
}

// A slave interrupted mid-byte (brown-out, ESD, a knocked jumper) keeps SDA
// pulled low and the bus is then dead for everyone - Wire just times out on
// every transaction. Nine manual clock pulses walk it out of that byte, then a
// manual STOP releases the bus.
static void i2cBusRecover() {
  Wire.end();
  pinMode(BMP_SCL, OUTPUT_OPEN_DRAIN);
  pinMode(BMP_SDA, INPUT_PULLUP);
  digitalWrite(BMP_SCL, HIGH);
  for (int i = 0; i < 9 && digitalRead(BMP_SDA) == LOW; i++) {
    digitalWrite(BMP_SCL, LOW);  delayMicroseconds(5);
    digitalWrite(BMP_SCL, HIGH); delayMicroseconds(5);
  }
  pinMode(BMP_SDA, OUTPUT_OPEN_DRAIN);
  digitalWrite(BMP_SDA, LOW);  delayMicroseconds(5);
  digitalWrite(BMP_SCL, HIGH); delayMicroseconds(5);
  digitalWrite(BMP_SDA, HIGH); delayMicroseconds(5);
  Wire.begin(BMP_SDA, BMP_SCL);
}

// Single guarded read. The range test is written positively so a NaN - what a
// missing sensor produces - is rejected too.
static bool bmpReadTempC(float& out) {
  float t = bmp.readTemperature();
  if (!(t >= -40.0f && t <= 85.0f)) {
    if (bmpFailStreak < 255) bmpFailStreak++;
    return false;
  }
  bmpFailStreak = 0;
  bmpOk         = true;
  out           = t;
  return true;
}

// Called from loop(). Cheap while the sensor is healthy; when it is not, it
// retries with backoff so a permanently absent sensor costs one begin() attempt
// per minute instead of one per iteration.
static void bmpHealthTick() {
  if (bmpOk && bmpFailStreak < BMP_FAIL_LIMIT) return;

  unsigned long now = millis();
  if (bmpNextRetryMs != 0 && (long)(now - bmpNextRetryMs) < 0) return;

  bmpOk = false;
  i2cBusRecover();
  if (bmpInitSensor()) {
    Serial.println("[bmp] senzor reinitializat");
    bmpFailStreak      = 0;
    bmpRetryBackoffMs  = BMP_RETRY_MIN_MS;
    lastTempSampleMs   = 0;   // resample on the very next tick
    lastPressureReadMs = 0;
  } else {
    bmpRetryBackoffMs *= 2;
    if (bmpRetryBackoffMs > BMP_RETRY_MAX_MS) bmpRetryBackoffMs = BMP_RETRY_MAX_MS;
  }
  bmpNextRetryMs = millis() + bmpRetryBackoffMs;
}

void tempSampleTick() {
  unsigned long now = millis();
  if (lastTempSampleMs != 0 && (now - lastTempSampleMs) < TEMP_SAMPLE_INTERVAL_MS) return;
  lastTempSampleMs = now;
  float tf;
  if (bmpReadTempC(tf)) lastTemp = (int)round(tf);
}

void pressureSampleTick() {
  // This used to hit the sensor on every single loop() iteration - thousands of
  // I2C transactions a second (readPressure() reads the temperature registers
  // too), which is both wasted time and the most likely way to wedge the bus in
  // the first place. Atmospheric pressure does not move that fast.
  unsigned long readNow = millis();
  if (lastPressureReadMs != 0 && (readNow - lastPressureReadMs) < BMP_READ_INTERVAL_MS) return;
  lastPressureReadMs = readNow;

  float hpa = bmp.readPressure() / 100.0F;
  // Written as a positive range test so a NaN reading (failed/absent sensor) is
  // rejected too. `hpa < 850 || hpa > 1100` is false for NaN, which let NaN
  // through to lastPressureHpa - and /state then serialised (int)round(NaN),
  // which is undefined behaviour and can emit a garbage pressureHpa value.
  if (!(hpa >= 850 && hpa <= 1100)) {
    // Counts towards the same failure streak as a bad temperature read, so
    // bmpHealthTick() can recover a sensor that only fails on this register.
    if (bmpFailStreak < 255) bmpFailStreak++;
    return;
  }
  lastPressureHpa = hpa;

  unsigned long now = millis();
  if (lastPressureSampleMs != 0 && (now - lastPressureSampleMs) < PRESSURE_SAMPLE_MS) {
    return;
  }
  lastPressureSampleMs = now;

  for (int i = 0; i < PRESSURE_HISTORY_LEN - 1; i++) pressureHistory[i] = pressureHistory[i + 1];
  pressureHistory[PRESSURE_HISTORY_LEN - 1] = hpa;
  if (pressureHistCount < PRESSURE_HISTORY_LEN) pressureHistCount++;
  float oldest = NAN;
  for (int i = 0; i < PRESSURE_HISTORY_LEN; i++) {
    if (!isnan(pressureHistory[i])) { oldest = pressureHistory[i]; break; }
  }
  if (!isnan(oldest)) {
    float diff = hpa - oldest;
    if (diff >= PRESSURE_TREND_THRESH_HPA)       pressureTrend = 1;
    else if (diff <= -PRESSURE_TREND_THRESH_HPA) pressureTrend = -1;
    else                                          pressureTrend = 0;
  }
}

static uint8_t  pressureColBuf[128];
static int      pressureColCount = 0;
static int      pressureScrollPos = 0;
static unsigned long pressureLastScrollMs = 0;
static NpState  pressureState    = NP_SHOW_START;
static unsigned long pressurePauseMs = 0;
static bool     pressureNeedsScroll = false;
static int      pressureWrapPass = 0;

void pressureBuildBuffer(int hpaInt) {
  pressureColCount = 0;
  if (scrollIconInBuffer(scrollTypePressure) && !hideIconPressure) {
    scrollBufPrependIcon(pressureColBuf, pressureColCount, 127, pressureIconForTrend(), PRESSURE_ICON_COLS);
  }

  int absVal = abs(hpaInt);
  uint8_t dtmp[TIKO_MAX_COLS]; int dw;
  if (absVal >= 1000) {
    tempAddGlyphTo(pressureColBuf, pressureColCount, digitGlyph(fontTypePressure, (absVal / 1000) % 10, dtmp, dw), dw);
    tempAddGlyphTo(pressureColBuf, pressureColCount, digitGlyph(fontTypePressure, (absVal / 100) % 10, dtmp, dw), dw);
    tempAddGlyphTo(pressureColBuf, pressureColCount, digitGlyph(fontTypePressure, (absVal / 10) % 10, dtmp, dw), dw);
    tempAddGlyphTo(pressureColBuf, pressureColCount, digitGlyph(fontTypePressure, absVal % 10, dtmp, dw), dw);
  } else if (absVal >= 100) {
    tempAddGlyphTo(pressureColBuf, pressureColCount, digitGlyph(fontTypePressure, absVal / 100, dtmp, dw), dw);
    tempAddGlyphTo(pressureColBuf, pressureColCount, digitGlyph(fontTypePressure, (absVal / 10) % 10, dtmp, dw), dw);
    tempAddGlyphTo(pressureColBuf, pressureColCount, digitGlyph(fontTypePressure, absVal % 10, dtmp, dw), dw);
  } else if (absVal >= 10) {
    tempAddGlyphTo(pressureColBuf, pressureColCount, digitGlyph(fontTypePressure, absVal / 10, dtmp, dw), dw);
    tempAddGlyphTo(pressureColBuf, pressureColCount, digitGlyph(fontTypePressure, absVal % 10, dtmp, dw), dw);
  } else {
    tempAddGlyphTo(pressureColBuf, pressureColCount, digitGlyph(fontTypePressure, absVal, dtmp, dw), dw);
  }

  if (pressureColCount > 0) pressureColCount--;
}

void pressureDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);

  if (!hideIconPressure && !scrollIconInBuffer(scrollTypePressure)) {

    const uint8_t* icon = pressureIconForTrend();
    for (int col = 0; col < PRESSURE_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (icon[row] & (0x80 >> col)) colVal |= (1 << row);
      }
      mx.setColumn(31 - col, colVal);
    }
    mx.setColumn(31 - PRESSURE_ICON_COLS, 0x00);
  }

  bool fullWidth = hideIconPressure || scrollIconInBuffer(scrollTypePressure);
  int textOffset = fullWidth ? 0 : (PRESSURE_ICON_COLS + PRESSURE_SEP_COLS);
  int textCols   = fullWidth ? 32 : PRESSURE_TEXT_COLS;
  for (int col = 0; col < textCols; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < pressureColCount) ? pressureColBuf[src] : 0x00;
    mx.setColumn(31 - (textOffset + col), val);
  }

  mxCommit();
}

void pressureInit(int hpaInt) {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mxCommit();

  pressureBuildBuffer(hpaInt);
  int pressureEffTextCols = (hideIconPressure || scrollIconInBuffer(scrollTypePressure)) ? 32 : PRESSURE_TEXT_COLS;
  pressureNeedsScroll = (pressureColCount > pressureEffTextCols);

  if (pressureNeedsScroll) {
    pressureScrollPos = 0;
    pressureState     = NP_PAUSE_BEFORE_RIGHT;
    pressurePauseMs   = millis();
    pressureDrawAtPos(0);
  } else {

    pressureDrawAtPos(0);
  }
}

// CURRENCY STANDARDS 

#define CURRENCY_FETCH_INTERVAL_MS   (6UL * 60UL * 60UL * 1000UL)
#define CURRENCY_TREND_THRESH_PCT    0.1f

char          currencyBase[4]     = "EUR";
char          currencyQuote[4]    = "RON";
bool          currencyCompareEnabled = false;
float         currencyRateNow     = NAN;
float         currencyRateMonthAgo = NAN;
bool          currencyValid       = false;
int8_t        currencyTrend       = 0;
unsigned long lastCurrencyFetch   = 0;

static inline const uint8_t* currencyIconForTrend() {
  const uint8_t* base = pressureIconFlat;
  if (currencyTrend > 0) base = pressureIconUp;
  else if (currencyTrend < 0) base = pressureIconDown;
  return resolveTileIcon(iconSelCurrency, base);
}

static uint8_t  currColBuf[128];
static int      currColCount = 0;
static int      currScrollPos = 0;
static unsigned long currLastScrollMs = 0;
static NpState  currState    = NP_SHOW_START;
static unsigned long currPauseMs = 0;
static bool     currNeedsScroll = false;
static int      currWrapPass = 0;

static void currBufAppendMD(char ch) {
  appendGlyphAuto(currColBuf, currColCount, 127, ch, fontTypeCurrency);
}

void currencyBuildBuffer() {
  currColCount = 0;
  if (scrollIconInBuffer(scrollTypeCurrency) && !hideIconCurrency) {
    scrollBufPrependIcon(currColBuf, currColCount, 127, currencyIconForTrend(), PRESSURE_ICON_COLS);
  }
  if (currencyCompareEnabled) {
    currBufAppendMD('1');
    currBufAppendMD(' ');
  }
  for (int ci = 0; currencyBase[ci] != '\0'; ci++) currBufAppendMD(currencyBase[ci]);
  if (currencyCompareEnabled) {
    currBufAppendMD(' ');
    currBufAppendMD('=');
    currBufAppendMD(' ');
    char rateStr[16];
    if (currencyValid && !isnan(currencyRateNow)) {

      if (fabsf(currencyRateNow) >= 1.0f) snprintf(rateStr, sizeof(rateStr), "%.2f", currencyRateNow);
      else                                snprintf(rateStr, sizeof(rateStr), "%.4f", currencyRateNow);
    } else {
      strcpy(rateStr, "...");
    }
    for (int ci = 0; rateStr[ci] != '\0'; ci++) currBufAppendMD(rateStr[ci]);
    currBufAppendMD(' ');
    for (int ci = 0; currencyQuote[ci] != '\0'; ci++) currBufAppendMD(currencyQuote[ci]);
  }

  if (currColCount > 0) currColCount--;
}

void currencyDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);

  if (!hideIconCurrency && !scrollIconInBuffer(scrollTypeCurrency)) {
    const uint8_t* icon = currencyIconForTrend();
    for (int col = 0; col < PRESSURE_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (icon[row] & (0x80 >> col)) colVal |= (1 << row);
      }
      mx.setColumn(31 - col, colVal);
    }
    mx.setColumn(31 - PRESSURE_ICON_COLS, 0x00);
  }

  bool fullWidth = hideIconCurrency || scrollIconInBuffer(scrollTypeCurrency);
  int textOffset = fullWidth ? 0 : (PRESSURE_ICON_COLS + PRESSURE_SEP_COLS);
  int textCols   = fullWidth ? 32 : PRESSURE_TEXT_COLS;
  for (int col = 0; col < textCols; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < currColCount) ? currColBuf[src] : 0x00;
    mx.setColumn(31 - (textOffset + col), val);
  }

  mxCommit();
}

void currencyInit() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mxCommit();

  currencyBuildBuffer();
  int currEffTextCols = (hideIconCurrency || scrollIconInBuffer(scrollTypeCurrency)) ? 32 : PRESSURE_TEXT_COLS;
  currNeedsScroll = (currColCount > currEffTextCols);

  currScrollPos = 0;
  currWrapPass  = 0;
  currencyDrawAtPos(0);
  currState   = NP_PAUSE_BEFORE_RIGHT;
  currPauseMs = millis();
}

static bool currencyFetchOne(const char* base, const char* quote, const char* dateStr, float& outRate) {
  if (WiFi.status() != WL_CONNECTED) return false;
  String url = "https://api.frankfurter.dev/v1/" + String(dateStr) +
               "?base=" + String(base) + "&symbols=" + String(quote);
  HTTPClient http;
  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    String body = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, body);
    if (!err) {
      float r = doc["rates"][quote] | NAN;
      if (!isnan(r)) { outRate = r; ok = true; }
    }
  }
  http.end();
  return ok;
}

// --- Non-blocking currency fetch ---------------------------------------
// Same problem as weatherFetch(), only worse: this ran up to TWO blocking
// HTTP requests back to back (currencyFetchOne has an 8s timeout each,
// see above), which could freeze the whole display for up to ~16 seconds
// right in the middle of loop(). Moved to a background task for the same
// reasons as weatherFetch().
static TaskHandle_t  currencyFetchTaskHandle = nullptr;
static volatile bool currencyFetchBusy       = false;
static volatile bool currencyFetchReady      = false;
static volatile bool currFetchOk             = false;
static float         currFetchRateNow;
static float         currFetchRateMonthAgo;
static bool          currFetchHaveMonthAgo;
static int8_t        currFetchTrend;

static void currencyFetchTask(void* pv) {
  bool ok = false;
  currFetchHaveMonthAgo = false;

  const char* refQuote = currencyCompareEnabled ? currencyQuote
                        : (strcasecmp(currencyBase, "EUR") == 0 ? "USD" : "EUR");

  struct tm ti;
  if (readLocalTime(ti)) {
    time_t nowEpoch = mktime(&ti);
    time_t monthAgoEpoch = nowEpoch - (30L * 24L * 60L * 60L);
    struct tm* tiMonthAgo = gmtime(&monthAgoEpoch);
    char monthAgoStr[11];
    snprintf(monthAgoStr, sizeof(monthAgoStr), "%04d-%02d-%02d",
             tiMonthAgo->tm_year + 1900, tiMonthAgo->tm_mon + 1, tiMonthAgo->tm_mday);

    float rNow = NAN, rOld = NAN;
    bool okNow = currencyFetchOne(currencyBase, refQuote, "latest", rNow);
    bool okOld = okNow && currencyFetchOne(currencyBase, refQuote, monthAgoStr, rOld);

    if (okNow) {
      currFetchRateNow = rNow;
      ok = true;
      if (okOld && rOld > 0) {
        currFetchRateMonthAgo = rOld;
        currFetchHaveMonthAgo = true;
        float pctDiff = (rNow - rOld) / rOld * 100.0f;
        if (pctDiff >= CURRENCY_TREND_THRESH_PCT)       currFetchTrend = 1;
        else if (pctDiff <= -CURRENCY_TREND_THRESH_PCT) currFetchTrend = -1;
        else                                            currFetchTrend = 0;
      }
    }
  }

  currFetchOk          = ok;
  currencyFetchReady    = true;
  currencyFetchBusy     = false;
  vTaskDelete(nullptr);
}

void currencyFetch() {
  if (currencyFetchBusy) return; // a fetch is already in flight

  lastCurrencyFetch  = millis(); // stamp now so loop() doesn't re-trigger every iteration
  currencyFetchBusy  = true;
  currencyFetchReady = false;
  xTaskCreatePinnedToCore(currencyFetchTask, "currFetch", 8192, nullptr, 1, &currencyFetchTaskHandle, 0);
}

// Call from loop(); cheap check, only does work once a fetch has completed.
static void currencyFetchPoll() {
  if (!currencyFetchReady) return;
  currencyFetchReady = false;
  if (currFetchOk) {
    currencyRateNow = currFetchRateNow;
    currencyValid   = true;
    if (currFetchHaveMonthAgo) {
      currencyRateMonthAgo = currFetchRateMonthAgo;
      currencyTrend        = currFetchTrend;
    }
  }
}

// SUBSCRIBER COUNTER TILE (YouTube)
//
// Shows the subscriber count as "icon + number", scrolling when the number does
// not fit - same behaviour as the currency tile.
//
// Only YouTube is implemented: it is the one platform of the obvious three that
// publishes a usable public endpoint (Data API v3, with a free API key). TikTok
// and Instagram expose no follower count without either an approved OAuth app or
// a third-party scraper, so tiles for them would have had no dependable source.
// The structure below is kept indexable so another platform can be slotted in if
// a workable source ever appears.

#define SOCIAL_COUNT 1
#define SOC_YT 0
#define SOCIAL_FETCH_INTERVAL_MS (10UL * 60UL * 1000UL)

struct SocialCfg {
  char     handle[80];    // channel link, @handle or channel id
  char     apiKey[80];    // YouTube Data API key
  uint32_t count;
  bool     valid;
  bool     showName;      // false: "12400"   true: "NUME: 12400"
};
SocialCfg     social[SOCIAL_COUNT];
unsigned long lastSocialFetch[SOCIAL_COUNT] = { 0 };

uint8_t iconSelYoutube = 0;

static inline int socialIndexForItem(uint8_t id) {
  return (id == ITEM_YOUTUBE) ? SOC_YT : -1;
}

static const uint8_t* socialIcon(int i) {
  (void)i;
  // Reuses the existing video glyph unless the user picked one in Icon Settings.
  return resolveTileIcon(iconSelYoutube, videoIcon);
}

// --- render -----------------------------------------------------------------
static uint8_t  socColBuf[128];
static int      socColCount = 0;
static int      socScrollPos = 0;
static unsigned long socLastScrollMs = 0;
static NpState  socState = NP_SHOW_START;
static unsigned long socPauseMs = 0;
static bool     socNeedsScroll = false;
static int      socWrapPass = 0;
static int      socActiveIdx = -1;

static void socBufAppend(char ch) {
  appendGlyphAuto(socColBuf, socColCount, 127, ch, fontTypeYoutube);
}

// Turns whatever the user typed - a full link, an @handle or a bare name - into
// something short enough to sit in front of the count. The '@' is dropped on
// purpose: the Tiko font has no glyph for it and would render a blank gap.
static void socialDisplayName(int i, char* out, size_t n) {
  const char* h = social[i].handle;
  const char* p = strrchr(h, '/');
  p = p ? p + 1 : h;
  if (*p == '@') p++;
  size_t len = 0;
  while (p[len] != '\0' && p[len] != '?' && len < n - 1) len++;
  memcpy(out, p, len);
  out[len] = '\0';
}

void socialBuildBuffer(int i) {
  socColCount = 0;
  if (scrollIconInBuffer(scrollTypeYoutube) && !hideIconYoutube) {
    scrollBufPrependIcon(socColBuf, socColCount, 127, socialIcon(i), PRESSURE_ICON_COLS);
  }
  char txt[96];
  char num[16];
  if (social[i].valid) snprintf(num, sizeof(num), "%lu", (unsigned long)social[i].count);
  else                 strcpy(num, "...");

  if (social[i].showName) {
    char name[32];
    socialDisplayName(i, name, sizeof(name));
    if (name[0] != '\0') snprintf(txt, sizeof(txt), "%s: %s", name, num);
    else                 snprintf(txt, sizeof(txt), "%s", num);
  } else {
    snprintf(txt, sizeof(txt), "%s", num);
  }

  for (int ci = 0; txt[ci] != '\0'; ci++) socBufAppend(txt[ci]);
  if (socColCount > 0) socColCount--;
}

void socialDrawAtPos(int pos) {
  int i = socActiveIdx;
  if (i < 0) return;
  mx.update(MD_MAX72XX::OFF);

  // Blank first so no pixel from the previous frame can survive in a column
  // this one does not write.
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);

  if (!hideIconYoutube && !scrollIconInBuffer(scrollTypeYoutube)) {
    const uint8_t* icon = socialIcon(i);
    for (int col = 0; col < PRESSURE_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (icon[row] & (0x80 >> col)) colVal |= (1 << row);
      }
      int physCol = 31 - col;
      if (physCol >= 0 && physCol < 32) mx.setColumn(physCol, colVal);
    }
  }

  bool fullWidth = hideIconYoutube || scrollIconInBuffer(scrollTypeYoutube);
  int textOffset = fullWidth ? 0 : (PRESSURE_ICON_COLS + PRESSURE_SEP_COLS);
  int textCols   = fullWidth ? 32 : PRESSURE_TEXT_COLS;
  for (int c = 0; c < textCols; c++) {
    int srcCol = pos + c;
    uint8_t colVal = (srcCol >= 0 && srcCol < socColCount) ? socColBuf[srcCol] : 0x00;
    int physCol = 31 - (textOffset + c);
    if (physCol >= 0 && physCol < 32) mx.setColumn(physCol, colVal);
  }
  mxCommit();
}

void socialInit(uint8_t itemId) {
  int i = socialIndexForItem(itemId);
  if (i < 0) return;
  socActiveIdx = i;

  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mxCommit();

  socialBuildBuffer(i);
  int effTextCols = (hideIconYoutube || scrollIconInBuffer(scrollTypeYoutube)) ? 32 : PRESSURE_TEXT_COLS;
  socNeedsScroll = (socColCount > effTextCols);
  socWrapPass    = 0;

  socScrollPos = 0;
  if (socNeedsScroll) {
    socState   = NP_PAUSE_BEFORE_RIGHT;
    socPauseMs = millis();
  } else {
    // Fits on the matrix: leave the scroll state machine idle. Nothing is
    // offset - the icon sits at the left edge and the number follows it.
    socState = NP_SHOW_START;
  }
  socialDrawAtPos(0);
}

// --- fetch ------------------------------------------------------------------
// The API returns subscriberCount as a *string*, so it is read as one and
// converted rather than relying on an implicit numeric cast.
static bool socialFetchYouTube(int i, uint32_t& out) {
  if (social[i].apiKey[0] == '\0' || social[i].handle[0] == '\0') return false;

  String h = String(social[i].handle);
  h.trim();
  String param;
  int at = h.lastIndexOf('@');
  if (at >= 0) {
    String tail = h.substring(at + 1);
    int slash = tail.indexOf('/');
    if (slash >= 0) tail = tail.substring(0, slash);
    param = "forHandle=@" + urlEncode(tail);
  } else {
    int slash = h.lastIndexOf('/');
    String tail = (slash >= 0) ? h.substring(slash + 1) : h;
    int q = tail.indexOf('?');
    if (q >= 0) tail = tail.substring(0, q);
    param = tail.startsWith("UC") ? ("id=" + urlEncode(tail))
                                  : ("forUsername=" + urlEncode(tail));
  }

  String url = "https://www.googleapis.com/youtube/v3/channels?part=statistics&" +
               param + "&key=" + urlEncode(String(social[i].apiKey));
  HTTPClient http;
  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    String body = http.getString();
    DynamicJsonDocument doc(2048);
    if (!deserializeJson(doc, body)) {
      const char* s = doc["items"][0]["statistics"]["subscriberCount"] | (const char*)nullptr;
      if (s && *s) { out = (uint32_t)strtoul(s, nullptr, 10); ok = true; }
    }
  } else {
    Serial.printf("[social] YouTube HTTP %d\n", code);
  }
  http.end();
  return ok;
}

// Runs on core 0 like the weather and currency fetches, so the matrix and the
// web server are never blocked by the request.
static volatile bool socialFetchBusy  = false;
static volatile bool socialFetchReady = false;
static volatile int  socialFetchIdx   = -1;
static volatile bool socialFetchOk    = false;
static uint32_t      socialFetchValue = 0;

static void socialFetchTask(void* pv) {
  int i = socialFetchIdx;
  uint32_t n = 0;
  bool ok = false;
  if (i >= 0 && WiFi.status() == WL_CONNECTED) {
    ok = socialFetchYouTube(i, n);
  }
  socialFetchValue = n;
  socialFetchOk    = ok;
  socialFetchReady = true;
  socialFetchBusy  = false;
  vTaskDelete(nullptr);
}

void socialFetch(int i) {
  if (i < 0 || i >= SOCIAL_COUNT) return;
  if (socialFetchBusy) return;
  if (WiFi.status() != WL_CONNECTED) return;
  lastSocialFetch[i] = millis();
  socialFetchIdx   = i;
  socialFetchBusy  = true;
  socialFetchReady = false;
  xTaskCreatePinnedToCore(socialFetchTask, "socFetch", 8192, nullptr, 1, nullptr, 0);
}

// Cheap check from loop(); only does work once a fetch has landed.
static void socialFetchPoll() {
  if (!socialFetchReady) return;
  socialFetchReady = false;
  int i = socialFetchIdx;
  if (i < 0 || i >= SOCIAL_COUNT) return;
  if (socialFetchOk) {
    social[i].count = socialFetchValue;
    social[i].valid = true;
    if (items[currentSlot].id == ITEM_YOUTUBE + i) socialInit(items[currentSlot].id);
  }
}

// Refreshes whichever tiles are enabled, one at a time.
static void socialTickFetch() {
  if (socialFetchBusy) return;
  for (int i = 0; i < SOCIAL_COUNT; i++) {
    uint8_t id = ITEM_YOUTUBE + i;
    // Cheapest question first. The items[] scan used to run on every pass
    // through loop(); now it only runs once a refresh is actually due.
    if (lastSocialFetch[i] != 0 && millis() - lastSocialFetch[i] < SOCIAL_FETCH_INTERVAL_MS) continue;
    if (!tileInManager(id)) continue;
    bool enabled = false;
    for (int k = 0; k < NUM_ITEMS; k++)
      if (items[k].id == id && items[k].enabled) { enabled = true; break; }
    if (!enabled) continue;
    socialFetch(i);
    return;
  }
}

// ETS2 SPEED DISPLAY

void ets2BuildBuffer(int speed) {
  ets2ColCount = 0;
  if (speed < 0) speed = 0;
  if (speed > 999) speed = 999;

  if (speed >= 100) {
    tempAddGlyphTo(ets2ColBuf, ets2ColCount, FONT[speed / 100]);
    tempAddGlyphTo(ets2ColBuf, ets2ColCount, FONT[(speed / 10) % 10]);
    tempAddGlyphTo(ets2ColBuf, ets2ColCount, FONT[speed % 10]);
  } else if (speed >= 10) {
    tempAddGlyphTo(ets2ColBuf, ets2ColCount, FONT[speed / 10]);
    tempAddGlyphTo(ets2ColBuf, ets2ColCount, FONT[speed % 10]);
  } else {
    tempAddGlyphTo(ets2ColBuf, ets2ColCount, FONT[speed]);
  }

  uint8_t tmp[8];
  uint8_t n;

  n = mx.getChar('k', sizeof(tmp), tmp);
  for (int i = 0; i < n && ets2ColCount < 63; i++) ets2ColBuf[ets2ColCount++] = tmp[i];
  if (ets2ColCount < 63) ets2ColBuf[ets2ColCount++] = 0x00;

  n = mx.getChar('m', sizeof(tmp), tmp);
  for (int i = 0; i < n && ets2ColCount < 63; i++) ets2ColBuf[ets2ColCount++] = tmp[i];
}

void ets2DrawAtPos() {
  mx.update(MD_MAX72XX::OFF);

  int offset = (32 - ets2ColCount) / 2;
  if (offset < 0) offset = 0;

  for (int col = 0; col < 32; col++) {
    int src = col - offset;
    uint8_t val = (src >= 0 && src < ets2ColCount) ? ets2ColBuf[src] : 0x00;
    mx.setColumn(31 - col, val);
  }

  mxCommit();
}

void ets2Init() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mxCommit();

  ets2BuildBuffer(ets2Speed);
  ets2DrawAtPos();
  ets2LastSpeed = ets2Speed;
}

bool ets2Tick() {
  if (!ets2Active) return false;

  if (millis() - lastEts2Ms > ETS2_TIMEOUT_MS) {
    ets2Active = false;
    beginP2CCapture();
    CycleItem& cur = items[currentSlot];
    if (cur.id == ITEM_NOW_PLAYING) npInit();
    else if (cur.id == ITEM_WEATHER) weatherInit();
    else if (cur.id == ITEM_MEMENTO) mementoInit();
    else if (cur.id == ITEM_CANVAS) drawCanvas();
    else if (cur.id == ITEM_CURRENCY) currencyInit();
    else if (socialIndexForItem(cur.id) >= 0) socialInit(cur.id);
    else { gLastStaticDrawMs = 0; gStaticDrawDone = false; }
    finishP2CTransition();
    return false;
  }

  if (ets2Speed != ets2LastSpeed) {
    ets2Init();
  }
  return true;
}

static int writeDigit(int d, int col) {
  writeChar(FONT[d], col); col += 5;
  writeGap(col);           col += 1;
  return col;
}

static int writeDigitFont(int d, int col, uint8_t fontMode) {
  uint8_t tmp[TIKO_MAX_COLS]; int w;
  const uint8_t* g = digitGlyph(fontMode, d, tmp, w);
  writeCharN(g, w, col); col += w;
  writeGap(col);         col += 1;
  return col;
}

static void dateAddGlyph(const uint8_t* glyph, int width = 5) {
  for (int i = 0; i < width && dateColCount < 126; i++)
    dateColBuf[dateColCount++] = glyph[i];
  if (dateColCount < 127) dateColBuf[dateColCount++] = 0x00;
}

static void dateAddNumber(int value, int digits) {
  int divisor = 1;
  for (int i = 1; i < digits; i++) divisor *= 10;
  for (int i = 0; i < digits; i++) {
    int d = (value / divisor) % 10;
    uint8_t dtmp[TIKO_MAX_COLS]; int dw;
    dateAddGlyph(digitGlyph(fontTypeDate, d, dtmp, dw), dw);
    divisor /= 10;
  }
}

const char* const WEEKDAY_NAMES_EN[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char* const WEEKDAY_NAMES_RO[7] = { "Dum", "Lun", "Mar", "Mie", "Joi", "Vin", "Sam" };

static void dateAddChar(char ch) {
  if (isPixelFont(fontTypeDate)) {
    uint8_t tmp[TIKO_MAX_COLS];
    uint8_t n = pixGetChar(fontTypeDate, ch, sizeof(tmp), tmp);
    if (n == 0) return;
    for (int i = 0; i < n && dateColCount < 126; i++) dateColBuf[dateColCount++] = tmp[i];
    if (dateColCount < 127) dateColBuf[dateColCount++] = 0x00;
    return;
  }
  if (ch >= '0' && ch <= '9') {
    dateAddGlyph(FONT[ch - '0']);
  } else {
    uint8_t tmp[8];
    uint8_t n = mx.getChar(ch, sizeof(tmp), tmp);
    if (n == 0) return;
    for (int i = 0; i < n && dateColCount < 126; i++) dateColBuf[dateColCount++] = tmp[i];
    if (dateColCount < 127) dateColBuf[dateColCount++] = 0x00;
  }
}

static void dateAddText(const char* s) {
  for (int i = 0; s[i] != '\0'; i++) dateAddChar(s[i]);
}

const char* const WEEKDAY_FULL_EN[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
const char* const WEEKDAY_FULL_RO[7] = { "Duminica", "Luni", "Marti", "Miercuri", "Joi", "Vineri", "Sambata" };

// Validates a custom date format pattern made of Y/M/D/W runs and literal '/' characters.
// Rules: Y run must be exactly 2 or 4 chars; M and D runs must be exactly 2 chars;
// W runs accept any length; any other character is invalid.
static bool isDateFmtLiteralChar(char c) {
  return c == '/' || c == '.' || c == '(' || c == ')' || c == ' ';
}

static bool dateValidateCustomFormat(const char* pattern, String* errOut = nullptr) {
  int len = strlen(pattern);
  if (len == 0 || len > 20) {
    if (errOut) *errOut = "Format invalid";
    return false;
  }
  bool hasContent = false;
  int i = 0;
  while (i < len) {
    char c = toupper(pattern[i]);
    if (c != 'Y' && c != 'M' && c != 'D' && c != 'W' && !isDateFmtLiteralChar(pattern[i])) {
      if (errOut) *errOut = "Caracter invalid";
      return false;
    }
    int j = i;
    while (j < len && toupper(pattern[j]) == c) j++;
    int count = j - i;
    if (c == 'Y') {
      hasContent = true;
      if (count != 2 && count != 4) {
        if (errOut) *errOut = "Y trebuie sa fie YY sau YYYY";
        return false;
      }
    } else if (c == 'M') {
      hasContent = true;
      if (count != 2) {
        if (errOut) *errOut = "M trebuie sa fie MM";
        return false;
      }
    } else if (c == 'D') {
      hasContent = true;
      if (count != 2) {
        if (errOut) *errOut = "D trebuie sa fie DD";
        return false;
      }
    } else if (c == 'W') {
      hasContent = true;
    }
    i = j;
  }
  if (!hasContent) {
    if (errOut) *errOut = "Formatul trebuie sa contina cel putin Y, M, D sau W";
    return false;
  }
  return true;
}

// Renders a validated custom pattern into the date scroll buffer.
static void dateRenderCustom(const char* pattern, int day, int month, int year, int wday) {
  int len = strlen(pattern);
  int i = 0;
  if (wday < 0 || wday > 6) wday = 0;
  while (i < len) {
    char c = toupper(pattern[i]);
    int j = i;
    while (j < len && toupper(pattern[j]) == c) j++;
    int count = j - i;
    if (c == 'Y') {
      if (count == 2) dateAddNumber(year % 100, 2);
      else dateAddNumber(year, 4);
    } else if (c == 'M') {
      dateAddNumber(month, 2);
    } else if (c == 'D') {
      dateAddNumber(day, 2);
    } else if (c == 'W') {
      const char* full = (dateLang == 1) ? WEEKDAY_FULL_RO[wday] : WEEKDAY_FULL_EN[wday];
      int flen = strlen(full);
      if (count >= flen) {
        dateAddText(full);
      } else {
        char buf[16];
        int n = (count < (int)sizeof(buf) - 1) ? count : (int)sizeof(buf) - 1;
        memcpy(buf, full, n);
        buf[n] = '\0';
        dateAddText(buf);
      }
    } else if (isDateFmtLiteralChar(pattern[i])) {
      for (int k = 0; k < count; k++) dateAddChar(pattern[i + k]);
    }
    i = j;
  }
}

void dateBuildBuffer(int day, int month, int year, int wday) {
  dateColCount = 0;
  if (scrollIconInBuffer(scrollTypeDate) && !hideIconDate) {
    scrollBufPrependIcon(dateColBuf, dateColCount, 127, resolveTileIcon(iconSelDate, dateIcon), DATE_ICON_COLS);
  }
  int yy = year % 100;

  switch (dateFormat) {
    case 0:
      dateAddNumber(year, 4);
      dateAddDot();
      dateAddNumber(month, 2);
      dateAddDot();
      dateAddNumber(day, 2);
      break;
    case 1:
      dateAddNumber(yy, 2);
      dateAddDot();
      dateAddNumber(month, 2);
      dateAddDot();
      dateAddNumber(day, 2);
      break;
    case 3:
      dateAddNumber(day, 2);
      dateAddDot();
      dateAddNumber(month, 2);
      break;
    case 4: {
      if (wday < 0 || wday > 6) wday = 0;
      const char* const* names = (dateLang == 1) ? WEEKDAY_NAMES_RO : WEEKDAY_NAMES_EN;
      dateAddText(names[wday]);
      dateAddChar(' ');
      dateAddNumber(day, 2);
      break;
    }
    case 5:
      dateRenderCustom(customDateFmt, day, month, year, wday);
      break;
    default:
      dateAddNumber(day, 2);
      dateAddDot();
      dateAddNumber(month, 2);
      dateAddDot();
      dateAddNumber(year, 4);
      break;
  }

  if (dateColCount > 0) dateColCount--;
}

void dateDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);

  if (!hideIconDate && !scrollIconInBuffer(scrollTypeDate)) {

    // Resolved once rather than 64 times inside the loop below.
    const uint8_t* dateIconBmp = resolveTileIcon(iconSelDate, dateIcon);
    for (int col = 0; col < DATE_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (dateIconBmp[row] & (0x80 >> col)) colVal |= (1 << row);
      }
      mx.setColumn(31 - col, colVal);
    }
    mx.setColumn(31 - DATE_ICON_COLS, 0x00);
  }

  bool fullWidth = hideIconDate || scrollIconInBuffer(scrollTypeDate);
  int textOffset = fullWidth ? 0 : (DATE_ICON_COLS + DATE_SEP_COLS);
  int textCols   = fullWidth ? 32 : DATE_TEXT_COLS;
  for (int col = 0; col < textCols; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < dateColCount) ? dateColBuf[src] : 0x00;
    mx.setColumn(31 - (textOffset + col), val);
  }

  mxCommit();
}

// Returns whether it actually put something on the panel. The caller has
// already cleared the display by the time this runs, so "did not draw" and
// "left the panel blank" are the same thing, and the caller has to know.
bool dateInit() {
  struct tm ti;
  if (!readLocalTime(ti)) return false;
  int day   = ti.tm_mday;
  int month = ti.tm_mon + 1;
  int year  = ti.tm_year + 1900;
  int wday  = ti.tm_wday;

  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mxCommit();

  dateBuildBuffer(day, month, year, wday);
  int dateEffTextCols = (hideIconDate || scrollIconInBuffer(scrollTypeDate)) ? 32 : DATE_TEXT_COLS;
  dateNeedsScroll = (dateColCount > dateEffTextCols);

  if (dateNeedsScroll) {
    dateScrollPos = 0;
    dateState     = NP_PAUSE_BEFORE_RIGHT;
    datePauseMs   = millis();
    dateDrawAtPos(0);
  } else {

    int offset = (dateEffTextCols - dateColCount) / 2;
    if (offset < 0) offset = 0;
    dateDrawAtPos(-offset);
  }
  return true;
}

void drawDate() {
  dateInit();
}

void drawHour() {
  struct tm ti;
  if (!readLocalTime(ti)) return;
  int h     = ti.tm_hour;
  int m     = ti.tm_min;
  int s     = ti.tm_sec;
  bool show = (s % 2 == 0);

  bool is12 = (hourFormat == 1);
  bool pm   = false;
  if (is12) {
    pm = (h >= 12);
    h  = h % 12;
    if (h == 0) h = 12;
  }

  mx.update(MD_MAX72XX::OFF);
  for (int i = 0; i < 32; i++) mx.setColumn(i, 0x00);

  // A leading zero only ever adds the tens digit; the centring below already
  // works off twoDigitH, so nothing else has to change.
  bool twoDigitH = (h >= 10) || hourLeadingZero;
  bool tiko = isPixelFont(fontType);   // Tiko or Mako
  // Every font keeps its digits one width, so the width of '0' is the width
  // of all of them: Marymba 5, Mako 4, Tiko 3. The centring below needs it.
  uint8_t w0tmp[TIKO_MAX_COLS]; int digitW;
  digitGlyph(fontType, 0, w0tmp, digitW);

  uint8_t colonTmp[TIKO_MAX_COLS];
  const uint8_t* colonGlyph = CHAR_COLON;
  int colonW = 2;
  if (tiko) { colonW = pixGetChar(fontType, ':', TIKO_MAX_COLS, colonTmp); colonGlyph = colonTmp; }

  int totalWidth = (twoDigitH ? 2 : 1) * (digitW + 1) + (colonW + 1) + 2 * digitW + 1;
  int col = (32 - totalWidth) / 2;

  if (twoDigitH) {
    col = writeDigitFont(h / 10, col, fontType);
  }
  col = writeDigitFont(h % 10, col, fontType);
  if (show) {
    writeCharN(colonGlyph, colonW, col); col += colonW; writeGap(col); col += 1;
  } else {
    for (int i = 0; i < colonW + 1; i++) { mx.setColumn(31 - col, 0x00); col++; }
  }
  col = writeDigitFont(m / 10, col, fontType);
  uint8_t dtmp[TIKO_MAX_COLS]; int dw;
  writeCharN(digitGlyph(fontType, m % 10, dtmp, dw), dw, col);

  mxCommit();
}

// Hour and Weekday tile: Tiko time on one side, the weekday in the Micro font
// centred in 13 columns on the other, and a one-row bar across those 13 either
// under the day (day on rows 0-4, bar on 6) or over it (bar on 0, day on 2-6),
// so both line up with the 7-row time. The time takes columns 0..16 and the day
// 19..31, or with hwSwap the day 0..12 and the time 15..31. Redrawn every
// 200 ms, like drawHour().
static int hwPutCols(uint8_t* fb, int col, const uint8_t* g, int n, int shift) {
  for (int i = 0; i < n; i++, col++)
    if (col >= 0 && col < 32) fb[col] |= (uint8_t)(g[i] << shift);
  return col;
}

void drawHourWeekday() {
  struct tm ti;
  if (!readLocalTime(ti)) return;
  int h = ti.tm_hour;
  if (hwFormat == 1) {
    h = h % 12;
    if (h == 0) h = 12;
  }
  bool twoDigitH = (h >= 10) || hwLeadZero;

  uint8_t fb[32] = { 0 };
  uint8_t g[TIKO_MAX_COLS];

  // 3-column digits with a gap after each, and a one-column colon instead of
  // Tiko's three-wide one, or "12:45 WED" would not fit. It hugs the day: on
  // the left it ends at column 16, on the right it starts at 15, so the gap
  // between the two is the same both ways.
  int col = hwSwap ? 15 : 17 - ((twoDigitH ? 7 : 3) + 3 + 7);
  if (twoDigitH) {
    tikoGetChar((char)('0' + h / 10), sizeof(g), g);
    col = hwPutCols(fb, col, g, 3, 0) + 1;
  }
  tikoGetChar((char)('0' + h % 10), sizeof(g), g);
  col = hwPutCols(fb, col, g, 3, 0) + 1;
  if (ti.tm_sec % 2 == 0) fb[col] |= 0x36;
  col += 2;
  tikoGetChar((char)('0' + ti.tm_min / 10), sizeof(g), g);
  col = hwPutCols(fb, col, g, 3, 0) + 1;
  tikoGetChar((char)('0' + ti.tm_min % 10), sizeof(g), g);
  hwPutCols(fb, col, g, 3, 0);

  bool above = (hwBarPos == HW_BAR_ABOVE);
  int dayX = hwSwap ? 0 : 19;   // first of the 13 columns the day and bar share
  const char* day = (dateLang == 1) ? WEEKDAY_NAMES_RO[ti.tm_wday] : WEEKDAY_NAMES_EN[ti.tm_wday];
  int dayW = -1;
  for (const char* p = day; *p; p++) dayW += microGetChar(*p, g) + 1;
  // Centred in its 13 columns; "WED", the widest name, fills them exactly.
  col = dayX + (dayW < 13 ? (13 - dayW) / 2 : 0);
  for (const char* p = day; *p; p++) {
    uint8_t n = microGetChar(*p, g);
    col = hwPutCols(fb, col, g, n, above ? 2 : 0) + 1;
  }

  int barEnd = dayX + ((hwBarMode == HW_BAR_SECONDS) ? ti.tm_sec * 13 / 60 : 12);
  for (int c = dayX; c <= barEnd; c++) fb[c] |= above ? 0x01 : 0x40;

  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(31 - c, fb[c]);
  mxCommit();
}

// NP MANUAL SCROLL
#define NP_SCROLL_SPEED_MS SCROLL_SPEED_DEFAULT

#define NP_DISPLAY_COLS    32

// Room for the whole text in the widest font (6 columns a letter with the
// gap) plus the icon. At 512 anything past ~85 characters was cut off.
#define NP_COL_MAX 1830
static uint8_t  npColBuf[NP_COL_MAX];
static int      npColCount  = 0;
static int      npScrollPos = 0;
static unsigned long npLastScrollMs = 0;


void npBuildBuffer() {

  char cleanBuf[sizeof(nowPlayingBuf)];
  sanitizeUtf8(nowPlayingBuf, cleanBuf, sizeof(cleanBuf));

  npColCount = 0;
  if (scrollIconInBuffer(scrollTypeNowPlaying) && !hideIconNowPlaying) {
    scrollBufPrependIcon(npColBuf, npColCount, NP_COL_MAX, npResolvedIcon(), NP_ICON_COLS);
  }
  for (int ci = 0; cleanBuf[ci] != '\0' && npColCount < NP_COL_MAX - 12; ci++) {
    appendGlyphAuto(npColBuf, npColCount, NP_COL_MAX, cleanBuf[ci], fontTypeNowPlaying);
  }

  if (npColCount > 0) npColCount--;
}

void npDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);

  if (!hideIconNowPlaying && !scrollIconInBuffer(scrollTypeNowPlaying)) {

    for (int col = 0; col < NP_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (npResolvedIcon()[row] & (0x80 >> col)) colVal |= (1 << row);
      }
      mx.setColumn(31 - col, colVal);
    }

    mx.setColumn(31 - NP_ICON_COLS, 0x00);
  }

  bool fullWidth = hideIconNowPlaying || scrollIconInBuffer(scrollTypeNowPlaying);
  int textOffset = fullWidth ? 0 : (NP_ICON_COLS + NP_SEP_COLS);
  int textCols   = fullWidth ? 32 : NP_TEXT_COLS;
  for (int col = 0; col < textCols; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < npColCount) ? npColBuf[src] : 0x00;
    mx.setColumn(31 - (textOffset + col), val);
  }

  mxCommit();
}

void npInit() {

  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < NP_DISPLAY_COLS; c++) mx.setColumn(c, 0x00);
  mxCommit();
  npBuildBuffer();
  npScrollPos = 0;
  npWrapPass = 0;
  npDrawAtPos(0);
  npState = NP_PAUSE_BEFORE_RIGHT;
  npPauseStartMs = millis();
}

// STOPWATCH DISPLAY

static uint8_t  swColBuf[160];
static int      swColCount     = 0;
static int      swScrollPos    = 0;
static int      swWrapPass     = 0;
static unsigned long swLastScrollMs  = 0;
static unsigned long swLastRedrawMs  = 0;
NpState       swState          = NP_SHOW_START;
unsigned long swPauseStartMs   = 0;

void swBuildBuffer() {
  char txt[16];
  swFormatAdaptive(txt, sizeof(txt), swGetElapsedMs());
  swColCount = 0;
  for (int ci = 0; txt[ci] != '\0' && swColCount < 150; ci++) {
    appendGlyphAuto(swColBuf, swColCount, 160, txt[ci], fontTypeStopwatch);
  }
  if (swColCount > 0) swColCount--;
}

void swDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);
  for (int col = 0; col < 32; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < swColCount) ? swColBuf[src] : 0x00;
    mx.setColumn(31 - col, val);
  }
  mxCommit();
}

void swInit() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mxCommit();
  swBuildBuffer();
  swScrollPos = 0;
  swLastRedrawMs = millis();
  if (swColCount <= 32) {

    swDrawAtPos(-((32 - swColCount) / 2));
    swState = NP_SHOW_START;
  } else {

    swDrawAtPos(0);
    swState = NP_PAUSE_BEFORE_RIGHT;
  }
  swPauseStartMs = millis();
}

bool swPriorityTick() {
  if (!swRunning) {
    if (swWasActive) {
      swWasActive = false;
      resumeCircuitTileFromPriority();
    }
    return false;
  }

  if (!swWasActive) {
    swWasActive = true;
    beginC2PCapture();
    swInit();
    finishC2PTransition(tileTransSw, tileTransSpd[TRSPD_SW]);
    return true;
  }

  unsigned long now = millis();

  if (swColCount <= 32) {
    if (now - swLastRedrawMs >= 1000) {
      swLastRedrawMs = now;
      swBuildBuffer();
      swDrawAtPos(-((32 - swColCount) / 2));
    }
    return true;
  }

  switch (swState) {
    case NP_PAUSE_BEFORE_RIGHT:
      if (now - swPauseStartMs >= NP_PAUSE_MS) {
        if (scrollIsWrap(scrollTypeStopwatch)) {
          swWrapPass = 0;
          swScrollPos = 0;
          swState = NP_SCROLL_WRAP;
        } else {
          swState = NP_SCROLL_RIGHT;
        }
        swLastScrollMs = now;
      }
      break;

    case NP_SCROLL_WRAP: {
      if (now - swLastScrollMs >= spd(SPD_STOPWATCH)) {
        swLastScrollMs = now;
        swScrollPos++;
        if (swScrollPos >= swColCount) {
          swWrapPass++;
          if (swWrapPass >= SCROLL_WRAP_PASSES) {

            swInit();
            break;
          }
          swScrollPos = -32;
        }
        swDrawAtPos(swScrollPos);
      }
      break;
    }

    case NP_SCROLL_RIGHT: {
      int maxPos = swColCount - 32;
      if (maxPos <= 0) {
        swState = NP_PAUSE_SHORT;
        swPauseStartMs = now;
        break;
      }
      if (now - swLastScrollMs >= spd(SPD_STOPWATCH)) {
        swLastScrollMs = now;
        swScrollPos++;
        swDrawAtPos(swScrollPos);
        if (swScrollPos >= maxPos) {
          swScrollPos = maxPos;
          swState = NP_PAUSE_AFTER_RIGHT;
          swPauseStartMs = now;
        }
      }
      break;
    }

    case NP_PAUSE_AFTER_RIGHT:
      if (now - swPauseStartMs >= NP_PAUSE_MS) {
        swState = NP_SCROLL_LEFT;
        swLastScrollMs = now;
      }
      break;

    case NP_SCROLL_LEFT:
      if (now - swLastScrollMs >= spd(SPD_STOPWATCH)) {
        swLastScrollMs = now;
        swScrollPos--;
        if (swScrollPos <= 0) {
          swScrollPos = 0;
          swDrawAtPos(0);
          swState = NP_PAUSE_BEFORE_NEXT;
          swPauseStartMs = now;
        } else {
          swDrawAtPos(swScrollPos);
        }
      }
      break;

    case NP_PAUSE_BEFORE_NEXT:
      if (now - swPauseStartMs >= NP_PAUSE_MS) {

        swInit();
      }
      break;

    case NP_PAUSE_SHORT:
      if (now - swPauseStartMs >= 4000UL) {
        swInit();
      }
      break;

    default: break;
  }

  return true;
}

// TIMER DISPLAY 

static uint8_t  timerColBuf[160];
static int      timerColCount     = 0;
static int      timerScrollPos    = 0;
static int      timerWrapPass     = 0;
static unsigned long timerLastScrollMs  = 0;
static unsigned long timerLastRedrawMs  = 0;
NpState       timerState          = NP_SHOW_START;
unsigned long timerPauseStartMs   = 0;

void timerBuildBuffer() {
  char txt[16];
  timerFormatText(txt, sizeof(txt));
  timerColCount = 0;
  for (int ci = 0; txt[ci] != '\0' && timerColCount < 150; ci++) {
    appendGlyphAuto(timerColBuf, timerColCount, 160, txt[ci], fontTypeTimer);
  }
  if (timerColCount > 0) timerColCount--;
}

void timerDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);
  for (int col = 0; col < 32; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < timerColCount) ? timerColBuf[src] : 0x00;
    mx.setColumn(31 - col, val);
  }
  mxCommit();
}

void timerBlankScreen() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mxCommit();
}

void timerInit() {
  timerBlankScreen();
  timerBuildBuffer();
  timerScrollPos = 0;
  timerLastRedrawMs = millis();
  if (timerColCount <= 32) {
    timerDrawAtPos(-((32 - timerColCount) / 2));
    timerState = NP_SHOW_START;
  } else {
    timerDrawAtPos(0);
    timerState = NP_PAUSE_BEFORE_RIGHT;
  }
  timerPauseStartMs = millis();
}

void timerTickRender() {
  unsigned long now = millis();
  if (timerColCount <= 32) {
    if (now - timerLastRedrawMs >= 1000) {
      timerLastRedrawMs = now;
      timerBuildBuffer();
      timerDrawAtPos(-((32 - timerColCount) / 2));
    }
    return;
  }
  switch (timerState) {
    case NP_PAUSE_BEFORE_RIGHT:
      if (now - timerPauseStartMs >= NP_PAUSE_MS) {
        if (scrollIsWrap(scrollTypeTimer)) {
          timerWrapPass = 0;
          timerScrollPos = 0;
          timerState = NP_SCROLL_WRAP;
        } else {
          timerState = NP_SCROLL_RIGHT;
        }
        timerLastScrollMs = now;
      }
      break;
    case NP_SCROLL_WRAP:
      if (now - timerLastScrollMs >= spd(SPD_TIMER)) {
        timerLastScrollMs = now;
        timerScrollPos++;
        if (timerScrollPos >= timerColCount) {
          timerWrapPass++;
          if (timerWrapPass >= SCROLL_WRAP_PASSES) { timerInit(); break; }
          timerScrollPos = -32;
        }
        timerDrawAtPos(timerScrollPos);
      }
      break;
    case NP_SCROLL_RIGHT: {
      int maxPos = timerColCount - 32;
      if (maxPos <= 0) { timerState = NP_PAUSE_SHORT; timerPauseStartMs = now; break; }
      if (now - timerLastScrollMs >= spd(SPD_TIMER)) {
        timerLastScrollMs = now;
        timerScrollPos++;
        timerDrawAtPos(timerScrollPos);
        if (timerScrollPos >= maxPos) {
          timerScrollPos = maxPos;
          timerState = NP_PAUSE_AFTER_RIGHT;
          timerPauseStartMs = now;
        }
      }
      break;
    }
    case NP_PAUSE_AFTER_RIGHT:
      if (now - timerPauseStartMs >= NP_PAUSE_MS) { timerState = NP_SCROLL_LEFT; timerLastScrollMs = now; }
      break;
    case NP_SCROLL_LEFT:
      if (now - timerLastScrollMs >= spd(SPD_TIMER)) {
        timerLastScrollMs = now;
        timerScrollPos--;
        if (timerScrollPos <= 0) {
          timerScrollPos = 0;
          timerDrawAtPos(0);
          timerState = NP_PAUSE_BEFORE_NEXT;
          timerPauseStartMs = now;
        } else {
          timerDrawAtPos(timerScrollPos);
        }
      }
      break;
    case NP_PAUSE_BEFORE_NEXT:
      if (now - timerPauseStartMs >= NP_PAUSE_MS) timerInit();
      break;
    case NP_PAUSE_SHORT:
      if (now - timerPauseStartMs >= 4000UL) timerInit();
      break;
    default: break;
  }
}

void timerBlinkTick() {
  unsigned long now = millis();
  if (now - timerBlinkLastMs >= 500) {
    timerBlinkLastMs = now;
    timerBlinkOn = !timerBlinkOn;
    if (timerBlinkOn) {
      timerBuildBuffer();
      timerDrawAtPos(-((32 - timerColCount) / 2));
    } else {
      timerBlankScreen();
    }
  }
}

bool timerPriorityTick() {
  if (!timerRunning && !timerFinished) {
    if (timerWasActive) {
      timerWasActive = false;
      resumeCircuitTileFromPriority();
    }
    return false;
  }
  if (!timerWasActive) {
    timerWasActive = true;
    beginC2PCapture();
    timerInit();
    finishC2PTransition(tileTransTmr, tileTransSpd[TRSPD_TMR]);
    return true;
  }
  if (timerFinished) {
    timerBlinkTick();
    return true;
  }
  timerCheckExpiry();
  if (timerFinished) {

    timerBlinkOn = true;
    timerBlinkLastMs = millis();
    timerBuildBuffer();
    timerDrawAtPos(-((32 - timerColCount) / 2));
    return true;
  }
  timerTickRender();
  return true;
}

// ALARM DISPLAY
//
// A ringing alarm shows the clock, blinking, so the panel is still telling you
// the time while it asks to be stopped. HH:MM in 24h whatever the hour tile is
// set to, because that is the way the alarm itself was entered. Five glyphs
// never come near 32 columns, so there is nothing here to scroll.

static uint8_t alarmColBuf[80];
static int     alarmColCount     = 0;
bool           alarmBlinkOn      = true;
unsigned long  alarmBlinkLastMs  = 0;
#define ALARM_BLINK_MS 450

void alarmBuildBuffer() {
  char txt[8];
  struct tm ti;
  if (readLocalTime(ti)) {
    snprintf(txt, sizeof(txt), "%02d:%02d", ti.tm_hour, ti.tm_min);
  } else if (alarmRingingIdx >= 0) {
    // No clock to read: show what the alarm was set to, which is the only other
    // honest thing to put there.
    snprintf(txt, sizeof(txt), "%02u:%02u", (unsigned)alarms[alarmRingingIdx].hour,
                                            (unsigned)alarms[alarmRingingIdx].minute);
  } else {
    strcpy(txt, "--:--");
  }
  alarmColCount = 0;
  for (int ci = 0; txt[ci] != '\0' && alarmColCount < 70; ci++) {
    appendGlyphAuto(alarmColBuf, alarmColCount, 80, txt[ci], fontType);
  }
  if (alarmColCount > 0) alarmColCount--;
}

void alarmDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);
  for (int col = 0; col < 32; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < alarmColCount) ? alarmColBuf[src] : 0x00;
    mx.setColumn(31 - col, val);
  }
  mxCommit();
}

void alarmBlankScreen() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mxCommit();
}

void alarmDrawCentered() {
  alarmBuildBuffer();
  alarmDrawAtPos(-((32 - alarmColCount) / 2));
}

void alarmInit() {
  alarmBlankScreen();
  alarmBlinkOn     = true;
  alarmBlinkLastMs = millis();
  alarmDrawCentered();
}

void alarmBlinkTick() {
  unsigned long now = millis();
  if (now - alarmBlinkLastMs < ALARM_BLINK_MS) return;
  alarmBlinkLastMs = now;
  alarmBlinkOn = !alarmBlinkOn;
  // Rebuilt on the way back on rather than once at the start, so the minute
  // rolls over on screen while the alarm is still ringing.
  if (alarmBlinkOn) alarmDrawCentered();
  else              alarmBlankScreen();
}

bool alarmPriorityTick() {
  if (!alarmRinging) {
    if (alarmWasActive) {
      alarmWasActive = false;
      resumeCircuitTileFromPriority();
    }
    return false;
  }
  if (!alarmWasActive) {
    alarmWasActive = true;
    beginC2PCapture();
    alarmInit();
    finishC2PTransition(tileTransAlarm, tileTransSpd[TRSPD_ALARM]);
    return true;
  }
  alarmBlinkTick();
  return true;
}

void notifBuildBuffer() {
  char cleanBuf[204];
  sanitizeUtf8(notifBuf, cleanBuf, sizeof(cleanBuf));
  notifColCount = 0;
  if (scrollIconInBuffer(notifScrollType()) && !notifHideIcon()) {
    scrollBufPrependIcon(notifColBuf, notifColCount, NOTIF_COL_MAX,
                         notifIconOverride ? notifIconOverride : resolveTileIcon(iconSelNotif, notifIcon),
                         NP_ICON_COLS);
  }
  for (int ci = 0; cleanBuf[ci] != '\0' && notifColCount < NOTIF_COL_MAX - 12; ci++) {
    appendGlyphAuto(notifColBuf, notifColCount, NOTIF_COL_MAX, cleanBuf[ci], notifFontType());
  }
  if (notifColCount > 0) notifColCount--;
}
void notifDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);
  if (!notifHideIcon() && !scrollIconInBuffer(notifScrollType())) {

    const uint8_t* nIcon = notifIconOverride ? notifIconOverride
                                             : resolveTileIcon(iconSelNotif, notifIcon);
    for (int col = 0; col < NP_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (nIcon[row] & (0x80 >> col)) colVal |= (1 << row);
      }
      mx.setColumn(31 - col, colVal);
    }
    mx.setColumn(31 - NP_ICON_COLS, 0x00);
  }

  bool fullWidth = notifHideIcon() || scrollIconInBuffer(notifScrollType());
  int textOffset = fullWidth ? 0 : (NP_ICON_COLS + NP_SEP_COLS);
  int textCols   = fullWidth ? 32 : NP_TEXT_COLS;
  for (int col = 0; col < textCols; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < notifColCount) ? notifColBuf[src] : 0x00;
    mx.setColumn(31 - (textOffset + col), val);
  }
  mxCommit();
}

void notifInit() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < NP_DISPLAY_COLS; c++) mx.setColumn(c, 0x00);
  mxCommit();
  notifBuildBuffer();
  notifScrollPos = 0;
  notifWrapPass = 0;
  notifDrawAtPos(0);
  notifState    = NP_PAUSE_BEFORE_RIGHT;
  notifPauseMs  = millis();
}

// MEMENTO DISPLAY
void mementoBuildBuffer() {
  char cleanBuf[sizeof(mementoBuf)];
  sanitizeUtf8(mementoBuf, cleanBuf, sizeof(cleanBuf));
  mementoColCount = 0;
  if (scrollIconInBuffer(scrollTypeReminder) && !hideIconReminder) {
    scrollBufPrependIcon(mementoColBuf, mementoColCount, MEMENTO_COL_MAX, resolveTileIcon(iconSelReminder, mementoIcon), NP_ICON_COLS);
  }
  for (int ci = 0; cleanBuf[ci] != '\0' && mementoColCount < MEMENTO_COL_MAX - 12; ci++) {
    appendGlyphAuto(mementoColBuf, mementoColCount, MEMENTO_COL_MAX, cleanBuf[ci], fontTypeReminder);
  }
  if (mementoColCount > 0) mementoColCount--;
}

void mementoDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);
  if (!hideIconReminder && !scrollIconInBuffer(scrollTypeReminder)) {

    // Resolved once rather than 64 times inside the loop below.
    const uint8_t* memIconBmp = resolveTileIcon(iconSelReminder, mementoIcon);
    for (int col = 0; col < NP_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (memIconBmp[row] & (0x80 >> col)) colVal |= (1 << row);
      }
      mx.setColumn(31 - col, colVal);
    }
    mx.setColumn(31 - NP_ICON_COLS, 0x00);
  }

  bool fullWidth = hideIconReminder || scrollIconInBuffer(scrollTypeReminder);
  int textOffset = fullWidth ? 0 : (NP_ICON_COLS + NP_SEP_COLS);
  int textCols   = fullWidth ? 32 : NP_TEXT_COLS;
  for (int col = 0; col < textCols; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < mementoColCount) ? mementoColBuf[src] : 0x00;
    mx.setColumn(31 - (textOffset + col), val);
  }
  mxCommit();
}

void mementoInit() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < NP_DISPLAY_COLS; c++) mx.setColumn(c, 0x00);
  mxCommit();
  mementoBuildBuffer();
  mementoScrollPos = 0;
  mementoWrapPass = 0;
  mementoDrawAtPos(0);
  mementoState   = NP_PAUSE_BEFORE_RIGHT;
  mementoPauseMs = millis();
}

// CANVAS TILE

void drawCanvas() {
  mx.update(MD_MAX72XX::OFF);
  for (int col = 0; col < CANVAS_COLS; col++) {
    mx.setColumn(31 - col, canvasBitmap[col]);
  }
  mxCommit();
}

String canvasBitmapToHex() {
  static const char* HEXD = "0123456789abcdef";
  String out;
  out.reserve(CANVAS_COLS * 2);
  for (int i = 0; i < CANVAS_COLS; i++) {
    out += HEXD[(canvasBitmap[i] >> 4) & 0x0F];
    out += HEXD[canvasBitmap[i] & 0x0F];
  }
  return out;
}

static inline int canvasHexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool canvasBitmapFromHex(const String& hex) {
  if ((int)hex.length() != CANVAS_COLS * 2) return false;
  uint8_t tmp[CANVAS_COLS];
  for (int i = 0; i < CANVAS_COLS; i++) {
    int hi = canvasHexNibble(hex[i * 2]);
    int lo = canvasHexNibble(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    tmp[i] = (uint8_t)((hi << 4) | lo);
  }
  memcpy(canvasBitmap, tmp, CANVAS_COLS);
  return true;
}

// SCREEN SAVER TILE

uint8_t       ssAnimSelected   = SS_ANIM_RANDOM;
uint8_t       ssCurrentAnim    = SS_ANIM_EQ;
uint8_t       ssLastRandomAnim = 0;
bool          ssWaitingAdvance = false;
unsigned long ssWaitStartMs    = 0;
unsigned long ssFrameMs        = 0;
#define SS_FRAME_INTERVAL 80UL

uint8_t ssFb[32];
static inline void ssClearFb() { memset(ssFb, 0, sizeof(ssFb)); }
static inline void ssSetPx(int x, int y, bool on) {
  if (x < 0 || x > 31 || y < 0 || y > 7) return;
  if (on) ssFb[x] |= (1 << y);
  else    ssFb[x] &= (uint8_t)~(1 << y);
}
static void ssRender() {
  mx.update(MD_MAX72XX::OFF);
  for (int i = 0; i < 32; i++) mx.setColumn(31 - i, ssFb[i]);
  mxCommit();
}

static void ssBlit(const uint16_t* rows, uint8_t rowCount, uint8_t w, int x0, int y0) {
  for (int r = 0; r < rowCount; r++) {
    uint16_t row = rows[r];
    for (int c = 0; c < w; c++) {
      if (row & (1 << (w - 1 - c))) ssSetPx(x0 + c, y0 + r, true);
    }
  }
}

static void ssFlashInvert(int times, int stepMs) {
  for (int i = 0; i < times; i++) {
    mx.update(MD_MAX72XX::OFF);
    for (int c = 0; c < 32; c++) mx.setColumn(c, ~ssFb[31 - c]);
    mxCommit();
    delay(stepMs);
    ssRender();
    delay(stepMs);
  }
}

// PONG

static float pongBallX, pongBallY, pongVX, pongVY, pongPrevBallX, pongPrevBallY;
static int   pongLPaddleY, pongRPaddleY;
static int   pongHits;
static int   pongFlashL, pongFlashR;
static bool  pongRSlowTick;
#define PONG_TARGET_HITS 12
#define PONG_MAX_SPEED   1.15f
#define PONG_CENTER_Y    2

void ssPongInit() {
  pongBallX = pongPrevBallX = 16; pongBallY = pongPrevBallY = 3.5f;
  pongVX = (random(0, 2) ? 1.0f : -1.0f) * 0.55f;
  pongVY = 0.20f + (random(0, 30) / 100.0f);
  if (random(0, 2)) pongVY = -pongVY;
  pongLPaddleY = pongRPaddleY = PONG_CENTER_Y;
  pongHits = 0; pongFlashL = pongFlashR = 0; pongRSlowTick = false;
}
// Functie: ssTickPong.
bool ssTickPong() {
  unsigned long now = millis();
  if (now - ssFrameMs < SS_FRAME_INTERVAL) return false;
  ssFrameMs = now;
  pongPrevBallX = pongBallX; pongPrevBallY = pongBallY;
  pongBallX += pongVX; pongBallY += pongVY;
  if (pongBallY <= 0) { pongBallY = 0; pongVY = -pongVY; }
  if (pongBallY >= 7) { pongBallY = 7; pongVY = -pongVY; }
  int by = (int)round(pongBallY);

  int lTarget = (pongVX < 0) ? constrain(by - 1, 0, 5) : PONG_CENTER_Y;
  if (pongLPaddleY < lTarget) pongLPaddleY++; else if (pongLPaddleY > lTarget) pongLPaddleY--;

  int rTarget = (pongVX > 0) ? constrain(by - 1, 0, 5) : PONG_CENTER_Y;
  pongRSlowTick = !pongRSlowTick;
  if (pongRSlowTick) {
    if (pongRPaddleY < rTarget) pongRPaddleY++; else if (pongRPaddleY > rTarget) pongRPaddleY--;
  }
  if (pongBallX <= 1 && pongVX < 0) {
    pongBallX = 1; pongVX = -pongVX * 1.04f;
    pongVX = constrain(pongVX, -PONG_MAX_SPEED, PONG_MAX_SPEED);
    pongVY += (random(-15, 16)) / 100.0f;
    pongHits++; pongFlashL = 3;
  }
  if (pongBallX >= 30 && pongVX > 0) {
    pongBallX = 30; pongVX = -pongVX * 1.04f;
    pongVX = constrain(pongVX, -PONG_MAX_SPEED, PONG_MAX_SPEED);
    pongVY += (random(-15, 16)) / 100.0f;
    pongHits++; pongFlashR = 3;
  }
  ssClearFb();
  for (int y = 0; y < 8; y += 2) ssSetPx(16, y, true);
  int lh = pongFlashL > 0 ? 5 : 3, rh = pongFlashR > 0 ? 5 : 3;
  int lStart = constrain(pongLPaddleY - (lh - 3) / 2, 0, 8 - lh);
  int rStart = constrain(pongRPaddleY - (rh - 3) / 2, 0, 8 - rh);
  for (int i = 0; i < lh; i++) ssSetPx(0, lStart + i, true);
  for (int i = 0; i < rh; i++) ssSetPx(31, rStart + i, true);
  if (pongFlashL > 0) pongFlashL--;
  if (pongFlashR > 0) pongFlashR--;
  ssSetPx((int)round(pongPrevBallX), (int)round(pongPrevBallY), true);
  ssSetPx((int)round(pongBallX), (int)round(pongBallY), true);
  ssRender();
  return pongHits >= PONG_TARGET_HITS;
}

// FIREWORKS

#define FW_MAX_PARTICLES 34
struct FwParticle { float x, y, vx, vy; int life, maxLife; bool active, rocket, willow, crackle; };
static FwParticle    fwP[FW_MAX_PARTICLES];
static int           fwLaunched;
static unsigned long fwNextLaunchMs;
#define FW_MAX_LAUNCHES 6
void ssFireworksInit() {
  for (int i = 0; i < FW_MAX_PARTICLES; i++) fwP[i].active = false;
  fwLaunched = 0;
  fwNextLaunchMs = millis() + 250;
}
static void fwCrackle(float x, float y) {
  int n = 3 + random(0, 2), placed = 0;
  for (int i = 0; i < FW_MAX_PARTICLES && placed < n; i++) {
    if (!fwP[i].active) {
      float ang = random(0, 360) * 0.0174533f;
      float spd = 0.3f + random(0, 40) / 100.0f;
      fwP[i].x = x; fwP[i].y = y;
      fwP[i].vx = cosf(ang) * spd; fwP[i].vy = sinf(ang) * spd;
      fwP[i].life = fwP[i].maxLife = 4 + random(0, 4);
      fwP[i].active = true; fwP[i].rocket = false; fwP[i].willow = false; fwP[i].crackle = false;
      placed++;
    }
  }
}
static void fwExplode(float x, float y, bool willow) {
  int n = willow ? (6 + random(0, 5)) : (9 + random(0, 8)), placed = 0;
  for (int i = 0; i < FW_MAX_PARTICLES && placed < n; i++) {
    if (!fwP[i].active) {
      float ang = (360.0f / n) * placed + random(-10, 10);
      ang *= 0.0174533f;
      float spd = willow ? (0.55f + random(0, 60) / 100.0f) : (0.45f + (random(0, 100) / 100.0f) * 0.55f);
      fwP[i].x = x; fwP[i].y = y;
      fwP[i].vx = cosf(ang) * spd; fwP[i].vy = sinf(ang) * spd;
      fwP[i].life = fwP[i].maxLife = willow ? (18 + random(0, 8)) : (9 + random(0, 7));
      fwP[i].active = true; fwP[i].rocket = false; fwP[i].willow = willow;
      fwP[i].crackle = (!willow && random(0, 100) < 30);
      placed++;
    }
  }
}

bool ssTickFireworks() {
  unsigned long now = millis();
  if (now - ssFrameMs < SS_FRAME_INTERVAL) return false;
  ssFrameMs = now;
  if (fwLaunched < FW_MAX_LAUNCHES && now >= fwNextLaunchMs) {
    int roCount = (fwLaunched == FW_MAX_LAUNCHES - 1) ? 2 : 1;
    for (int r = 0; r < roCount; r++) {
      for (int i = 0; i < FW_MAX_PARTICLES; i++) {
        if (!fwP[i].active) {
          fwP[i].x = 3 + random(0, 26); fwP[i].y = 7;
          fwP[i].vx = random(-10, 11) / 100.0f; fwP[i].vy = -0.85f - random(0, 20) / 100.0f;
          fwP[i].life = 100; fwP[i].active = true; fwP[i].rocket = true;
          fwP[i].willow = (random(0, 2) == 0); fwP[i].crackle = false;
          break;
        }
      }
    }
    fwLaunched++;
    fwNextLaunchMs = now + 900 + random(0, 700);
  }
  ssClearFb();
  bool anyAlive = false;
  for (int i = 0; i < FW_MAX_PARTICLES; i++) {
    if (!fwP[i].active) continue;
    anyAlive = true;
    float px = fwP[i].x, py = fwP[i].y;
    fwP[i].x += fwP[i].vx; fwP[i].y += fwP[i].vy;
    if (fwP[i].rocket) {
      if (fwP[i].y <= 1 + random(0, 3) || random(0, 100) < 4) { fwExplode(fwP[i].x, fwP[i].y, fwP[i].willow); fwP[i].active = false; continue; }
      ssSetPx((int)round(px), (int)round(py) + 1, true);
      ssSetPx((int)round(px), (int)round(py) + 2, true);
    } else if (fwP[i].willow) {
      fwP[i].vx *= 0.92f; fwP[i].vy = fwP[i].vy * 0.92f + 0.05f;
      fwP[i].life--;
      if (fwP[i].life <= 0 || fwP[i].y > 7 || fwP[i].x < 0 || fwP[i].x > 31) { fwP[i].active = false; continue; }
      if (fwP[i].life < 5 && (fwP[i].life % 2 == 0)) continue;
    } else {
      fwP[i].vy += 0.028f;
      fwP[i].life--;
      bool outOfBounds = (fwP[i].y > 7 || fwP[i].x < 0 || fwP[i].x > 31);
      if (fwP[i].life <= 0 || outOfBounds) {
        if (fwP[i].crackle && !outOfBounds) fwCrackle(fwP[i].x, fwP[i].y);
        fwP[i].active = false; continue;
      }
      if (fwP[i].life < fwP[i].maxLife / 3 && (fwP[i].life % 2 == 0)) continue;
    }
    ssSetPx((int)round(fwP[i].x), (int)round(fwP[i].y), true);
  }
  ssRender();
  return (fwLaunched >= FW_MAX_LAUNCHES && !anyAlive);
}

// EQUALIZER

#define EQ_BARS 8
static float          eqHeight[EQ_BARS], eqTarget[EQ_BARS], eqPeak[EQ_BARS];
static unsigned long  eqNextRetarget, eqNextBass, eqDoneAt;
void ssEqInit() {
  for (int i = 0; i < EQ_BARS; i++) { eqHeight[i] = 1; eqTarget[i] = 1 + random(0, 8); eqPeak[i] = 1; }
  eqNextRetarget = millis();
  eqNextBass = millis() + 2500UL + random(0, 1500);
  eqDoneAt = millis() + 11000UL;
}
bool ssTickEq() {
  unsigned long now = millis();
  if (now - ssFrameMs < SS_FRAME_INTERVAL) return false;
  ssFrameMs = now;
  if (now >= eqNextBass) {
    for (int i = 0; i < EQ_BARS; i++) eqTarget[i] = 8;
    eqNextRetarget = now + 90;
    eqNextBass = now + 2500UL + random(0, 2000);
  } else if (now >= eqNextRetarget) {
    for (int i = 0; i < EQ_BARS; i++) eqTarget[i] = 1 + random(0, 8);
    eqNextRetarget = now + 130 + random(0, 140);
  }
  ssClearFb();
  float maxH = 0;
  for (int i = 0; i < EQ_BARS; i++) {
    if (eqHeight[i] < eqTarget[i]) eqHeight[i] += 1.1f;
    else if (eqHeight[i] > eqTarget[i]) eqHeight[i] -= 0.6f;
    if (eqHeight[i] > maxH) maxH = eqHeight[i];
    if (eqHeight[i] > eqPeak[i]) eqPeak[i] = eqHeight[i];
    else eqPeak[i] -= 0.12f;
    if (eqPeak[i] < 1) eqPeak[i] = 1;
    int h = constrain((int)round(eqHeight[i]), 1, 8);
    for (int y = 0; y < h; y++) { ssSetPx(i * 4, 7 - y, true); ssSetPx(i * 4 + 1, 7 - y, true); }
    int peakInt = constrain((int)round(eqPeak[i]), 1, 8);
    if (eqPeak[i] - eqHeight[i] > 0.6f) { int py = 7 - (peakInt - 1); ssSetPx(i * 4, py, true); ssSetPx(i * 4 + 1, py, true); }
  }
  if (maxH > 7.2f) { ssSetPx(0, 0, true); ssSetPx(31, 0, true); }
  ssRender();
  return now >= eqDoneAt;
}

// Main dispatch
void ssInit() {
  mx.update(MD_MAX72XX::OFF);
  for (int i = 0; i < 32; i++) mx.setColumn(i, 0x00);
  mxCommit();
  ssClearFb();
  ssWaitingAdvance = false;
  ssFrameMs = 0;
  if (ssAnimSelected == SS_ANIM_RANDOM) {
    uint8_t pick;
    do { pick = 1 + random(0, SS_ANIM_COUNT); } while (pick == ssLastRandomAnim && SS_ANIM_COUNT > 1);
    ssLastRandomAnim = pick;
    ssCurrentAnim = pick;
  } else {
    ssCurrentAnim = ssAnimSelected;
  }
  switch (ssCurrentAnim) {
    case SS_ANIM_PONG:      ssPongInit();      break;
    case SS_ANIM_FIREWORKS: ssFireworksInit(); break;
    case SS_ANIM_EQ:        ssEqInit();        break;
    default:                ssCurrentAnim = SS_ANIM_EQ; ssEqInit(); break;
  }
}
void ssTick() {
  unsigned long now = millis();
  if (ssWaitingAdvance) {
    if (now - ssWaitStartMs >= 1000UL) advanceSlot();
    return;
  }
  bool done = false;
  switch (ssCurrentAnim) {
    case SS_ANIM_PONG:      done = ssTickPong();      break;
    case SS_ANIM_FIREWORKS: done = ssTickFireworks(); break;
    case SS_ANIM_EQ:        done = ssTickEq();        break;
    default: done = true; break;
  }
  if (done) { ssWaitingAdvance = true; ssWaitStartMs = now; }
}

void resumeAfterNotif() {
  if (ets2Active) {
    ets2LastSpeed = -1;
    ets2Init();
    return;
  }
  beginP2CCapture();
  CycleItem& cur = items[currentSlot];
  if (cur.id == ITEM_NOW_PLAYING) {
    npInit();
  } else if (cur.id == ITEM_WEATHER) {
    weatherInit();
  } else if (cur.id == ITEM_MEMENTO) {
    mementoInit();
  } else if (cur.id == ITEM_CANVAS) {
    drawCanvas();
  } else if (cur.id == ITEM_SCREENSAVER) {
    ssRender();
  } else if (cur.id == ITEM_CURRENCY) {
    currencyInit();
  } else if (socialIndexForItem(cur.id) >= 0) {
    socialInit(cur.id);
  } else {
    mx.update(MD_MAX72XX::OFF);
    for (int i = 0; i < 32; i++) mx.setColumn(i, 0x00);
    mxCommit();
    gLastStaticDrawMs = 0;
    gStaticDrawDone   = false;
  }
  finishP2CTransition();
}
bool notifTick() {
  if (!notifActive) return false;
  unsigned long now = millis();
  switch (notifState) {
    case NP_SHOW_START:
      break;
    case NP_PAUSE_BEFORE_RIGHT:
      if (now - notifPauseMs >= NP_PAUSE_MS) {
        if (scrollIsWrap(notifScrollType())) {
          notifWrapPass = 0;
          notifScrollPos = 0;
          notifState = NP_SCROLL_WRAP;
        } else {
          notifState = NP_SCROLL_RIGHT;
        }
        notifLastScrollMs = now;
      }
      break;
    case NP_SCROLL_WRAP: {
      if (now - notifLastScrollMs >= notifScrollSpeed()) {
        notifLastScrollMs = now;
        notifScrollPos++;
        if (notifScrollPos >= notifColCount) {
          notifWrapPass++;
          if (notifWrapPass >= SCROLL_WRAP_PASSES) {

            notifActive = false;
            notifIconOverride = nullptr;
            webAccessAlertActive = false;
            notifBuf[0] = '\0';
            resumeAfterNotif();
            return false;
          }
          notifScrollPos = -((notifHideIcon() || scrollIconInBuffer(notifScrollType())) ? 32 : NP_TEXT_COLS);
        }
        notifDrawAtPos(notifScrollPos);
      }
      break;
    }
    case NP_SCROLL_RIGHT: {
      int maxPos = notifColCount - ((notifHideIcon() || scrollIconInBuffer(notifScrollType())) ? 32 : NP_TEXT_COLS);
      if (maxPos <= 0) {
        notifState = NP_PAUSE_SHORT;
        notifPauseMs = now;
        break;
      }
      if (now - notifLastScrollMs >= notifScrollSpeed()) {
        notifLastScrollMs = now;
        notifScrollPos++;
        notifDrawAtPos(notifScrollPos);
        if (notifScrollPos >= maxPos) {
          notifScrollPos = maxPos;
          notifState = NP_PAUSE_AFTER_RIGHT;
          notifPauseMs = now;
        }
      }
      break;
    }
    case NP_PAUSE_AFTER_RIGHT:
      if (now - notifPauseMs >= NP_PAUSE_MS) {
        notifState = NP_SCROLL_LEFT;
        notifLastScrollMs = now;
      }
      break;
    case NP_SCROLL_LEFT:
      if (now - notifLastScrollMs >= notifScrollSpeed()) {
        notifLastScrollMs = now;
        notifScrollPos--;
        if (notifScrollPos <= 0) {
          notifScrollPos = 0;
          notifDrawAtPos(0);
          notifState = NP_PAUSE_BEFORE_NEXT;
          notifPauseMs = now;
        } else {
          notifDrawAtPos(notifScrollPos);
        }
      }
      break;
    case NP_PAUSE_BEFORE_NEXT:
      if (now - notifPauseMs >= NP_PAUSE_MS) {

        notifActive = false;
        notifIconOverride = nullptr;
        webAccessAlertActive = false;
        notifBuf[0] = '\0';
        resumeAfterNotif();
        return false;
      }
      break;
    case NP_PAUSE_SHORT:
      if (now - notifPauseMs >= 4000UL) {
        notifActive = false;
        notifIconOverride = nullptr;
        webAccessAlertActive = false;
        notifBuf[0] = '\0';
        resumeAfterNotif();
        return false;
      }
      break;
  }
  return true;
}

// SHOW IP ADDRESS (Touch Sensor Action)

void ipBuildBuffer() {
  char cleanBuf[24];
  sanitizeUtf8(ipBuf, cleanBuf, sizeof(cleanBuf));
  ipColCount = 0;
  if (scrollIconInBuffer(scrollTypeIp) && !hideIconIp) {
    scrollBufPrependIcon(ipColBuf, ipColCount, 128, resolveTileIcon(iconSelIp, ipIcon), NP_ICON_COLS);
  }
  for (int ci = 0; cleanBuf[ci] != '\0' && ipColCount < 120; ci++) {
    appendGlyphAuto(ipColBuf, ipColCount, 128, cleanBuf[ci], fontTypeIp);
  }
  if (ipColCount > 0) ipColCount--;
}

void ipDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);
  if (!hideIconIp && !scrollIconInBuffer(scrollTypeIp)) {

    // Resolved once rather than 64 times inside the loop below.
    const uint8_t* ipIconBmp = resolveTileIcon(iconSelIp, ipIcon);
    for (int col = 0; col < NP_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (ipIconBmp[row] & (0x80 >> col)) colVal |= (1 << row);
      }
      mx.setColumn(31 - col, colVal);
    }
    mx.setColumn(31 - NP_ICON_COLS, 0x00);
  }

  bool fullWidth = hideIconIp || scrollIconInBuffer(scrollTypeIp);
  int textOffset = fullWidth ? 0 : (NP_ICON_COLS + NP_SEP_COLS);
  int textCols   = fullWidth ? 32 : NP_TEXT_COLS;
  for (int col = 0; col < textCols; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < ipColCount) ? ipColBuf[src] : 0x00;
    mx.setColumn(31 - (textOffset + col), val);
  }
  mxCommit();
}

void ipInit() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < NP_DISPLAY_COLS; c++) mx.setColumn(c, 0x00);
  mxCommit();
  ipBuildBuffer();
  ipScrollPos = 0;
  ipWrapPass = 0;
  ipDrawAtPos(0);
  ipState    = NP_PAUSE_BEFORE_RIGHT;
  ipPauseMs  = millis();
}

bool ipTick() {
  if (!ipShowActive) return false;
  unsigned long now = millis();
  switch (ipState) {
    case NP_SHOW_START:
      break;
    case NP_PAUSE_BEFORE_RIGHT:
      if (now - ipPauseMs >= NP_PAUSE_MS) {
        if (scrollIsWrap(scrollTypeIp)) {
          ipWrapPass = 0;
          ipScrollPos = 0;
          ipState = NP_SCROLL_WRAP;
        } else {
          ipState = NP_SCROLL_RIGHT;
        }
        ipLastScrollMs = now;
      }
      break;
    case NP_SCROLL_WRAP: {
      if (now - ipLastScrollMs >= spd(SPD_IP)) {
        ipLastScrollMs = now;
        ipScrollPos++;
        if (ipScrollPos >= ipColCount) {
          ipWrapPass++;
          if (ipWrapPass >= SCROLL_WRAP_PASSES) {

            ipShowActive = false;
            ipBuf[0] = '\0';
            resumeAfterNotif();
            return false;
          }
          ipScrollPos = -((hideIconIp || scrollIconInBuffer(scrollTypeIp)) ? 32 : NP_TEXT_COLS);
        }
        ipDrawAtPos(ipScrollPos);
      }
      break;
    }
    case NP_SCROLL_RIGHT: {
      int maxPos = ipColCount - ((hideIconIp || scrollIconInBuffer(scrollTypeIp)) ? 32 : NP_TEXT_COLS);
      if (maxPos <= 0) {
        ipState = NP_PAUSE_SHORT;
        ipPauseMs = now;
        break;
      }
      if (now - ipLastScrollMs >= spd(SPD_IP)) {
        ipLastScrollMs = now;
        ipScrollPos++;
        ipDrawAtPos(ipScrollPos);
        if (ipScrollPos >= maxPos) {
          ipScrollPos = maxPos;
          ipState = NP_PAUSE_AFTER_RIGHT;
          ipPauseMs = now;
        }
      }
      break;
    }
    case NP_PAUSE_AFTER_RIGHT:
      if (now - ipPauseMs >= NP_PAUSE_MS) {
        ipState = NP_SCROLL_LEFT;
        ipLastScrollMs = now;
      }
      break;
    case NP_SCROLL_LEFT:
      if (now - ipLastScrollMs >= spd(SPD_IP)) {
        ipLastScrollMs = now;
        ipScrollPos--;
        if (ipScrollPos <= 0) {
          ipScrollPos = 0;
          ipDrawAtPos(0);
          ipState = NP_PAUSE_BEFORE_NEXT;
          ipPauseMs = now;
        } else {
          ipDrawAtPos(ipScrollPos);
        }
      }
      break;
    case NP_PAUSE_BEFORE_NEXT:
      if (now - ipPauseMs >= NP_PAUSE_MS) {

        ipShowActive = false;
        ipBuf[0] = '\0';
        resumeAfterNotif();
        return false;
      }
      break;
    case NP_PAUSE_SHORT:
      if (now - ipPauseMs >= 4000UL) {
        ipShowActive = false;
        ipBuf[0] = '\0';
        resumeAfterNotif();
        return false;
      }
      break;
  }
  return true;
}

// starts the "Show IP Address" one-shot display; it interrupts whatever is
// currently on screen (normal circuit tile OR any other priority tile) —
// the underlying state of other features (stopwatch running, timer, etc.)
// is left untouched and simply resumes visually once the IP display ends.
void startIpShowFromTouch() {
  if (ipShowActive) return; // already showing; ignore a re-trigger mid-animation

  String localIp = provisionMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  localIp.toCharArray(ipBuf, sizeof(ipBuf));
  ipShowActive = true;
  beginC2PCapture();
  ipInit();
  finishC2PTransition(tileTransIp, tileTransSpd[TRSPD_IP]);
}

// NOW PLAYING (Priority Tiles)

bool npPriorityTick() {
  if (!nowPlayingIsPriority || !nowPlayingActive) {
    if (npPriorityWasActive) {
      npPriorityWasActive = false;
      resumeCircuitTileFromPriority();
    }
    return false;
  }

  int npIdx = -1;
  for (int i = 0; i < NUM_ITEMS; i++) {
    if (items[i].id == ITEM_NOW_PLAYING) { npIdx = i; break; }
  }
  if (npIdx >= 0 && !items[npIdx].enabled) {
    if (npPriorityWasActive) {
      npPriorityWasActive = false;
      resumeCircuitTileFromPriority();
    }
    return false;
  }

  if (!npPriorityWasActive) {
    npPriorityWasActive = true;
    beginC2PCapture();
    npInit();
    finishC2PTransition(tileTransNp, tileTransSpd[TRSPD_NP]);
    return true;
  }

  unsigned long now = millis();
  switch (npState) {

    case NP_SHOW_START:
      break;

    case NP_PAUSE_BEFORE_RIGHT:
      if (now - npPauseStartMs >= NP_PAUSE_MS) {
        if (scrollIsWrap(scrollTypeNowPlaying)) {
          npWrapPass = 0;
          npScrollPos = 0;
          npState = NP_SCROLL_WRAP;
        } else {
          npState = NP_SCROLL_RIGHT;
        }
        npLastScrollMs = now;
      }
      break;

    case NP_SCROLL_WRAP: {
      if (now - npLastScrollMs >= spd(SPD_NOWPLAYING)) {
        npLastScrollMs = now;
        npScrollPos++;
        if (npScrollPos >= npColCount) {
          npWrapPass++;
          if (npWrapPass >= SCROLL_WRAP_PASSES) {

            npInit();
            break;
          }
          npScrollPos = -((hideIconNowPlaying || scrollIconInBuffer(scrollTypeNowPlaying)) ? 32 : NP_TEXT_COLS);
        }
        npDrawAtPos(npScrollPos);
      }
      break;
    }

    case NP_SCROLL_RIGHT: {
      int maxPos = npColCount - ((hideIconNowPlaying || scrollIconInBuffer(scrollTypeNowPlaying)) ? 32 : NP_TEXT_COLS);
      if (maxPos <= 0) {
        npState = NP_PAUSE_SHORT;
        npPauseStartMs = now;
        break;
      }
      if (now - npLastScrollMs >= spd(SPD_NOWPLAYING)) {
        npLastScrollMs = now;
        npScrollPos++;
        npDrawAtPos(npScrollPos);
        if (npScrollPos >= maxPos) {
          npScrollPos = maxPos;
          npState = NP_PAUSE_AFTER_RIGHT;
          npPauseStartMs = now;
        }
      }
      break;
    }

    case NP_PAUSE_AFTER_RIGHT:
      if (now - npPauseStartMs >= NP_PAUSE_MS) {
        npState = NP_SCROLL_LEFT;
        npLastScrollMs = now;
      }
      break;

    case NP_SCROLL_LEFT:
      if (now - npLastScrollMs >= spd(SPD_NOWPLAYING)) {
        npLastScrollMs = now;
        npScrollPos--;
        if (npScrollPos <= 0) {
          npScrollPos = 0;
          npDrawAtPos(0);
          npState = NP_PAUSE_BEFORE_NEXT;
          npPauseStartMs = now;
        } else {
          npDrawAtPos(npScrollPos);
        }
      }
      break;

    case NP_PAUSE_BEFORE_NEXT:
      if (now - npPauseStartMs >= NP_PAUSE_MS) {

        npInit();
      }
      break;

    case NP_PAUSE_SHORT:
      if (now - npPauseStartMs >= 4000UL) {
        npInit();
      }
      break;
  }

  return true;
}

void rebuildNowPlayingBuf() {
  if (npDisplayMode == 1) {
    snprintf(nowPlayingBuf, sizeof(nowPlayingBuf), "%s", nowArtist);
  } else if (npDisplayMode == 2) {
    snprintf(nowPlayingBuf, sizeof(nowPlayingBuf), "%s", nowTitle);
  } else {
    snprintf(nowPlayingBuf, sizeof(nowPlayingBuf), "%s - %s", nowArtist, nowTitle);
  }
}

// WEATHER DISPLAY

static void wxBufAppend(const uint8_t* src, int n, bool gap = true) {
  for (int i = 0; i < n && wxColCount < 511; i++)
    wxColBuf[wxColCount++] = src[i];
  if (gap && wxColCount < 512)
    wxColBuf[wxColCount++] = 0x00;
}

static void wxBufAppendMD(char ch) {
  uint8_t tmp[8];
  uint8_t n = isPixelFont(fontTypeWeather)
                ? pixGetChar(fontTypeWeather, ch, sizeof(tmp), tmp)
                : mx.getChar(ch, sizeof(tmp), tmp);
  if (n == 0) return;
  for (int i = 0; i < n && wxColCount < 511; i++)
    wxColBuf[wxColCount++] = tmp[i];
  if (wxColCount < 512) wxColBuf[wxColCount++] = 0x00;
}

static void sanitizeUtf8(const char* src, char* dst, int dstLen) {
  int di = 0;
  int si = 0;
  while (src[si] != '\0' && di < dstLen - 1) {
    unsigned char c = (unsigned char)src[si];

    if (c < 0x80) {

      dst[di++] = (char)c;
      si++;
    } else if ((c & 0xE0) == 0xC0) {

      if ((unsigned char)src[si+1] == 0) { si++; continue; }
      uint32_t cp = ((c & 0x1F) << 6) | ((unsigned char)src[si+1] & 0x3F);
      si += 2;

      char rep = 0;
      switch (cp) {

        case 0x00E0: case 0x00E1: case 0x00E2: case 0x00E3:
        case 0x00E4: case 0x00E5: case 0x0103: case 0x01CE: rep = 'a'; break;
        case 0x00C0: case 0x00C1: case 0x00C2: case 0x00C3:
        case 0x00C4: case 0x00C5: case 0x0102: rep = 'A'; break;

        case 0x00E8: case 0x00E9: case 0x00EA: case 0x00EB: rep = 'e'; break;
        case 0x00C8: case 0x00C9: case 0x00CA: case 0x00CB: rep = 'E'; break;

        case 0x00EC: case 0x00ED: case 0x00EE: case 0x00EF:
        case 0x012B: rep = 'i'; break;
        case 0x00CC: case 0x00CD: case 0x00CE: case 0x00CF: rep = 'I'; break;

        case 0x00F2: case 0x00F3: case 0x00F4: case 0x00F5:
        case 0x00F6: case 0x00F8: rep = 'o'; break;
        case 0x00D2: case 0x00D3: case 0x00D4: case 0x00D5:
        case 0x00D6: case 0x00D8: rep = 'O'; break;

        case 0x00F9: case 0x00FA: case 0x00FB: case 0x00FC: rep = 'u'; break;
        case 0x00D9: case 0x00DA: case 0x00DB: case 0x00DC: rep = 'U'; break;

        case 0x015F: case 0x0219: rep = 's'; break;
        case 0x015E: case 0x0218: rep = 'S'; break;

        case 0x0163: case 0x021B: rep = 't'; break;
        case 0x0162: case 0x021A: rep = 'T'; break;

        case 0x00F1: rep = 'n'; break;
        case 0x00D1: rep = 'N'; break;

        case 0x00E7: rep = 'c'; break;
        case 0x00C7: rep = 'C'; break;

        case 0x00B0: rep = 0x01; break;

        default: rep = 0; break;
      }
      if (rep != 0 && rep != 0x01) dst[di++] = rep;

    } else if ((c & 0xF0) == 0xE0) {

      si += 3;

    } else if ((c & 0xF8) == 0xF0) {

      si += 4;

    } else {

      si++;
    }
  }
  dst[di] = '\0';
}

void weatherBuildBuffer() {
  wxColCount = 0;
  if (scrollIconInBuffer(scrollTypeWeather) && !hideIconWeather) {
    scrollBufPrependIcon(wxColBuf, wxColCount, 511, getWeatherIcon(), NP_ICON_COLS);
  }
  if (!weatherValid) {

    for (int ci = 0; weatherBuf[ci] != '\0'; ci++) wxBufAppendMD(weatherBuf[ci]);
    if (wxColCount > 0) wxColCount--;
    return;
  }

  int displayTemp = (tempUnit == 1) ? (int)(weatherTempC * 9.0f / 5.0f + 32) : (int)weatherTempC;
  bool neg        = (displayTemp < 0);
  int  absTemp    = abs(displayTemp);
  bool twoDigit   = (absTemp >= 10);

  bool wxTiko = isPixelFont(fontTypeWeather);   // Tiko or Mako
  uint8_t dtmp[TIKO_MAX_COLS]; int dw;

  int  hum      = weatherHumidity;
  bool wantTemp = (wxPreset & WX_SHOW_TEMP) != 0;
  bool wantHum  = (wxPreset & WX_SHOW_HUM) != 0 && hum >= 0;
  bool wantDesc = (wxPreset & WX_SHOW_DESC) != 0 && weatherDesc[0] != '\0';
  // Everything the preset asked for is missing (humidity-only preset with no
  // humidity reading, say). Fall back to the temperature rather than leaving
  // the matrix blank for the whole slot.
  if (!wantTemp && !wantHum && !wantDesc) wantTemp = true;

  if (wantTemp) {
  if (neg) {
    static const uint8_t CHAR_MINUS[3] = {0x08, 0x08, 0x08};
    if (wxTiko) wxBufAppend(TIKO_MINUS, 3);
    else        wxBufAppend(CHAR_MINUS, 3);
  }

  if (wxTiko) {
    if (twoDigit) {
      wxBufAppend(digitGlyph(fontTypeWeather, absTemp / 10, dtmp, dw), dw);
      wxBufAppend(digitGlyph(fontTypeWeather, absTemp % 10, dtmp, dw), dw);
    } else {
      wxBufAppend(digitGlyph(fontTypeWeather, absTemp, dtmp, dw), dw);
    }
    wxBufAppend(pixDeg(fontTypeWeather), 3);
    uint8_t utmp[TIKO_MAX_COLS];
    uint8_t uw = pixGetChar(fontTypeWeather, (tempUnit == 1) ? 'F' : 'C', TIKO_MAX_COLS, utmp);
    wxBufAppend(utmp, uw);
  } else {
    if (twoDigit) {
      wxBufAppend(FONT[absTemp / 10], 5);
      wxBufAppend(FONT[absTemp % 10], 5);
    } else {
      wxBufAppend(FONT[absTemp], 5);
    }
    wxBufAppend(CHAR_DEG, 5);
    wxBufAppend((tempUnit == 1) ? CHAR_F : CHAR_C, 5);
  }
  }

  if (wantHum) {
    if (wantTemp) {
      if (wxColCount < 511) wxColBuf[wxColCount++] = 0x00;
      if (wxColCount < 511) wxColBuf[wxColCount++] = 0x00;
      if (wxColCount < 511) wxColBuf[wxColCount++] = 0x00;
    }
    int humTens = hum / 10;
    int humOnes = hum % 10;
    if (hum >= 100) {
      wxBufAppend(digitGlyph(fontTypeWeather, 1, dtmp, dw), dw);
      wxBufAppend(digitGlyph(fontTypeWeather, 0, dtmp, dw), dw);
      wxBufAppend(digitGlyph(fontTypeWeather, 0, dtmp, dw), dw);
    } else if (hum >= 10) {
      wxBufAppend(digitGlyph(fontTypeWeather, humTens, dtmp, dw), dw);
      wxBufAppend(digitGlyph(fontTypeWeather, humOnes, dtmp, dw), dw);
    } else {
      wxBufAppend(digitGlyph(fontTypeWeather, humOnes, dtmp, dw), dw);
    }
    wxBufAppendMD('%');
  }

  if (wantDesc) {
    if (wantTemp || wantHum) {
      if (wxColCount < 511) wxColBuf[wxColCount++] = 0x00;
      if (wxColCount < 511) wxColBuf[wxColCount++] = 0x00;
    }
    char cleanDesc[48];
    sanitizeUtf8(weatherDesc, cleanDesc, sizeof(cleanDesc));
    for (int ci = 0; cleanDesc[ci] != '\0' && wxColCount < 500; ci++)
      wxBufAppendMD(cleanDesc[ci]);
  }

  if (wxColCount > 0) wxColCount--;
}

void weatherDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);
  if (!hideIconWeather && !scrollIconInBuffer(scrollTypeWeather)) {

    const uint8_t* icon = getWeatherIcon();
    for (int col = 0; col < NP_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (icon[row] & (0x80 >> col)) colVal |= (1 << row);
      }
      mx.setColumn(31 - col, colVal);
    }
    mx.setColumn(31 - NP_ICON_COLS, 0x00);
  }

  bool fullWidth = hideIconWeather || scrollIconInBuffer(scrollTypeWeather);
  int textOffset = fullWidth ? 0 : (NP_ICON_COLS + NP_SEP_COLS);
  int textCols   = fullWidth ? 32 : NP_TEXT_COLS;
  for (int col = 0; col < textCols; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < wxColCount) ? wxColBuf[src] : 0x00;
    mx.setColumn(31 - (textOffset + col), val);
  }
  mxCommit();
}
void weatherInit() {

  if (!weatherValid) {
    snprintf(weatherBuf, sizeof(weatherBuf), "No data");
  }

  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < NP_DISPLAY_COLS; c++) mx.setColumn(c, 0x00);
  mxCommit();
  weatherBuildBuffer();
  wxScrollPos = 0;
  wxWrapPass = 0;
  weatherDrawAtPos(0);
  wxState = NP_PAUSE_BEFORE_RIGHT;
  wxPauseStartMs = millis();
}
// --- Non-blocking weather fetch --------------------------------------
// The HTTP request used to run directly inside loop(), which stalled the
// whole matrix (including any scroll in progress) for up to 5s (the
// http.setTimeout value) every WEATHER_FETCH_INTERVAL_MS. That is what
// caused tiles to randomly freeze mid-scroll and then resume where they
// left off. The actual network call now runs on its own FreeRTOS task
// (pinned to core 0) so loop() / the matrix refresh is never blocked.
// Results are staged in wxFetch* variables and only copied into the
// "live" weather* variables from loop() (same thread that reads them
// for drawing), so there is no risk of tearing a struct mid-read.
static TaskHandle_t  weatherFetchTaskHandle = nullptr;
static volatile bool weatherFetchBusy       = false;
static volatile bool weatherFetchReady      = false;
static volatile bool wxFetchOk              = false;
static float         wxFetchTempC;
static int           wxFetchHumidity;
static char          wxFetchDesc[sizeof(weatherDesc)];
static char          wxFetchCondition[sizeof(weatherCondition)];

static void weatherFetchTask(void* pv) {
  bool ok = false;
  if (strlen(weatherApiKey) > 0 && WiFi.status() == WL_CONNECTED) {
    char url[256];
    snprintf(url, sizeof(url),
      "http://api.openweathermap.org/data/2.5/weather?lat=%.4f&lon=%.4f&appid=%s&units=metric&lang=%s",
      weatherLat, weatherLon, weatherApiKey, weatherLang);
    HTTPClient http;
    http.begin(url);
    http.setTimeout(5000);
    int code = http.GET();
    if (code == 200) {
      String payload = http.getString();
      StaticJsonDocument<1024> doc;
      DeserializationError err = deserializeJson(doc, payload);
      if (!err) {
        wxFetchTempC    = doc["main"]["temp"] | -999.0f;
        wxFetchHumidity = doc["main"]["humidity"] | -1;
        const char* desc = doc["weather"][0]["description"] | "";
        strncpy(wxFetchDesc, desc, sizeof(wxFetchDesc) - 1);
        wxFetchDesc[sizeof(wxFetchDesc)-1] = '\0';
        if (wxFetchDesc[0] >= 'a' && wxFetchDesc[0] <= 'z') wxFetchDesc[0] -= 32;

        const char* cond = doc["weather"][0]["main"] | "";
        strncpy(wxFetchCondition, cond, sizeof(wxFetchCondition) - 1);
        wxFetchCondition[sizeof(wxFetchCondition)-1] = '\0';
        ok = true;
      }
    }
    http.end();
  }
  wxFetchOk         = ok;
  weatherFetchReady = true;
  weatherFetchBusy  = false;
  vTaskDelete(nullptr);
}

void weatherFetch() {
  if (strlen(weatherApiKey) == 0) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (weatherFetchBusy) return; // a fetch is already in flight

  lastWeatherFetch  = millis(); // stamp now so loop() doesn't re-trigger every iteration
  weatherFetchBusy  = true;
  weatherFetchReady = false;
  xTaskCreatePinnedToCore(weatherFetchTask, "wxFetch", 8192, nullptr, 1, &weatherFetchTaskHandle, 0);
}

// Call from loop(); cheap check, only does work once a fetch has completed.
static void weatherFetchPoll() {
  if (!weatherFetchReady) return;
  weatherFetchReady = false;
  if (wxFetchOk) {
    weatherTempC    = wxFetchTempC;
    weatherHumidity = wxFetchHumidity;
    strncpy(weatherDesc, wxFetchDesc, sizeof(weatherDesc) - 1);
    weatherDesc[sizeof(weatherDesc)-1] = '\0';
    strncpy(weatherCondition, wxFetchCondition, sizeof(weatherCondition) - 1);
    weatherCondition[sizeof(weatherCondition)-1] = '\0';
    weatherValid = true;
  }
}

// CYCLE HELPERS

void sortItems() {
  for (int i = 0; i < NUM_ITEMS - 1; i++) {
    for (int j = i + 1; j < NUM_ITEMS; j++) {
      if (items[j].order < items[i].order) {
        CycleItem tmp = items[i];
        items[i] = items[j];
        items[j] = tmp;
      }
    }
  }
}

int nextActiveSlot(int from) {
  for (int i = 1; i <= NUM_ITEMS; i++) {
    int idx = (from + i) % NUM_ITEMS;
    if (!items[idx].enabled) continue;

    if (items[idx].id == ITEM_NOW_PLAYING && (nowPlayingIsPriority || !nowPlayingActive)) continue;

    if (items[idx].id == ITEM_WEATHER && strlen(weatherApiKey) == 0) continue;

    if (items[idx].id == ITEM_MEMENTO && strlen(mementoBuf) == 0) continue;
    return idx;
  }
  return -1;
}

int prevActiveSlot(int from) {
  for (int i = 1; i <= NUM_ITEMS; i++) {
    int idx = ((from - i) % NUM_ITEMS + NUM_ITEMS) % NUM_ITEMS;
    if (!items[idx].enabled) continue;
    if (items[idx].id == ITEM_NOW_PLAYING && (nowPlayingIsPriority || !nowPlayingActive)) continue;
    if (items[idx].id == ITEM_WEATHER && strlen(weatherApiKey) == 0) continue;
    if (items[idx].id == ITEM_MEMENTO && strlen(mementoBuf) == 0) continue;
    return idx;
  }
  return -1;
}

bool higherPriorityTileActive(uint8_t id) {
  int rank = -1;
  for (int i = 0; i < NUM_PRIORITY_IDS; i++) if (priorityOrder[i] == id) { rank = i; break; }
  if (rank < 0) return false;
  for (int i = 0; i < rank; i++) {
    switch (priorityOrder[i]) {
      case PRIORITY_ID_NOTIF:      if (notifActive && !webAccessAlertActive) return true; break;
      case PRIORITY_ID_WEB:        if (notifActive &&  webAccessAlertActive) return true; break;
      case PRIORITY_ID_ETS2:       if (ets2Active) return true; break;
      case PRIORITY_ID_NOWPLAYING: {
        int npIdx = -1;
        for (int j = 0; j < NUM_ITEMS; j++) if (items[j].id == ITEM_NOW_PLAYING) { npIdx = j; break; }
        if (nowPlayingIsPriority && nowPlayingActive && (npIdx < 0 || items[npIdx].enabled)) return true;
        break;
      }
      case PRIORITY_ID_STOPWATCH:  if (swRunning) return true; break;
      case PRIORITY_ID_TIMER:      if (timerRunning || timerFinished) return true; break;
      case PRIORITY_ID_ALARM:      if (alarmRinging) return true; break;
    }
  }
  return false;
}

// The names the web interface knows these by. Pulled out of
// priorityOrderToString() so the live marker names them the same way the saved
// order does - two switches would be two chances to disagree.
static const char* priorityIdName(uint8_t id) {
  switch (id) {
    case PRIORITY_ID_NOTIF:      return "notif";
    case PRIORITY_ID_WEB:        return "webaccess";
    case PRIORITY_ID_ETS2:       return "ets2";
    case PRIORITY_ID_NOWPLAYING: return "nowplaying";
    case PRIORITY_ID_STOPWATCH:  return "stopwatch";
    case PRIORITY_ID_TIMER:      return "timer";
    case PRIORITY_ID_ALARM:      return "alarm";
  }
  return "";
}

String priorityOrderToString() {
  String out = "";
  for (int i = 0; i < NUM_PRIORITY_IDS; i++) {
    if (i > 0) out += ",";
    out += priorityIdName(priorityOrder[i]);
  }
  return out;
}

void rebuildPriorityOrderFromEts2Order() {
  priorityOrder[0] = PRIORITY_ID_ALARM;
  if (ets2OrderFirst) {
    priorityOrder[1] = PRIORITY_ID_ETS2;
    priorityOrder[2] = PRIORITY_ID_NOTIF;
  } else {
    priorityOrder[1] = PRIORITY_ID_NOTIF;
    priorityOrder[2] = PRIORITY_ID_ETS2;
  }
  priorityOrder[3] = PRIORITY_ID_NOWPLAYING;
  priorityOrder[4] = PRIORITY_ID_STOPWATCH;
  priorityOrder[5] = PRIORITY_ID_TIMER;
  priorityOrder[6] = PRIORITY_ID_WEB;
}

// TILE TRANSITIONS: generic transition engine between 32x8 pixel frames
// effect: 0=Nothing 1=Scroll Left 2=Scroll Right 3=Scroll Up 4=Scroll Down 5=Morph 6=Fade 7=Expand Left 8=Expand Right 9=Expand Centre

static const uint8_t TRANS_BAYER8[8][8] = {
  { 0,32, 8,40, 2,34,10,42},
  {48,16,56,24,50,18,58,26},
  {12,44, 4,36,14,46, 6,38},
  {60,28,52,20,62,30,54,22},
  { 3,35,11,43, 1,33, 9,41},
  {51,19,59,27,49,17,57,25},
  {15,47, 7,39,13,45, 5,37},
  {63,31,55,23,61,29,53,21}
};


void transCaptureCols(uint8_t *dst) {
  for (int c = 0; c < 32; c++) dst[c] = mx.getColumn(c);
}

void transWriteCols(const uint8_t *cols) {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, cols[c]);
  mxCommit();
}

static void transWriteBlank() {
  uint8_t blank[32];
  for (int c = 0; c < 32; c++) blank[c] = 0x00;
  transWriteCols(blank);
}

// Delay with "ease-in-out": slower near the ends (start/finish), faster in the middle.
static int easedStepDelay(int s, int steps, int baseDelay, int extraDelay, int easeZone) {
  int distFromEdge = s < (steps - s) ? s : (steps - s);
  if (distFromEdge >= easeZone) return baseDelay;
  return baseDelay + extraDelay * (easeZone - distFromEdge) / easeZone;
}

// Pace of the transition being played, in percent (see TILE TRANSITION SPEED).
static uint8_t gTransPct = TRSPD_DEFAULT;

// delay() scaled by that pace: 200 % halves every pause, 50 % doubles it. Never
// under 1 ms, so even the fastest setting still shows every frame.
static void transDelay(int ms) {
  long d = (long)ms * 100 / gTransPct;
  delay(d > 0 ? (unsigned long)d : 1UL);
}

static void playScrollTransition(const uint8_t *oldCols, const uint8_t *newCols, bool toLeft) {
  const int GAP = 4; // black columns between tiles, during the scroll
  const int VW  = 32 + GAP + 32;
  uint8_t virt[VW];
  const int STEPS = VW - 32;
  if (toLeft) {
    // the old tile exits to the left, the new one enters from the right, with black space between them
    for (int c = 0; c < 32; c++) virt[c] = oldCols[c];
    for (int c = 0; c < GAP; c++) virt[32 + c] = 0x00;
    for (int c = 0; c < 32; c++) virt[32 + GAP + c] = newCols[c];
    for (int s = 0; s <= STEPS; s++) {
      uint8_t frame[32];
      for (int c = 0; c < 32; c++) frame[c] = virt[s + c];
      transWriteCols(frame);
      transDelay(easedStepDelay(s, STEPS, 4, 10, 8));
    }
  } else {
    // the old tile exits to the right, the new one enters from the left, with black space between them
    for (int c = 0; c < 32; c++) virt[c] = newCols[c];
    for (int c = 0; c < GAP; c++) virt[32 + c] = 0x00;
    for (int c = 0; c < 32; c++) virt[32 + GAP + c] = oldCols[c];
    for (int s = STEPS; s >= 0; s--) {
      uint8_t frame[32];
      for (int c = 0; c < 32; c++) frame[c] = virt[s + c];
      transWriteCols(frame);
      transDelay(easedStepDelay(STEPS - s, STEPS, 4, 10, 8));
    }
  }
}

static void playVerticalScrollTransition(const uint8_t *oldCols, const uint8_t *newCols, bool up) {
  const int GAP   = 2; // black rows between tiles, during the scroll
  const int STEPS = 8 + GAP;
  for (int s = 0; s <= STEPS; s++) {
    uint8_t frame[32];
    for (int c = 0; c < 32; c++) {
      uint32_t virtCol = up
        ? (((uint32_t)newCols[c] << (8 + GAP)) | oldCols[c])
        : (((uint32_t)oldCols[c] << (8 + GAP)) | newCols[c]);
      int shift = up ? s : (STEPS - s);
      frame[c] = (uint8_t)((virtCol >> shift) & 0xFF);
    }
    transWriteCols(frame);
    transDelay(easedStepDelay(s, STEPS, 14, 24, 3));
  }
}

static void playFadeTransition(const uint8_t *newCols) {
  if (curBrightness == 0) { transWriteCols(newCols); return; } // screen off, no animation
  int target = (int)curBrightness - 1; // 0..15, current hardware level
  if (target < 0) target = 0;

  const int FADE_OUT_MS = 1000; // fade out on the old tile, ~1 second
  const int PAUSE_MS    = 100;  // black pause between tiles
  const int FADE_IN_MS  = 450;  // fade in on the new tile

  int outDelay = FADE_OUT_MS / (target + 1);
  if (outDelay < 1) outDelay = 1;
  for (int lvl = target; lvl >= 0; lvl--) {
    mx.control(MD_MAX72XX::INTENSITY, (uint8_t)lvl);
    transDelay(outDelay);
  }

  transWriteCols(newCols); // change content while the screen is completely off
  transDelay(PAUSE_MS);

  int inDelay = FADE_IN_MS / (target + 1);
  if (inDelay < 1) inDelay = 1;
  for (int lvl = 0; lvl <= target; lvl++) {
    mx.control(MD_MAX72XX::INTENSITY, (uint8_t)lvl);
    transDelay(inDelay);
  }
}

static void playMorphTransition(const uint8_t *oldCols, const uint8_t *newCols) {
  // dissolve using Bayer matrix: new pixels gradually appear over the old ones
  const int STEPS = 24;
  for (int step = 1; step <= STEPS; step++) {
    int S = step * 4; // 4..96 - covers the Bayer thresholds (0..63) with an overlap of 32
    uint8_t frame[32];
    for (int c = 0; c < 32; c++) {
      uint8_t colVal = 0;
      for (int r = 0; r < 8; r++) {
        bool oldBit   = (oldCols[c] >> r) & 0x01;
        bool newBit   = (newCols[c] >> r) & 0x01;
        int  bv       = TRANS_BAYER8[r][c % 8];
        bool showOld  = bv >= (S - 32);
        bool showNew  = bv < S;
        if ((oldBit && showOld) || (newBit && showNew)) colVal |= (1 << r);
      }
      frame[c] = colVal;
    }
    transWriteCols(frame);
    transDelay(14);
  }
  transWriteCols(newCols);
}

static void expandFrameOpen(uint8_t *frame, const uint8_t *cols, uint8_t effect, int s) {
  for (int c = 0; c < 32; c++) frame[c] = 0x00;
  if (effect == 7) { // Expand Left: grows from column 0 towards 31
    for (int c = 0; c < s && c < 32; c++) frame[c] = cols[c];
  } else if (effect == 8) { // Expand Right: grows from column 31 towards 0
    for (int c = 31; c > 31 - s && c >= 0; c--) frame[c] = cols[c];
  } else { // Expand Centre: grows symmetrically from the middle towards the edges
    int half = s / 2;
    int lo = 16 - half, hi = 15 + half;
    if (s % 2 == 1) hi++;
    if (lo < 0) lo = 0;
    if (hi > 31) hi = 31;
    for (int c = lo; c <= hi; c++) frame[c] = cols[c];
  }
}

static void expandFrameClose(uint8_t *frame, const uint8_t *cols, uint8_t effect, int s) {
  // Closes the old tile from the direction OPPOSITE to the opening direction.
  for (int c = 0; c < 32; c++) frame[c] = cols[c];
  if (effect == 7) { // opposite of Left = collapses from the right
    for (int c = 31; c > 31 - s && c >= 0; c--) frame[c] = 0x00;
  } else if (effect == 8) { // opposite of Right = collapses from the left
    for (int c = 0; c < s && c < 32; c++) frame[c] = 0x00;
  } else { // opposite of Centre = collapses from the edges towards the center
    int half = s / 2;
    int lo = half, hi = 31 - half;
    if (s % 2 == 1) hi--;
    if (lo > hi) { for (int c = 0; c < 32; c++) frame[c] = 0x00; return; }
    for (int c = 0; c < lo; c++) frame[c] = 0x00;
    for (int c = hi + 1; c < 32; c++) frame[c] = 0x00;
  }
}

static void playExpandTransition(const uint8_t *oldCols, const uint8_t *newCols, uint8_t effect) {
  // effect: 7=Expand Left, 8=Expand Right, 9=Expand Centre
  const int STEPS = 32;
  // Phase 1: the old tile "closes" (collapses) from the opposite direction
  for (int s = 1; s <= STEPS; s++) {
    uint8_t frame[32];
    expandFrameClose(frame, oldCols, effect, s);
    transWriteCols(frame);
    transDelay(6);
  }
  // black pause between tiles
  transWriteBlank();
  transDelay(100);
  // Phase 2: the new tile "opens" gradually, like in Pokemon, from the chosen direction
  for (int s = 1; s <= STEPS; s++) {
    uint8_t frame[32];
    expandFrameOpen(frame, newCols, effect, s);
    transWriteCols(frame);
    transDelay(6);
  }
  transWriteCols(newCols);
}

void playTileTransition(uint8_t effect, const uint8_t *oldCols, const uint8_t *newCols, uint8_t pct) {
  // Out of range - or 0, from a caller with no setting of its own - plays at
  // the pace the effect was drawn at.
  gTransPct = (pct >= TRSPD_MIN && pct <= TRSPD_MAX) ? pct : TRSPD_DEFAULT;
  switch (effect) {
    case 1: playScrollTransition(oldCols, newCols, true);          break;
    case 2: playScrollTransition(oldCols, newCols, false);         break;
    case 3: playVerticalScrollTransition(oldCols, newCols, true);  break;
    case 4: playVerticalScrollTransition(oldCols, newCols, false); break;
    case 5: playMorphTransition(oldCols, newCols);                 break;
    case 6: playFadeTransition(newCols);                           break;
    case 7: case 8: case 9: playExpandTransition(oldCols, newCols, effect); break;
    default: transWriteCols(newCols);                              break; // 0 = Nothing
  }
}

uint8_t getCircuitTileTransition(uint8_t itemId) {
  switch (itemId) {
    case ITEM_HOUR:        return tileTransHour;
    case ITEM_DATE:        return tileTransDate;
    case ITEM_TEMP:        return tileTransTemp;
    case ITEM_NOW_PLAYING: return tileTransNp;
    case ITEM_WEATHER:     return tileTransWx;
    case ITEM_MEMENTO:     return tileTransRem;
    case ITEM_CANVAS:      return tileTransCanvas;
    case ITEM_PRESSURE:    return tileTransPress;
    case ITEM_SCREENSAVER: return tileTransSs;
    case ITEM_CURRENCY:    return tileTransCurr;
    case ITEM_YOUTUBE:     return tileTransYt;
    case ITEM_HOURWEEK:    return tileTransHw;
    default:               return tileTransGlobal;
  }
}

// The pace that goes with getCircuitTileTransition()'s effect.
uint8_t getCircuitTileTransSpeed(uint8_t itemId) {
  switch (itemId) {
    case ITEM_HOUR:        return tileTransSpd[TRSPD_HOUR];
    case ITEM_DATE:        return tileTransSpd[TRSPD_DATE];
    case ITEM_TEMP:        return tileTransSpd[TRSPD_TEMP];
    case ITEM_NOW_PLAYING: return tileTransSpd[TRSPD_NP];
    case ITEM_WEATHER:     return tileTransSpd[TRSPD_WX];
    case ITEM_MEMENTO:     return tileTransSpd[TRSPD_REM];
    case ITEM_CANVAS:      return tileTransSpd[TRSPD_CANVAS];
    case ITEM_PRESSURE:    return tileTransSpd[TRSPD_PRESS];
    case ITEM_SCREENSAVER: return tileTransSpd[TRSPD_SS];
    case ITEM_CURRENCY:    return tileTransSpd[TRSPD_CURR];
    case ITEM_YOUTUBE:     return tileTransSpd[TRSPD_YT];
    case ITEM_HOURWEEK:    return tileTransSpd[TRSPD_HW];
    default:               return tileTransSpeed;
  }
}

static uint8_t gPreC2P[32];
static uint8_t gPreP2C[32];
void beginC2PCapture() { transCaptureCols(gPreC2P); }
// Each priority tile now supplies its own effect. Until this took a parameter,
// tileTransNotif / Ets2 / Sw / Tmr / Ip were saved, sent in /state and editable
// in the UI, but never read: every circuit-to-priority switch used tileTransC2P.
// Passing 0 (or nothing) keeps the old behaviour for callers with no per-tile
// setting of their own. The pace travels with the effect: the tile's own when
// it supplied one, the C2P setting's when that is what plays.
void finishC2PTransition(uint8_t tileTrans, uint8_t tileSpd) {
  uint8_t eff = tileTrans ? tileTrans : tileTransC2P;
  uint8_t pct = tileTrans ? tileSpd : tileTransSpd[TRSPD_C2P];
  if (eff == 0) return;
  uint8_t newCols[32];
  transCaptureCols(newCols);
  transWriteCols(gPreC2P);
  playTileTransition(eff, gPreC2P, newCols, pct);
}
void beginP2CCapture() { transCaptureCols(gPreP2C); }
void finishP2CTransition() {
  if (tileTransP2C == 0) return;
  uint8_t newCols[32];
  transCaptureCols(newCols);
  transWriteCols(gPreP2C);
  playTileTransition(tileTransP2C, gPreP2C, newCols, tileTransSpd[TRSPD_P2C]);
}

void drawCircuitTileFrame() {
  if (items[currentSlot].id == ITEM_NOW_PLAYING) {
    npInit();
  } else if (items[currentSlot].id == ITEM_WEATHER) {
    weatherInit();
  } else if (items[currentSlot].id == ITEM_MEMENTO) {
    mementoInit();
  } else if (items[currentSlot].id == ITEM_SCREENSAVER) {
    ssInit();
  } else if (items[currentSlot].id == ITEM_CURRENCY) {
    currencyInit();
  } else if (socialIndexForItem(items[currentSlot].id) >= 0) {
    socialInit(items[currentSlot].id);
  } else {

    mx.update(MD_MAX72XX::OFF);
    for (int i = 0; i < 32; i++) mx.setColumn(i, 0x00);
    mxCommit();

    switch (items[currentSlot].id) {
      case ITEM_HOUR:
        gLastStaticDrawMs = millis();
        drawHour();
        break;
      case ITEM_HOURWEEK:
        gLastStaticDrawMs = millis();
        drawHourWeekday();
        break;
      case ITEM_DATE:
        // Latched only if the draw happened: the clear above is already on the
        // panel, so a dateInit() that drew nothing has to be repeated rather
        // than remembered as done.
        gStaticDrawDone = dateInit();
        break;
      case ITEM_TEMP: {
        // Opportunistic fresh read, then draw unconditionally. tempInit()
        // renders "--" for TEMP_NO_READING, so the tile can no longer end up
        // with nothing on the matrix; loop() replaces the placeholder in place
        // the moment a real reading lands.
        float tf;
        if (bmpReadTempC(tf)) lastTemp = (int)round(tf);
        gStaticDrawDone = true;
        gTempShownValue = lastTemp;
        tempInit(lastTemp);
        break;
      }
      case ITEM_CANVAS:

        gStaticDrawDone = true;
        drawCanvas();
        break;
      case ITEM_PRESSURE: {

        pressureSampleTick();
        gStaticDrawDone = true;
        pressureInit((int)round(lastPressureHpa));
        break;
      }
    }
  }
}

void enterSlot(bool playTransition) {
  uint8_t transOldCols[32];
  transCaptureCols(transOldCols);

  uint8_t transEffect = playTransition ? getCircuitTileTransition(items[currentSlot].id) : 0;
  uint8_t transPct    = getCircuitTileTransSpeed(items[currentSlot].id);
  gSuppressHwFlash = (transEffect != 0);
  drawCircuitTileFrame();
  gSuppressHwFlash = false;

  if (transEffect != 0) {
    uint8_t transNewCols[32];
    transCaptureCols(transNewCols);
    transWriteCols(transOldCols);
    playTileTransition(transEffect, transOldCols, transNewCols, transPct);
  }
}

// redisplays the current Circuit Tiles tile after a Priority Tile
void resumeCircuitTileFromPriority() {
  beginP2CCapture();
  drawCircuitTileFrame();
  finishP2CTransition();
}

void advanceSlot() {
  beepSwitch();
  inScrollAnim = false;
  inFadeOut    = false;
  inFadeIn     = false;
  npState      = NP_SHOW_START;
  dateState    = NP_SHOW_START;

  gLastStaticDrawMs = 0;
  gStaticDrawDone   = false;

  int next = nextActiveSlot(currentSlot);
  if (next == -1) return;
  bool sameTile = (next == currentSlot);
  currentSlot = next;
  slotStartMs = millis();
  enterSlot(!sameTile);
}

void retreatSlot() {
  beepSwitch();
  inScrollAnim = false;
  inFadeOut    = false;
  inFadeIn     = false;
  npState      = NP_SHOW_START;
  dateState    = NP_SHOW_START;

  gLastStaticDrawMs = 0;
  gStaticDrawDone   = false;

  int prev = prevActiveSlot(currentSlot);
  if (prev == -1) return;
  bool sameTile = (prev == currentSlot);
  currentSlot = prev;
  slotStartMs = millis();
  enterSlot(!sameTile);
}

// SAVE / LOAD
bool isValidCircuitItemId(uint8_t id) {
  switch (id) {
    case ITEM_HOUR: case ITEM_DATE: case ITEM_TEMP: case ITEM_NOW_PLAYING:
    case ITEM_WEATHER: case ITEM_MEMENTO: case ITEM_CANVAS:
    case ITEM_PRESSURE: case ITEM_SCREENSAVER: case ITEM_CURRENCY:
    case ITEM_YOUTUBE: case ITEM_HOURWEEK:
      return true;
    default:
      return false;
  }
}

void repairItemIds() {
  static const uint8_t ALL_IDS[NUM_ITEMS] = {
    ITEM_HOUR, ITEM_DATE, ITEM_TEMP, ITEM_NOW_PLAYING, ITEM_WEATHER,
    ITEM_MEMENTO, ITEM_CANVAS, ITEM_PRESSURE, ITEM_SCREENSAVER, ITEM_CURRENCY,
    ITEM_YOUTUBE, ITEM_HOURWEEK
  };
  bool used[16] = { false };
  bool needsFix[NUM_ITEMS] = { false };
  for (int i = 0; i < NUM_ITEMS; i++) {
    if (!isValidCircuitItemId(items[i].id) || items[i].id >= 16 || used[items[i].id]) {
      needsFix[i] = true;
    } else {
      used[items[i].id] = true;
    }
  }
  int nextMissing = 0;
  for (int i = 0; i < NUM_ITEMS; i++) {
    if (!needsFix[i]) continue;
    while (nextMissing < NUM_ITEMS && used[ALL_IDS[nextMissing]]) nextMissing++;
    if (nextMissing < NUM_ITEMS) {
      items[i].id = ALL_IDS[nextMissing];
      used[ALL_IDS[nextMissing]] = true;
    }
  }
}

void loadSettings() {
  prefs.begin("settings", true);
  buzzerOn      = prefs.getBool("buzzer", true);
  buzzerVolume  = prefs.getUChar("buzzerVol", 80);
  // A clock from before the categories starts all four at its one volume.
  buzzVolNotif  = prefs.getUChar("bzVolNotif", buzzerVolume);
  buzzVolAuto   = prefs.getUChar("bzVolAuto",  buzzerVolume);
  buzzVolAlarm  = prefs.getUChar("bzVolAlarm", buzzerVolume);
  buzzVolTouch  = prefs.getUChar("bzVolTouch", buzzerVolume);
  prefs.getString("buzzerPreset", buzzerPreset, sizeof(buzzerPreset));
  if (strlen(buzzerPreset) == 0) strcpy(buzzerPreset, "calm");
  prefs.getString("evSndTile",  eventSoundTile,  sizeof(eventSoundTile));
  if (strlen(eventSoundTile) == 0) strcpy(eventSoundTile, "calm");
  prefs.getString("evSndWifi",  eventSoundWifi,  sizeof(eventSoundWifi));
  if (strlen(eventSoundWifi) == 0) strcpy(eventSoundWifi, "urgent");
  prefs.getString("evSndNotif", eventSoundNotif, sizeof(eventSoundNotif));
  if (strlen(eventSoundNotif) == 0) strcpy(eventSoundNotif, "soft");
  prefs.getString("evSndWeb", eventSoundWeb, sizeof(eventSoundWeb));
  if (strlen(eventSoundWeb) == 0) strcpy(eventSoundWeb, "soft");
  prefs.getString("evSndEts2",  eventSoundEts2,  sizeof(eventSoundEts2));
  if (strlen(eventSoundEts2) == 0) strcpy(eventSoundEts2, "urgent");
  prefs.getString("evSndTouch", eventSoundTouch, sizeof(eventSoundTouch));
  if (strlen(eventSoundTouch) == 0) strcpy(eventSoundTouch, "soft");
  prefs.getString("accentColor", accentColor, sizeof(accentColor));
  if (strlen(accentColor) == 0) strcpy(accentColor, "#d0bcff");
  touchTapAction       = prefs.getUChar("touchTap", 8);
  touchDoubleTapAction = prefs.getUChar("touchDbl", 0);
  npDisplayMode = prefs.getUChar("npmode", 0);
  ssAnimSelected = prefs.getUChar("ssAnim", SS_ANIM_RANDOM);
  tempUnit      = prefs.getUChar("tempunit", 0);
  hourFormat    = prefs.getUChar("hourformat", 0);
  hourLeadingZero = prefs.getBool("hourLeadZero", false);
  hwFormat      = prefs.getUChar("hwFmt", 0);
  hwLeadZero    = prefs.getBool("hwLeadZero", false);
  hwBarMode     = prefs.getUChar("hwBarMode", HW_BAR_SECONDS);
  hwBarPos      = prefs.getUChar("hwBarPos", HW_BAR_BELOW);
  hwSwap        = prefs.getBool("hwSwap", false);
  netTimeSync   = prefs.getBool("netTimeSync", true);
  showGrayedContent = prefs.getBool("showGrayed", true);
  autoSleepOn  = prefs.getBool("autoSleep", false);
  autoSleepSec = prefs.getUInt("autoSleepSec", 1800);
  if (autoSleepSec == 0) autoSleepSec = 1800;
  liveTileHighlight = prefs.getBool("liveHl", true);
  webUiDark         = prefs.getBool("uiDark", true);
  webUiShape        = prefs.getUChar("uiShape", 0);
  if (webUiShape > 3) webUiShape = 0;
  prefs.getString("uiColors", webUiColors, sizeof(webUiColors));
  prefs.getString("uiLang", webUiLang, sizeof(webUiLang));
  if (strcmp(webUiLang, "en") != 0) strcpy(webUiLang, "ro");
  defaultStartMode = prefs.getUChar("startMode", START_MODE_WIFI);
  if (defaultStartMode > START_MODE_AP) defaultStartMode = START_MODE_WIFI;
  dateFormat    = prefs.getUChar("dateformat", 2);
  dateLang      = prefs.getUChar("datelang", 0);
  prefs.getString("dateCustFmt", customDateFmt, sizeof(customDateFmt));
  notifEnabled  = prefs.getBool("notifEn", true);
  webAccessEnabled = prefs.getBool("webEn", true);
  ets2Enabled   = prefs.getBool("ets2En", true);
  ets2OrderFirst = prefs.getBool("ets2Ord", false);
  rebuildPriorityOrderFromEts2Order();
  nowPlayingIsPriority = prefs.getBool("npIsPrio", false);
  {
    uint8_t savedOrder[NUM_PRIORITY_IDS];
    size_t got = prefs.getBytes("prioOrder", savedOrder, NUM_PRIORITY_IDS);
    // An order saved before the alarm existed is one byte short. Rather than
    // throw the user's ranking away over it, take it as it stands and put the
    // alarm in front - where a fresh install gets it too.
    if (got == NUM_PRIORITY_IDS - 1) {
      for (int i = NUM_PRIORITY_IDS - 1; i > 0; i--) savedOrder[i] = savedOrder[i - 1];
      savedOrder[0] = PRIORITY_ID_ALARM;
      got = NUM_PRIORITY_IDS;
    }
    bool validPerm = (got == NUM_PRIORITY_IDS);
    if (validPerm) {
      bool seen[NUM_PRIORITY_IDS] = { false };
      for (int i = 0; i < NUM_PRIORITY_IDS; i++) {
        if (savedOrder[i] >= NUM_PRIORITY_IDS || seen[savedOrder[i]]) { validPerm = false; break; }
        seen[savedOrder[i]] = true;
      }
    }
    if (validPerm) {
      for (int i = 0; i < NUM_PRIORITY_IDS; i++) priorityOrder[i] = savedOrder[i];
    }

  }
  if (!nowPlayingIsPriority) npPriorityWasActive = false;
  hideTileIcons  = prefs.getBool("hideIcons", false);
  hideIconDate       = prefs.getBool("hideIconDate",  hideTileIcons);
  hideIconTemp       = prefs.getBool("hideIconTemp",  hideTileIcons);
  hideIconReminder   = prefs.getBool("hideIconRem",   hideTileIcons);
  hideIconWeather    = prefs.getBool("hideIconWx",    hideTileIcons);
  hideIconNotif      = prefs.getBool("hideIconNotif", hideTileIcons);
  hideIconNowPlaying = prefs.getBool("hideIconNp",    hideTileIcons);
  npAdaptiveIcon     = prefs.getBool("npAdaptIcon",   true);
  hideIconPressure   = prefs.getBool("hideIconPress", hideTileIcons);
  hideIconCurrency   = prefs.getBool("hideIconCurr",  hideTileIcons);
  hideIconYoutube    = prefs.getBool("hideIconYt",    hideTileIcons);
  hideIconWebAccess  = prefs.getBool("hideIconWeb",   hideTileIcons);
  hideIconIp         = prefs.getBool("hideIconIp",    hideTileIcons);
  iconSelDate       = prefs.getUChar("iconSelDate",  0);
  iconSelTemp       = prefs.getUChar("iconSelTemp",  0);
  iconSelReminder   = prefs.getUChar("iconSelRem",   0);
  iconSelNotif      = prefs.getUChar("iconSelNotif", 0);
  {
    // Migrate the old single "iconSelNp" override (if present) into
    // both new per-source slots the first time this runs.
    uint8_t legacyIconSelNp = prefs.getUChar("iconSelNp", 0);
    iconSelNpMusic = prefs.getUChar("iconSelNpM", legacyIconSelNp);
    iconSelNpVideo = prefs.getUChar("iconSelNpV", legacyIconSelNp);
  }
  iconSelPressure   = prefs.getUChar("iconSelPress", 0);
  iconSelCurrency   = prefs.getUChar("iconSelCurr",  0);
  iconSelIp         = prefs.getUChar("iconSelIp",    0);
  iconWxSunny       = prefs.getUChar("iconWxSunny",  0);
  iconWxCloud       = prefs.getUChar("iconWxCloud",  0);
  iconWxRain        = prefs.getUChar("iconWxRain",   0);
  iconWxStorm       = prefs.getUChar("iconWxStorm",  0);
  iconWxSnow        = prefs.getUChar("iconWxSnow",   0);
  iconWxWind        = prefs.getUChar("iconWxWind",   0);
  iconWxNight       = prefs.getUChar("iconWxNight",  0);
  scrollType     = prefs.getUChar("scrollType", 0);
  scrollTypeDate       = prefs.getUChar("scrollTypeDate",   scrollType);
  scrollTypeTemp       = prefs.getUChar("scrollTypeTemp",   scrollType);
  scrollTypeReminder   = prefs.getUChar("scrollTypeRem",    scrollType);
  scrollTypeWeather    = prefs.getUChar("scrollTypeWx",     scrollType);
  scrollTypeNotif      = prefs.getUChar("scrollTypeNotif",  scrollType);
  scrollTypeNowPlaying = prefs.getUChar("scrollTypeNp",     scrollType);
  scrollTypePressure   = prefs.getUChar("scrollTypePress",  0);
  scrollTypeCurrency   = prefs.getUChar("scrollTypeCurr",   0);
  scrollTypeYoutube    = prefs.getUChar("scrollTypeYt",     scrollType);
  scrollTypeWebAccess  = prefs.getUChar("scrollTypeWeb",    scrollType);
  scrollTypeStopwatch  = prefs.getUChar("scrollTypeSw",     scrollType);
  scrollTypeTimer      = prefs.getUChar("scrollTypeTmr",    0);
  scrollTypeIp         = prefs.getUChar("scrollTypeIp",     scrollType);
  scrollSpeed = prefs.getUChar("scrollSpeed", NP_SCROLL_SPEED_MS);
  if (scrollSpeed < SCROLL_SPEED_MIN || scrollSpeed > SCROLL_SPEED_MAX) scrollSpeed = NP_SCROLL_SPEED_MS;
  for (int i = 0; i < SCROLL_SPEED_COUNT; i++) {
    scrollSpeedTile[i] = prefs.getUChar((String("spd") + SPD_KEYS[i]).c_str(), scrollSpeed);
    if (scrollSpeedTile[i] < SCROLL_SPEED_MIN || scrollSpeedTile[i] > SCROLL_SPEED_MAX)
      scrollSpeedTile[i] = scrollSpeed;
  }

  fontType         = prefs.getUChar("fontType",     0);
  fontTypeDate       = prefs.getUChar("fontTypeDate",   fontType);
  fontTypeTemp       = prefs.getUChar("fontTypeTemp",   fontType);
  fontTypeReminder   = prefs.getUChar("fontTypeRem",    fontType);
  fontTypeWeather    = prefs.getUChar("fontTypeWx",     fontType);
  fontTypeNotif      = prefs.getUChar("fontTypeNotif",  fontType);
  fontTypeNowPlaying = prefs.getUChar("fontTypeNp",     fontType);
  fontTypePressure   = prefs.getUChar("fontTypePress",  fontType);
  fontTypeCurrency   = prefs.getUChar("fontTypeCurr",   fontType);
  fontTypeYoutube    = prefs.getUChar("fontTypeYt",     fontType);
  fontTypeWebAccess  = prefs.getUChar("fontTypeWeb",    fontType);
  fontTypeStopwatch  = prefs.getUChar("fontTypeSw",     fontType);
  fontTypeTimer      = prefs.getUChar("fontTypeTmr",    fontType);
  fontTypeIp         = prefs.getUChar("fontTypeIp",     fontType);
  // A value this build has no font for (saved by a newer one) would reach
  // draw paths that only know the modes above; the default always draws.
  {
    uint8_t* fts[] = { &fontType, &fontTypeDate, &fontTypeTemp, &fontTypeReminder,
                       &fontTypeWeather, &fontTypeNotif, &fontTypeNowPlaying,
                       &fontTypePressure, &fontTypeCurrency, &fontTypeYoutube,
                       &fontTypeWebAccess, &fontTypeStopwatch, &fontTypeTimer, &fontTypeIp };
    for (uint8_t* p : fts) if (*p >= FONT_MODE_COUNT) *p = FONT_MODE_DEFAULT;
  }
  tileTransGlobal = prefs.getUChar("trGlobal", 0);
  tileTransHour   = prefs.getUChar("trHour",   tileTransGlobal);
  tileTransDate   = prefs.getUChar("trDate",   tileTransGlobal);
  tileTransTemp   = prefs.getUChar("trTemp",   tileTransGlobal);
  tileTransNp     = prefs.getUChar("trNp",     tileTransGlobal);
  tileTransWx     = prefs.getUChar("trWx",     tileTransGlobal);
  tileTransRem    = prefs.getUChar("trRem",    tileTransGlobal);
  tileTransCanvas = prefs.getUChar("trCanvas", tileTransGlobal);
  tileTransPress  = prefs.getUChar("trPress",  tileTransGlobal);
  tileTransSs     = prefs.getUChar("trSs",     tileTransGlobal);
  tileTransCurr   = prefs.getUChar("trCurr",   tileTransGlobal);
  tileTransYt     = prefs.getUChar("trYt",     tileTransGlobal);
  tileTransHw     = prefs.getUChar("trHw",     tileTransGlobal);
  tileTransWeb    = prefs.getUChar("trWeb",    tileTransGlobal);
  tileTransNotif  = prefs.getUChar("trNotif",  tileTransGlobal);
  tileTransEts2   = prefs.getUChar("trEts2",   tileTransGlobal);
  tileTransSw     = prefs.getUChar("trSw",     tileTransGlobal);
  tileTransTmr    = prefs.getUChar("trTmr",    tileTransGlobal);
  tileTransAlarm  = prefs.getUChar("trAlarm",  tileTransGlobal);
  tileTransIp     = prefs.getUChar("trIp",     tileTransGlobal);
  tileTransC2P    = prefs.getUChar("trC2P",    tileTransGlobal);
  tileTransP2C    = prefs.getUChar("trP2C",    tileTransGlobal);
  tileTransSpeed  = prefs.getUChar("trSpeed",  TRSPD_DEFAULT);
  if (tileTransSpeed < TRSPD_MIN || tileTransSpeed > TRSPD_MAX) tileTransSpeed = TRSPD_DEFAULT;
  // A shorter blob from an older build leaves the newer entries on the global.
  for (int i = 0; i < TRSPD_COUNT; i++) tileTransSpd[i] = tileTransSpeed;
  prefs.getBytes("trSpeeds", tileTransSpd, TRSPD_COUNT);
  for (int i = 0; i < TRSPD_COUNT; i++)
    if (tileTransSpd[i] < TRSPD_MIN || tileTransSpd[i] > TRSPD_MAX) tileTransSpd[i] = tileTransSpeed;
  prefs.getString("evSndTimer", eventSoundTimer, sizeof(eventSoundTimer));
  if (strlen(eventSoundTimer) == 0) strcpy(eventSoundTimer, "calm");
  timerDurationSec = prefs.getUInt("timerDurSec", 300);
  if (timerDurationSec == 0) timerDurationSec = 300;
  timerPreset = prefs.getUChar("timerPreset", 0);
  timerRemainingSnapMs = (unsigned long)timerDurationSec * 1000UL;
  timerFinished = false;
  timerWasActive = false;
  // Each alarm is one packed word plus its tone. A key that was never written
  // leaves the built-in default in place, which is what a first boot wants.
  for (int i = 0; i < ALARM_COUNT; i++) {
    char k[12];
    snprintf(k, sizeof(k), "alarm%d", i);
    uint32_t packed = prefs.getUInt(k, 0xFFFFFFFFUL);
    if (packed != 0xFFFFFFFFUL) {
      uint8_t h = (uint8_t)(packed & 0xFF);
      uint8_t m = (uint8_t)((packed >> 8) & 0xFF);
      if (h < 24 && m < 60) {
        alarms[i].hour   = h;
        alarms[i].minute = m;
        alarms[i].days   = (uint8_t)((packed >> 16) & 0x7F);
      }
      alarms[i].enabled = ((packed >> 23) & 1) != 0;
      // Bit 25 marks a word written by a build that knows about presence. An
      // older one has neither bit, and an alarm it had switched on is one the
      // owner set on purpose - so that is what comes back.
      alarms[i].present = ((packed >> 25) & 1)
                            ? (((packed >> 24) & 1) != 0)
                            : alarms[i].enabled;
    }
    snprintf(k, sizeof(k), "alarmTone%d", i);
    prefs.getString(k, alarms[i].tone, sizeof(alarms[i].tone));
    if (strlen(alarms[i].tone) == 0) strcpy(alarms[i].tone, "classic");
    alarmFired[i] = false;
  }
  // Nothing rings the instant the device comes up: an alarm whose minute went by
  // while it was off has gone by, and one whose minute is now gets picked up by
  // the first alarmTick() after the clock is set.
  alarmRinging    = false;
  alarmRingingIdx = -1;
  alarmWasActive  = false;
  for (int i = 0; i < NUM_ITEMS; i++) {
    String base = "it" + String(i);
    items[i].id          = prefs.getUChar((base + "id").c_str(),  items[i].id);
    items[i].enabled     = prefs.getBool((base + "en").c_str(),  items[i].enabled);
    items[i].durationSec = prefs.getUShort((base + "dur").c_str(), items[i].durationSec);
    items[i].order       = prefs.getUChar((base + "ord").c_str(),  items[i].order);
  }

  repairItemIds();
  tileHiddenMask = (uint16_t)prefs.getUShort("tileHidden", 0);
  prioHiddenMask = prefs.getUChar("prioHidden", 0);
  applyTileHiddenMask();
  curBrightness = prefs.getUChar("bright",  BRIGHTNESS);
  dimAutoOn     = prefs.getBool("dimAuto",  false);
  dimFromH      = prefs.getUChar("dimFH",   22);
  dimFromM      = prefs.getUChar("dimFM",   0);
  dimToH        = prefs.getUChar("dimTH",   7);
  dimToM        = prefs.getUChar("dimTM",   0);
  dimLevel      = prefs.getUChar("dimLvl",  1);
  prefs.getString("wxCity",   weatherCity,   sizeof(weatherCity));
  prefs.getString("wxApiKey", weatherApiKey, sizeof(weatherApiKey));
  // A key mangled by the old masked-field bug is not a key. Drop it, so the
  // tile says "no key" and asks for a new one instead of spending every fetch
  // on a 401 nobody can see.
  if (weatherApiKey[0] && !wxKeyLooksValid(String(weatherApiKey))) weatherApiKey[0] = '\0';
  prefs.getString("wxLang",   weatherLang,   sizeof(weatherLang));
  wxPreset = prefs.getUChar("wxPreset", WX_SHOW_ALL);
  if (wxPreset == 0 || wxPreset > WX_SHOW_ALL) wxPreset = WX_SHOW_ALL;
  weatherLat = prefs.getFloat("wxLat", weatherLat);
  weatherLon = prefs.getFloat("wxLon", weatherLon);
  {
    // Social tiles: three slots stored under indexed keys.
    prefs.getString("socH0", social[SOC_YT].handle, sizeof(social[SOC_YT].handle));
    prefs.getString("socK0", social[SOC_YT].apiKey, sizeof(social[SOC_YT].apiKey));
    social[SOC_YT].showName = prefs.getBool("socN0", false);
    social[SOC_YT].count = 0;
    social[SOC_YT].valid = false;
    iconSelYoutube = prefs.getUChar("icoYt", 0);
  }
  prefs.getString("currBase",  currencyBase,  sizeof(currencyBase));
  if (strlen(currencyBase) == 0) strcpy(currencyBase, "EUR");
  prefs.getString("currQuote", currencyQuote, sizeof(currencyQuote));
  if (strlen(currencyQuote) == 0) strcpy(currencyQuote, "RON");
  currencyCompareEnabled = prefs.getBool("currCompare", false);
  prefs.getString("memoText", mementoBuf, sizeof(mementoBuf));
  prefs.getBytes("canvasBmp", canvasBitmap, CANVAS_COLS);
  prefs.end();
  sortItems();
}

// DEFERRED SETTINGS WRITE
//
// saveSettings() pushes 118 keys into NVS. It used to run synchronously on every
// single change - each dropdown in Scroll Type, each brightness tap on the touch
// sensor - so one UI interaction meant 118 NVS transactions, and holding the
// touch pad meant a burst of them. Callers now mark the settings dirty instead
// and loop() does one real write once the changes stop.
static bool          settingsDirty   = false;
static unsigned long settingsDirtyMs = 0;
#define SETTINGS_FLUSH_MS 800UL

void saveSettingsDeferred() {
  settingsDirty   = true;
  settingsDirtyMs = millis();
}

// Write now if anything is pending - used before a deliberate reboot.
void saveSettingsFlush() {
  if (!settingsDirty) return;
  settingsDirty = false;
  saveSettings();
}

// Drop anything pending - used by the factory reset, so a queued write cannot
// repopulate the namespace we just wiped.
void saveSettingsCancel() { settingsDirty = false; }

static void settingsFlushTick() {
  if (!settingsDirty) return;
  if (millis() - settingsDirtyMs < SETTINGS_FLUSH_MS) return;
  saveSettingsFlush();
}

void saveSettings() {
  prefs.begin("settings", false);
  prefs.putBool("buzzer", buzzerOn);
  prefs.putUChar("buzzerVol", buzzerVolume);
  prefs.putUChar("bzVolNotif", buzzVolNotif);
  prefs.putUChar("bzVolAuto",  buzzVolAuto);
  prefs.putUChar("bzVolAlarm", buzzVolAlarm);
  prefs.putUChar("bzVolTouch", buzzVolTouch);
  prefs.putString("buzzerPreset", buzzerPreset);
  prefs.putUChar("npmode", npDisplayMode);
  prefs.putUChar("ssAnim", ssAnimSelected);
  prefs.putUChar("tempunit", tempUnit);
  prefs.putUChar("hourformat", hourFormat);
  prefs.putBool("hourLeadZero", hourLeadingZero);
  prefs.putUChar("hwFmt", hwFormat);
  prefs.putBool("hwLeadZero", hwLeadZero);
  prefs.putUChar("hwBarMode", hwBarMode);
  prefs.putUChar("hwBarPos", hwBarPos);
  prefs.putBool("hwSwap", hwSwap);
  prefs.putBool("netTimeSync", netTimeSync);
  prefs.putBool("showGrayed", showGrayedContent);
  prefs.putBool("autoSleep", autoSleepOn);
  prefs.putUInt("autoSleepSec", autoSleepSec);
  prefs.putBool("liveHl", liveTileHighlight);
  prefs.putBool("uiDark", webUiDark);
  prefs.putUChar("uiShape", webUiShape);
  prefs.putString("uiColors", webUiColors);
  prefs.putString("uiLang", webUiLang);
  prefs.putUChar("startMode", defaultStartMode);
  prefs.putUChar("dateformat", dateFormat);
  prefs.putUChar("datelang", dateLang);
  prefs.putString("dateCustFmt", customDateFmt);
  prefs.putUChar("touchTap", touchTapAction);
  prefs.putUChar("touchDbl", touchDoubleTapAction);
  prefs.putBool("notifEn", notifEnabled);
  prefs.putBool("webEn", webAccessEnabled);
  prefs.putBool("ets2En", ets2Enabled);
  prefs.putBool("ets2Ord", ets2OrderFirst);
  prefs.putBool("npIsPrio", nowPlayingIsPriority);
  prefs.putBytes("prioOrder", priorityOrder, NUM_PRIORITY_IDS);
  prefs.putBool("hideIcons", hideTileIcons);
  prefs.putBool("hideIconDate",  hideIconDate);
  prefs.putBool("hideIconTemp",  hideIconTemp);
  prefs.putBool("hideIconRem",   hideIconReminder);
  prefs.putBool("hideIconWx",    hideIconWeather);
  prefs.putBool("hideIconNotif", hideIconNotif);
  prefs.putBool("hideIconNp",    hideIconNowPlaying);
  prefs.putBool("npAdaptIcon",   npAdaptiveIcon);
  prefs.putBool("hideIconPress", hideIconPressure);
  prefs.putBool("hideIconCurr",  hideIconCurrency);
  prefs.putBool("hideIconYt",    hideIconYoutube);
  prefs.putBool("hideIconWeb",   hideIconWebAccess);
  prefs.putBool("hideIconIp",    hideIconIp);
  prefs.putUChar("iconSelDate",  iconSelDate);
  prefs.putUChar("iconSelTemp",  iconSelTemp);
  prefs.putUChar("iconSelRem",   iconSelReminder);
  prefs.putUChar("iconSelNotif", iconSelNotif);
  prefs.putUChar("iconSelNpM",   iconSelNpMusic);
  prefs.putUChar("iconSelNpV",   iconSelNpVideo);
  prefs.putUChar("iconSelPress", iconSelPressure);
  prefs.putUChar("iconSelCurr",  iconSelCurrency);
  prefs.putUChar("iconSelIp",    iconSelIp);
  prefs.putUChar("iconWxSunny",  iconWxSunny);
  prefs.putUChar("iconWxCloud",  iconWxCloud);
  prefs.putUChar("iconWxRain",   iconWxRain);
  prefs.putUChar("iconWxStorm",  iconWxStorm);
  prefs.putUChar("iconWxSnow",   iconWxSnow);
  prefs.putUChar("iconWxWind",   iconWxWind);
  prefs.putUChar("iconWxNight",  iconWxNight);
  prefs.putUChar("scrollType", scrollType);
  prefs.putUChar("scrollTypeDate",  scrollTypeDate);
  prefs.putUChar("scrollTypeTemp",  scrollTypeTemp);
  prefs.putUChar("scrollTypeRem",   scrollTypeReminder);
  prefs.putUChar("scrollTypeWx",    scrollTypeWeather);
  prefs.putUChar("scrollTypeNotif", scrollTypeNotif);
  prefs.putUChar("scrollTypeNp",    scrollTypeNowPlaying);
  prefs.putUChar("scrollTypePress", scrollTypePressure);
  prefs.putUChar("scrollTypeCurr",  scrollTypeCurrency);
  prefs.putUChar("scrollTypeYt",    scrollTypeYoutube);
  prefs.putUChar("scrollTypeWeb",   scrollTypeWebAccess);
  prefs.putUChar("scrollTypeSw",    scrollTypeStopwatch);
  prefs.putUChar("scrollTypeTmr",   scrollTypeTimer);
  prefs.putUChar("scrollTypeIp",    scrollTypeIp);
  prefs.putUChar("scrollSpeed", scrollSpeed);
  for (int i = 0; i < SCROLL_SPEED_COUNT; i++)
    prefs.putUChar((String("spd") + SPD_KEYS[i]).c_str(), scrollSpeedTile[i]);

  prefs.putUChar("fontType",     fontType);
  prefs.putUChar("fontTypeDate",  fontTypeDate);
  prefs.putUChar("fontTypeTemp",  fontTypeTemp);
  prefs.putUChar("fontTypeRem",   fontTypeReminder);
  prefs.putUChar("fontTypeWx",    fontTypeWeather);
  prefs.putUChar("fontTypeNotif", fontTypeNotif);
  prefs.putUChar("fontTypeNp",    fontTypeNowPlaying);
  prefs.putUChar("fontTypePress", fontTypePressure);
  prefs.putUChar("fontTypeCurr",  fontTypeCurrency);
  prefs.putUChar("fontTypeYt",    fontTypeYoutube);
  prefs.putUChar("fontTypeWeb",   fontTypeWebAccess);
  prefs.putUChar("fontTypeSw",    fontTypeStopwatch);
  prefs.putUChar("fontTypeTmr",   fontTypeTimer);
  prefs.putUChar("fontTypeIp",    fontTypeIp);
  prefs.putUChar("trGlobal", tileTransGlobal);
  prefs.putUChar("trHour",   tileTransHour);
  prefs.putUChar("trDate",   tileTransDate);
  prefs.putUChar("trTemp",   tileTransTemp);
  prefs.putUChar("trNp",     tileTransNp);
  prefs.putUChar("trWx",     tileTransWx);
  prefs.putUChar("trRem",    tileTransRem);
  prefs.putUChar("trCanvas", tileTransCanvas);
  prefs.putUChar("trPress",  tileTransPress);
  prefs.putUChar("trSs",     tileTransSs);
  prefs.putUChar("trCurr",   tileTransCurr);
  prefs.putUChar("trYt",     tileTransYt);
  prefs.putUChar("trHw",     tileTransHw);
  prefs.putUChar("trWeb",    tileTransWeb);
  prefs.putUChar("trNotif",  tileTransNotif);
  prefs.putUChar("trEts2",   tileTransEts2);
  prefs.putUChar("trSw",     tileTransSw);
  prefs.putUChar("trTmr",    tileTransTmr);
  prefs.putUChar("trAlarm",  tileTransAlarm);
  prefs.putUChar("trIp",     tileTransIp);
  prefs.putUChar("trC2P",    tileTransC2P);
  prefs.putUChar("trP2C",    tileTransP2C);
  prefs.putUChar("trSpeed",  tileTransSpeed);
  prefs.putBytes("trSpeeds", tileTransSpd, TRSPD_COUNT);
  prefs.putString("evSndTimer", eventSoundTimer);
  prefs.putUInt("timerDurSec", timerDurationSec);
  prefs.putUChar("timerPreset", timerPreset);
  for (int i = 0; i < ALARM_COUNT; i++) {
    char k[12];
    snprintf(k, sizeof(k), "alarm%d", i);
    prefs.putUInt(k, (uint32_t)alarms[i].hour
                   | ((uint32_t)alarms[i].minute << 8)
                   | ((uint32_t)(alarms[i].days & 0x7F) << 16)
                   | ((uint32_t)(alarms[i].enabled ? 1 : 0) << 23)
                   | ((uint32_t)(alarms[i].present ? 1 : 0) << 24)
                   | (1UL << 25));
    snprintf(k, sizeof(k), "alarmTone%d", i);
    prefs.putString(k, alarms[i].tone);
  }
  prefs.putUShort("tileHidden", tileHiddenMask);
  prefs.putUChar("prioHidden", prioHiddenMask);
  for (int i = 0; i < NUM_ITEMS; i++) {
    String base = "it" + String(i);
    prefs.putUChar((base + "id").c_str(),   items[i].id);
    prefs.putBool((base + "en").c_str(),    items[i].enabled);
    prefs.putUShort((base + "dur").c_str(), items[i].durationSec);
    prefs.putUChar((base + "ord").c_str(),  items[i].order);
  }
  prefs.putUChar("bright",  curBrightness);
  prefs.putBool("dimAuto",  dimAutoOn);
  prefs.putUChar("dimFH",   dimFromH);
  prefs.putUChar("dimFM",   dimFromM);
  prefs.putUChar("dimTH",   dimToH);
  prefs.putUChar("dimTM",   dimToM);
  prefs.putUChar("dimLvl",  dimLevel);
  prefs.putString("wxCity",   weatherCity);
  prefs.putString("wxApiKey", weatherApiKey);
  prefs.putString("wxLang",   weatherLang);
  prefs.putUChar("wxPreset", wxPreset);
  prefs.putFloat("wxLat",    weatherLat);
  prefs.putFloat("wxLon",    weatherLon);
  {
    prefs.putString("socH0", social[SOC_YT].handle);
    prefs.putString("socK0", social[SOC_YT].apiKey);
    prefs.putBool("socN0", social[SOC_YT].showName);
    prefs.putUChar("icoYt", iconSelYoutube);
  }
  prefs.putString("currBase",  currencyBase);
  prefs.putString("currQuote", currencyQuote);
  prefs.putBool("currCompare", currencyCompareEnabled);
  prefs.putString("memoText", mementoBuf);
  prefs.putBytes("canvasBmp", canvasBitmap, CANVAS_COLS);
  prefs.end();
}

// WIFI
String scanNetworks() {
  // WiFi.scanNetworks() with no args runs SYNCHRONOUSLY and blocks the calling
  // task - and since this whole app runs on the single-threaded, synchronous
  // WebServer (not an async server), that blocked every other HTTP request
  // (including /state, and a second /scan) for the full scan duration
  // (commonly several seconds), plus froze loop() itself (display updates,
  // etc.) for that window. This is rewritten around the async SDK scan so the
  // handler always returns immediately; the client polls until results land.
  int n = WiFi.scanComplete();

  if (n == WIFI_SCAN_RUNNING) {
    // A scan (ours or one kicked off moments ago) is still in flight. Say so
    // explicitly instead of returning an empty list, so the client can tell
    // "try again shortly" apart from "no networks found".
    return "{\"scanning\":true,\"networks\":[]}";
  }

  if (n == WIFI_SCAN_FAILED) {
    // No scan has completed yet (first call ever, or the previous one
    // errored). Kick one off and tell the client to poll again.
    WiFi.scanNetworks(true /* async */);
    return "{\"scanning\":true,\"networks\":[]}";
  }

  // n >= 0: a completed scan's results are ready.
  String json;
  json.reserve(64 + n * 96);
  json = "{\"scanning\":false,\"networks\":[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    wifi_auth_mode_t auth = WiFi.encryptionType(i);
    bool secured = (auth != WIFI_AUTH_OPEN);
    String authStr;
    switch (auth) {
      case WIFI_AUTH_OPEN:          authStr = "Open"; break;
      case WIFI_AUTH_WEP:           authStr = "WEP"; break;
      case WIFI_AUTH_WPA_PSK:       authStr = "WPA"; break;
      case WIFI_AUTH_WPA2_PSK:      authStr = "WPA2"; break;
      case WIFI_AUTH_WPA_WPA2_PSK:  authStr = "WPA/WPA2"; break;
      case WIFI_AUTH_WPA3_PSK:      authStr = "WPA3"; break;
      case WIFI_AUTH_WPA2_WPA3_PSK: authStr = "WPA2/WPA3"; break;
      default:                      authStr = "WPA2"; break;
    }
    json += "{\"ssid\":\"";
    json += WiFi.SSID(i);
    json += "\",\"rssi\":";
    json += String(WiFi.RSSI(i));
    json += ",\"secured\":";
    json += (secured ? "true" : "false");
    json += ",\"auth\":\"";
    json += authStr;
    json += "\"}";
  }
  json += "]}";
  WiFi.scanDelete();
  // Kick off the next scan now so a poll a few seconds from now (silent
  // auto-scan or a manual refresh) finds fresh results already waiting
  // instead of blocking again.
  WiFi.scanNetworks(true);
  return json;
}

// HTML Web Interface for Octoglow
// The dashboard and the login shell now live in portal.html / auth.html next to
// this sketch and are compiled in gzipped, which is why they are not inline any
// more: as raw text they were 418 KB, about a quarter of the whole binary.
// After editing either .html, regenerate the header:  python build_web.py
#include "web_assets.h"

// Both pages are stored gzipped, so they go out with Content-Encoding: gzip
// and the browser inflates them. Every browser sends Accept-Encoding: gzip,
// and nothing else on the device fetches these two pages - Octoglow Connect
// only talks to the JSON endpoints.
// NetworkClient::write() loops on select() until the whole buffer is out, so
// pushing the page blocks loop() - and therefore the matrix animation - for as
// long as the transfer takes. The pages only change when the firmware does, so
// they carry an ETag: on every open after the first the browser revalidates and
// gets a 304 with no body, and nothing stalls.
static void sendGzipHtml(const uint8_t* body, uint32_t len, const char* etag) {
  uint32_t _t0 = micros();
  server.client().setNoDelay(true);
  String tag = String("\"") + etag + "-" + String(len) + "\"";

  if (server.hasHeader("If-None-Match") && server.header("If-None-Match") == tag) {
    hwPage304++;
    server.sendHeader("ETag", tag);
    server.sendHeader("Cache-Control", "no-cache");
    server.send(304, "text/html", "");
    { uint32_t d = micros() - _t0; if (d > hwPageUs) hwPageUs = d; }
    return;
  }

  hwPageFull++;
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("ETag", tag);
  // no-cache means "revalidate every time", not "do not store" - the browser
  // keeps the body and we answer the revalidation with 304.
  server.sendHeader("Cache-Control", "no-cache");
  server.setContentLength(len);
  server.send(200, "text/html", "");
  server.sendContent_P((PGM_P)body, len);
  { uint32_t d = micros() - _t0; if (d > hwPageUs) hwPageUs = d; }
}


// SERVER HANDLERS
// AUTH HANDLERS

// Handler HTTP: curent (JSON) for Auth.
void handleAuthState() {
  server.send(200, "application/json",
    String("{\"configured\":") + (authConfigured ? "true" : "false") + "}");
}

// Handler HTTP route /authSetup on web.
void handleAuthSetup() {
  if (authConfigured) { server.send(403, "text/plain", "Cont deja existent"); return; }
  if (!server.hasArg("user") || !server.hasArg("pass")) {
    server.send(400, "text/plain", "Lipsesc campuri"); return;
  }
  String u = server.arg("user"); u.trim();
  String p = server.arg("pass");
  if (u.length() < 1 || u.length() > 32 || p.length() < 4) {
    server.send(400, "text/plain", "Date invalide"); return;
  }
  char hash[65];
  sha256hex(p.c_str(), hash);
  saveAuth(u.c_str(), hash);

  String newTok = makeSessionToken(u.c_str());
  server.sendHeader("Set-Cookie", String("sc_tok=") + newTok +
    "; Path=/; HttpOnly; Max-Age=" + String(SESSION_TTL_SECONDS) + "; SameSite=Lax");
  server.send(200, "application/json", String("{\"ok\":true,\"user\":\"") + u + "\"}");
}


void handleLogin() {
  if (!authConfigured) { server.send(403, "text/plain", "Niciun cont configurat"); return; }
  if (!server.hasArg("user") || !server.hasArg("pass")) {
    server.send(400, "text/plain", "Lipsesc campuri"); return;
  }
  String u = server.arg("user"); u.trim();
  String p = server.arg("pass");
  char hash[65];
  sha256hex(p.c_str(), hash);
  if (u != String(authUser) || strcmp(hash, authPassHash) != 0) {
    server.send(401, "application/json", "{\"ok\":false,\"err\":\"Credentiale incorecte\"}");
    return;
  }

  String newTok = makeSessionToken(u.c_str());
  server.sendHeader("Set-Cookie", String("sc_tok=") + newTok +
    "; Path=/; HttpOnly; Max-Age=" + String(SESSION_TTL_SECONDS) + "; SameSite=Lax");
  server.send(200, "application/json", String("{\"ok\":true,\"user\":\"") + u + "\"}");
}

void handleLogout() {
  // Token-ul e stateless (nu exista nicio lista de sesiuni pe ESP), deci
  // "logout" inseamna doar stergerea cookie-ului din browser-ul curent.
  server.sendHeader("Set-Cookie", "sc_tok=; Path=/; HttpOnly; Max-Age=0; SameSite=Lax");
  server.send(200, "text/plain", "ok");
}

void handleUpdateAccount() {
  if (!provisionMode && getTokenUser().length() == 0) {
    server.send(401, "application/json", "{\"ok\":false,\"err\":\"Neautentificat\"}");
    return;
  }

  if (!provisionMode) {
    String curPass = server.hasArg("curpass") ? server.arg("curpass") : "";
    char curHash[65];
    sha256hex(curPass.c_str(), curHash);
    if (strcmp(curHash, authPassHash) != 0) {
      server.send(401, "application/json", "{\"ok\":false,\"err\":\"Parola actuala incorecta\"}");
      return;
    }
  }
  String newUser = server.hasArg("user") ? server.arg("user") : String(authUser);
  newUser.trim();
  if (newUser.length() < 1 || newUser.length() > 32) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"Username invalid\"}");
    return;
  }
  String newPass = server.hasArg("newpass") ? server.arg("newpass") : "";
  char finalHash[65];
  if (newPass.length() > 0) {
    if (newPass.length() < 4) {
      server.send(400, "application/json", "{\"ok\":false,\"err\":\"Parola noua trebuie sa aiba cel putin 4 caractere\"}");
      return;
    }
    sha256hex(newPass.c_str(), finalHash);
  } else {
    strncpy(finalHash, authPassHash, 64); finalHash[64] = '\0';
  }
  saveAuth(newUser.c_str(), finalHash);
  server.send(200, "application/json", String("{\"ok\":true,\"user\":\"") + newUser + "\"}");
}

// SOFTWARE UPDATE (OTA)

bool swOtaAuthOk = false;
String swOtaErr = "";

String swReadUpdateError() {
  String out = "";
  StreamString ss;
  Update.printError(ss);
  out = ss.readString();
  out.trim();
  return out;
}

void handleSwUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    swOtaAuthOk = provisionMode || (getTokenUser().length() > 0);
    swOtaErr = "";
    if (swOtaAuthOk) {
      Serial.printf("Software Update: %s\n", upload.filename.c_str());

      if (Update.isRunning()) Update.abort();
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        swOtaErr = swReadUpdateError();
        Serial.println("Update.begin a esuat: " + swOtaErr);
      }
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (swOtaAuthOk && swOtaErr.length() == 0) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        swOtaErr = swReadUpdateError();
        Serial.println("Update.write a esuat: " + swOtaErr);
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (swOtaAuthOk && swOtaErr.length() == 0) {
      if (Update.end(true)) {
        Serial.printf("Software Update Success: %u bytes\n", upload.totalSize);
      } else {
        swOtaErr = swReadUpdateError();
        Serial.println("Update.end a esuat: " + swOtaErr);
      }
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (Update.isRunning()) Update.abort();
    swOtaErr = "Upload intrerupt";
  }
}

void handleSwUpdateUploadDone() {
  server.sendHeader("Connection", "close");
  if (!swOtaAuthOk) {
    server.send(401, "application/json", "{\"ok\":false,\"err\":\"Neautentificat\"}");
    return;
  }
  if (swOtaErr.length() > 0 || Update.hasError()) {
    String err = swOtaErr.length() > 0 ? swOtaErr : swReadUpdateError();
    if (err.length() == 0) err = "Eroare necunoscuta";
    String body = "{\"ok\":false,\"err\":\"" + err + "\"}";
    server.send(200, "application/json", body);
  } else {
    server.send(200, "application/json", "{\"ok\":true}");
    saveSettingsFlush();
    delay(500);
    ESP.restart();
  }
}

void handleWhoami() {
  if (provisionMode) {
    String u = authConfigured ? String(authUser) : String("Admin");
    server.send(200, "application/json", String("{\"user\":\"") + u + "\"}");
    return;
  }
  String u = getTokenUser();
  if (u.length() == 0) { server.send(401, "text/plain", "Neautentificat"); return; }
  server.send(200, "application/json", String("{\"user\":\"") + u + "\"}");
}

void handleDashboard() {
  if (!provisionMode && getTokenUser().length() == 0) {
    server.send(401, "text/plain", "Neautentificat");
    return;
  }

  // Announced on the matrix every time a browser opens the dashboard - one
  // request per page load, so a phone connecting shows it again. Deliberately
  // not gated on notifEnabled: that switch is labelled "PC Notifications" and
  // covers what Octoglow Connect sends, which this is not. Skipped while
  // something more important is on screen.
  if (webAccessEnabled && !provisionMode &&
      !higherPriorityTileActive(PRIORITY_ID_WEB)) {
    strncpy(notifBuf, "Web Interface accesed", sizeof(notifBuf) - 1);
    notifBuf[sizeof(notifBuf) - 1] = '\0';
    notifIconOverride  = keyIcon;
    webAccessAlertActive = true;
    notifActive = true;
    beginC2PCapture();
    notifInit();
    finishC2PTransition(tileTransWeb, tileTransSpd[TRSPD_WEB]);
    nbPlayPreset(eventSoundWeb, BZ_CAT_NOTIF);
  }
  sendGzipHtml(PORTAL_HTML_GZ, PORTAL_HTML_GZ_LEN, PORTAL_HTML_GZ_SRC_SHA);
}



void handleRoot() {
  if (provisionMode) {
    sendGzipHtml(PORTAL_HTML_GZ, PORTAL_HTML_GZ_LEN, PORTAL_HTML_GZ_SRC_SHA);
    return;
  }
  sendGzipHtml(AUTH_SHELL_GZ, AUTH_SHELL_GZ_LEN, AUTH_SHELL_GZ_SRC_SHA);
}
void handleScan() {
  if (!checkAuth()) return;
  server.send(200, "application/json", scanNetworks());
}
void handleConnect() {
  if (!checkAuth()) return;
  if (!server.hasArg("ssid")) { server.send(400, "text/plain", "Lipseste SSID"); return; }
  String newSSID = server.arg("ssid");
  String newPass = server.hasArg("pass") ? server.arg("pass") : "";
  prefs.begin("wifi", false);
  prefs.putString("ssid", newSSID);
  gWifiSsidCached = newSSID;
  prefs.putString("pass", newPass);
  prefs.end();

  server.send(200, "text/plain", "Credentiale salvate!");
  delay(500);
  if (provisionMode) {
    stopProvisionMode();
  }
  connectToWiFi(newSSID.c_str(), newPass.c_str());
}

void handleSettings() {
  if (!checkAuth()) return;
  if (server.hasArg("buzzer")) buzzerOn = (server.arg("buzzer") == "1");
  bool hideIconTouched = false;
  if (server.hasArg("hideIcons")) {
    bool newVal = (server.arg("hideIcons") == "1");

    if (newVal != hideTileIcons) {
      hideIconTouched = true;
      hideTileIcons = newVal;

      hideIconDate       = hideTileIcons;
      hideIconTemp       = hideTileIcons;
      hideIconReminder   = hideTileIcons;
      hideIconWeather    = hideTileIcons;
      hideIconNotif      = hideTileIcons;
      hideIconNowPlaying = hideTileIcons;
      hideIconPressure   = hideTileIcons;
      hideIconCurrency   = hideTileIcons;
      hideIconYoutube    = hideTileIcons;
      hideIconWebAccess  = hideTileIcons;
    }
  }

  if (server.hasArg("hideIconDate"))       { bool v = (server.arg("hideIconDate")       == "1"); if (v != hideIconDate)       hideIconTouched = true; hideIconDate       = v; }
  if (server.hasArg("hideIconTemp"))       { bool v = (server.arg("hideIconTemp")       == "1"); if (v != hideIconTemp)       hideIconTouched = true; hideIconTemp       = v; }
  if (server.hasArg("hideIconReminder"))   { bool v = (server.arg("hideIconReminder")   == "1"); if (v != hideIconReminder)   hideIconTouched = true; hideIconReminder   = v; }
  if (server.hasArg("hideIconWeather"))    { bool v = (server.arg("hideIconWeather")    == "1"); if (v != hideIconWeather)    hideIconTouched = true; hideIconWeather    = v; }
  if (server.hasArg("hideIconNotif"))      { bool v = (server.arg("hideIconNotif")      == "1"); if (v != hideIconNotif)      hideIconTouched = true; hideIconNotif      = v; }
  if (server.hasArg("hideIconNowPlaying")) { bool v = (server.arg("hideIconNowPlaying") == "1"); if (v != hideIconNowPlaying) hideIconTouched = true; hideIconNowPlaying = v; }
  if (server.hasArg("hideIconPressure"))   { bool v = (server.arg("hideIconPressure")   == "1"); if (v != hideIconPressure)   hideIconTouched = true; hideIconPressure   = v; }
  if (server.hasArg("hideIconCurrency"))   { bool v = (server.arg("hideIconCurrency")   == "1"); if (v != hideIconCurrency)   hideIconTouched = true; hideIconCurrency   = v; }
  if (server.hasArg("hideIconYoutube"))    { bool v = (server.arg("hideIconYoutube")    == "1"); if (v != hideIconYoutube)    hideIconTouched = true; hideIconYoutube    = v; }
  if (server.hasArg("hideIconWebAccess"))  { bool v = (server.arg("hideIconWebAccess")  == "1"); if (v != hideIconWebAccess)  hideIconTouched = true; hideIconWebAccess  = v; }
  if (server.hasArg("hideIconIp"))         { bool v = (server.arg("hideIconIp")         == "1"); if (v != hideIconIp)         hideIconTouched = true; hideIconIp         = v; }
  if (server.hasArg("npAdaptiveIcon"))     { npAdaptiveIcon = (server.arg("npAdaptiveIcon") == "1"); }
  bool iconSelTouched = false;
  if (server.hasArg("iconSelYt"))    { int v = server.arg("iconSelYt").toInt();    if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconSelYoutube)   iconSelTouched = true; iconSelYoutube   = (uint8_t)v; } }
  if (server.hasArg("iconSelDate"))  { int v = server.arg("iconSelDate").toInt();  if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconSelDate)       iconSelTouched = true; iconSelDate       = (uint8_t)v; } }
  if (server.hasArg("iconSelTemp"))  { int v = server.arg("iconSelTemp").toInt();  if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconSelTemp)       iconSelTouched = true; iconSelTemp       = (uint8_t)v; } }
  if (server.hasArg("iconSelRem"))   { int v = server.arg("iconSelRem").toInt();   if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconSelReminder)   iconSelTouched = true; iconSelReminder   = (uint8_t)v; } }
  if (server.hasArg("iconSelNotif")) { int v = server.arg("iconSelNotif").toInt(); if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconSelNotif)      iconSelTouched = true; iconSelNotif      = (uint8_t)v; } }
  if (server.hasArg("iconSelNpMusic")) { int v = server.arg("iconSelNpMusic").toInt(); if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconSelNpMusic) iconSelTouched = true; iconSelNpMusic = (uint8_t)v; } }
  if (server.hasArg("iconSelNpVideo")) { int v = server.arg("iconSelNpVideo").toInt(); if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconSelNpVideo) iconSelTouched = true; iconSelNpVideo = (uint8_t)v; } }
  if (server.hasArg("iconSelPress")) { int v = server.arg("iconSelPress").toInt(); if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconSelPressure)   iconSelTouched = true; iconSelPressure   = (uint8_t)v; } }
  if (server.hasArg("iconSelCurr"))  { int v = server.arg("iconSelCurr").toInt();  if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconSelCurrency)   iconSelTouched = true; iconSelCurrency   = (uint8_t)v; } }
  if (server.hasArg("iconSelIp"))    { int v = server.arg("iconSelIp").toInt();    if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconSelIp)         iconSelTouched = true; iconSelIp         = (uint8_t)v; } }
  if (server.hasArg("iconWxSunny"))  { int v = server.arg("iconWxSunny").toInt();  if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconWxSunny)       iconSelTouched = true; iconWxSunny       = (uint8_t)v; } }
  if (server.hasArg("iconWxCloud"))  { int v = server.arg("iconWxCloud").toInt();  if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconWxCloud)       iconSelTouched = true; iconWxCloud       = (uint8_t)v; } }
  if (server.hasArg("iconWxRain"))   { int v = server.arg("iconWxRain").toInt();   if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconWxRain)        iconSelTouched = true; iconWxRain        = (uint8_t)v; } }
  if (server.hasArg("iconWxStorm"))  { int v = server.arg("iconWxStorm").toInt();  if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconWxStorm)       iconSelTouched = true; iconWxStorm       = (uint8_t)v; } }
  if (server.hasArg("iconWxSnow"))   { int v = server.arg("iconWxSnow").toInt();   if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconWxSnow)        iconSelTouched = true; iconWxSnow        = (uint8_t)v; } }
  if (server.hasArg("iconWxWind"))   { int v = server.arg("iconWxWind").toInt();   if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconWxWind)        iconSelTouched = true; iconWxWind        = (uint8_t)v; } }
  if (server.hasArg("iconWxNight"))  { int v = server.arg("iconWxNight").toInt();  if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) { if ((uint8_t)v != iconWxNight)       iconSelTouched = true; iconWxNight       = (uint8_t)v; } }
  bool scrollTypeTouched = hideIconTouched || iconSelTouched;
  if (server.hasArg("scrollType")) {
    int st = server.arg("scrollType").toInt();

    if (st >= 0 && st <= 3 && (uint8_t)st != scrollType) {
      scrollTypeTouched = true;
      scrollType = (uint8_t)st;

      scrollTypeDate       = (uint8_t)st;
      scrollTypeTemp       = (uint8_t)st;
      scrollTypeReminder   = (uint8_t)st;
      scrollTypeWeather    = (uint8_t)st;
      scrollTypeNotif      = (uint8_t)st;
      scrollTypeNowPlaying = (uint8_t)st;
      scrollTypePressure   = (uint8_t)st;
      scrollTypeStopwatch  = (uint8_t)st;
      scrollTypeCurrency   = (uint8_t)st;
      scrollTypeYoutube    = (uint8_t)st;
      scrollTypeWebAccess  = (uint8_t)st;
    }
  }

  if (server.hasArg("scrollTypeDate")) {
    int st = server.arg("scrollTypeDate").toInt();
    if (st >= 0 && st <= 3) { if ((uint8_t)st != scrollTypeDate) scrollTypeTouched = true; scrollTypeDate = (uint8_t)st; }
  }
  if (server.hasArg("scrollTypeTemp")) {
    int st = server.arg("scrollTypeTemp").toInt();
    if (st >= 0 && st <= 3) { if ((uint8_t)st != scrollTypeTemp) scrollTypeTouched = true; scrollTypeTemp = (uint8_t)st; }
  }
  if (server.hasArg("scrollTypeReminder")) {
    int st = server.arg("scrollTypeReminder").toInt();
    if (st >= 0 && st <= 3) { if ((uint8_t)st != scrollTypeReminder) scrollTypeTouched = true; scrollTypeReminder = (uint8_t)st; }
  }
  if (server.hasArg("scrollTypeWeather")) {
    int st = server.arg("scrollTypeWeather").toInt();
    if (st >= 0 && st <= 3) { if ((uint8_t)st != scrollTypeWeather) scrollTypeTouched = true; scrollTypeWeather = (uint8_t)st; }
  }
  if (server.hasArg("scrollTypeNotif")) {
    int st = server.arg("scrollTypeNotif").toInt();
    if (st >= 0 && st <= 3) { if ((uint8_t)st != scrollTypeNotif) scrollTypeTouched = true; scrollTypeNotif = (uint8_t)st; }
  }
  if (server.hasArg("scrollTypeNowPlaying")) {
    int st = server.arg("scrollTypeNowPlaying").toInt();
    if (st >= 0 && st <= 3) { if ((uint8_t)st != scrollTypeNowPlaying) scrollTypeTouched = true; scrollTypeNowPlaying = (uint8_t)st; }
  }
  if (server.hasArg("scrollTypePressure")) {
    int st = server.arg("scrollTypePressure").toInt();
    if (st >= 0 && st <= 3) { if ((uint8_t)st != scrollTypePressure) scrollTypeTouched = true; scrollTypePressure = (uint8_t)st; }
  }
  if (server.hasArg("scrollTypeStopwatch")) {
    int st = server.arg("scrollTypeStopwatch").toInt();
    if (st >= 0 && st <= 3) { if ((uint8_t)st != scrollTypeStopwatch) scrollTypeTouched = true; scrollTypeStopwatch = (uint8_t)st; }
  }
  if (server.hasArg("scrollTypeCurrency")) {
    int st = server.arg("scrollTypeCurrency").toInt();
    if (st >= 0 && st <= 3) { if ((uint8_t)st != scrollTypeCurrency) scrollTypeTouched = true; scrollTypeCurrency = (uint8_t)st; }
  }
  if (server.hasArg("showgrayed")) showGrayedContent = (server.arg("showgrayed") == "1");
  if (server.hasArg("autosleep")) {
    autoSleepOn = (server.arg("autosleep") == "1");
    // Switching the timer on restarts the countdown rather than measuring from
    // whenever the clock was last touched, which could be hours ago.
    lastActivityMs = millis();
  }
  if (server.hasArg("autosleepsec")) {
    long v = server.arg("autosleepsec").toInt();
    // One minute to twenty-four hours. Anything shorter would put the panel out
    // while you were still looking at it.
    if (v >= 60 && v <= 86400) { autoSleepSec = (uint32_t)v; lastActivityMs = millis(); }
  }
  if (server.hasArg("livehl"))     liveTileHighlight = (server.arg("livehl") == "1");
  if (server.hasArg("uidark"))     webUiDark         = (server.arg("uidark") == "1");
  if (server.hasArg("uilang")) {
    String l = server.arg("uilang");
    // Only the two the interface actually ships; anything else is ignored
    // rather than stored and handed back as a language nobody can read.
    if (l == "en" || l == "ro") strcpy(webUiLang, l.c_str());
  }
  if (server.hasArg("startmode")) {
    int m = server.arg("startmode").toInt();
    // Takes effect at the next power-up; nothing switches under the user here.
    if (m >= START_MODE_WIFI && m <= START_MODE_AP) defaultStartMode = (uint8_t)m;
  }
  // Moving the global slider re-applies it to every tile, the same way the
  // global scroll type does.
  if (server.hasArg("scrollSpeed")) {
    int v = server.arg("scrollSpeed").toInt();
    if (v >= SCROLL_SPEED_MIN && v <= SCROLL_SPEED_MAX) {
      scrollSpeed = (uint8_t)v;
      for (int i = 0; i < SCROLL_SPEED_COUNT; i++) scrollSpeedTile[i] = (uint8_t)v;
    }
  }
  for (int i = 0; i < SCROLL_SPEED_COUNT; i++) {
    String a = String("scrollSpeed") + SPD_KEYS[i];
    if (server.hasArg(a)) {
      int v = server.arg(a).toInt();
      if (v >= SCROLL_SPEED_MIN && v <= SCROLL_SPEED_MAX) scrollSpeedTile[i] = (uint8_t)v;
    }
  }
  if (server.hasArg("scrollTypeYoutube")) {
    int st = server.arg("scrollTypeYoutube").toInt();
    if (st >= 0 && st <= 3) { if ((uint8_t)st != scrollTypeYoutube) scrollTypeTouched = true; scrollTypeYoutube = (uint8_t)st; }
  }
  if (server.hasArg("scrollTypeWebAccess")) {
    int st = server.arg("scrollTypeWebAccess").toInt();
    if (st >= 0 && st <= 3) { if ((uint8_t)st != scrollTypeWebAccess) scrollTypeTouched = true; scrollTypeWebAccess = (uint8_t)st; }
  }
  if (server.hasArg("scrollTypeIp")) {
    int st = server.arg("scrollTypeIp").toInt();
    if (st >= 0 && st <= 3) { if ((uint8_t)st != scrollTypeIp) scrollTypeTouched = true; scrollTypeIp = (uint8_t)st; }
  }

  bool fontTypeTouched = false;
  if (server.hasArg("fontType")) {
    int ft = server.arg("fontType").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT && (uint8_t)ft != fontType) {
      fontTypeTouched = true;
      fontType = (uint8_t)ft;

      fontTypeDate       = (uint8_t)ft;
      fontTypeTemp       = (uint8_t)ft;
      fontTypeReminder   = (uint8_t)ft;
      fontTypeWeather    = (uint8_t)ft;
      fontTypeNotif      = (uint8_t)ft;
      fontTypeNowPlaying = (uint8_t)ft;
      fontTypePressure   = (uint8_t)ft;
      fontTypeStopwatch  = (uint8_t)ft;
      fontTypeCurrency   = (uint8_t)ft;
      fontTypeYoutube    = (uint8_t)ft;
      fontTypeWebAccess  = (uint8_t)ft;
      fontTypeTimer      = (uint8_t)ft;
      fontTypeIp         = (uint8_t)ft;
    }
  }
  if (server.hasArg("fontTypeDate")) {
    int ft = server.arg("fontTypeDate").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypeDate) fontTypeTouched = true; fontTypeDate = (uint8_t)ft; }
  }
  if (server.hasArg("fontTypeTemp")) {
    int ft = server.arg("fontTypeTemp").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypeTemp) fontTypeTouched = true; fontTypeTemp = (uint8_t)ft; }
  }
  if (server.hasArg("fontTypeReminder")) {
    int ft = server.arg("fontTypeReminder").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypeReminder) fontTypeTouched = true; fontTypeReminder = (uint8_t)ft; }
  }
  if (server.hasArg("fontTypeWeather")) {
    int ft = server.arg("fontTypeWeather").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypeWeather) fontTypeTouched = true; fontTypeWeather = (uint8_t)ft; }
  }
  if (server.hasArg("fontTypeNotif")) {
    int ft = server.arg("fontTypeNotif").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypeNotif) fontTypeTouched = true; fontTypeNotif = (uint8_t)ft; }
  }
  if (server.hasArg("fontTypeNowPlaying")) {
    int ft = server.arg("fontTypeNowPlaying").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypeNowPlaying) fontTypeTouched = true; fontTypeNowPlaying = (uint8_t)ft; }
  }
  if (server.hasArg("fontTypePressure")) {
    int ft = server.arg("fontTypePressure").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypePressure) fontTypeTouched = true; fontTypePressure = (uint8_t)ft; }
  }
  if (server.hasArg("fontTypeStopwatch")) {
    int ft = server.arg("fontTypeStopwatch").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypeStopwatch) fontTypeTouched = true; fontTypeStopwatch = (uint8_t)ft; }
  }
  if (server.hasArg("fontTypeCurrency")) {
    int ft = server.arg("fontTypeCurrency").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypeCurrency) fontTypeTouched = true; fontTypeCurrency = (uint8_t)ft; }
  }
  if (server.hasArg("fontTypeYoutube")) {
    int ft = server.arg("fontTypeYoutube").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypeYoutube) fontTypeTouched = true; fontTypeYoutube = (uint8_t)ft; }
  }
  if (server.hasArg("fontTypeWebAccess")) {
    int ft = server.arg("fontTypeWebAccess").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypeWebAccess) fontTypeTouched = true; fontTypeWebAccess = (uint8_t)ft; }
  }
  if (server.hasArg("fontTypeTimer")) {
    int ft = server.arg("fontTypeTimer").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypeTimer) fontTypeTouched = true; fontTypeTimer = (uint8_t)ft; }
  }
  if (server.hasArg("fontTypeIp")) {
    int ft = server.arg("fontTypeIp").toInt();
    if (ft >= 0 && ft < FONT_MODE_COUNT) { if ((uint8_t)ft != fontTypeIp) fontTypeTouched = true; fontTypeIp = (uint8_t)ft; }
  }
  if (fontTypeTouched) {

    if (notifActive) {
      notifBuildBuffer();
    } else if (ipShowActive) {
      ipBuildBuffer();
    } else if (swRunning) {
      swInit();
    } else {
      CycleItem& cur = items[currentSlot];
      if (cur.id == ITEM_NOW_PLAYING)     npInit();
      else if (cur.id == ITEM_WEATHER)    weatherInit();
      else if (cur.id == ITEM_MEMENTO)    mementoInit();
      else if (cur.id == ITEM_DATE)       dateInit();
      else if (cur.id == ITEM_TEMP)       tempInit(lastTemp);
      else if (cur.id == ITEM_PRESSURE)   pressureInit((int)round(lastPressureHpa));
      else if (cur.id == ITEM_CURRENCY)   currencyInit();
      else if (socialIndexForItem(cur.id) >= 0) socialInit(cur.id);
    }
  }

  if (scrollTypeTouched) {

    if (notifActive) {
      notifBuildBuffer();
    } else if (ipShowActive) {
      ipBuildBuffer();
    } else if (swRunning) {
      swInit();
    } else {
      CycleItem& cur = items[currentSlot];
      if (cur.id == ITEM_NOW_PLAYING)     npInit();
      else if (cur.id == ITEM_WEATHER)    weatherInit();
      else if (cur.id == ITEM_MEMENTO)    mementoInit();
      else if (cur.id == ITEM_DATE)       dateInit();
      else if (cur.id == ITEM_TEMP)       tempInit(lastTemp);
      else if (cur.id == ITEM_PRESSURE)   pressureInit((int)round(lastPressureHpa));
      else if (cur.id == ITEM_CURRENCY)   currencyInit();
      else if (socialIndexForItem(cur.id) >= 0) socialInit(cur.id);
    }
  }

  if (server.hasArg("tileTransGlobal")) {
    int t = server.arg("tileTransGlobal").toInt();
    if (t >= 0 && t <= 9) tileTransGlobal = (uint8_t)t;
  }
  if (server.hasArg("tileTransHour"))   { int t = server.arg("tileTransHour").toInt();   if (t >= 0 && t <= 9) tileTransHour   = (uint8_t)t; }
  if (server.hasArg("tileTransDate"))   { int t = server.arg("tileTransDate").toInt();   if (t >= 0 && t <= 9) tileTransDate   = (uint8_t)t; }
  if (server.hasArg("tileTransTemp"))   { int t = server.arg("tileTransTemp").toInt();   if (t >= 0 && t <= 9) tileTransTemp   = (uint8_t)t; }
  if (server.hasArg("tileTransNp"))     { int t = server.arg("tileTransNp").toInt();     if (t >= 0 && t <= 9) tileTransNp     = (uint8_t)t; }
  if (server.hasArg("tileTransWx"))     { int t = server.arg("tileTransWx").toInt();     if (t >= 0 && t <= 9) tileTransWx     = (uint8_t)t; }
  if (server.hasArg("tileTransRem"))    { int t = server.arg("tileTransRem").toInt();    if (t >= 0 && t <= 9) tileTransRem    = (uint8_t)t; }
  if (server.hasArg("tileTransCanvas")) { int t = server.arg("tileTransCanvas").toInt(); if (t >= 0 && t <= 9) tileTransCanvas = (uint8_t)t; }
  if (server.hasArg("tileTransPress"))  { int t = server.arg("tileTransPress").toInt();  if (t >= 0 && t <= 9) tileTransPress  = (uint8_t)t; }
  if (server.hasArg("tileTransSs"))     { int t = server.arg("tileTransSs").toInt();     if (t >= 0 && t <= 9) tileTransSs     = (uint8_t)t; }
  if (server.hasArg("tileTransCurr"))   { int t = server.arg("tileTransCurr").toInt();   if (t >= 0 && t <= 9) tileTransCurr   = (uint8_t)t; }
  if (server.hasArg("tileTransYt"))     { int t = server.arg("tileTransYt").toInt();     if (t >= 0 && t <= 9) tileTransYt     = (uint8_t)t; }
  if (server.hasArg("tileTransHw"))     { int t = server.arg("tileTransHw").toInt();     if (t >= 0 && t <= 9) tileTransHw     = (uint8_t)t; }
  if (server.hasArg("tileTransWeb"))    { int t = server.arg("tileTransWeb").toInt();    if (t >= 0 && t <= 9) tileTransWeb    = (uint8_t)t; }
  if (server.hasArg("tileTransNotif"))  { int t = server.arg("tileTransNotif").toInt();  if (t >= 0 && t <= 9) tileTransNotif  = (uint8_t)t; }
  if (server.hasArg("tileTransEts2"))   { int t = server.arg("tileTransEts2").toInt();   if (t >= 0 && t <= 9) tileTransEts2   = (uint8_t)t; }
  if (server.hasArg("tileTransSw"))     { int t = server.arg("tileTransSw").toInt();     if (t >= 0 && t <= 9) tileTransSw     = (uint8_t)t; }
  if (server.hasArg("tileTransTmr"))    { int t = server.arg("tileTransTmr").toInt();    if (t >= 0 && t <= 9) tileTransTmr    = (uint8_t)t; }
  if (server.hasArg("tileTransAlarm"))  { int t = server.arg("tileTransAlarm").toInt();  if (t >= 0 && t <= 9) tileTransAlarm  = (uint8_t)t; }
  if (server.hasArg("tileTransIp"))     { int t = server.arg("tileTransIp").toInt();     if (t >= 0 && t <= 9) tileTransIp     = (uint8_t)t; }
  if (server.hasArg("tileTransC2P"))    { int t = server.arg("tileTransC2P").toInt();    if (t >= 0 && t <= 9) tileTransC2P    = (uint8_t)t; }
  if (server.hasArg("tileTransP2C"))    { int t = server.arg("tileTransP2C").toInt();    if (t >= 0 && t <= 9) tileTransP2C    = (uint8_t)t; }
  // Moving the global speed slider re-applies it to every transition, the way
  // the global scroll speed does.
  if (server.hasArg("tileTransSpd")) {
    int v = server.arg("tileTransSpd").toInt();
    if (v >= TRSPD_MIN && v <= TRSPD_MAX) {
      tileTransSpeed = (uint8_t)v;
      for (int i = 0; i < TRSPD_COUNT; i++) tileTransSpd[i] = (uint8_t)v;
    }
  }
  for (int i = 0; i < TRSPD_COUNT; i++) {
    String a = String("tileTransSpd") + TRSPD_KEYS[i];
    if (server.hasArg(a)) {
      int v = server.arg(a).toInt();
      if (v >= TRSPD_MIN && v <= TRSPD_MAX) tileTransSpd[i] = (uint8_t)v;
    }
  }

  // Settings-only requests (for example transition settings) contain no tile
  // payload. If a tile payload is present, it must contain the entire valid
  // circuit list; never persist a partially received/retry-corrupted list.
  if (server.hasArg("id0")) {
    CycleItem newItems[NUM_ITEMS];
    bool seenIds[16] = { false };
    for (int i = 0; i < NUM_ITEMS; i++) {
      String idKey  = "id"  + String(i);
      String enKey  = "en"  + String(i);
      String durKey = "dur" + String(i);
      if (!server.hasArg(idKey) || !server.hasArg(enKey) || !server.hasArg(durKey)) {
        server.send(400, "text/plain", "Lista de tile-uri este incompleta");
        return;
      }
      int id = server.arg(idKey).toInt();
      int duration = server.arg(durKey).toInt();
      if (id < 0 || id >= 16 || !isValidCircuitItemId((uint8_t)id) || seenIds[id] || duration < 1 || duration > 3600) {
        server.send(400, "text/plain", "Lista de tile-uri este invalida");
        return;
      }
      seenIds[id] = true;
      newItems[i].id          = (uint8_t)id;
      newItems[i].enabled     = (server.arg(enKey) == "1");
      newItems[i].durationSec = (uint16_t)duration;
      newItems[i].order       = i;
    }
    for (int i = 0; i < NUM_ITEMS; i++) items[i] = newItems[i];
  }

  repairItemIds();
  saveSettingsDeferred();
  server.send(200, "text/plain", "OK");
}

void handleBrightness() {
  if (!checkAuth()) return;
  if (server.hasArg("level")) {
    curBrightness = (uint8_t)constrain(server.arg("level").toInt(), 0, 16);
  }
  if (server.hasArg("dimAuto")) {
    dimAutoOn = (server.arg("dimAuto") == "1");
  }
  if (server.hasArg("dimFrom")) {
    String t = server.arg("dimFrom");
    if (t.length() == 5) {
      dimFromH = t.substring(0,2).toInt();
      dimFromM = t.substring(3,5).toInt();
    }
  }
  if (server.hasArg("dimTo")) {
    String t = server.arg("dimTo");
    if (t.length() == 5) {
      dimToH = t.substring(0,2).toInt();
      dimToM = t.substring(3,5).toInt();
    }
  }
  if (server.hasArg("dimLevel")) {
    dimLevel = (uint8_t)constrain(server.arg("dimLevel").toInt(), 0, 16);
  }

  applyBrightness();
  saveSettingsDeferred();
  server.send(200, "text/plain", "OK");
}

void handleBuzzerSett() {
  if (!checkAuth()) return;
  // The main slider moves every category with it; the Volume page then sets
  // them apart.
  if (server.hasArg("volume")) {
    buzzerVolume = (uint8_t)constrain(server.arg("volume").toInt(), 0, 100);
    buzzVolNotif = buzzVolAuto = buzzVolAlarm = buzzVolTouch = buzzerVolume;
  }
  if (server.hasArg("volnotif")) buzzVolNotif = (uint8_t)constrain(server.arg("volnotif").toInt(), 0, 100);
  if (server.hasArg("volauto"))  buzzVolAuto  = (uint8_t)constrain(server.arg("volauto").toInt(), 0, 100);
  if (server.hasArg("volalarm")) buzzVolAlarm = (uint8_t)constrain(server.arg("volalarm").toInt(), 0, 100);
  if (server.hasArg("voltouch")) buzzVolTouch = (uint8_t)constrain(server.arg("voltouch").toInt(), 0, 100);
  // A volume nobody can hear is a volume nobody can set: every slider asks for
  // a chime at its new level as soon as it is let go.
  String pv = server.arg("preview");
  if (pv.length() && buzzerOn && !alarmRinging) {
    nbStop();
    nbEnqCat = pv == "notif" ? BZ_CAT_NOTIF : pv == "auto"  ? BZ_CAT_AUTO  :
               pv == "alarm" ? BZ_CAT_ALARM : pv == "touch" ? BZ_CAT_TOUCH : BZ_CAT_NONE;
    nbEnqueueEnv(1568, 450, BZ_ENV_BELL);
  }
  if (server.hasArg("preset")) {
    String p = server.arg("preset");
    p.toCharArray(buzzerPreset, sizeof(buzzerPreset));
  }
  saveSettingsDeferred();
  server.send(200, "text/plain", "OK");
}

void handleEventSoundSett() {
  if (!checkAuth()) return;
  if (!server.hasArg("event") || !server.hasArg("preset")) {
    server.send(400, "text/plain", "Lipseste event/preset");
    return;
  }
  String ev = server.arg("event");
  String p  = server.arg("preset");
  char* target = nullptr;
  size_t targetSize = 0;
  const char* prefKey = nullptr;
  if (ev == "tile")       { target = eventSoundTile;  targetSize = sizeof(eventSoundTile);  prefKey = "evSndTile"; }
  else if (ev == "wifi")  { target = eventSoundWifi;  targetSize = sizeof(eventSoundWifi);  prefKey = "evSndWifi"; }
  else if (ev == "notif") { target = eventSoundNotif; targetSize = sizeof(eventSoundNotif); prefKey = "evSndNotif"; }
  else if (ev == "web")   { target = eventSoundWeb;   targetSize = sizeof(eventSoundWeb);   prefKey = "evSndWeb"; }
  else if (ev == "ets2")  { target = eventSoundEts2;  targetSize = sizeof(eventSoundEts2);  prefKey = "evSndEts2"; }
  else if (ev == "touch") { target = eventSoundTouch; targetSize = sizeof(eventSoundTouch); prefKey = "evSndTouch"; }
  else if (ev == "timer") { target = eventSoundTimer; targetSize = sizeof(eventSoundTimer); prefKey = "evSndTimer"; }
  if (!target) {
    server.send(400, "text/plain", "Eveniment necunoscut");
    return;
  }
  p.toCharArray(target, targetSize);
  prefs.begin("settings", false);
  prefs.putString(prefKey, p);
  prefs.end();
  // Picking a sound plays it, at the volume of the category it will play in.
  if (server.arg("preview") == "1" && buzzerOn && !alarmRinging) {
    uint8_t cat = (ev == "notif" || ev == "web") ? BZ_CAT_NOTIF : ev == "touch" ? BZ_CAT_TOUCH : BZ_CAT_AUTO;
    nbPlayPreset(target, cat);
  }
  server.send(200, "text/plain", "OK");
}

void handleTouchSett() {
  if (!checkAuth()) return;
  if (server.hasArg("tap")) {
    touchTapAction = (uint8_t)constrain(server.arg("tap").toInt(), 0, 8);
  }
  if (server.hasArg("dbl")) {
    touchDoubleTapAction = (uint8_t)constrain(server.arg("dbl").toInt(), 0, 8);
  }
  saveSettingsDeferred();
  server.send(200, "text/plain", "OK");
}

bool isValidHexColor(const String& s) {
  if (s.length() != 7 || s.charAt(0) != '#') return false;
  for (int i = 1; i < 7; i++) {
    char c = s.charAt(i);
    if (!isxdigit((unsigned char)c)) return false;
  }
  return true;
}

// Four comma-separated slots, each empty or #rrggbb. An empty string is all four
// left to the theme.
bool isValidUiColors(const String& s) {
  if (s.length() == 0) return true;
  if (s.length() >= sizeof(webUiColors)) return false;
  int parts = 0, start = 0;
  for (int i = 0; i <= (int)s.length(); i++) {
    if (i == (int)s.length() || s.charAt(i) == ',') {
      String p = s.substring(start, i);
      if (p.length() && !isValidHexColor(p)) return false;
      parts++;
      start = i + 1;
    }
  }
  return parts == 4;
}

void handleAccentSett() {
  if (!checkAuth()) return;
  bool hasHex   = server.hasArg("hex");
  bool hasShape = server.hasArg("shape");
  bool hasColors = server.hasArg("colors");
  if (!hasHex && !hasShape && !hasColors) {
    server.send(400, "text/plain", "Lipseste hex, shape sau colors");
    return;
  }
  String hex;
  if (hasHex) {
    hex = server.arg("hex");
    hex.trim();
    hex.toLowerCase();
    if (!isValidHexColor(hex)) {
      server.send(400, "text/plain", "Culoare invalida");
      return;
    }
  }
  String colors;
  if (hasColors) {
    colors = server.arg("colors");
    colors.trim();
    colors.toLowerCase();
    if (!isValidUiColors(colors)) {
      server.send(400, "text/plain", "Culori invalide");
      return;
    }
  }
  uint8_t shape = webUiShape;
  if (hasShape) {
    long s = server.arg("shape").toInt();
    if (s < 0 || s > 3) {
      server.send(400, "text/plain", "Forma invalida");
      return;
    }
    shape = (uint8_t)s;
  }
  // Nimic nu se scrie pana cand tot ce a venit nu e valid, ca o forma gresita
  // sa nu lase in urma o culoare deja salvata.
  prefs.begin("settings", false);
  if (hasHex) {
    hex.toCharArray(accentColor, sizeof(accentColor));
    prefs.putString("accentColor", accentColor);
  }
  if (hasShape) {
    webUiShape = shape;
    prefs.putUChar("uiShape", webUiShape);
  }
  if (hasColors) {
    colors.toCharArray(webUiColors, sizeof(webUiColors));
    prefs.putString("uiColors", webUiColors);
  }
  prefs.end();
  server.send(200, "text/plain", "OK");
}

// Escapes a string for safe embedding inside a JSON string literal.
// Several /state fields (WiFi SSID, memento text, custom AP name, weather
// city, ...) are free text the user can set to almost anything, including
// double quotes or backslashes. Without escaping, a single " in e.g. a
// memento note corrupts the entire JSON payload and every /state fetch on
// the dashboard fails to parse (silently retried, then surfaced as
// "Nu s-au putut incarca tile-urile. Reincarca pagina.").
String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", (uint8_t)c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

// Streams a response out in small chunks instead of building the whole body in
// one String.
//
// /state is by far the largest response in the portal (~2.8KB typical). It used
// to be assembled into a single String that reserve()d 6KB up front and was then
// handed to server.send() in one piece. Two things made that fail intermittently
// on a device that has been running for a while:
//
//   * A ~6KB *contiguous* allocation is not always available. The heap gets
//     fragmented over time, and the weather/currency fetch tasks allocate on
//     core 0 while a request is being served on core 1, so whether the block is
//     available at the moment the portal loads is essentially luck.
//   * When an Arduino String cannot grow, concat() returns false and leaves the
//     String *unchanged*. Both String::reserve() and String::operator+= discard
//     that result, so a failed allocation did not raise anything - it silently
//     dropped fields and produced truncated, invalid JSON. The portal's
//     `r.json()` then threw, all 5 retries hit the same fragmented heap, and the
//     user got "nothing loads at all" with no indication why.
//
// Writing through a small rolling buffer keeps the peak allocation at ~1KB
// (which the heap can virtually always satisfy), and a failed append is retried
// after flushing instead of being silently discarded.
struct ChunkedResponse {
  static const size_t FLUSH_AT = 4096;
  String buf;
  bool   truncated = false;

  void begin(const char* contentType) {
    // Nagle holds a small segment back waiting for an ACK, which is exactly the
    // wrong behaviour for a handful of chunk headers: it turned a few hundred
    // microseconds of work into tens of milliseconds of loop() being blocked.
    server.client().setNoDelay(true);
    buf.reserve(FLUSH_AT + 128);
    server.setContentLength(CONTENT_LENGTH_UNKNOWN); // -> HTTP chunked transfer
    server.send(200, contentType, "");
  }
  ChunkedResponse& operator+=(const char* s)   { append(s);         return *this; }
  ChunkedResponse& operator+=(const String& s) { append(s.c_str()); return *this; }
  void append(const char* s) {
    if (!buf.concat(s)) {
      flush();                          // free what we are holding, then retry
      if (!buf.concat(s)) truncated = true;
    }
    if (buf.length() >= FLUSH_AT) flush();
  }
  void flush() {
    if (buf.length()) {
      server.sendContent(buf);
      buf = "";                         // keeps the reserved capacity
    }
  }
  void end() { flush(); server.sendContent(""); }
};

// Which tiles the Tile Manager is currently showing. Its own endpoint rather
// than a pair of fields on /settings, because /settings only accepts a tile
// payload that is complete and valid - and adding a tile back is precisely the
// moment the two would have to be sent together and agree.
void handleTileHidden() {
  if (!checkAuth()) return;
  if (server.hasArg("circuit")) {
    long v = server.arg("circuit").toInt();
    if (v < 0 || v > 0xFFFF) { server.send(400, "text/plain", "Masca invalida"); return; }
    tileHiddenMask = (uint16_t)v;
  }
  if (server.hasArg("prio")) {
    long v = server.arg("prio").toInt();
    if (v < 0 || v > 0xFF) { server.send(400, "text/plain", "Masca invalida"); return; }
    prioHiddenMask = (uint8_t)v;
  }
  applyTileHiddenMask();
  saveSettingsDeferred();
  server.send(200, "text/plain", "OK");
}

// Polled once a second for as long as the Tile Manager is open, so it stays
// deliberately tiny - /state carries every setting on the device and is far too
// heavy to ask for at that rate.
void handleLiveTile() {
  if (!checkAuth()) return;
  String j = "{\"tile\":";
  j += String((int)gLiveTileId);
  j += ",\"prio\":\"";
  j += gLivePrio;
  j += "\"}";
  server.send(200, "application/json", j);
}

// SCREEN MIRROR
//
// What the matrix is lighting right now, for Octoglow Connect or anything else
// that wants to draw a copy. The server cannot push, so this is polled, and it is
// built to cost next to nothing: 32 column reads from the MD_MAX72XX buffer the
// panel is fed from, a reply under 100 bytes, no heap work beyond sending it.
//
//   GET /getscreen              {"on":true,"b":8,"px":"<64 hex digits>"}
//   GET /getscreen?format=grid  8 lines of 32 '0'/'1', top row first
//
// px is two hex digits per column, leftmost column first; within a column bit 0
// is the top row. b is the brightness level the panel runs at (1-16). When the
// panel is dark - sleep, screen switched off, level 0 - "on" is false and every
// pixel reads 0, because nothing is lit.
//
// A request is answered between two passes of loop(), so a frame caught halfway
// through a transition is never seen, only whole frames. The server closes every
// connection after answering, so each poll is a fresh TCP connection: one at a
// time, 50-100 ms apart, is the pace to use.
void handleGetScreen() {
  if (!checkAuth()) return;
  bool lit = mxPanelLevel > 0;
  uint8_t cols[32];
  // mx column 31 is the leftmost one on the panel.
  for (int x = 0; x < 32; x++) cols[x] = lit ? mx.getColumn(31 - x) : 0;
  server.sendHeader("Cache-Control", "no-store");
  if (server.arg("format") == "grid") {
    char g[8 * 33 + 1];
    char* p = g;
    for (int y = 0; y < 8; y++) {
      for (int x = 0; x < 32; x++) *p++ = ((cols[x] >> y) & 1) ? '1' : '0';
      *p++ = '\n';
    }
    *p = '\0';
    server.send(200, "text/plain", g);
    return;
  }
  static const char HEXD[] = "0123456789abcdef";
  char j[100];
  int n = snprintf(j, sizeof(j), "{\"on\":%s,\"b\":%u,\"px\":\"", lit ? "true" : "false", (unsigned)mxPanelLevel);
  for (int x = 0; x < 32; x++) {
    j[n++] = HEXD[cols[x] >> 4];
    j[n++] = HEXD[cols[x] & 0x0F];
  }
  j[n++] = '"';
  j[n++] = '}';
  j[n] = '\0';
  server.send(200, "application/json", j);
}

// SCREEN STREAM
//
// /getscreen answers when asked; this sends. Polling costs a fresh TCP connection
// per frame, and with WiFi modem sleep on every one of them waits for the next
// beacon, so a polled copy stutters. A client that wants a live copy - Octoglow
// Connect - asks /screensub for the key over its logged-in session, then sends
// that key to UDP port SCREEN_UDP_PORT about once a second from the socket it
// wants the frames on. Each such packet renews the subscription; with none for
// SCREEN_SUB_TIMEOUT_MS the stream stops. The client speaking first is also what
// gets the frames through a desktop firewall: they arrive as replies.
//
// While subscribed, a frame that differs from the last one sent goes out as one
// 40-byte datagram - at most one per SCREEN_MIN_GAP_MS - and the frame is sent
// again every SCREEN_KEEPALIVE_MS, so a lost packet heals and a still clock can be
// told from a gone one. screenStreamTick() runs from mxCommit() as well as from
// loop(), so the frames of a blocking transition go out too. Modem sleep is off
// for as long as someone is subscribed and back on once they leave.
//
// Datagram: "OGF1", sequence (uint16, big endian), flags (bit 0: panel lit),
// brightness level, then 32 columns laid out like /getscreen's px - leftmost
// first, bit 0 the top row, all zero while the panel is dark.
#define SCREEN_UDP_PORT        4211
#define SCREEN_SUB_TIMEOUT_MS  4000UL
#define SCREEN_MIN_GAP_MS      25UL
#define SCREEN_CHECK_MS        20UL
#define SCREEN_KEEPALIVE_MS    500UL

static WiFiUDP       screenUdp;
static bool          screenUdpOpen     = false;
static uint32_t      screenKey         = 0;
static bool          screenSubActive   = false;
static IPAddress     screenSubIp;
static uint16_t      screenSubPort     = 0;
static unsigned long screenSubHeardMs  = 0;
static unsigned long screenLastSendMs  = 0;
static unsigned long screenLastCheckMs = 0;
static unsigned long screenLastReadMs  = 0;
static bool          screenSendNow     = false;
static uint16_t      screenSeq         = 0;
static uint8_t       screenLastSent[34];
static bool          screenInTick      = false;

// Subscription packets are "OGSK" and the key, big endian - exactly 8 bytes.
// A handful per call at most, so a flood cannot hold loop() here.
static void screenReadSubscriptions(unsigned long now) {
  for (int i = 0; i < 8; i++) {
    int len = screenUdp.parsePacket();
    if (len <= 0) break;
    uint8_t buf[8];
    if (len != 8 || screenUdp.read(buf, sizeof(buf)) != 8) continue;
    if (memcmp(buf, "OGSK", 4) != 0 || screenKey == 0) continue;
    uint32_t k = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
                 ((uint32_t)buf[6] << 8)  |  (uint32_t)buf[7];
    if (k != screenKey) continue;
    IPAddress ip   = screenUdp.remoteIP();
    uint16_t  port = screenUdp.remotePort();
    if (!screenSubActive || !(ip == screenSubIp) || port != screenSubPort) {
      screenSubIp   = ip;
      screenSubPort = port;
      screenSendNow = true;
      if (!screenSubActive) {
        screenSubActive = true;
        WiFi.setSleep(false);
      }
    }
    screenSubHeardMs = now;
  }
}

void screenStreamTick() {
  if (!screenUdpOpen || screenInTick) return;
  screenInTick = true;
  unsigned long now = millis();

  if (now - screenLastReadMs >= 50) {
    screenLastReadMs = now;
    screenReadSubscriptions(now);
  }
  if (screenSubActive && now - screenSubHeardMs > SCREEN_SUB_TIMEOUT_MS) {
    screenSubActive = false;
    WiFi.setSleep(true);
  }

  // gSuppressHwFlash means the buffer holds a frame the panel is not showing.
  if (screenSubActive && !gSuppressHwFlash &&
      (screenSendNow || (now - screenLastCheckMs >= SCREEN_CHECK_MS &&
                         now - screenLastSendMs >= SCREEN_MIN_GAP_MS))) {
    screenLastCheckMs = now;
    uint8_t frame[34];
    bool lit = mxPanelLevel > 0;
    for (int x = 0; x < 32; x++) frame[x] = lit ? mx.getColumn(31 - x) : 0;
    frame[32] = lit ? 1 : 0;
    frame[33] = mxPanelLevel;
    if (screenSendNow || now - screenLastSendMs >= SCREEN_KEEPALIVE_MS ||
        memcmp(frame, screenLastSent, sizeof(frame)) != 0) {
      uint8_t pkt[40];
      memcpy(pkt, "OGF1", 4);
      pkt[4] = (uint8_t)(screenSeq >> 8);
      pkt[5] = (uint8_t)screenSeq;
      pkt[6] = frame[32];
      pkt[7] = frame[33];
      memcpy(pkt + 8, frame, 32);
      screenSeq++;
      if (screenUdp.beginPacket(screenSubIp, screenSubPort)) {
        screenUdp.write(pkt, sizeof(pkt));
        screenUdp.endPacket();
      }
      memcpy(screenLastSent, frame, sizeof(frame));
      screenLastSendMs = now;
      screenSendNow    = false;
    }
  }
  screenInTick = false;
}

// Hands a logged-in client the key for the stream, opening the UDP socket the
// first time anyone asks. The key lasts until the clock restarts.
void handleScreenSub() {
  if (!checkAuth()) return;
  if (!screenUdpOpen) screenUdpOpen = screenUdp.begin(SCREEN_UDP_PORT);
  if (!screenUdpOpen) {
    server.send(503, "text/plain", "UDP indisponibil");
    return;
  }
  while (screenKey == 0) screenKey = esp_random();
  char j[72];
  snprintf(j, sizeof(j), "{\"port\":%u,\"key\":\"%08lx\",\"timeoutMs\":%lu}",
           (unsigned)SCREEN_UDP_PORT, (unsigned long)screenKey, (unsigned long)SCREEN_SUB_TIMEOUT_MS);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

void handleState() {
  uint32_t _st0 = micros();
  if (!checkAuth()) return;
  String ssid = gWifiSsidCached;

  String localIP = provisionMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();

  // Each field is appended individually (rather than one long chained `+`
  // expression) so every += only needs one small temporary String for its
  // right-hand side. The previous single chained expression created a cascade of
  // growing temporary String allocations while being evaluated (one per `+`),
  // which fragmented the heap further.
  ChunkedResponse json;
  json.begin("application/json");
  json += "{\"buzzer\":";
  json += String(buzzerOn ? "true" : "false");
  json += ",\"buzzerVolume\":";
  json += String(buzzerVolume);
  json += ",\"buzzerVolNotif\":";
  json += String(buzzVolNotif);
  json += ",\"buzzerVolAuto\":";
  json += String(buzzVolAuto);
  json += ",\"buzzerVolAlarm\":";
  json += String(buzzVolAlarm);
  json += ",\"buzzerVolTouch\":";
  json += String(buzzVolTouch);
  json += ",\"buzzerPreset\":\"";
  json += String(buzzerPreset);
  json += "\"";
  json += ",\"ssid\":\"";
  json += jsonEscape(ssid);
  json += "\"";
  json += ",\"ip\":\"";
  json += localIP;
  json += "\"";
  json += ",\"ap\":";
  json += String(provisionMode ? "true" : "false");
  json += ",\"apSsid\":\"";
  json += jsonEscape(getApSsid());
  json += "\"";
  json += ",\"version\":\"";
  json += FW_VERSION;
  json += "\"";
  json += ",\"uptime\":";
  json += String(millis() / 1000);
  json += ",\"lastTemp\":";
  json += String(lastTemp);
  json += ",\"bmpOk\":";
  json += (bmpOk ? "true" : "false");
  json += ",\"items\":[";
  for (int i = 0; i < NUM_ITEMS; i++) {
    if (i > 0) json += ",";
    json += "{\"id\":";
    json += String(items[i].id);
    json += ",\"enabled\":";
    json += (items[i].enabled ? "true" : "false");
    json += ",\"dur\":";
    json += String(items[i].durationSec);
    json += "}";
  }
  json += "]";
  char tFrom[6], tTo[6];
  snprintf(tFrom, 6, "%02d:%02d", dimFromH, dimFromM);
  snprintf(tTo,   6, "%02d:%02d", dimToH,   dimToM);
  json += ",\"bright\":";
  json += String(curBrightness);
  json += ",\"dimAuto\":";
  json += String(dimAutoOn ? "true" : "false");
  json += ",\"dimFrom\":\"";
  json += String(tFrom);
  json += "\"";
  json += ",\"dimTo\":\"";
  json += String(tTo);
  json += "\"";
  json += ",\"dimLevel\":";
  json += String(dimLevel);
  json += ",\"tempunit\":";
  json += String(tempUnit);
  json += ",\"hourformat\":";
  json += String(hourFormat);
  json += ",\"hourLeadingZero\":";
  json += String(hourLeadingZero ? "true" : "false");
  json += ",\"hwFormat\":";
  json += String(hwFormat);
  json += ",\"hwLeadZero\":";
  json += String(hwLeadZero ? "true" : "false");
  json += ",\"hwBarMode\":";
  json += String(hwBarMode);
  json += ",\"hwBarPos\":";
  json += String(hwBarPos);
  json += ",\"hwSwap\":";
  json += String(hwSwap ? "true" : "false");
  json += ",\"netTimeSync\":";
  json += String(netTimeSync ? "true" : "false");
  json += ",\"defaultStartMode\":";
  json += String(defaultStartMode);
  json += ",\"dateformat\":";
  json += String(dateFormat);
  json += ",\"datelang\":";
  json += String(dateLang);
  json += ",\"customdatefmt\":\"";
  json += jsonEscape(String(customDateFmt));
  json += "\"";
  json += ",\"wxCity\":\"";
  json += jsonEscape(String(weatherCity));
  json += "\"";
  json += ",\"wxPreset\":";
  json += String(wxPreset);
  json += ",\"wxLang\":\"";
  json += jsonEscape(String(weatherLang));
  json += "\"";
  json += ",\"wxHasKey\":";
  json += String(strlen(weatherApiKey) > 0 ? "true" : "false");
  // Social tiles. The API key is never sent back to the browser - only whether
  // one is stored - the same way the weather key is handled.
  for (int i = 0; i < SOCIAL_COUNT; i++) {
    json += ",\"soc";
    json += String(i);
    json += "Handle\":\"";
    json += jsonEscape(String(social[i].handle));
    json += "\"";
    json += ",\"soc";
    json += String(i);
    json += "ShowName\":";
    json += (social[i].showName ? "true" : "false");
    json += ",\"soc";
    json += String(i);
    json += "HasKey\":";
    json += String(social[i].apiKey[0] ? "true" : "false");
    json += ",\"soc";
    json += String(i);
    json += "KeyLen\":";
    json += String(strlen(social[i].apiKey));
    json += ",\"soc";
    json += String(i);
    json += "Count\":";
    json += String(social[i].count);
    json += ",\"soc";
    json += String(i);
    json += "Valid\":";
    json += String(social[i].valid ? "true" : "false");
  }
  json += ",\"iconSelYoutube\":";
  json += String(iconSelYoutube);
  json += ",\"currencyBase\":\"";
  json += String(currencyBase);
  json += "\"";
  json += ",\"currencyQuote\":\"";
  json += String(currencyQuote);
  json += "\"";
  json += ",\"currencyCompare\":";
  json += String(currencyCompareEnabled ? "true" : "false");
  json += ",\"liveHl\":";
  json += String(liveTileHighlight ? "true" : "false");
  json += ",\"uiDark\":";
  json += String(webUiDark ? "true" : "false");
  json += ",\"uiLang\":\"";
  json += String(webUiLang);
  json += "\"";
  json += ",\"showGrayed\":";
  json += String(showGrayedContent ? "true" : "false");
  json += ",\"autoSleep\":";
  json += String(autoSleepOn ? "true" : "false");
  json += ",\"autoSleepSec\":";
  json += String(autoSleepSec);
  json += ",\"sleeping\":";
  json += String(sleepActive ? "true" : "false");
  json += ",\"tileHidden\":";
  json += String(tileHiddenMask);
  json += ",\"prioHidden\":";
  json += String(prioHiddenMask);
  json += ",\"webEnabled\":";
  json += String(webAccessEnabled ? "true" : "false");
  json += ",\"notifEnabled\":";
  json += String(notifEnabled ? "true" : "false");
  json += ",\"ets2Enabled\":";
  json += String(ets2Enabled ? "true" : "false");
  json += ",\"ets2OrderFirst\":";
  json += String(ets2OrderFirst ? "true" : "false");
  json += ",\"nowPlayingIsPriority\":";
  json += String(nowPlayingIsPriority ? "true" : "false");
  json += ",\"priorityOrder\":\"";
  json += priorityOrderToString();
  json += "\"";
  json += ",\"hideIcons\":";
  json += String(hideTileIcons ? "true" : "false");
  json += ",\"hideIconDate\":";
  json += String(hideIconDate ? "true" : "false");
  json += ",\"hideIconTemp\":";
  json += String(hideIconTemp ? "true" : "false");
  json += ",\"hideIconReminder\":";
  json += String(hideIconReminder ? "true" : "false");
  json += ",\"hideIconWeather\":";
  json += String(hideIconWeather ? "true" : "false");
  json += ",\"hideIconNotif\":";
  json += String(hideIconNotif ? "true" : "false");
  json += ",\"hideIconNowPlaying\":";
  json += String(hideIconNowPlaying ? "true" : "false");
  json += ",\"hideIconPressure\":";
  json += String(hideIconPressure ? "true" : "false");
  json += ",\"hideIconCurrency\":";
  json += String(hideIconCurrency ? "true" : "false");
  json += ",\"hideIconYoutube\":";
  json += String(hideIconYoutube ? "true" : "false");
  json += ",\"hideIconWebAccess\":";
  json += String(hideIconWebAccess ? "true" : "false");
  json += ",\"hideIconIp\":";
  json += String(hideIconIp ? "true" : "false");
  json += ",\"npAdaptiveIcon\":";
  json += String(npAdaptiveIcon ? "true" : "false");
  json += ",\"iconSelDate\":";
  json += String(iconSelDate);
  json += ",\"iconSelTemp\":";
  json += String(iconSelTemp);
  json += ",\"iconSelReminder\":";
  json += String(iconSelReminder);
  json += ",\"iconSelNotif\":";
  json += String(iconSelNotif);
  json += ",\"iconSelNpMusic\":";
  json += String(iconSelNpMusic);
  json += ",\"iconSelNpVideo\":";
  json += String(iconSelNpVideo);
  json += ",\"iconSelPressure\":";
  json += String(iconSelPressure);
  json += ",\"iconSelCurrency\":";
  json += String(iconSelCurrency);
  json += ",\"iconSelIp\":";
  json += String(iconSelIp);
  json += ",\"iconWxSunny\":";
  json += String(iconWxSunny);
  json += ",\"iconWxCloud\":";
  json += String(iconWxCloud);
  json += ",\"iconWxRain\":";
  json += String(iconWxRain);
  json += ",\"iconWxStorm\":";
  json += String(iconWxStorm);
  json += ",\"iconWxSnow\":";
  json += String(iconWxSnow);
  json += ",\"iconWxWind\":";
  json += String(iconWxWind);
  json += ",\"iconWxNight\":";
  json += String(iconWxNight);
  json += ",\"scrollType\":";
  json += String(scrollType);
  json += ",\"scrollTypeDate\":";
  json += String(scrollTypeDate);
  json += ",\"scrollTypeTemp\":";
  json += String(scrollTypeTemp);
  json += ",\"scrollTypeReminder\":";
  json += String(scrollTypeReminder);
  json += ",\"scrollTypeWeather\":";
  json += String(scrollTypeWeather);
  json += ",\"scrollTypeNotif\":";
  json += String(scrollTypeNotif);
  json += ",\"scrollTypeNowPlaying\":";
  json += String(scrollTypeNowPlaying);
  json += ",\"scrollTypePressure\":";
  json += String(scrollTypePressure);
  json += ",\"scrollTypeStopwatch\":";
  json += String(scrollTypeStopwatch);
  json += ",\"scrollTypeCurrency\":";
  json += String(scrollTypeCurrency);
  json += ",\"scrollTypeYoutube\":";
  json += String(scrollTypeYoutube);
  json += ",\"scrollTypeWebAccess\":";
  json += String(scrollTypeWebAccess);
  json += ",\"scrollTypeTimer\":";
  json += String(scrollTypeTimer);
  json += ",\"scrollSpeed\":";
  json += String(scrollSpeed);
  for (int i = 0; i < SCROLL_SPEED_COUNT; i++) {
    json += ",\"scrollSpeed";
    json += SPD_KEYS[i];
    json += "\":";
    json += String(scrollSpeedTile[i]);
  }
  json += ",\"scrollTypeIp\":";
  json += String(scrollTypeIp);
  json += ",\"fontType\":";
  json += String(fontType);
  json += ",\"fontTypeDate\":";
  json += String(fontTypeDate);
  json += ",\"fontTypeTemp\":";
  json += String(fontTypeTemp);
  json += ",\"fontTypeReminder\":";
  json += String(fontTypeReminder);
  json += ",\"fontTypeWeather\":";
  json += String(fontTypeWeather);
  json += ",\"fontTypeNotif\":";
  json += String(fontTypeNotif);
  json += ",\"fontTypeNowPlaying\":";
  json += String(fontTypeNowPlaying);
  json += ",\"fontTypePressure\":";
  json += String(fontTypePressure);
  json += ",\"fontTypeStopwatch\":";
  json += String(fontTypeStopwatch);
  json += ",\"fontTypeCurrency\":";
  json += String(fontTypeCurrency);
  json += ",\"fontTypeYoutube\":";
  json += String(fontTypeYoutube);
  json += ",\"fontTypeWebAccess\":";
  json += String(fontTypeWebAccess);
  json += ",\"fontTypeTimer\":";
  json += String(fontTypeTimer);
  json += ",\"fontTypeIp\":";
  json += String(fontTypeIp);
  json += ",\"tileTransGlobal\":";
  json += String(tileTransGlobal);
  json += ",\"tileTransHour\":";
  json += String(tileTransHour);
  json += ",\"tileTransDate\":";
  json += String(tileTransDate);
  json += ",\"tileTransTemp\":";
  json += String(tileTransTemp);
  json += ",\"tileTransNp\":";
  json += String(tileTransNp);
  json += ",\"tileTransWx\":";
  json += String(tileTransWx);
  json += ",\"tileTransRem\":";
  json += String(tileTransRem);
  json += ",\"tileTransCanvas\":";
  json += String(tileTransCanvas);
  json += ",\"tileTransPress\":";
  json += String(tileTransPress);
  json += ",\"tileTransSs\":";
  json += String(tileTransSs);
  json += ",\"tileTransCurr\":";
  json += String(tileTransCurr);
  json += ",\"tileTransYt\":";
  json += String(tileTransYt);
  json += ",\"tileTransHw\":";
  json += String(tileTransHw);
  json += ",\"tileTransWeb\":";
  json += String(tileTransWeb);
  json += ",\"tileTransNotif\":";
  json += String(tileTransNotif);
  json += ",\"tileTransEts2\":";
  json += String(tileTransEts2);
  json += ",\"tileTransSw\":";
  json += String(tileTransSw);
  json += ",\"tileTransTmr\":";
  json += String(tileTransTmr);
  json += ",\"tileTransAlarm\":";
  json += String(tileTransAlarm);
  json += ",\"tileTransIp\":";
  json += String(tileTransIp);
  json += ",\"tileTransC2P\":";
  json += String(tileTransC2P);
  json += ",\"tileTransP2C\":";
  json += String(tileTransP2C);
  json += ",\"tileTransSpd\":";
  json += String(tileTransSpeed);
  for (int i = 0; i < TRSPD_COUNT; i++) {
    json += ",\"tileTransSpd";
    json += TRSPD_KEYS[i];
    json += "\":";
    json += String(tileTransSpd[i]);
  }
  json += ",\"timerDurationSec\":";
  json += String(timerDurationSec);
  json += ",\"timerPreset\":";
  json += String(timerPreset);
  json += ",\"alarms\":[";
  for (int i = 0; i < ALARM_COUNT; i++) {
    if (i > 0) json += ",";
    json += "{\"h\":";
    json += String(alarms[i].hour);
    json += ",\"m\":";
    json += String(alarms[i].minute);
    json += ",\"d\":";
    json += String(alarms[i].days);
    json += ",\"en\":";
    json += (alarms[i].enabled ? "true" : "false");
    json += ",\"p\":";
    json += (alarms[i].present ? "true" : "false");
    json += ",\"tone\":\"";
    json += jsonEscape(String(alarms[i].tone));
    json += "\"}";
  }
  json += "]";
  json += ",\"pressureHpa\":";
  json += String((int)round(lastPressureHpa));
  json += ",\"pressureTrend\":";
  json += String(pressureTrend);
  json += ",\"currencyValid\":";
  json += String(currencyValid ? "true" : "false");
  json += ",\"currencyRate\":";
  json += String((currencyValid && !isnan(currencyRateNow)) ? currencyRateNow : 0.0f, 4);
  json += ",\"currencyTrend\":";
  json += String(currencyTrend);
  json += ",\"mementoText\":\"";
  json += jsonEscape(String(mementoBuf));
  json += "\"";
  json += ",\"canvasBmp\":\"";
  json += canvasBitmapToHex();
  json += "\"";
  json += ",\"evSndTile\":\"";
  json += jsonEscape(String(eventSoundTile));
  json += "\"";
  json += ",\"evSndWifi\":\"";
  json += jsonEscape(String(eventSoundWifi));
  json += "\"";
  json += ",\"evSndWeb\":\"";
  json += jsonEscape(String(eventSoundWeb));
  json += "\"";
  json += ",\"evSndNotif\":\"";
  json += jsonEscape(String(eventSoundNotif));
  json += "\"";
  json += ",\"evSndEts2\":\"";
  json += jsonEscape(String(eventSoundEts2));
  json += "\"";
  json += ",\"evSndTouch\":\"";
  json += jsonEscape(String(eventSoundTouch));
  json += "\"";
  json += ",\"evSndTimer\":\"";
  json += jsonEscape(String(eventSoundTimer));
  json += "\"";
  json += ",\"touchTapAction\":";
  json += String(touchTapAction);
  json += ",\"touchDoubleTapAction\":";
  json += String(touchDoubleTapAction);
  json += ",\"uiShape\":";
  json += String(webUiShape);
  json += ",\"uiColors\":\"";
  json += String(webUiColors);
  json += "\"";
  json += ",\"accentColor\":\"";
  json += jsonEscape(String(accentColor));
  json += "\"";
  json += ",\"ssAnim\":";
  json += String(ssAnimSelected);
  json += "}";
  uint32_t _stBuild = micros() - _st0;
  if (_stBuild > hwStateBuildUs) hwStateBuildUs = _stBuild;
  json.end();
  { uint32_t d = micros() - _st0; if (d > hwStateUs) hwStateUs = d; }
  // Headers are already on the wire by this point, so a truncated body cannot be
  // turned into an error status - but it can at least be reported instead of
  // looking like an unexplained portal failure.
  if (json.truncated) {
    Serial.printf("[state] response truncated - out of memory (free heap %u, largest block %u)\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  }
}

// HARDWARE MONITOR
//
// On-die temperature. Not every ESP32 variant has a usable internal sensor -
// the original ESP32 reports a constant 53.33 regardless of reality - so this
// is gated at compile time on the targets that do, and the reading is then
// range-checked before being trusted. When it is unavailable the field is
// reported as absent and the UI drops the gauge, the same way it does for a
// board with no PSRAM.
static bool readChipTemp(float& out) {
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2) || \
    defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
  float t = temperatureRead();
  if (isnan(t) || t < -40.0f || t > 125.0f) return false;
  out = t;
  return true;
#else
  (void)out;
  return false;
#endif
}

// Small enough (~380 bytes) to go out in one send(); no need for the chunked
// path /state uses. Every value here is measured: read straight from the SDK,
// or counted by hwMonitorTick(). Nothing is estimated.
void handleHwReset() {
  if (!checkAuth()) return;
  hwResetPeaks();
  server.send(200, "text/plain", "OK");
}

void handleHwState() {
  if (!checkAuth()) return;
  String j;
  j.reserve(512);
  j = "{\"cpuFreqMhz\":";
  j += String(ESP.getCpuFreqMHz());
  j += ",\"cores\":";
  j += String(ESP.getChipCores());
  j += ",\"chip\":\"";
  j += String(ESP.getChipModel());
  j += "\"";
  j += ",\"loopsPerSec\":";
  j += String(hwLoopsPerSec);
  j += ",\"loopMaxUs\":";
  j += String(hwLoopMaxUsLast);
  j += ",\"loopMaxUsEver\":";
  j += String(hwLoopMaxUsEver);
  j += ",\"httpMaxUs\":";
  j += String(hwHttpMaxUs);
  j += ",\"httpMaxUri\":\"";
  j += jsonEscape(String(hwHttpMaxUri));
  j += "\"";
  j += ",\"pageUs\":";
  j += String(hwPageUs);
  j += ",\"stateUs\":";
  j += String(hwStateUs);
  j += ",\"stateBuildUs\":";
  j += String(hwStateBuildUs);
  j += ",\"pageFull\":";
  j += String(hwPageFull);
  j += ",\"page304\":";
  j += String(hwPage304);
  j += ",\"heapFree\":";
  j += String(ESP.getFreeHeap());
  j += ",\"heapSize\":";
  j += String(ESP.getHeapSize());
  j += ",\"heapMinFree\":";
  j += String(ESP.getMinFreeHeap());
  j += ",\"heapMaxAlloc\":";
  j += String(ESP.getMaxAllocHeap());
  j += ",\"psramSize\":";
  j += String(ESP.getPsramSize());
  j += ",\"psramFree\":";
  j += String(ESP.getFreePsram());
  j += ",\"sketchSize\":";
  j += String(ESP.getSketchSize());
  j += ",\"sketchFree\":";
  j += String(ESP.getFreeSketchSpace());
  j += ",\"flashSize\":";
  j += String(ESP.getFlashChipSize());
  j += ",\"uptime\":";
  j += String(millis() / 1000);
  float chipTempC = 0.0f;
  bool hasTemp = readChipTemp(chipTempC);
  j += ",\"hasTemp\":";
  j += String(hasTemp ? "true" : "false");
  j += ",\"tempC\":";
  j += String(hasTemp ? chipTempC : 0.0f, 1);
  j += ",\"tempUnit\":";
  j += String(tempUnit);
  j += "}";
  server.send(200, "application/json", j);
}

// BACKUP AND RESTORE
//
// There is no single existing endpoint that accepts a full settings object,
// so this handler mirrors the field-by-field approach already used by
// handleSettings()/loadSettings()/saveSettings() - it sets the same globals
// those functions use, then reuses saveSettings() to persist everything in
// one go instead of writing to Preferences directly. Fields that /state
// (and therefore a backup produced by the web UI) never exposes - wifi
// credentials, auth credentials, the weather API key - are simply absent
// from the payload and are left untouched here as well.
void handleBackupRestore() {
  if (!checkAuth()) return;
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"Lipseste corpul cererii\"}");
    return;
  }
  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err || !doc.is<JsonObject>()) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"JSON invalid\"}");
    return;
  }
  JsonObject s = doc.as<JsonObject>();

  if (s.containsKey("buzzer"))        buzzerOn     = s["buzzer"].as<bool>();
  if (s.containsKey("buzzerVolume")) {
    int v = s["buzzerVolume"].as<int>();
    if (v >= 0 && v <= 100) buzzerVolume = buzzVolNotif = buzzVolAuto = buzzVolAlarm = buzzVolTouch = (uint8_t)v;
  }
  // A backup from before the categories carries only the one volume, set above.
  if (s.containsKey("buzzerVolNotif")) { int v = s["buzzerVolNotif"].as<int>(); if (v >= 0 && v <= 100) buzzVolNotif = (uint8_t)v; }
  if (s.containsKey("buzzerVolAuto"))  { int v = s["buzzerVolAuto"].as<int>();  if (v >= 0 && v <= 100) buzzVolAuto  = (uint8_t)v; }
  if (s.containsKey("buzzerVolAlarm")) { int v = s["buzzerVolAlarm"].as<int>(); if (v >= 0 && v <= 100) buzzVolAlarm = (uint8_t)v; }
  if (s.containsKey("buzzerVolTouch")) { int v = s["buzzerVolTouch"].as<int>(); if (v >= 0 && v <= 100) buzzVolTouch = (uint8_t)v; }
  if (s.containsKey("buzzerPreset")) {
    const char* v = s["buzzerPreset"] | "";
    if (strlen(v) > 0) strncpy(buzzerPreset, v, sizeof(buzzerPreset) - 1), buzzerPreset[sizeof(buzzerPreset) - 1] = '\0';
  }

  if (s.containsKey("items") && s["items"].is<JsonArray>()) {
    JsonArray arr = s["items"];
    for (JsonObject it : arr) {
      if (!it.containsKey("id")) continue;
      uint8_t id = it["id"].as<uint8_t>();
      for (int i = 0; i < NUM_ITEMS; i++) {
        if (items[i].id == id) {
          if (it.containsKey("enabled")) items[i].enabled = it["enabled"].as<bool>();
          if (it.containsKey("dur")) {
            int d = it["dur"].as<int>();
            if (d > 0 && d <= 65535) items[i].durationSec = (uint16_t)d;
          }
          break;
        }
      }
    }
  }

  if (s.containsKey("bright")) {
    int v = s["bright"].as<int>();
    if (v >= 0 && v <= 255) curBrightness = (uint8_t)v;
  }
  if (s.containsKey("dimAuto"))  dimAutoOn = s["dimAuto"].as<bool>();
  if (s.containsKey("dimFrom")) {
    const char* v = s["dimFrom"] | "";
    int h, m;
    if (sscanf(v, "%d:%d", &h, &m) == 2 && h >= 0 && h <= 23 && m >= 0 && m <= 59) { dimFromH = h; dimFromM = m; }
  }
  if (s.containsKey("dimTo")) {
    const char* v = s["dimTo"] | "";
    int h, m;
    if (sscanf(v, "%d:%d", &h, &m) == 2 && h >= 0 && h <= 23 && m >= 0 && m <= 59) { dimToH = h; dimToM = m; }
  }
  if (s.containsKey("dimLevel")) {
    int v = s["dimLevel"].as<int>();
    if (v >= 0 && v <= 255) dimLevel = (uint8_t)v;
  }

  if (s.containsKey("wxPreset"))   { int v = s["wxPreset"].as<int>();   if (v >= 1 && v <= WX_SHOW_ALL) wxPreset = (uint8_t)v; }
  if (s.containsKey("tempunit"))   { int v = s["tempunit"].as<int>();   if (v == 0 || v == 1) tempUnit = (uint8_t)v; }
  if (s.containsKey("hourLeadingZero")) hourLeadingZero = s["hourLeadingZero"].as<bool>();
  if (s.containsKey("netTimeSync")) netTimeSync = s["netTimeSync"].as<bool>();
  if (s.containsKey("showGrayed")) showGrayedContent = s["showGrayed"].as<bool>();
  if (s.containsKey("autoSleep")) autoSleepOn = s["autoSleep"].as<bool>();
  if (s.containsKey("autoSleepSec")) {
    long v = s["autoSleepSec"].as<long>();
    if (v >= 60 && v <= 86400) autoSleepSec = (uint32_t)v;
  }
  if (s.containsKey("liveHl")) liveTileHighlight = s["liveHl"].as<bool>();
  if (s.containsKey("uiDark")) webUiDark = s["uiDark"].as<bool>();
  if (s.containsKey("uiLang")) {
    const char* v = s["uiLang"] | "";
    if (strcmp(v, "en") == 0 || strcmp(v, "ro") == 0) strcpy(webUiLang, v);
  }
  // Restored before the item list below, so applyTileHiddenMask() at the end of
  // this function judges the masks against the items the backup actually holds.
  if (s.containsKey("tileHidden")) {
    long v = s["tileHidden"].as<long>();
    if (v >= 0 && v <= 0xFFFF) tileHiddenMask = (uint16_t)v;
  }
  if (s.containsKey("prioHidden")) {
    long v = s["prioHidden"].as<long>();
    if (v >= 0 && v <= 0xFF) prioHiddenMask = (uint8_t)v;
  }
  // Spelled out rather than via restoreRange(): that lambda is declared
  // further down this function, well after this line.
  if (s.containsKey("defaultStartMode")) {
    int v = s["defaultStartMode"].as<int>();
    if (v >= START_MODE_WIFI && v <= START_MODE_AP) defaultStartMode = (uint8_t)v;
  }
  if (s.containsKey("hourformat")) { int v = s["hourformat"].as<int>(); if (v == 0 || v == 1) hourFormat = (uint8_t)v; }
  if (s.containsKey("hwFormat"))   { int v = s["hwFormat"].as<int>();   if (v == 0 || v == 1) hwFormat = (uint8_t)v; }
  if (s.containsKey("hwLeadZero")) hwLeadZero = s["hwLeadZero"].as<bool>();
  if (s.containsKey("hwBarMode"))  { int v = s["hwBarMode"].as<int>();  if (v == 0 || v == 1) hwBarMode = (uint8_t)v; }
  if (s.containsKey("hwBarPos"))   { int v = s["hwBarPos"].as<int>();   if (v == 0 || v == 1) hwBarPos = (uint8_t)v; }
  if (s.containsKey("hwSwap"))     hwSwap = s["hwSwap"].as<bool>();
  if (s.containsKey("dateformat")) { int v = s["dateformat"].as<int>(); if (v >= 0 && v <= 5) dateFormat = (uint8_t)v; }
  if (s.containsKey("datelang"))   { int v = s["datelang"].as<int>();   if (v >= 0) dateLang = (uint8_t)v; }
  if (s.containsKey("customdatefmt")) {
    const char* v = s["customdatefmt"] | "";
    strncpy(customDateFmt, v, sizeof(customDateFmt) - 1); customDateFmt[sizeof(customDateFmt) - 1] = '\0';
  }

  if (s.containsKey("wxCity")) {
    const char* v = s["wxCity"] | "";
    strncpy(weatherCity, v, sizeof(weatherCity) - 1); weatherCity[sizeof(weatherCity) - 1] = '\0';
  }
  if (s.containsKey("wxLang")) {
    const char* v = s["wxLang"] | "";
    strncpy(weatherLang, v, sizeof(weatherLang) - 1); weatherLang[sizeof(weatherLang) - 1] = '\0';
  }
  // Notă: cheia API meteo (weatherApiKey) nu e inclusă în /state din motive
  // de securitate, deci nu poate fi nici restaurată - trebuie reintrodusa manual.

  if (s.containsKey("currencyBase")) {
    const char* v = s["currencyBase"] | "";
    if (strlen(v) > 0 && strlen(v) < sizeof(currencyBase)) strcpy(currencyBase, v);
  }
  if (s.containsKey("currencyQuote")) {
    const char* v = s["currencyQuote"] | "";
    if (strlen(v) > 0 && strlen(v) < sizeof(currencyQuote)) strcpy(currencyQuote, v);
  }
  if (s.containsKey("currencyCompare")) currencyCompareEnabled = s["currencyCompare"].as<bool>();

  if (s.containsKey("notifEnabled"))      notifEnabled = s["notifEnabled"].as<bool>();
  if (s.containsKey("webEnabled"))        webAccessEnabled = s["webEnabled"].as<bool>();
  if (s.containsKey("ets2Enabled"))       ets2Enabled = s["ets2Enabled"].as<bool>();
  if (s.containsKey("ets2OrderFirst"))    ets2OrderFirst = s["ets2OrderFirst"].as<bool>();
  if (s.containsKey("nowPlayingIsPriority")) nowPlayingIsPriority = s["nowPlayingIsPriority"].as<bool>();

  if (s.containsKey("priorityOrder")) {
    String ord = s["priorityOrder"].as<String>();
    uint8_t newOrder[NUM_PRIORITY_IDS];
    int count = 0;
    bool sawNowPlaying = false, sawStopwatch = false, sawTimer = false, sawWeb = false, sawAlarm = false;
    int startIdx = 0;
    for (int i = 0; i <= (int)ord.length() && count < NUM_PRIORITY_IDS; i++) {
      if (i == (int)ord.length() || ord[i] == ',') {
        String tok = ord.substring(startIdx, i);
        tok.trim();
        if (tok == "notif")           newOrder[count++] = PRIORITY_ID_NOTIF;
        else if (tok == "ets2")       newOrder[count++] = PRIORITY_ID_ETS2;
        else if (tok == "nowplaying") { newOrder[count++] = PRIORITY_ID_NOWPLAYING; sawNowPlaying = true; }
        else if (tok == "stopwatch")  { newOrder[count++] = PRIORITY_ID_STOPWATCH; sawStopwatch = true; }
        else if (tok == "timer")      { newOrder[count++] = PRIORITY_ID_TIMER; sawTimer = true; }
        else if (tok == "webaccess")  { newOrder[count++] = PRIORITY_ID_WEB; sawWeb = true; }
        else if (tok == "alarm")      { newOrder[count++] = PRIORITY_ID_ALARM; sawAlarm = true; }
        startIdx = i + 1;
      }
    }
    // Restored from a backup taken before the alarm existed. It goes back in at
    // the top, not the bottom, for the reason the default order has it there.
    if (!sawAlarm && count < NUM_PRIORITY_IDS) {
      for (int i = count; i > 0; i--) newOrder[i] = newOrder[i - 1];
      newOrder[0] = PRIORITY_ID_ALARM;
      count++;
    }
    if (!sawNowPlaying && count < NUM_PRIORITY_IDS) newOrder[count++] = PRIORITY_ID_NOWPLAYING;
    if (!sawStopwatch  && count < NUM_PRIORITY_IDS) newOrder[count++] = PRIORITY_ID_STOPWATCH;
    if (!sawTimer      && count < NUM_PRIORITY_IDS) newOrder[count++] = PRIORITY_ID_TIMER;
    if (!sawWeb        && count < NUM_PRIORITY_IDS) newOrder[count++] = PRIORITY_ID_WEB;
    if (count == NUM_PRIORITY_IDS) {
      for (int i = 0; i < NUM_PRIORITY_IDS; i++) priorityOrder[i] = newOrder[i];
    }
  }

  if (s.containsKey("hideIcons"))           hideTileIcons      = s["hideIcons"].as<bool>();
  if (s.containsKey("hideIconDate"))        hideIconDate       = s["hideIconDate"].as<bool>();
  if (s.containsKey("hideIconTemp"))        hideIconTemp       = s["hideIconTemp"].as<bool>();
  if (s.containsKey("hideIconReminder"))    hideIconReminder   = s["hideIconReminder"].as<bool>();
  if (s.containsKey("hideIconWeather"))     hideIconWeather    = s["hideIconWeather"].as<bool>();
  if (s.containsKey("hideIconNotif"))       hideIconNotif      = s["hideIconNotif"].as<bool>();
  if (s.containsKey("hideIconNowPlaying"))  hideIconNowPlaying = s["hideIconNowPlaying"].as<bool>();
  if (s.containsKey("hideIconPressure"))    hideIconPressure   = s["hideIconPressure"].as<bool>();
  if (s.containsKey("hideIconCurrency"))    hideIconCurrency   = s["hideIconCurrency"].as<bool>();
  if (s.containsKey("hideIconYoutube"))     hideIconYoutube    = s["hideIconYoutube"].as<bool>();
  if (s.containsKey("hideIconWebAccess"))   hideIconWebAccess  = s["hideIconWebAccess"].as<bool>();
  if (s.containsKey("hideIconIp"))          hideIconIp         = s["hideIconIp"].as<bool>();
  if (s.containsKey("npAdaptiveIcon"))      npAdaptiveIcon     = s["npAdaptiveIcon"].as<bool>();

  auto restoreIcon = [&](const char* key, uint8_t &target) {
    if (s.containsKey(key)) {
      int v = s[key].as<int>();
      if (v >= 0 && v <= (int)ICON_CATALOG_COUNT) target = (uint8_t)v;
    }
  };
  restoreIcon("iconSelDate",     iconSelDate);
  restoreIcon("iconSelTemp",     iconSelTemp);
  restoreIcon("iconSelReminder", iconSelReminder);
  restoreIcon("iconSelNotif",    iconSelNotif);
  restoreIcon("iconSelNpMusic",  iconSelNpMusic);
  restoreIcon("iconSelNpVideo",  iconSelNpVideo);
  restoreIcon("iconSelPressure", iconSelPressure);
  restoreIcon("iconSelCurrency", iconSelCurrency);
  restoreIcon("iconSelIp",       iconSelIp);
  restoreIcon("iconWxSunny",     iconWxSunny);
  restoreIcon("iconWxCloud",     iconWxCloud);
  restoreIcon("iconWxRain",      iconWxRain);
  restoreIcon("iconWxStorm",     iconWxStorm);
  restoreIcon("iconWxSnow",      iconWxSnow);
  restoreIcon("iconWxWind",      iconWxWind);
  restoreIcon("iconWxNight",     iconWxNight);

  auto restoreRange = [&](const char* key, uint8_t &target, int lo, int hi) {
    if (s.containsKey(key)) {
      int v = s[key].as<int>();
      if (v >= lo && v <= hi) target = (uint8_t)v;
    }
  };
  restoreRange("scrollType",          scrollType,           0, 3);
  restoreRange("scrollTypeDate",      scrollTypeDate,       0, 3);
  restoreRange("scrollTypeTemp",      scrollTypeTemp,       0, 3);
  restoreRange("scrollTypeReminder",  scrollTypeReminder,   0, 3);
  restoreRange("scrollTypeWeather",   scrollTypeWeather,    0, 3);
  restoreRange("scrollTypeNotif",     scrollTypeNotif,      0, 3);
  restoreRange("scrollTypeNowPlaying",scrollTypeNowPlaying, 0, 3);
  restoreRange("scrollTypePressure",  scrollTypePressure,   0, 3);
  restoreRange("scrollTypeStopwatch", scrollTypeStopwatch,  0, 3);
  restoreRange("scrollTypeCurrency",  scrollTypeCurrency,   0, 3);
  restoreRange("scrollTypeYoutube",   scrollTypeYoutube,    0, 3);
  restoreRange("scrollTypeWebAccess", scrollTypeWebAccess,  0, 3);
  restoreRange("scrollTypeTimer",     scrollTypeTimer,      0, 3);
  restoreRange("scrollTypeIp",        scrollTypeIp,         0, 3);
  restoreRange("scrollSpeed", scrollSpeed, SCROLL_SPEED_MIN, SCROLL_SPEED_MAX);
  for (int i = 0; i < SCROLL_SPEED_COUNT; i++) {
    String k = String("scrollSpeed") + SPD_KEYS[i];
    if (s.containsKey(k)) {
      int v = s[k].as<int>();
      if (v >= SCROLL_SPEED_MIN && v <= SCROLL_SPEED_MAX) scrollSpeedTile[i] = (uint8_t)v;
    }
  }

  restoreRange("fontType",         fontType,           0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypeDate",     fontTypeDate,       0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypeTemp",     fontTypeTemp,       0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypeReminder", fontTypeReminder,   0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypeWeather",  fontTypeWeather,    0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypeNotif",    fontTypeNotif,      0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypeNowPlaying",fontTypeNowPlaying,0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypePressure", fontTypePressure,   0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypeStopwatch",fontTypeStopwatch,  0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypeCurrency", fontTypeCurrency,   0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypeYoutube",  fontTypeYoutube,    0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypeWebAccess", fontTypeWebAccess, 0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypeTimer",    fontTypeTimer,      0, FONT_MODE_COUNT - 1);
  restoreRange("fontTypeIp",       fontTypeIp,         0, FONT_MODE_COUNT - 1);

  restoreRange("tileTransGlobal", tileTransGlobal, 0, 255);
  restoreRange("tileTransHour",   tileTransHour,   0, 255);
  restoreRange("tileTransDate",   tileTransDate,   0, 255);
  restoreRange("tileTransTemp",   tileTransTemp,   0, 255);
  restoreRange("tileTransNp",     tileTransNp,     0, 255);
  restoreRange("tileTransWx",     tileTransWx,     0, 255);
  restoreRange("tileTransRem",    tileTransRem,    0, 255);
  restoreRange("tileTransCanvas", tileTransCanvas, 0, 255);
  restoreRange("tileTransPress",  tileTransPress,  0, 255);
  restoreRange("tileTransSs",     tileTransSs,     0, 255);
  restoreRange("tileTransCurr",   tileTransCurr,   0, 255);
  restoreRange("tileTransYt",     tileTransYt,     0, 255);
  restoreRange("tileTransHw",     tileTransHw,     0, 255);
  restoreRange("tileTransWeb",    tileTransWeb,    0, 255);
  restoreRange("tileTransNotif",  tileTransNotif,  0, 255);
  restoreRange("tileTransEts2",   tileTransEts2,   0, 255);
  restoreRange("tileTransSw",     tileTransSw,     0, 255);
  restoreRange("tileTransTmr",    tileTransTmr,    0, 255);
  restoreRange("tileTransAlarm",  tileTransAlarm,  0, 255);
  restoreRange("tileTransIp",     tileTransIp,     0, 255);
  restoreRange("tileTransC2P",    tileTransC2P,    0, 255);
  restoreRange("tileTransP2C",    tileTransP2C,    0, 255);
  restoreRange("tileTransSpd",    tileTransSpeed,  TRSPD_MIN, TRSPD_MAX);
  for (int i = 0; i < TRSPD_COUNT; i++) {
    String k = String("tileTransSpd") + TRSPD_KEYS[i];
    if (s.containsKey(k)) {
      int v = s[k].as<int>();
      if (v >= TRSPD_MIN && v <= TRSPD_MAX) tileTransSpd[i] = (uint8_t)v;
    }
  }

  if (s.containsKey("timerDurationSec")) {
    long v = s["timerDurationSec"].as<long>();
    if (v > 0 && v <= 359999) timerDurationSec = (uint32_t)v;
  }
  if (s.containsKey("timerPreset")) timerPreset = s["timerPreset"].as<uint8_t>();

  // Every field is range-checked on the way back in: a backup is a file the user
  // can edit, and an hour of 40 would be an alarm that never rings again.
  if (s.containsKey("alarms") && s["alarms"].is<JsonArray>()) {
    JsonArray arr = s["alarms"];
    int i = 0;
    for (JsonObject a : arr) {
      if (i >= ALARM_COUNT) break;
      int h = a["h"] | -1, m = a["m"] | -1, d = a["d"] | -1;
      if (h >= 0 && h <= 23)  alarms[i].hour   = (uint8_t)h;
      if (m >= 0 && m <= 59)  alarms[i].minute = (uint8_t)m;
      if (d >= 0 && d <= 127) alarms[i].days   = (uint8_t)d;
      if (a.containsKey("en")) alarms[i].enabled = a["en"].as<bool>();
      // A backup taken before alarms could be added carries no "p"; the alarms
      // in it are the ones its owner had, so they come back.
      alarms[i].present = a.containsKey("p") ? a["p"].as<bool>() : alarms[i].enabled;
      const char* t = a["tone"] | "";
      if (strlen(t) > 0 && strlen(t) < sizeof(alarms[i].tone)) strcpy(alarms[i].tone, t);
      i++;
    }
  }

  if (s.containsKey("mementoText")) {
    const char* v = s["mementoText"] | "";
    strncpy(mementoBuf, v, sizeof(mementoBuf) - 1); mementoBuf[sizeof(mementoBuf) - 1] = '\0';
  }
  if (s.containsKey("canvasBmp")) {
    const char* v = s["canvasBmp"] | "";
    canvasBitmapFromHex(String(v));
  }

  if (s.containsKey("evSndTile"))  { const char* v = s["evSndTile"]  | ""; if (strlen(v)) { strncpy(eventSoundTile,  v, sizeof(eventSoundTile)  - 1); eventSoundTile[sizeof(eventSoundTile) - 1]   = '\0'; } }
  if (s.containsKey("evSndWifi"))  { const char* v = s["evSndWifi"]  | ""; if (strlen(v)) { strncpy(eventSoundWifi,  v, sizeof(eventSoundWifi)  - 1); eventSoundWifi[sizeof(eventSoundWifi) - 1]   = '\0'; } }
  if (s.containsKey("evSndNotif")) { const char* v = s["evSndNotif"] | ""; if (strlen(v)) { strncpy(eventSoundNotif, v, sizeof(eventSoundNotif) - 1); eventSoundNotif[sizeof(eventSoundNotif) - 1] = '\0'; } }
  if (s.containsKey("evSndWeb")) { const char* v = s["evSndWeb"] | ""; if (strlen(v)) { strncpy(eventSoundWeb, v, sizeof(eventSoundWeb) - 1); eventSoundWeb[sizeof(eventSoundWeb) - 1] = '\0'; } }
  if (s.containsKey("evSndEts2"))  { const char* v = s["evSndEts2"]  | ""; if (strlen(v)) { strncpy(eventSoundEts2,  v, sizeof(eventSoundEts2)  - 1); eventSoundEts2[sizeof(eventSoundEts2) - 1]   = '\0'; } }
  if (s.containsKey("evSndTouch")) { const char* v = s["evSndTouch"] | ""; if (strlen(v)) { strncpy(eventSoundTouch, v, sizeof(eventSoundTouch) - 1); eventSoundTouch[sizeof(eventSoundTouch) - 1] = '\0'; } }
  if (s.containsKey("evSndTimer")) { const char* v = s["evSndTimer"] | ""; if (strlen(v)) { strncpy(eventSoundTimer, v, sizeof(eventSoundTimer) - 1); eventSoundTimer[sizeof(eventSoundTimer) - 1] = '\0'; } }

  if (s.containsKey("touchTapAction"))       touchTapAction       = s["touchTapAction"].as<uint8_t>();
  if (s.containsKey("touchDoubleTapAction")) touchDoubleTapAction = s["touchDoubleTapAction"].as<uint8_t>();

  if (s.containsKey("accentColor")) {
    const char* v = s["accentColor"] | "";
    if (strlen(v) >= 4 && strlen(v) < sizeof(accentColor) && v[0] == '#') strcpy(accentColor, v);
  }
  if (s.containsKey("uiShape")) {
    uint8_t v = s["uiShape"].as<uint8_t>();
    if (v <= 3) webUiShape = v;
  }
  if (s.containsKey("uiColors")) {
    const char* v = s["uiColors"] | "";
    if (isValidUiColors(String(v))) strcpy(webUiColors, v);
  }
  if (s.containsKey("ssAnim")) ssAnimSelected = s["ssAnim"].as<uint8_t>();

  // The item list and the masks both came out of the same backup; this is where
  // they are finally judged against each other.
  applyTileHiddenMask();

  saveSettings();

  server.send(200, "application/json", "{\"ok\":true}");
  delay(500);
  ESP.restart();
}

// FACTORY RESET
//
// Wipes every Preferences namespace the project uses ("settings", "wifi",
// "auth", "apcfg") so the device comes back up exactly as it would out of
// the box: no saved WiFi network -> setup() falls through to
// startProvisionMode() on the next boot, and loadAuth()/loadSettings() will
// re-populate their in-memory defaults from the (now empty) namespaces.
void handleFactoryReset() {
  if (!checkAuth()) return;
  const char* namespacesToWipe[] = { "settings", "wifi", "auth", "apcfg" };
  for (size_t i = 0; i < sizeof(namespacesToWipe) / sizeof(namespacesToWipe[0]); i++) {
    prefs.begin(namespacesToWipe[i], false);
    prefs.clear();
    prefs.end();
  }
  saveSettingsCancel();
  server.send(200, "application/json", "{\"ok\":true}");
  delay(500);
  ESP.restart();
}

// MEMENTO HANDLER
void handleMementoSett() {
  if (!checkAuth()) return;
  if (server.hasArg("text")) {
    String txt = server.arg("text");
    txt.trim();

    if (txt.length() > 120) txt = txt.substring(0, 120);
    txt.toCharArray(mementoBuf, sizeof(mementoBuf));
    saveSettingsDeferred();

    if (items[currentSlot].id == ITEM_MEMENTO) mementoInit();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste text");
  }
}

// CANVAS HANDLER

void handleCanvasSett() {
  if (!checkAuth()) return;
  if (!server.hasArg("bmp")) {
    server.send(400, "text/plain", "Lipseste bmp");
    return;
  }
  if (!canvasBitmapFromHex(server.arg("bmp"))) {
    server.send(400, "text/plain", "Format bmp invalid");
    return;
  }
  prefs.begin("settings", false);
  prefs.putBytes("canvasBmp", canvasBitmap, CANVAS_COLS);
  prefs.end();

  if (items[currentSlot].id == ITEM_CANVAS) drawCanvas();
  server.send(200, "text/plain", "OK");
}

void handleWeatherSett() {
  if (!checkAuth()) return;
  bool changed = false;
  if (server.hasArg("apikey")) {
    String k = server.arg("apikey");
    k.trim();
    if (k.length() > 0) {
      // Refuse loudly rather than store something that will silently fail every
      // fetch from here on.
      if (!wxKeyLooksValid(k)) {
        server.send(400, "text/plain", "Cheie API invalida");
        return;
      }
      k.toCharArray(weatherApiKey, sizeof(weatherApiKey));
      changed = true;
    }
  }
  if (server.hasArg("lat") && server.hasArg("lon") && server.hasArg("name")) {
    weatherLat = server.arg("lat").toFloat();
    weatherLon = server.arg("lon").toFloat();
    server.arg("name").toCharArray(weatherCity, sizeof(weatherCity));
    changed = true;
  }
  if (server.hasArg("preset")) {
    int p = server.arg("preset").toInt();
    if (p >= 1 && p <= WX_SHOW_ALL) {
      wxPreset = (uint8_t)p;
      changed = true;
      // Redraw straight away if the tile is the one on screen, so the change is
      // visible without waiting for the next fetch or slot.
      if (items[currentSlot].id == ITEM_WEATHER) weatherInit();
    }
  }
  if (changed) {
    saveSettingsDeferred();
    lastWeatherFetch = 0;
  }
  server.send(200, "text/plain", "OK");
}

String urlEncode(const String& s) {
  String enc = "";
  for (int i = 0; i < (int)s.length(); i++) {
    char c = s[i];
    if (isalnum(c) || c=='-' || c=='_' || c=='.' || c=='~') {
      enc += c;
    } else if (c == ' ') {
      enc += '+';
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      enc += buf;
    }
  }
  return enc;
}

void handleWeatherSearch() {
  if (!checkAuth()) return;
  if (!server.hasArg("q")) {
    server.send(400, "text/plain", "Lipseste parametrul q");
    return;
  }
  String q   = server.arg("q");
  String key = server.arg("key");

  if (key.length() == 0 && strlen(weatherApiKey) > 0) {
    key = String(weatherApiKey);
  }
  if (key.length() == 0) {
    server.send(400, "text/plain", "Lipseste cheia API");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    server.send(503, "text/plain", "No WiFi");
    return;
  }
  String url = "http://api.openweathermap.org/geo/1.0/direct?q=" +
               urlEncode(q) + "&limit=5&appid=" + urlEncode(key);
  Serial.println("[WxSearch] q=" + q + " keyLen=" + String(key.length()));
  Serial.println("[WxSearch] URL: " + url);
  HTTPClient http;
  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();
  Serial.println("[WxSearch] HTTP code: " + String(code));
  if (code == 200) {
    String body = http.getString();
    server.send(200, "application/json", body);
  } else if (code == 401) {
    server.send(401, "text/plain", "Cheie API invalida");
  } else if (code <= 0) {
    server.send(503, "text/plain", "Eroare retea: " + String(code));
  } else {
    server.send(502, "text/plain", "OWM error " + String(code));
  }
  http.end();
}

void handleWxLang() {
  if (!checkAuth()) return;
  if (server.hasArg("lang")) {
    String l = server.arg("lang");
    l.trim();
    if (l.length() >= 2 && l.length() <= 7) {
      l.toCharArray(weatherLang, sizeof(weatherLang));
      saveSettingsDeferred();
      lastWeatherFetch = 0;
      server.send(200, "text/plain", "OK");
      return;
    }
  }
  server.send(400, "text/plain", "Parametru invalid");
}

void handleWeatherState() {
  if (!checkAuth()) return;
  String json = "{\"valid\":" + String(weatherValid ? "true" : "false") +
                ",\"city\":\"" + String(weatherCity) + "\"" +
                ",\"lat\":" + String(weatherLat, 4) +
                ",\"lon\":" + String(weatherLon, 4) +
                ",\"hasKey\":" + String(strlen(weatherApiKey) > 0 ? "true" : "false") + ",\"keyLen\":" + String(strlen(weatherApiKey)) +
                ",\"lang\":\"" + String(weatherLang) + "\"" +
                ",\"temp\":" + String(weatherTempC, 1) +
                ",\"humidity\":" + String(weatherHumidity) +
                ",\"desc\":\"" + String(weatherDesc) + "\"}";
  server.send(200, "application/json", json);
}

void handleWeatherKey() {
  if (!checkAuth()) return;

  String json = "{\"key\":\"" + String(weatherApiKey) + "\"}";
  server.send(200, "application/json", json);
}

// CURRENCY STANDARDS HANDLERS


// Settings for one social counter tile. The key is only overwritten when a new
// one is actually supplied, so re-saving the dialog without retyping it does
// not wipe a stored key.
void handleSocialKey() {
  if (!checkAuth()) return;
  int i = server.hasArg("i") ? server.arg("i").toInt() : 0;
  if (i < 0 || i >= SOCIAL_COUNT) {
    server.send(400, "text/plain", "Index invalid");
    return;
  }
  server.send(200, "application/json",
              String("{\"key\":\"") + jsonEscape(String(social[i].apiKey)) + "\"}");
}

void handleSocialSett() {
  if (!checkAuth()) return;
  if (!server.hasArg("i")) {
    server.send(400, "text/plain", "Lipseste i");
    return;
  }
  int i = server.arg("i").toInt();
  if (i < 0 || i >= SOCIAL_COUNT) {
    server.send(400, "text/plain", "Index invalid");
    return;
  }
  if (server.hasArg("handle")) {
    String h = server.arg("handle");
    h.trim();
    strncpy(social[i].handle, h.c_str(), sizeof(social[i].handle) - 1);
    social[i].handle[sizeof(social[i].handle) - 1] = '\0';
  }
  if (server.hasArg("key")) {
    String k = server.arg("key");
    k.trim();
    if (k.length() > 0) {
      strncpy(social[i].apiKey, k.c_str(), sizeof(social[i].apiKey) - 1);
      social[i].apiKey[sizeof(social[i].apiKey) - 1] = '\0';
    }
  }
  if (server.hasArg("clearkey") && server.arg("clearkey") == "1") social[i].apiKey[0] = '\0';
  if (server.hasArg("showname")) social[i].showName = (server.arg("showname") == "1");

  saveSettingsDeferred();
  social[i].valid    = false;
  lastSocialFetch[i] = 0;          // refetch on the next loop pass
  server.send(200, "text/plain", "OK");
}

void handleCurrencySett() {
  if (!checkAuth()) return;
  bool changed = false;
  if (server.hasArg("base")) {
    String b = server.arg("base");
    b.trim(); b.toUpperCase();
    if (b.length() == 3) { b.toCharArray(currencyBase, sizeof(currencyBase)); changed = true; }
  }
  if (server.hasArg("compare")) {
    bool c = (server.arg("compare") == "1");
    if (c != currencyCompareEnabled) { currencyCompareEnabled = c; changed = true; }
  }
  if (server.hasArg("quote")) {
    String q = server.arg("quote");
    q.trim(); q.toUpperCase();
    if (q.length() == 3) { q.toCharArray(currencyQuote, sizeof(currencyQuote)); changed = true; }
  }
  if (changed) {
    saveSettingsDeferred();
    lastCurrencyFetch = 0;
    currencyValid = false;

    if (items[currentSlot].id == ITEM_CURRENCY) currencyInit();
  }
  server.send(200, "text/plain", "OK");
}

void handleCurrencyState() {
  if (!checkAuth()) return;
  String json = "{\"base\":\"" + jsonEscape(String(currencyBase)) + "\"" +
                ",\"quote\":\"" + jsonEscape(String(currencyQuote)) + "\"" +
                ",\"compare\":" + String(currencyCompareEnabled ? "true" : "false") +
                ",\"valid\":" + String(currencyValid ? "true" : "false") +
                ",\"rate\":" + String((currencyValid && !isnan(currencyRateNow)) ? currencyRateNow : 0.0f, 4) +
                ",\"trend\":" + String(currencyTrend) + "}";
  server.send(200, "application/json", json);
}

// WHAT COUNTS AS ACTIVITY
//
// Both of these are Octoglow Connect talking to the clock, which is the whole
// reason sleep keeps the network up: something arriving is a reason to light
// the panel again.
void handleNowPlaying() {
  noteActivity();
  if (!checkAuth()) return;
  bool updated = false;

  // Optional playback-source classification from Octoglow Connect: "video"
  // or "music" (anything else / missing => treat as music/unknown).
  if (server.hasArg("kind")) {
    npIsVideoSource = (server.arg("kind") == "video");
  }

  if (server.hasArg("artist") && server.hasArg("title")) {
    server.arg("artist").toCharArray(nowArtist, sizeof(nowArtist));
    server.arg("title").toCharArray(nowTitle,  sizeof(nowTitle));
    rebuildNowPlayingBuf();
    updated = true;
  } else if (server.hasArg("text")) {

    String txt = server.arg("text");
    int sep = txt.indexOf(" - ");
    if (sep >= 0) {
      txt.substring(0, sep).toCharArray(nowArtist, sizeof(nowArtist));
      txt.substring(sep + 3).toCharArray(nowTitle,  sizeof(nowTitle));
    } else {
      txt.toCharArray(nowArtist, sizeof(nowArtist));
      nowTitle[0] = '\0';
    }
    rebuildNowPlayingBuf();
    updated = true;
  }
  if (updated) {
    lastNowPlayingMs = millis();
    nowPlayingActive = true;
    if (items[currentSlot].id == ITEM_NOW_PLAYING &&
        (npState == NP_SHOW_START || npState == NP_PAUSE_BEFORE_RIGHT)) {
      npInit();
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste artist/title sau text");
  }
}

void handleNpState() {
  if (!checkAuth()) return;
  String json = "{\"active\":" + String(nowPlayingActive ? "true" : "false") +
                ",\"text\":\"" + jsonEscape(String(nowPlayingBuf)) + "\"" +
                ",\"npmode\":" + String(npDisplayMode) +
                ",\"hourformat\":" + String(hourFormat) +
                ",\"dateformat\":" + String(dateFormat) +
                ",\"lastTemp\":" + String(lastTemp) +
                ",\"wxValid\":" + String(weatherValid ? "true" : "false") +
                ",\"wxTemp\":" + String(weatherTempC, 1) +
                ",\"wxHumidity\":" + String(weatherHumidity) +
                ",\"wxDesc\":\"" + String(weatherDesc) + "\"}";
  server.send(200, "application/json", json);
}

// SCREEN SAVER HANDLER
void handleScreensaverSett() {
  if (!checkAuth()) return;
  if (server.hasArg("anim")) {
    uint8_t a = (uint8_t)server.arg("anim").toInt();
    if (a <= SS_ANIM_COUNT) {
      ssAnimSelected = a;
      saveSettingsDeferred();

      if (items[currentSlot].id == ITEM_SCREENSAVER) ssInit();
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste anim");
  }
}

void handleNpMode() {
  if (!checkAuth()) return;
  if (server.hasArg("mode")) {
    uint8_t m = (uint8_t)server.arg("mode").toInt();
    if (m <= 2) {
      npDisplayMode = m;
      rebuildNowPlayingBuf();
      saveSettingsDeferred();

      if (items[currentSlot].id == ITEM_NOW_PLAYING) npInit();
      else if (nowPlayingIsPriority && nowPlayingActive) npInit();
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste mode");
  }
}

// NOTIFICATION HANDLERS

void handleNotification() {
  noteActivity();
  if (!checkAuth()) return;
  if (!notifEnabled) {
    server.send(200, "text/plain", "DISABLED");
    return;
  }

  if (higherPriorityTileActive(PRIORITY_ID_NOTIF)) {
    server.send(200, "text/plain", "SUPPRESSED");
    return;
  }
  if (server.hasArg("text")) {
    String txt = server.arg("text");
    txt.trim();
    if (txt.length() == 0) {
      server.send(400, "text/plain", "Text gol");
      return;
    }

    if (txt.length() > 200) {
      txt = txt.substring(0, 200) + "...";
    }
    txt.toCharArray(notifBuf, sizeof(notifBuf));
    notifActive = true;
    beginC2PCapture();
    notifInit();
    finishC2PTransition(tileTransNotif, tileTransSpd[TRSPD_NOTIF]);
    nbPlayPreset(eventSoundNotif, BZ_CAT_NOTIF);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste text");
  }
}

void handleWebToggle() {
  if (!checkAuth()) return;
  if (!server.hasArg("enabled")) {
    server.send(400, "text/plain", "Lipseste enabled");
    return;
  }
  webAccessEnabled = (server.arg("enabled") == "1");
  if (!webAccessEnabled && webAccessAlertActive) {
    // Switching it off while its own alert is on screen clears it, rather than
    // leaving the message stranded until it finishes scrolling.
    notifActive = false;
    notifIconOverride = nullptr;
    webAccessAlertActive = false;
    notifBuf[0] = '\0';
    resumeAfterNotif();
  }
  saveSettingsDeferred();
  server.send(200, "text/plain", "OK");
}

void handleNotifToggle() {
  if (!checkAuth()) return;
  if (server.hasArg("enabled")) {
    notifEnabled = (server.arg("enabled") == "1");
    if (!notifEnabled && notifActive) {

      notifActive = false;
      notifIconOverride = nullptr;
      webAccessAlertActive = false;
      notifBuf[0] = '\0';
      beginP2CCapture();
      CycleItem& cur = items[currentSlot];
      if (cur.id == ITEM_NOW_PLAYING) npInit();
      else if (cur.id == ITEM_WEATHER) weatherInit();
      else if (cur.id == ITEM_CURRENCY) currencyInit();
    else if (socialIndexForItem(cur.id) >= 0) socialInit(cur.id);
      else { gLastStaticDrawMs = 0; gStaticDrawDone = false; }
      finishP2CTransition();
    }
    saveSettingsDeferred();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste enabled");
  }
}

void handleNotifState() {
  if (!checkAuth()) return;
  String json = "{\"enabled\":" + String(notifEnabled ? "true" : "false") +
                ",\"active\":"  + String(notifActive  ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

// ETS2 HANDLER

void handleEts2Speed() {
  if (!checkAuth()) return;
  if (!ets2Enabled) {
    server.send(200, "text/plain", "DISABLED");
    return;
  }
  if (server.hasArg("speed")) {
    int spd = server.arg("speed").toInt();
    if (spd < 0) spd = 0;
    if (spd > 999) spd = 999;
    ets2Speed = spd;
    lastEts2Ms = millis();
    if (!ets2Active) {
      ets2Active = true;
      beginC2PCapture();
      ets2Init();
      finishC2PTransition(tileTransEts2, tileTransSpd[TRSPD_ETS2]);
    }

    if (spd > ETS2_SPEED_LIMIT) {
      if (!ets2SpeedAlertFired) {
        ets2SpeedAlertFired = true;
        nbPlayPreset(eventSoundEts2, BZ_CAT_AUTO);
      }
    } else {
      ets2SpeedAlertFired = false;
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste speed");
  }
}

void handleEts2Toggle() {
  if (!checkAuth()) return;
  if (server.hasArg("enabled")) {
    ets2Enabled = (server.arg("enabled") == "1");
    if (!ets2Enabled && ets2Active) {

      ets2Active = false;
      ets2Speed = 0;
      ets2LastSpeed = -1;
      beginP2CCapture();
      CycleItem& cur = items[currentSlot];
      if (cur.id == ITEM_NOW_PLAYING) npInit();
      else if (cur.id == ITEM_WEATHER) weatherInit();
      else if (cur.id == ITEM_CURRENCY) currencyInit();
    else if (socialIndexForItem(cur.id) >= 0) socialInit(cur.id);
      else { gLastStaticDrawMs = 0; gStaticDrawDone = false; }
      finishP2CTransition();
    }
    saveSettingsDeferred();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste enabled");
  }
}

void handleEts2State() {
  if (!checkAuth()) return;
  String json = "{\"enabled\":" + String(ets2Enabled ? "true" : "false") +
                ",\"active\":"  + String(ets2Active  ? "true" : "false") +
                ",\"speed\":"   + String(ets2Speed) + "}";
  server.send(200, "application/json", json);
}

void handleEts2Order() {
  if (!checkAuth()) return;
  if (server.hasArg("first")) {
    ets2OrderFirst = (server.arg("first") == "1");
    saveSettingsDeferred();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste first");
  }
}

// STOPWATCH HANDLERS

void handleSwStart() {
  if (!checkAuth()) return;
  swRunning   = true;
  swStartMs   = millis();
  swElapsedMs = 0;
  server.send(200, "text/plain", "OK");
}

void handleSwStop() {
  if (!checkAuth()) return;
  if (swRunning) {
    swElapsedMs = millis() - swStartMs;
    swRunning   = false;
    swWasActive = false;

    CycleItem& cur = items[currentSlot];
    if (cur.id == ITEM_NOW_PLAYING) npInit();
    else if (cur.id == ITEM_WEATHER) weatherInit();
    else if (cur.id == ITEM_CURRENCY) currencyInit();
    else if (socialIndexForItem(cur.id) >= 0) socialInit(cur.id);
    else { gLastStaticDrawMs = 0; gStaticDrawDone = false; }
  }
  char txt[16];
  swFormatFull(txt, sizeof(txt), swElapsedMs);
  String json = "{\"elapsedMs\":" + String(swElapsedMs) + ",\"text\":\"" + String(txt) + "\"}";
  server.send(200, "application/json", json);
}

void handleSwState() {
  if (!checkAuth()) return;
  unsigned long ms = swGetElapsedMs();
  char txt[16];
  swFormatFull(txt, sizeof(txt), ms);
  String json = "{\"running\":" + String(swRunning ? "true" : "false") +
                ",\"elapsedMs\":" + String(ms) +
                ",\"text\":\"" + String(txt) + "\"}";
  server.send(200, "application/json", json);
}

// TIMER HANDLERS

void handleTimerSett() {
  if (!checkAuth()) return;
  if (server.hasArg("durationSec")) {
    long d = server.arg("durationSec").toInt();
    if (d > 0 && d <= 359999 && !timerRunning && !timerFinished) {
      timerDurationSec = (uint32_t)d;
      timerRemainingSnapMs = (unsigned long)timerDurationSec * 1000UL;
    }
  }
  if (server.hasArg("preset")) {
    uint8_t p = (uint8_t)server.arg("preset").toInt();
    if (p <= 4) timerPreset = p;
  }
  if (server.hasArg("scrollType")) {
    uint8_t st = (uint8_t)server.arg("scrollType").toInt();
    if (st <= 3) scrollTypeTimer = st;
  }
  saveSettingsDeferred();
  server.send(200, "text/plain", "OK");
}


void handleTimerStart() {
  if (!checkAuth()) return;
  timerStart();
  server.send(200, "text/plain", "OK");
}


void handleTimerPause() {
  if (!checkAuth()) return;
  if (timerFinished) {
    timerDismiss();
  } else {
    timerPause();
  }
  timerWasActive = false;
  server.send(200, "text/plain", "OK");
}

// ALARM HANDLERS

void handleAlarmSett() {
  if (!checkAuth()) return;
  if (!server.hasArg("idx")) { server.send(400, "text/plain", "Lipseste indexul alarmei"); return; }
  int idx = server.arg("idx").toInt();
  if (idx < 0 || idx >= ALARM_COUNT) { server.send(400, "text/plain", "Index invalid"); return; }
  AlarmCfg& a = alarms[idx];
  bool wasEnabled = a.enabled;
  bool timeChanged = false;

  if (server.hasArg("hour")) {
    int v = server.arg("hour").toInt();
    if (v < 0 || v > 23) { server.send(400, "text/plain", "Ora invalida"); return; }
    if ((uint8_t)v != a.hour) timeChanged = true;
    a.hour = (uint8_t)v;
  }
  if (server.hasArg("minute")) {
    int v = server.arg("minute").toInt();
    if (v < 0 || v > 59) { server.send(400, "text/plain", "Minut invalid"); return; }
    if ((uint8_t)v != a.minute) timeChanged = true;
    a.minute = (uint8_t)v;
  }
  if (server.hasArg("days")) {
    int v = server.arg("days").toInt();
    if (v < 0 || v > 127) { server.send(400, "text/plain", "Zile invalide"); return; }
    if ((uint8_t)v != a.days) timeChanged = true;
    a.days = (uint8_t)v;
  }
  if (server.hasArg("enabled")) a.enabled = (server.arg("enabled") == "1");
  if (server.hasArg("present")) a.present = (server.arg("present") == "1");
  if (server.hasArg("tone")) {
    String t = server.arg("tone");
    if (t.length() > 0 && t.length() < sizeof(a.tone)) t.toCharArray(a.tone, sizeof(a.tone));
  }

  // The latch has no meaning once the time it was latched against has changed,
  // and none either for an alarm that was off. Setting it when the new time is
  // the minute it already is happens on purpose: entering 07:30 while it is
  // 07:30 should not set the thing off under your fingers. It rings next time.
  if (timeChanged || (!wasEnabled && a.enabled)) {
    // A newly added alarm has no history either; the same suppression applies,
    // so adding one at 07:30 while it is 07:30 does not go off under your hand.
    alarmFired[idx] = false;
    struct tm ti;
    if (readLocalTime(ti) && ti.tm_hour == a.hour && ti.tm_min == a.minute &&
        (a.days & ALARM_WDAY_BIT(ti.tm_wday))) {
      alarmFired[idx] = true;
    }
  }
  // Switching off the alarm that is ringing stops it, and so does deleting it.
  // Changing its tone or its days does not: it is still the alarm that woke you.
  if (alarmRinging && alarmRingingIdx == idx && (!a.enabled || !a.present)) alarmStopRinging();

  // Alarm tones run for seconds rather than the fraction of a second an event
  // beep does, so there is a way to hear one before committing to being woken
  // by it. Never while an alarm is actually ringing - that queue is spoken for.
  if (server.arg("preview") == "1" && buzzerOn && !alarmRinging) {
    nbStop();
    alarmEnqueueTone(a.tone);
  }

  saveSettingsDeferred();
  server.send(200, "text/plain", "OK");
}

void handleAlarmStop() {
  if (!checkAuth()) return;
  alarmStopRinging();
  server.send(200, "text/plain", "OK");
}

// Polled while the Tile Manager is open, so it stays small. The countdown is
// computed here rather than in the browser because the alarm fires on this
// clock, and the phone looking at the page may not be on the same one.
void handleAlarmState() {
  if (!checkAuth()) return;
  int nextIdx = -1;
  int mins = alarmMinutesUntilNext(&nextIdx);
  String j = "{\"ringing\":";
  j += (alarmRinging ? "true" : "false");
  j += ",\"idx\":";
  j += String((int)alarmRingingIdx);
  j += ",\"nextMin\":";
  j += String(mins);
  j += ",\"nextIdx\":";
  j += String(nextIdx);
  j += ",\"nextTime\":\"";
  if (nextIdx >= 0) {
    char t[8];
    snprintf(t, sizeof(t), "%02u:%02u", (unsigned)alarms[nextIdx].hour, (unsigned)alarms[nextIdx].minute);
    j += t;
  }
  j += "\"}";
  server.send(200, "application/json", j);
}

void handleTimerState() {
  if (!checkAuth()) return;
  timerCheckExpiry();
  char txt[16];
  timerFormatText(txt, sizeof(txt));
  String json = "{\"running\":" + String(timerRunning ? "true" : "false") +
                ",\"finished\":" + String(timerFinished ? "true" : "false") +
                ",\"remainingSec\":" + String(timerGetRemainingSec()) +
                ",\"durationSec\":" + String(timerDurationSec) +
                ",\"text\":\"" + String(txt) + "\"}";
  server.send(200, "application/json", json);
}


void handlePriorityOrder() {
  if (!checkAuth()) return;
  if (!server.hasArg("order")) {
    server.send(400, "text/plain", "Lipseste order");
    return;
  }
  String ord = server.arg("order");
  uint8_t newOrder[NUM_PRIORITY_IDS];
  int count = 0;
  bool sawNowPlaying = false;
  bool sawStopwatch = false;
  bool sawTimer = false;
  bool sawWeb = false;
  bool sawAlarm = false;
  int startIdx = 0;
  for (int i = 0; i <= (int)ord.length() && count < NUM_PRIORITY_IDS; i++) {
    if (i == (int)ord.length() || ord[i] == ',') {
      String tok = ord.substring(startIdx, i);
      tok.trim();
      if (tok == "notif")           newOrder[count++] = PRIORITY_ID_NOTIF;
      else if (tok == "ets2")       newOrder[count++] = PRIORITY_ID_ETS2;
      else if (tok == "nowplaying") { newOrder[count++] = PRIORITY_ID_NOWPLAYING; sawNowPlaying = true; }
      else if (tok == "stopwatch")  { newOrder[count++] = PRIORITY_ID_STOPWATCH; sawStopwatch = true; }
      else if (tok == "timer")      { newOrder[count++] = PRIORITY_ID_TIMER; sawTimer = true; }
      else if (tok == "webaccess")  { newOrder[count++] = PRIORITY_ID_WEB; sawWeb = true; }
      else if (tok == "alarm")      { newOrder[count++] = PRIORITY_ID_ALARM; sawAlarm = true; }
      startIdx = i + 1;
    }
  }

  // A client cached from before the alarm existed leaves it out of the order.
  // Filled in first, and at the top rather than the bottom, for the reason the
  // default order has it there.
  if (!sawAlarm && count < NUM_PRIORITY_IDS) {
    for (int i = count; i > 0; i--) newOrder[i] = newOrder[i - 1];
    newOrder[0] = PRIORITY_ID_ALARM;
    count++;
  }

  if (!sawNowPlaying && count < NUM_PRIORITY_IDS) {
    newOrder[count++] = PRIORITY_ID_NOWPLAYING;
  }

  if (!sawStopwatch && count < NUM_PRIORITY_IDS) {
    newOrder[count++] = PRIORITY_ID_STOPWATCH;
  }

  if (!sawTimer && count < NUM_PRIORITY_IDS) {
    newOrder[count++] = PRIORITY_ID_TIMER;
  }

  if (!sawWeb && count < NUM_PRIORITY_IDS) {
    newOrder[count++] = PRIORITY_ID_WEB;
  }
  if (count != NUM_PRIORITY_IDS) {
    server.send(400, "text/plain", "Ordine invalida");
    return;
  }
  for (int i = 0; i < NUM_PRIORITY_IDS; i++) priorityOrder[i] = newOrder[i];
  // nowPlayingIsPriority used to be inferred purely from whether "nowplaying"
  // appeared in the submitted order string. That made it collateral damage of
  // ANY priority-list save that didn't happen to include nowplaying (e.g. the
  // client's hardcoded default priorityItems before /state has loaded, or a
  // stale client copy left over from a failed /state fetch) - such a save
  // would silently flip Now Playing back into Circuit Tiles even though the
  // user never touched it. The client now sends an explicit npPriority flag
  // reflecting its actual current UI state; fall back to the old inference
  // only if an older/other client omits it, for backward compatibility.
  if (server.hasArg("npPriority")) {
    nowPlayingIsPriority = server.arg("npPriority") == "1";
  } else {
    nowPlayingIsPriority = sawNowPlaying;
  }
  if (!nowPlayingIsPriority) npPriorityWasActive = false;

  int notifRank = -1, ets2Rank = -1;
  for (int i = 0; i < NUM_PRIORITY_IDS; i++) {
    if (priorityOrder[i] == PRIORITY_ID_NOTIF) notifRank = i;
    if (priorityOrder[i] == PRIORITY_ID_ETS2)  ets2Rank  = i;
  }
  ets2OrderFirst = (ets2Rank < notifRank);
  saveSettingsDeferred();
  server.send(200, "text/plain", "OK");
}

void handleHourFormat() {
  if (!checkAuth()) return;
  if (server.hasArg("leadzero")) {
    hourLeadingZero = (server.arg("leadzero") == "1");
    saveSettingsDeferred();
    // The hour tile redraws itself every 200 ms, so it picks this up on its own.
    server.send(200, "text/plain", "OK");
    return;
  }
  if (server.hasArg("fmt")) {
    uint8_t f = (uint8_t)server.arg("fmt").toInt();
    if (f <= 1) {
      hourFormat = f;
      saveSettingsDeferred();
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste fmt");
  }
}

// Takes any of fmt / leadzero / barmode / barpos / swap. The tile redraws itself every
// 200 ms, so the panel picks the change up on its own.
void handleHourWeekSett() {
  if (!checkAuth()) return;
  bool any = false;
  if (server.hasArg("fmt"))      { int v = server.arg("fmt").toInt();     if (v == 0 || v == 1) { hwFormat  = (uint8_t)v; any = true; } }
  if (server.hasArg("leadzero")) { hwLeadZero = (server.arg("leadzero") == "1"); any = true; }
  if (server.hasArg("barmode"))  { int v = server.arg("barmode").toInt(); if (v == 0 || v == 1) { hwBarMode = (uint8_t)v; any = true; } }
  if (server.hasArg("barpos"))   { int v = server.arg("barpos").toInt();  if (v == 0 || v == 1) { hwBarPos  = (uint8_t)v; any = true; } }
  if (server.hasArg("swap"))     { hwSwap = (server.arg("swap") == "1"); any = true; }
  if (!any) {
    server.send(400, "text/plain", "Lipsesc setarile");
    return;
  }
  saveSettingsDeferred();
  server.send(200, "text/plain", "OK");
}

void handleDateFormat() {
  if (!checkAuth()) return;
  if (server.hasArg("fmt")) {
    uint8_t f = (uint8_t)server.arg("fmt").toInt();
    if (f <= 5) {
      dateFormat = f;
      saveSettingsDeferred();

      if (items[currentSlot].id == ITEM_DATE) {
        dateInit();
      }
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste fmt");
  }
}

void handleDateLang() {
  if (!checkAuth()) return;
  if (server.hasArg("lang")) {
    uint8_t l = (uint8_t)server.arg("lang").toInt();
    if (l <= 1) {
      dateLang = l;
      saveSettingsDeferred();

      if (items[currentSlot].id == ITEM_DATE) {
        dateInit();
      }
      server.send(200, "text/plain", "OK");
      return;
    }
  }
  server.send(400, "text/plain", "Parametru invalid");
}

void handleDateCustomFmt() {
  if (!checkAuth()) return;
  if (!server.hasArg("pattern")) {
    server.send(400, "text/plain", "Lipseste pattern");
    return;
  }
  String p = server.arg("pattern");
  p.toUpperCase();
  p.trim();
  String err;
  if (!dateValidateCustomFormat(p.c_str(), &err)) {
    server.send(400, "text/plain", err);
    return;
  }
  p.toCharArray(customDateFmt, sizeof(customDateFmt));
  dateFormat = 5;
  saveSettingsDeferred();

  if (items[currentSlot].id == ITEM_DATE) {
    dateInit();
  }
  server.send(200, "text/plain", "OK");
}

void handleTempUnit() {
  if (!checkAuth()) return;
  if (server.hasArg("unit")) {
    uint8_t u = (uint8_t)server.arg("unit").toInt();
    if (u <= 1) {
      tempUnit = u;
      saveSettingsDeferred();
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste unit");
  }
}

// WIFI MANAGEMENT
void startProvisionMode() {
  Serial.println("==> Pornire mod Provisioning");
  provisionMode = true;
  // Nothing here will ever reach an NTP server, so if the clock has never been
  // set it never will be while we are in this mode. Give it a plausible date:
  // every getLocalTime() below returns straight away instead of spinning, and
  // session cookies stay verifiable. A clock that is already right is left
  // alone - seedManualClock() only fills in an invalid one.
  seedManualClock();
  // Same reason: the clock has just been made valid, so the schedule can be
  // applied before the access point badge goes up.
  applyBrightness();
  WiFi.disconnect(true);
  delay(200);
  // With AP as the start mode the badge is an announcement and the spinner
  // covers the handover to the tile loop. Otherwise the badge IS the screen
  // from here on, so it goes up once and stays - no hold, no spinner, none of
  // the badge/spinner/badge flicker that bought nothing.
  const bool apIsHome = (defaultStartMode == START_MODE_AP);
  drawApScreen();
  if (apIsHome) {
    delay(AP_SCREEN_HOLD_MS);
    // softAP() and the route table below both block, so the spinner turns
    // either side of them rather than through them - same as the Wi-Fi path,
    // where the frames only advance between WiFi.status() polls.
    loadAnimWait(400);
  }
  WiFi.mode(WIFI_AP);
  String apSsid = getApSsid();
  String apPass = getApPass();
  WiFi.softAP(apSsid.c_str(), apPass.length() > 0 ? apPass.c_str() : "");
  if (apIsHome) loadAnimWait(400);
  Serial.print("AP pornit: ");
  Serial.print(apSsid);
  Serial.print(" - IP: ");
  Serial.println(WiFi.softAPIP());
  server.on("/",           HTTP_GET,  handleRoot);
  server.on("/scan",       HTTP_GET,  handleScan);
  server.on("/connect",    HTTP_POST, handleConnect);
  server.on("/settings",   HTTP_POST, handleSettings);
  server.on("/restorebackup", HTTP_POST, handleBackupRestore);
  server.on("/factoryreset", HTTP_POST, handleFactoryReset);
  server.on("/brightness",  HTTP_POST, handleBrightness);
  server.on("/buzzersett", HTTP_POST, handleBuzzerSett);
  server.on("/state",      HTTP_GET,  handleState);
  server.on("/hwstate",    HTTP_GET,  handleHwState);
  server.on("/hwreset",    HTTP_POST, handleHwReset);
  server.on("/livetile",   HTTP_GET,  handleLiveTile);
  server.on("/getscreen",  HTTP_GET,  handleGetScreen);
  server.on("/screensub",  HTTP_GET,  handleScreenSub);
  server.on("/tilehidden", HTTP_POST, handleTileHidden);
  server.on("/socialsett", HTTP_POST, handleSocialSett);
  server.on("/socialkey",  HTTP_GET,  handleSocialKey);
  server.on("/nowplaying", HTTP_POST, handleNowPlaying);
  server.on("/npstate",    HTTP_GET,  handleNpState);
  server.on("/npmode",     HTTP_POST, handleNpMode);
  server.on("/screensaversett", HTTP_POST, handleScreensaverSett);
  server.on("/tempunit",   HTTP_POST, handleTempUnit);
  server.on("/hourformat",  HTTP_POST, handleHourFormat);
  server.on("/hwsett",      HTTP_POST, handleHourWeekSett);
  server.on("/dateformat",  HTTP_POST, handleDateFormat);
  server.on("/datelang",    HTTP_POST, handleDateLang);
  server.on("/datecustomfmt", HTTP_POST, handleDateCustomFmt);
  server.on("/weathersett",  HTTP_POST, handleWeatherSett);
  server.on("/weathersearch", HTTP_GET,  handleWeatherSearch);
  server.on("/weatherstate", HTTP_GET,  handleWeatherState);
  server.on("/weatherkey",   HTTP_GET,  handleWeatherKey);
  server.on("/wxlang",       HTTP_POST, handleWxLang);
  server.on("/currencysett",  HTTP_POST, handleCurrencySett);
  server.on("/currencystate", HTTP_GET,  handleCurrencyState);
  server.on("/notification", HTTP_POST, handleNotification);
  server.on("/notiftoggle",  HTTP_POST, handleNotifToggle);
  server.on("/webtoggle",    HTTP_POST, handleWebToggle);
  server.on("/notifstate",   HTTP_GET,  handleNotifState);
  server.on("/ets2speed",    HTTP_POST, handleEts2Speed);
  server.on("/ets2toggle",   HTTP_POST, handleEts2Toggle);
  server.on("/ets2state",    HTTP_GET,  handleEts2State);
  server.on("/ets2order",    HTTP_POST, handleEts2Order);
  server.on("/priorityorder", HTTP_POST, handlePriorityOrder);
  server.on("/swstart",       HTTP_POST, handleSwStart);
  server.on("/swstop",        HTTP_POST, handleSwStop);
  server.on("/swstate",       HTTP_GET,  handleSwState);
  server.on("/timersett",     HTTP_POST, handleTimerSett);
  server.on("/timerstart",    HTTP_POST, handleTimerStart);
  server.on("/timerpause",    HTTP_POST, handleTimerPause);
  server.on("/timerstate",    HTTP_GET,  handleTimerState);
  server.on("/alarmsett",     HTTP_POST, handleAlarmSett);
  server.on("/alarmstop",     HTTP_POST, handleAlarmStop);
  server.on("/alarmstate",    HTTP_GET,  handleAlarmState);
  server.on("/mementosett",   HTTP_POST, handleMementoSett);
  server.on("/canvassett",    HTTP_POST, handleCanvasSett);
  server.on("/eventsoundsett", HTTP_POST, handleEventSoundSett);
  server.on("/touchsett",     HTTP_POST, handleTouchSett);
  server.on("/accentsett",    HTTP_POST, handleAccentSett);
  server.on("/startap",       HTTP_POST, handleStartAp);
  server.on("/apstate",       HTTP_GET,  handleApState);
  server.on("/wifiinfo",      HTTP_GET,  handleWifiInfo);
  server.on("/power",         HTTP_POST, handlePower);
  server.on("/apsett",        HTTP_POST, handleApSett);
  server.on("/timesett",      HTTP_POST, handleTimeSett);
  server.on("/appass",        HTTP_GET,  handleApPass);
  server.on("/stopap",        HTTP_POST, handleStopAp);
  // AUTH
  server.on("/authstate",  HTTP_GET,  handleAuthState);
  server.on("/authsetup",  HTTP_POST, handleAuthSetup);
  server.on("/login",      HTTP_POST, handleLogin);
  server.on("/logout",     HTTP_POST, handleLogout);
  server.on("/updateaccount", HTTP_POST, handleUpdateAccount);
  server.on("/whoami",     HTTP_GET,  handleWhoami);
  server.on("/dashboard",  HTTP_GET,  handleDashboard);
  server.on("/swupdateupload", HTTP_POST, handleSwUpdateUploadDone, handleSwUpdateUpload);
  const char* hdrs[] = {"Cookie", "If-None-Match"};
  server.collectHeaders(hdrs, 2);
  server.begin();

  if (apIsHome) {
    loadAnimWait(400);
    // The tile loop takes the panel from here. Hand the current slot a fresh
    // timer rather than let it inherit a stale one from before the switch and
    // flip straight away.
    slotStartMs = millis();
  }
}

// Switching the radio from inside an HTTP handler tears the interface down
// while handleClient() is still on the stack holding a socket on it. Flag the
// switch and let loop() perform it once the response has gone out.
// 0 = nothing pending, 1 = go to AP, 2 = go back to Wi-Fi.
static uint8_t pendingModeSwitch = 0;

void handleStartAp() {
  if (!checkAuth()) return;
  server.send(200, "text/plain", "ok");
  pendingModeSwitch = 1;
}

// What the station connection actually is. There is no call on this chip that
// reports the rate a frame just went out at, so "speed" here is the ceiling of
// the mode that was negotiated - said as much in the interface, next to the
// RSSI, which is the number that actually moves.
void handleWifiInfo() {
  if (!checkAuth()) return;
  bool conn = (WiFi.status() == WL_CONNECTED);
  String j = "{\"connected\":";
  j += (conn ? "true" : "false");
  if (conn) {
    j += ",\"ssid\":\"";
    j += jsonEscape(WiFi.SSID());
    j += "\",\"bssid\":\"";
    j += WiFi.BSSIDstr();
    j += "\",\"rssi\":";
    j += String(WiFi.RSSI());
    j += ",\"channel\":";
    j += String(WiFi.channel());
    j += ",\"ip\":\"";
    j += WiFi.localIP().toString();
    j += "\",\"gw\":\"";
    j += WiFi.gatewayIP().toString();
    j += "\",\"mask\":\"";
    j += WiFi.subnetMask().toString();
    j += "\",\"dns\":\"";
    j += WiFi.dnsIP().toString();
    j += "\",\"mac\":\"";
    j += WiFi.macAddress();
    j += "\"";
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
      const char* phy = ap.phy_11ax ? "Wi-Fi 6 (802.11ax)"
                      : ap.phy_11n  ? "Wi-Fi 4 (802.11n)"
                      : ap.phy_11g  ? "802.11g"
                      : ap.phy_11b  ? "802.11b"
                                    : "802.11";
      bool ht40 = (ap.bandwidth == WIFI_BW_HT40);
      // One spatial stream, which is all this radio has. HT20 tops out at 72,
      // HT40 at 150; g and b have no channel width to speak of.
      int mbps = ap.phy_11n ? (ht40 ? 150 : 72) : ap.phy_11g ? 54 : ap.phy_11b ? 11 : 0;
      j += ",\"phy\":\"";
      j += phy;
      j += "\",\"bw\":";
      j += String(ht40 ? 40 : 20);
      j += ",\"maxMbps\":";
      j += String(mbps);
    }
  }
  j += "}";
  server.send(200, "application/json", j);
}

// The three power options. Each answers before it acts, because acting means
// the connection this request came in on is about to go away.
void handlePower() {
  if (!checkAuth()) return;
  String a = server.arg("action");
  if (a == "sleep") {
    sleepEnter();
    server.send(200, "text/plain", "OK");
    return;
  }
  if (a == "wake") {
    sleepWake();
    server.send(200, "text/plain", "OK");
    return;
  }
  if (a == "reboot") {
    server.send(200, "text/plain", "OK");
    saveSettingsFlush();
    delay(500);
    ESP.restart();
    return;
  }
  if (a == "off") {
    // Checked before answering, not after: going dark with no armed wake source
    // would leave the USB cable as the only way to switch the clock back on.
    if (esp_sleep_enable_ext0_wakeup((gpio_num_t)TOUCH_PIN, 1) != ESP_OK) {
      server.send(500, "text/plain", "Senzorul tactil nu poate porni ceasul inapoi");
      return;
    }
    server.send(200, "text/plain", "OK");
    delay(500);
    powerOffNow();
    return;
  }
  server.send(400, "text/plain", "Actiune necunoscuta");
}

void handleApState() {
  if (!checkAuth()) return;
  String ssid = getApSsid();
  String pass = getApPass();
  // The time block rides along with the AP state because that is the one screen
  // showing it - one request instead of two every time the screen opens.
  struct tm lt;
  char nowBuf[24] = "";
  if (readLocalTime(lt)) strftime(nowBuf, sizeof(nowBuf), "%Y-%m-%dT%H:%M:%S", &lt);
  String json = "{\"ssid\":\"" + ssid + "\"" +
                ",\"hasPass\":" + String(pass.length() > 0 ? "true" : "false") +
                ",\"passLen\":" + String(pass.length()) +
                ",\"netTime\":" + String(netTimeSync ? "true" : "false") +
                ",\"devTime\":\"" + String(nowBuf) + "\"" +
                ",\"timeValid\":" + String(TIME_LOOKS_VALID(time(nullptr)) ? "true" : "false") +
                ",\"mode\":\"" + String(provisionMode ? "ap" : "sta") + "\"}";
  server.send(200, "application/json", json);
}

void handleApPass() {
  if (!checkAuth()) return;
  String json = "{\"pass\":\"" + getApPass() + "\"}";
  server.send(200, "application/json", json);
}

void handleStopAp() {
  if (!checkAuth()) return;
  prefs.begin("wifi", false);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();
  if (ssid.length() == 0) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"Nicio retea WiFi salvata\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
  pendingModeSwitch = 2;
}

void handleTimeSett() {
  if (!checkAuth()) return;

  if (server.hasArg("netsync")) {
    bool on = (server.arg("netsync") == "1");
    if (on != netTimeSync) {
      netTimeSync = on;
      saveSettingsDeferred();
      if (on) {
        // Go and fetch the real time now rather than making the user reboot.
        if (WiFi.status() == WL_CONNECTED) {
          configTime(0, 0, "pool.ntp.org", "time.nist.gov");
          setenv("TZ", "UTC0", 1); tzset();
          tzDetectAsync();
        }
      } else {
        stopNtp();
        seedManualClock();
      }
    }
  }

  if (server.hasArg("y")) {
    if (netTimeSync) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"err\":\"Opriti mai intai preluarea orei de pe retea\"}");
      return;
    }
    if (!applyManualTime(server.arg("y").toInt(),  server.arg("mo").toInt(),
                         server.arg("d").toInt(),  server.arg("h").toInt(),
                         server.arg("mi").toInt(),
                         server.hasArg("s") ? server.arg("s").toInt() : 0)) {
      server.send(400, "application/json",
                  "{\"ok\":false,\"err\":\"Data sau ora invalida\"}");
      return;
    }
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

void handleApSett() {
  if (!checkAuth()) return;
  if (!server.hasArg("ssid")) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"Lipseste SSID\"}");
    return;
  }
  String newSsid = server.arg("ssid");
  newSsid.trim();
  if (newSsid.length() == 0) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"SSID-ul nu poate fi gol\"}");
    return;
  }
  if (newSsid.length() > 32) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"SSID prea lung (max 32 caractere)\"}");
    return;
  }
  String newPass = server.hasArg("pass") ? server.arg("pass") : getApPass();
  if (newPass.length() > 0 && newPass.length() < 8) {
    server.send(400, "application/json", "{\"ok\":false,\"err\":\"Parola trebuie sa aiba minim 8 caractere (sau lasata goala)\"}");
    return;
  }
  prefs.begin("apcfg", false);
  prefs.putString("ssid", newSsid);
  gWifiSsidCached = newSsid;
  prefs.putString("pass", newPass);
  prefs.end();

  if (provisionMode) {
    WiFi.softAPdisconnect(true);
    delay(150);
    WiFi.softAP(newSsid.c_str(), newPass.length() > 0 ? newPass.c_str() : "");
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void stopProvisionMode() {
  Serial.println("==> Oprire mod Provisioning");
  server.stop();
  WiFi.softAPdisconnect(true);
  delay(200);
  provisionMode = false;
}

bool connectToWiFi(const char* ssid, const char* pass) {
  if (strlen(ssid) == 0) return false;

  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);

  // In the font picked under Tile Manager -> Font Type, like the tiles, and
  // kept clear of the logo in columns 24..31.
  uint8_t lbl[24]; int lblN = 0;
  for (const char* p = "Wifi"; *p; p++) appendGlyphAuto(lbl, lblN, sizeof(lbl), *p, fontType);
  for (int i = 0; i < lblN; i++) mx.setColumn(31 - i, lbl[i]);

  for (int col = 0; col < 8; col++) {
    uint8_t colVal = 0;
    for (int row = 0; row < 8; row++) {
      if (wifiLogo[row] & (0x80 >> col)) colVal |= (1 << row);
    }
    mx.setColumn(31 - (24 + col), colVal);
  }
  mxCommit();
  Serial.printf("Conectare la: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  // Hold the Wi-Fi logo for the first slice of the wait, then hand over to the
  // spinner, which runs for as long as the connection actually takes.
  const int logoHold = (WIFI_CONNECT_ATTEMPTS * LOAD_LOGO_HOLD_PCT) / 100;
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_ATTEMPTS) {
    if (attempts < logoHold) delay(500);   // logo stays on screen
    else                     loadAnimWait(500);
    Serial.print("."); attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectat! IP: " + WiFi.localIP().toString());

    server.on("/",           HTTP_GET,  handleRoot);
    server.on("/scan",       HTTP_GET,  handleScan);
    server.on("/connect",    HTTP_POST, handleConnect);
    server.on("/settings",   HTTP_POST, handleSettings);
    server.on("/restorebackup", HTTP_POST, handleBackupRestore);
    server.on("/factoryreset", HTTP_POST, handleFactoryReset);
  server.on("/brightness",  HTTP_POST, handleBrightness);
    server.on("/buzzersett", HTTP_POST, handleBuzzerSett);
    server.on("/state",      HTTP_GET,  handleState);
    server.on("/hwstate",    HTTP_GET,  handleHwState);
    server.on("/hwreset",    HTTP_POST, handleHwReset);
    server.on("/livetile",   HTTP_GET,  handleLiveTile);
    server.on("/getscreen",  HTTP_GET,  handleGetScreen);
    server.on("/screensub",  HTTP_GET,  handleScreenSub);
    server.on("/tilehidden", HTTP_POST, handleTileHidden);
    server.on("/socialsett", HTTP_POST, handleSocialSett);
    server.on("/socialkey",  HTTP_GET,  handleSocialKey);
    server.on("/nowplaying", HTTP_POST, handleNowPlaying);
    server.on("/npstate",    HTTP_GET,  handleNpState);
    server.on("/npmode",     HTTP_POST, handleNpMode);
    server.on("/screensaversett", HTTP_POST, handleScreensaverSett);
    server.on("/tempunit",   HTTP_POST, handleTempUnit);
    server.on("/hourformat",  HTTP_POST, handleHourFormat);
    server.on("/hwsett",      HTTP_POST, handleHourWeekSett);
    server.on("/dateformat",  HTTP_POST, handleDateFormat);
    server.on("/datelang",    HTTP_POST, handleDateLang);
    server.on("/datecustomfmt", HTTP_POST, handleDateCustomFmt);
    server.on("/weathersett",  HTTP_POST, handleWeatherSett);
  server.on("/weathersearch", HTTP_GET,  handleWeatherSearch);
    server.on("/weatherstate", HTTP_GET,  handleWeatherState);
    server.on("/weatherkey",   HTTP_GET,  handleWeatherKey);
  server.on("/wxlang",       HTTP_POST, handleWxLang);
  server.on("/currencysett",  HTTP_POST, handleCurrencySett);
  server.on("/currencystate", HTTP_GET,  handleCurrencyState);
    server.on("/notification", HTTP_POST, handleNotification);
    server.on("/notiftoggle",  HTTP_POST, handleNotifToggle);
    server.on("/webtoggle",    HTTP_POST, handleWebToggle);
    server.on("/notifstate",   HTTP_GET,  handleNotifState);
    server.on("/ets2speed",    HTTP_POST, handleEts2Speed);
    server.on("/ets2toggle",   HTTP_POST, handleEts2Toggle);
    server.on("/ets2state",    HTTP_GET,  handleEts2State);
    server.on("/ets2order",    HTTP_POST, handleEts2Order);
    server.on("/priorityorder", HTTP_POST, handlePriorityOrder);
    server.on("/swstart",       HTTP_POST, handleSwStart);
    server.on("/swstop",        HTTP_POST, handleSwStop);
    server.on("/swstate",       HTTP_GET,  handleSwState);
    server.on("/timersett",     HTTP_POST, handleTimerSett);
    server.on("/timerstart",    HTTP_POST, handleTimerStart);
    server.on("/timerpause",    HTTP_POST, handleTimerPause);
    server.on("/timerstate",    HTTP_GET,  handleTimerState);
    server.on("/alarmsett",     HTTP_POST, handleAlarmSett);
    server.on("/alarmstop",     HTTP_POST, handleAlarmStop);
    server.on("/alarmstate",    HTTP_GET,  handleAlarmState);
    server.on("/mementosett",   HTTP_POST, handleMementoSett);
    server.on("/canvassett",    HTTP_POST, handleCanvasSett);
    server.on("/eventsoundsett", HTTP_POST, handleEventSoundSett);
    server.on("/touchsett",     HTTP_POST, handleTouchSett);
    server.on("/accentsett",    HTTP_POST, handleAccentSett);
    server.on("/startap",       HTTP_POST, handleStartAp);
    server.on("/apstate",       HTTP_GET,  handleApState);
    server.on("/wifiinfo",      HTTP_GET,  handleWifiInfo);
    server.on("/power",         HTTP_POST, handlePower);
    server.on("/apsett",        HTTP_POST, handleApSett);
    server.on("/timesett",      HTTP_POST, handleTimeSett);
  server.on("/appass",        HTTP_GET,  handleApPass);
  server.on("/stopap",        HTTP_POST, handleStopAp);
    // AUTH
    server.on("/authstate",  HTTP_GET,  handleAuthState);
    server.on("/authsetup",  HTTP_POST, handleAuthSetup);
    server.on("/login",      HTTP_POST, handleLogin);
    server.on("/logout",     HTTP_POST, handleLogout);
    server.on("/updateaccount", HTTP_POST, handleUpdateAccount);
    server.on("/whoami",     HTTP_GET,  handleWhoami);
    server.on("/dashboard",  HTTP_GET,  handleDashboard);
    server.on("/swupdateupload", HTTP_POST, handleSwUpdateUploadDone, handleSwUpdateUpload);
    const char* hdrs2[] = {"Cookie", "If-None-Match"};
    server.collectHeaders(hdrs2, 2);
    server.begin();

    if (netTimeSync) {
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      setenv("TZ", "UTC0", 1); tzset();
      autoDetectTimezoneAnimated();
      struct tm ti_init;
      int retry = 0;
      while (!readLocalTime(ti_init) && retry < 10) { loadAnimWait(500); retry++; }
    } else {
      // Manual mode: no NTP poll and no timezone lookup, so the hand-set clock
      // stays exactly where the user put it. TZ is deliberately left alone -
      // resetting it here would shift an already-set clock on every reconnect.
      stopNtp();
      seedManualClock();
    }
    // Whichever branch ran, there is a clock now where there may not have been
    // one before - and the dim schedule is a function of the clock. Waiting for
    // loop()'s next tick would show a bright frame first.
    applyBrightness();
    slotStartMs = millis();
    return true;
  } else {
    Serial.println("\nConectare esuata!");
    P.setTextAlignment(PA_CENTER);
    P.print("ERR");
    delay(1500);
    return false;
  }
}

// POWER
//
// Sleeping only darkens the panel - the tile loop, the web server and the
// station connection all carry on, which is what lets a notification wake it.
// The MAX7219's own shutdown is what applyBrightness() already reaches for at
// level 0, so there is nothing new here to turn the display off with.
void sleepEnter() {
  if (sleepActive) return;
  sleepActive = true;
  applyBrightness();
}

void sleepWake() {
  lastActivityMs = millis();
  if (!sleepActive) return;
  sleepActive = false;
  applyBrightness();
  // No redraw: the loop never stopped writing frames, so the panel lights up on
  // whatever it was already showing rather than on a stale one.
}

// Anything that means somebody, or something, is using the clock. Called from
// the touch sensor and from every route Octoglow Connect comes in on.
void noteActivity() {
  lastActivityMs = millis();
  if (sleepActive) sleepWake();
}

static void autoSleepTick() {
  if (!autoSleepOn || autoSleepSec == 0 || sleepActive) return;
  // An alarm mid-ring is not an idle clock.
  if (alarmRinging) { lastActivityMs = millis(); return; }
  if (millis() - lastActivityMs >= (unsigned long)autoSleepSec * 1000UL) sleepEnter();
}

// True once the wake source is armed. Checked rather than assumed: without it
// the device would go to sleep with no way back short of unplugging it.
static bool armTouchWake() {
  return esp_sleep_enable_ext0_wakeup((gpio_num_t)TOUCH_PIN, 1) == ESP_OK;
}

// Deep sleep. Everything stops - no display, no WiFi, no web server - and only
// a held touch brings it back. setup() is where that hold is checked.
bool powerOffNow() {
  if (!armTouchWake()) return false;
  saveSettingsFlush();
  buzzOff();
  applyMaxBrightness(0);
  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);
  // A finger still on the pad would be read as the wake it is waiting for, and
  // the device would come straight back up.
  unsigned long t0 = millis();
  while (digitalRead(TOUCH_PIN) == HIGH && millis() - t0 < 5000UL) delay(20);
  delay(80);
  esp_deep_sleep_start();
  return true;  // never reached
}

// Called from setup() before anything is brought up. A wake that is not a
// deliberate hold goes straight back to sleep, so brushing the pad does
// nothing.
static void powerOnHoldGate() {
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) return;
  pinMode(TOUCH_PIN, INPUT);
  unsigned long t0 = millis();
  while (millis() - t0 < POWER_ON_HOLD_MS) {
    if (digitalRead(TOUCH_PIN) != HIGH) {
      // Released too early. Back to sleep - but only if the wake source arms
      // again, because a device that cannot wake is a device that is bricked
      // until somebody unplugs it. If it will not arm, carry on booting.
      if (!armTouchWake()) return;
      delay(120);
      esp_deep_sleep_start();
    }
    delay(10);
  }
}

// TOUCH

void executeTouchAction(uint8_t action) {
  // In provisioning mode the AP badge is the only thing on the panel - loop()
  // returns before any tile or IP renderer runs. Moving through the slots or
  // starting the IP display would leave a frame that nothing ever advances,
  // which is how "Show IP Address" used to freeze the display after a switch to
  // AP mode. The rest only change brightness or the buzzer, so they pass.
  if (provisionMode && (action == 1 || action == 2 || action == 8)) return;

  switch (action) {
    case 1:
      retreatSlot();
      break;
    case 2:
      advanceSlot();
      break;
    case 3:
      toggleScreenPower();
      break;
    case 4:
      touchScreenOff = false;
      curBrightness = (uint8_t)constrain((int)curBrightness + 1, 0, 16);
      applyBrightness();
      saveSettingsDeferred();
      break;
    case 5:
      touchScreenOff = false;
      curBrightness = (uint8_t)constrain((int)curBrightness - 1, 0, 16);
      applyBrightness();
      saveSettingsDeferred();
      break;
    case 6:
      buzzerOn = !buzzerOn;
      saveSettingsDeferred();
      break;
    case 7:
      saveSettingsFlush();
      ESP.restart();
      break;
    case 8:
      startIpShowFromTouch();
      break;
    default:
      break;
  }
}

void toggleScreenPower() {
  touchScreenOff = !touchScreenOff;
  applyBrightness();
}

void registerTouchTap() {
  unsigned long now = millis();
  if (tapPending && (now - lastTapReleaseMs) <= DOUBLE_TAP_WINDOW_MS) {
    tapPending = false;
    executeTouchAction(touchDoubleTapAction);
  } else {
    tapPending = true;
    lastTapReleaseMs = now;
  }
}

void checkPendingTap() {
  if (tapPending && (millis() - lastTapReleaseMs) > DOUBLE_TAP_WINDOW_MS) {
    tapPending = false;
    executeTouchAction(touchTapAction);
  }
}

void handleTouch() {
  bool touched = (digitalRead(TOUCH_PIN) == HIGH);
  if (touched && !touchActive) {
    touchActive = true; touchStartMs = millis(); touchShortFired = false;
    // A ringing alarm owns the touch sensor. Stopping it happens on the press,
    // not on the release, so it is instant - and touchShortFired swallows that
    // whole press: the configured tap action does not also run, and holding
    // the finger down afterwards does not fall into the access point switch.
    // tapPending is cleared as well, or a tap from just before the alarm rang
    // could still fire behind it. The next press behaves normally.
    if (alarmRinging) {
      alarmStopRinging();
      touchShortFired = true;
      tapPending = false;
    } else if (sleepActive) {
      // Waking is what this press is for, the same way stopping the alarm is.
      // The configured action does not also run; the next press is normal.
      sleepWake();
      touchShortFired = true;
      tapPending = false;
    } else {
      noteActivity();
    }
  } else if (!touched && touchActive) {
    if (!touchShortFired) {
      beepTouch();
      registerTouchTap();
    }
    touchActive = false; touchShortFired = false;
  }
  if (touchActive && !touchShortFired && (millis() - touchStartMs >= TOUCH_HOLD_MS)) {
    // touchActive deliberately stays true: clearing it here re-armed the
    // detector while the finger was still down, so one long press fired the
    // mode switch again a second later, and the release that followed counted
    // as a fresh tap. touchShortFired alone suppresses both until a real
    // release is seen.
    touchShortFired = true;
    if (!provisionMode) {
      startProvisionMode();
    } else {
      stopProvisionMode();
      prefs.begin("wifi", true);
      String s = prefs.getString("ssid", "");
      String p = prefs.getString("pass", "");
      prefs.end();
      connectToWiFi(s.c_str(), p.c_str());
    }
  }
  checkPendingTap();
}

// SETUP
void setup() {
  Serial.begin(115200);
  // Before anything is powered up: a wake from Power Off that was not a
  // deliberate hold goes straight back to sleep.
  powerOnHoldGate();
  loadAuth();
  pinMode(TOUCH_PIN, INPUT);
  buzzOff();
  buzzStartTask();
  Wire.begin(BMP_SDA, BMP_SCL);
  mx.begin();
  P.begin();
  if (!bmpInitSensor()) {
    // Not fatal any more: bmpHealthTick() keeps retrying while the clock runs,
    // so a sensor that lost the power-up race comes back on its own instead of
    // needing a reboot.
    Serial.println("BMP280 negasit - se va reincerca automat");
  } else {
    delay(50);
    tempSampleTick();
    pressureSampleTick();
  }
  loadSettings();
  // Before WiFi, so the fallback is in place even when the board never leaves
  // provisioning mode and connectToWiFi() is never reached.
  if (!netTimeSync) seedManualClock();
  // applyBrightness(), not applyMaxBrightness(curBrightness): the raw level
  // ignores the dim schedule, so everything drawn between here and the first
  // pass through loop() - the whole connection sequence - used to run at full
  // brightness however late at night it was. In manual-time mode the clock was
  // just seeded above, so the schedule already has a time to work from.
  applyBrightness();
  refreshWifiSsidCache();
  prefs.begin("wifi", true);
  String savedSSID = prefs.getString("ssid", "");
  String savedPass = prefs.getString("pass", "");
  prefs.end();
  // Default Start Mode decides what comes up after a power cut. The Wi-Fi path
  // still falls back to the access point when there is nothing to connect to.
  if (defaultStartMode == START_MODE_AP) {
    startProvisionMode();
  } else if (savedSSID.length() > 0) {
    if (!connectToWiFi(savedSSID.c_str(), savedPass.c_str())) startProvisionMode();
  } else {
    startProvisionMode();
  }

  currentSlot = 0;
  if (!items[currentSlot].enabled) {
    int next = nextActiveSlot(0);
    if (next != -1) currentSlot = next;
  }
  slotStartMs = millis();
}

// LOOP
void applyBrightness() {
  static uint8_t lastApplied = 255;
  uint8_t target;
  if (sleepActive || touchScreenOff) {
    target = 0;
  } else if (!dimAutoOn) {
    target = curBrightness;
  } else {
    struct tm ti;
    if (!readLocalTime(ti)) {
      target = curBrightness;
    } else {
      int nowMins  = ti.tm_hour * 60 + ti.tm_min;
      int fromMins = dimFromH   * 60 + dimFromM;
      int toMins   = dimToH     * 60 + dimToM;
      bool inDim;
      if (fromMins <= toMins) {
        inDim = (nowMins >= fromMins && nowMins < toMins);
      } else {
        inDim = (nowMins >= fromMins || nowMins < toMins);
      }
      target = inDim ? dimLevel : curBrightness;
    }
  }
  if (target != lastApplied) {
    applyMaxBrightness(target);
    lastApplied = target;
  }
}

// Runs from the top of loop(), so handleClient() has finished with the client
// and closed it before the interface underneath goes away.
static void doPendingModeSwitch() {
  uint8_t m = pendingModeSwitch;
  pendingModeSwitch = 0;
  if (m == 1) {
    WiFi.disconnect(true);
    delay(300);
    startProvisionMode();
  } else if (m == 2) {
    if (provisionMode) stopProvisionMode();
    prefs.begin("wifi", true);
    String s = prefs.getString("ssid", "");
    String p = prefs.getString("pass", "");
    prefs.end();
    connectToWiFi(s.c_str(), p.c_str());
  }
}

void loop() {
  hwMonitorTick();
  hwClearPeaksAfterBoot();
  screenStreamTick();
  handleTouch();

  if (pendingModeSwitch) {
    doPendingModeSwitch();
    return;
  }

  // Changing Default Start Mode while the access point is already up flips this
  // without going through startProvisionMode(), so the badge has to be put back
  // by hand - otherwise the panel keeps whatever tile frame was drawn last.
  static bool tileLoopWasSuspended = false;
  bool tileLoopNowSuspended = tileLoopSuspended();
  if (tileLoopNowSuspended && !tileLoopWasSuspended) drawApScreen();
  tileLoopWasSuspended = tileLoopNowSuspended;

  // Ahead of the early return below: an alarm goes off at its time whether or
  // not the access point happens to be up. The panel belongs to the AP badge
  // while that is showing, so in AP mode the alarm is heard rather than seen -
  // and the touch sensor still stops it.
  alarmTick();
  alarmToneTick();
  autoSleepTick();

  if (tileLoopNowSuspended) {
    // The access point badge is not a tile, so nothing in the manager is live.
    gLiveTileId = -1;
    gLivePrio   = "";
    serviceHttp();
    // settingsFlushTick() sits near the bottom of loop(), behind this return -
    // so for as long as this branch was taken, nothing changed in AP mode ever
    // reached NVS. Deferred writes have to be flushed here too.
    settingsFlushTick();
    return;
  }

  // WiFi Connection Lost detects the connected > disconnected transition.
  // There is no station connection to lose while the access point is up, and
  // wifiWasConnected starts out true - so without this the first iteration
  // after booting into AP announced a drop that never happened. Holding it
  // false also means reconnecting later reads as a clean false -> true, with no
  // spurious alert on the way back.
  if (provisionMode) {
    wifiWasConnected = false;
  } else {
    bool wifiNowConnected = (WiFi.status() == WL_CONNECTED);
    if (wifiWasConnected && !wifiNowConnected) {
      nbPlayPreset(eventSoundWifi, BZ_CAT_AUTO);
      // Something the owner would want to see, so it counts as activity.
      noteActivity();
    }
    wifiWasConnected = wifiNowConnected;
  }

  if (nowPlayingActive && (millis() - lastNowPlayingMs > NOW_PLAYING_TIMEOUT_MS)) {
    nowPlayingActive = false;
    nowPlayingBuf[0] = '\0';
    npPriorityWasActive = false;

    if (!nowPlayingIsPriority && items[currentSlot].id == ITEM_NOW_PLAYING) {
      advanceSlot();
      serviceHttp();
      if (tileLoopSuspended()) return;
      return;
    }
  }

  // Show IP Address (touch action) takes precedence over everything else; it is a
  // short, user-triggered, one-shot display and is not part of the reorderable
  // Priority Tiles order.
  if (ipTick()) {
    // Show IP Address is a touch action, not a row in the manager.
    gLiveTileId = -1;
    gLivePrio   = "";
    serviceHttp();
    if (tileLoopSuspended()) return;
    return;
  }

  // Priority Tiles: the first active tile in the configured order interrupts the normal loop
  for (int pi = 0; pi < NUM_PRIORITY_IDS; pi++) {
    bool handled = false;
    switch (priorityOrder[pi]) {
      // Both slots run the same renderer; webAccessAlertActive decides which
      // one the message on screen belongs to, so each gets its own rank.
      case PRIORITY_ID_NOTIF:      handled = !webAccessAlertActive && notifTick(); break;
      case PRIORITY_ID_WEB:        handled =  webAccessAlertActive && notifTick(); break;
      case PRIORITY_ID_ETS2:       handled = ets2Tick();       break;
      case PRIORITY_ID_NOWPLAYING: handled = npPriorityTick(); break;
      case PRIORITY_ID_STOPWATCH:  handled = swPriorityTick(); break;
      case PRIORITY_ID_TIMER:      handled = timerPriorityTick(); break;
      case PRIORITY_ID_ALARM:      handled = alarmPriorityTick(); break;
    }
    if (handled) {
      gLiveTileId = -1;
      gLivePrio   = priorityIdName(priorityOrder[pi]);
      serviceHttp();
      if (tileLoopSuspended()) return;
      return;
    }
  }

  CycleItem& cur = items[currentSlot];
  gLiveTileId = (int8_t)cur.id;
  gLivePrio   = "";

  if (cur.id == ITEM_NOW_PLAYING) {

    if (notifActive) {
      serviceHttp();
      if (tileLoopSuspended()) return;
      return;
    }
    unsigned long now = millis();
    switch (npState) {

      case NP_SHOW_START:
        break;

      case NP_PAUSE_BEFORE_RIGHT:
        if (now - npPauseStartMs >= NP_PAUSE_MS) {
          if (scrollIsWrap(scrollTypeNowPlaying)) {
            npWrapPass = 0;
            npScrollPos = 0;
            npState = NP_SCROLL_WRAP;
          } else {
            npState = NP_SCROLL_RIGHT;
          }
          npLastScrollMs = now;
        }
        break;

      case NP_SCROLL_WRAP: {
        if (now - npLastScrollMs >= spd(SPD_NOWPLAYING)) {
          npLastScrollMs = now;
          npScrollPos++;
          if (npScrollPos >= npColCount) {
            npWrapPass++;
            if (npWrapPass >= SCROLL_WRAP_PASSES) {
              advanceSlot();
              serviceHttp();
              if (tileLoopSuspended()) return;
              return;
            }
            npScrollPos = -((hideIconNowPlaying || scrollIconInBuffer(scrollTypeNowPlaying)) ? 32 : NP_TEXT_COLS);
          }
          npDrawAtPos(npScrollPos);
        }
        break;
      }

      case NP_SCROLL_RIGHT: {
        int maxPos = npColCount - ((hideIconNowPlaying || scrollIconInBuffer(scrollTypeNowPlaying)) ? 32 : NP_TEXT_COLS);
        if (maxPos <= 0) {
          npState = NP_PAUSE_SHORT;
          npPauseStartMs = now;
          break;
        }
        if (now - npLastScrollMs >= spd(SPD_NOWPLAYING)) {
          npLastScrollMs = now;
          npScrollPos++;
          npDrawAtPos(npScrollPos);
          if (npScrollPos >= maxPos) {
            npScrollPos = maxPos;
            npState = NP_PAUSE_AFTER_RIGHT;
            npPauseStartMs = now;
          }
        }
        break;
      }

      case NP_PAUSE_AFTER_RIGHT:
        if (now - npPauseStartMs >= NP_PAUSE_MS) {
          npState = NP_SCROLL_LEFT;
          npLastScrollMs = now;
        }
        break;

      case NP_SCROLL_LEFT:
        if (now - npLastScrollMs >= spd(SPD_NOWPLAYING)) {
          npLastScrollMs = now;
          npScrollPos--;
          if (npScrollPos <= 0) {
            npScrollPos = 0;
            npDrawAtPos(0);
            npState = NP_PAUSE_BEFORE_NEXT;
            npPauseStartMs = now;
          } else {
            npDrawAtPos(npScrollPos);
          }
        }
        break;

      case NP_PAUSE_BEFORE_NEXT:
        if (now - npPauseStartMs >= NP_PAUSE_MS) {
          advanceSlot();
          serviceHttp();
          if (tileLoopSuspended()) return;
          return;
        }
        break;

      case NP_PAUSE_SHORT:
        if (now - npPauseStartMs >= 4000UL) {
          advanceSlot();
          serviceHttp();
          if (tileLoopSuspended()) return;
          return;
        }
        break;
    }

    serviceHttp();
    if (tileLoopSuspended()) return;
    return;
  }

  // WEATHER SCROLL (identical to NP)
  if (cur.id == ITEM_WEATHER) {
    unsigned long now = millis();
    switch (wxState) {
      case NP_SHOW_START:
        break;
      case NP_PAUSE_BEFORE_RIGHT:
        if (now - wxPauseStartMs >= NP_PAUSE_MS) {
          if (scrollIsWrap(scrollTypeWeather)) {
            wxWrapPass = 0;
            wxScrollPos = 0;
            wxState = NP_SCROLL_WRAP;
          } else {
            wxState = NP_SCROLL_RIGHT;
          }
          wxLastScrollMs = now;
        }
        break;
      case NP_SCROLL_WRAP: {
        if (now - wxLastScrollMs >= spd(SPD_WEATHER)) {
          wxLastScrollMs = now;
          wxScrollPos++;
          if (wxScrollPos >= wxColCount) {
            wxWrapPass++;
            if (wxWrapPass >= SCROLL_WRAP_PASSES) {
              advanceSlot();
              serviceHttp();
              if (tileLoopSuspended()) return;
              return;
            }
            wxScrollPos = -((hideIconWeather || scrollIconInBuffer(scrollTypeWeather)) ? 32 : NP_TEXT_COLS);
          }
          weatherDrawAtPos(wxScrollPos);
        }
        break;
      }
      case NP_SCROLL_RIGHT: {
        int maxPos = wxColCount - ((hideIconWeather || scrollIconInBuffer(scrollTypeWeather)) ? 32 : NP_TEXT_COLS);
        if (maxPos <= 0) {
          wxState = NP_PAUSE_SHORT;
          wxPauseStartMs = now;
          break;
        }
        if (now - wxLastScrollMs >= spd(SPD_WEATHER)) {
          wxLastScrollMs = now;
          wxScrollPos++;
          weatherDrawAtPos(wxScrollPos);
          if (wxScrollPos >= maxPos) {
            wxScrollPos = maxPos;
            wxState = NP_PAUSE_AFTER_RIGHT;
            wxPauseStartMs = now;
          }
        }
        break;
      }
      case NP_PAUSE_AFTER_RIGHT:
        if (now - wxPauseStartMs >= NP_PAUSE_MS) {
          wxState = NP_SCROLL_LEFT;
          wxLastScrollMs = now;
        }
        break;
      case NP_SCROLL_LEFT:
        if (now - wxLastScrollMs >= spd(SPD_WEATHER)) {
          wxLastScrollMs = now;
          wxScrollPos--;
          if (wxScrollPos <= 0) {
            wxScrollPos = 0;
            weatherDrawAtPos(0);
            wxState = NP_PAUSE_BEFORE_NEXT;
            wxPauseStartMs = now;
          } else {
            weatherDrawAtPos(wxScrollPos);
          }
        }
        break;
      case NP_PAUSE_BEFORE_NEXT:
        if (now - wxPauseStartMs >= NP_PAUSE_MS) {
          advanceSlot();
          serviceHttp();
          if (tileLoopSuspended()) return;
          return;
        }
        break;
      case NP_PAUSE_SHORT:
        if (now - wxPauseStartMs >= 4000UL) {
          advanceSlot();
          serviceHttp();
          if (tileLoopSuspended()) return;
          return;
        }
        break;
    }
    serviceHttp();
    if (tileLoopSuspended()) return;
    return;
  }

  // CURRENCY STANDARDS 
  if (cur.id == ITEM_CURRENCY) {
    unsigned long now = millis();
    switch (currState) {
      case NP_SHOW_START:
        break;
      case NP_PAUSE_BEFORE_RIGHT:
        if (now - currPauseMs >= NP_PAUSE_MS) {
          if (scrollIsWrap(scrollTypeCurrency)) {
            currWrapPass = 0;
            currScrollPos = 0;
            currState = NP_SCROLL_WRAP;
          } else {
            currState = NP_SCROLL_RIGHT;
          }
          currLastScrollMs = now;
        }
        break;
      case NP_SCROLL_WRAP: {
        if (now - currLastScrollMs >= spd(SPD_CURRENCY)) {
          currLastScrollMs = now;
          currScrollPos++;
          if (currScrollPos >= currColCount) {
            currWrapPass++;
            if (currWrapPass >= SCROLL_WRAP_PASSES) {
              advanceSlot();
              serviceHttp();
              if (tileLoopSuspended()) return;
              return;
            }
            currScrollPos = -((hideIconCurrency || scrollIconInBuffer(scrollTypeCurrency)) ? 32 : PRESSURE_TEXT_COLS);
          }
          currencyDrawAtPos(currScrollPos);
        }
        break;
      }
      case NP_SCROLL_RIGHT: {
        int maxPos = currColCount - ((hideIconCurrency || scrollIconInBuffer(scrollTypeCurrency)) ? 32 : PRESSURE_TEXT_COLS);
        if (maxPos <= 0) {
          currState = NP_PAUSE_SHORT;
          currPauseMs = now;
          break;
        }
        if (now - currLastScrollMs >= spd(SPD_CURRENCY)) {
          currLastScrollMs = now;
          currScrollPos++;
          currencyDrawAtPos(currScrollPos);
          if (currScrollPos >= maxPos) {
            currScrollPos = maxPos;
            currState = NP_PAUSE_AFTER_RIGHT;
            currPauseMs = now;
          }
        }
        break;
      }
      case NP_PAUSE_AFTER_RIGHT:
        if (now - currPauseMs >= NP_PAUSE_MS) {
          currState = NP_SCROLL_LEFT;
          currLastScrollMs = now;
        }
        break;
      case NP_SCROLL_LEFT:
        if (now - currLastScrollMs >= spd(SPD_CURRENCY)) {
          currLastScrollMs = now;
          currScrollPos--;
          if (currScrollPos <= 0) {
            currScrollPos = 0;
            currencyDrawAtPos(0);
            currState = NP_PAUSE_BEFORE_NEXT;
            currPauseMs = now;
          } else {
            currencyDrawAtPos(currScrollPos);
          }
        }
        break;
      case NP_PAUSE_BEFORE_NEXT:
        if (now - currPauseMs >= NP_PAUSE_MS) {
          advanceSlot();
          serviceHttp();
          if (tileLoopSuspended()) return;
          return;
        }
        break;
      case NP_PAUSE_SHORT:
        if (now - currPauseMs >= 4000UL) {
          advanceSlot();
          serviceHttp();
          if (tileLoopSuspended()) return;
          return;
        }
        break;
    }
    serviceHttp();
    if (tileLoopSuspended()) return;
    return;
  }
  // SOCIAL COUNTER TILES - same scroll behaviour as the currency tile
  // When the text already fits, nothing below runs: the tile falls through to
  // the generic slot timer at the end of loop(), so it honours its configured
  // display time instead of the scroll machine's own pauses.
  if (socialIndexForItem(cur.id) >= 0 && socNeedsScroll) {
    unsigned long now = millis();
    switch (socState) {
      case NP_SHOW_START:
        break;
      case NP_PAUSE_BEFORE_RIGHT:
        if (now - socPauseMs >= NP_PAUSE_MS) {
          if (scrollIsWrap(scrollTypeYoutube)) {
            socWrapPass = 0;
            socScrollPos = 0;
            socState = NP_SCROLL_WRAP;
          } else {
            socState = NP_SCROLL_RIGHT;
          }
          socLastScrollMs = now;
        }
        break;
      case NP_SCROLL_WRAP: {
        if (now - socLastScrollMs >= spd(SPD_YOUTUBE)) {
          socLastScrollMs = now;
          socScrollPos++;
          if (socScrollPos >= socColCount) {
            socWrapPass++;
            if (socWrapPass >= SCROLL_WRAP_PASSES) {
              advanceSlot();
              serviceHttp();
              if (tileLoopSuspended()) return;
              return;
            }
            socScrollPos = -((hideIconYoutube || scrollIconInBuffer(scrollTypeYoutube)) ? 32 : PRESSURE_TEXT_COLS);
          }
          socialDrawAtPos(socScrollPos);
        }
        break;
      }
      case NP_SCROLL_RIGHT: {
        int maxPos = socColCount - ((hideIconYoutube || scrollIconInBuffer(scrollTypeYoutube)) ? 32 : PRESSURE_TEXT_COLS);
        if (maxPos <= 0) {
          socState = NP_PAUSE_SHORT;
          socPauseMs = now;
          break;
        }
        if (now - socLastScrollMs >= spd(SPD_YOUTUBE)) {
          socLastScrollMs = now;
          socScrollPos++;
          socialDrawAtPos(socScrollPos);
          if (socScrollPos >= maxPos) {
            socScrollPos = maxPos;
            socState = NP_PAUSE_AFTER_RIGHT;
            socPauseMs = now;
          }
        }
        break;
      }
      case NP_PAUSE_AFTER_RIGHT:
        if (now - socPauseMs >= NP_PAUSE_MS) {
          socState = NP_SCROLL_LEFT;
          socLastScrollMs = now;
        }
        break;
      case NP_SCROLL_LEFT:
        if (now - socLastScrollMs >= spd(SPD_YOUTUBE)) {
          socLastScrollMs = now;
          socScrollPos--;
          if (socScrollPos <= 0) {
            socScrollPos = 0;
            socialDrawAtPos(0);
            socState = NP_PAUSE_BEFORE_NEXT;
            socPauseMs = now;
          } else {
            socialDrawAtPos(socScrollPos);
          }
        }
        break;
      case NP_PAUSE_BEFORE_NEXT:
        if (now - socPauseMs >= NP_PAUSE_MS) {
          advanceSlot();
          serviceHttp();
          if (tileLoopSuspended()) return;
          return;
        }
        break;
      case NP_PAUSE_SHORT:
        if (now - socPauseMs >= 4000UL) {
          advanceSlot();
          serviceHttp();
          if (tileLoopSuspended()) return;
          return;
        }
        break;
    }
    serviceHttp();
    if (tileLoopSuspended()) return;
    return;
  }


  // MEMENTO 
  if (cur.id == ITEM_MEMENTO) {
    unsigned long now = millis();
    switch (mementoState) {
      case NP_SHOW_START:
        break;
      case NP_PAUSE_BEFORE_RIGHT:
        if (now - mementoPauseMs >= NP_PAUSE_MS) {
          if (scrollIsWrap(scrollTypeReminder)) {
            mementoWrapPass = 0;
            mementoScrollPos = 0;
            mementoState = NP_SCROLL_WRAP;
          } else {
            mementoState = NP_SCROLL_RIGHT;
          }
          mementoLastScrollMs = now;
        }
        break;
      case NP_SCROLL_WRAP: {
        if (now - mementoLastScrollMs >= spd(SPD_REMINDER)) {
          mementoLastScrollMs = now;
          mementoScrollPos++;
          if (mementoScrollPos >= mementoColCount) {
            mementoWrapPass++;
            if (mementoWrapPass >= SCROLL_WRAP_PASSES) {
              advanceSlot();
              serviceHttp();
              if (tileLoopSuspended()) return;
              return;
            }
            mementoScrollPos = -((hideIconReminder || scrollIconInBuffer(scrollTypeReminder)) ? 32 : NP_TEXT_COLS);
          }
          mementoDrawAtPos(mementoScrollPos);
        }
        break;
      }
      case NP_SCROLL_RIGHT: {
        int maxPos = mementoColCount - ((hideIconReminder || scrollIconInBuffer(scrollTypeReminder)) ? 32 : NP_TEXT_COLS);
        if (maxPos <= 0) {
          mementoState = NP_PAUSE_SHORT;
          mementoPauseMs = now;
          break;
        }
        if (now - mementoLastScrollMs >= spd(SPD_REMINDER)) {
          mementoLastScrollMs = now;
          mementoScrollPos++;
          mementoDrawAtPos(mementoScrollPos);
          if (mementoScrollPos >= maxPos) {
            mementoScrollPos = maxPos;
            mementoState = NP_PAUSE_AFTER_RIGHT;
            mementoPauseMs = now;
          }
        }
        break;
      }
      case NP_PAUSE_AFTER_RIGHT:
        if (now - mementoPauseMs >= NP_PAUSE_MS) {
          mementoState = NP_SCROLL_LEFT;
          mementoLastScrollMs = now;
        }
        break;
      case NP_SCROLL_LEFT:
        if (now - mementoLastScrollMs >= spd(SPD_REMINDER)) {
          mementoLastScrollMs = now;
          mementoScrollPos--;
          if (mementoScrollPos <= 0) {
            mementoScrollPos = 0;
            mementoDrawAtPos(0);
            mementoState = NP_PAUSE_BEFORE_NEXT;
            mementoPauseMs = now;
          } else {
            mementoDrawAtPos(mementoScrollPos);
          }
        }
        break;
      case NP_PAUSE_BEFORE_NEXT:
        if (now - mementoPauseMs >= NP_PAUSE_MS) {
          advanceSlot();
          serviceHttp();
          if (tileLoopSuspended()) return;
          return;
        }
        break;
      case NP_PAUSE_SHORT:
        if (now - mementoPauseMs >= 4000UL) {
          advanceSlot();
          serviceHttp();
          if (tileLoopSuspended()) return;
          return;
        }
        break;
    }
    serviceHttp();
    if (tileLoopSuspended()) return;
    return;
  }

  serviceHttp();
  if (tileLoopSuspended()) return;

  unsigned long nowMs = millis();

  static unsigned long lastBrightCheck = 0;
  if (nowMs - lastBrightCheck >= 1000UL) {
    lastBrightCheck = nowMs;
    applyBrightness();
  }

  // Both fetches below only kick off a background task and return
  // immediately (see weatherFetch()/currencyFetch() definitions) - they
  // no longer block loop() while the HTTP request is in flight.
  // Both are gated on the tile still being in the manager: a removed tile has
  // no row to show the answer on, and each refresh is an HTTPS request plus an
  // 8 KB task. The poll calls stay unconditional so a fetch already in flight
  // when the tile was removed still gets collected instead of leaking its flag.
  if (tileInManager(ITEM_WEATHER) && strlen(weatherApiKey) > 0 &&
      (lastWeatherFetch == 0 || nowMs - lastWeatherFetch >= WEATHER_FETCH_INTERVAL_MS)) {
    weatherFetch();
  }
  weatherFetchPoll();

  if (tileInManager(ITEM_CURRENCY) &&
      (lastCurrencyFetch == 0 || nowMs - lastCurrencyFetch >= CURRENCY_FETCH_INTERVAL_MS)) {
    currencyFetch();
  }
  currencyFetchPoll();

  socialTickFetch();
  socialFetchPoll();

  settingsFlushTick();
  bmpHealthTick();
  tempSampleTick();
  pressureSampleTick();

  nowMs = millis();

  switch (cur.id) {
    case ITEM_HOUR:

      if (nowMs - gLastStaticDrawMs >= 200) {
        gLastStaticDrawMs = nowMs;
        drawHour();
      }
      break;
    case ITEM_HOURWEEK:
      if (nowMs - gLastStaticDrawMs >= 200) {
        gLastStaticDrawMs = nowMs;
        drawHourWeekday();
      }
      break;
    case ITEM_DATE: {

      if (!gStaticDrawDone) {
        gStaticDrawDone = dateInit();
      }

      // dateState stays NP_SHOW_START until dateInit() starts the scroll, and
      // dateNeedsScroll survives from the previous time this tile was shown -
      // so the two can disagree. Entering the state machine on that combination
      // lands on `default:` and returns out of loop() on every pass, without
      // ever reaching the slot-duration check below: the tile could never hand
      // over. Falling through instead keeps the rotation going whatever
      // happened to the draw.
      if (dateNeedsScroll && dateState != NP_SHOW_START) {
        unsigned long now = millis();
        unsigned long halfSlot = (unsigned long)(cur.durationSec / 2) * 1000UL;
        if (halfSlot < 1000UL) halfSlot = 1000UL;
        int dateEffTextCols = (hideIconDate || scrollIconInBuffer(scrollTypeDate)) ? 32 : DATE_TEXT_COLS;
        int maxScroll = dateColCount - dateEffTextCols;
        if (maxScroll < 0) maxScroll = 0;
        switch (dateState) {
          case NP_PAUSE_BEFORE_RIGHT:

            if (now - datePauseMs >= halfSlot) {
              if (scrollIsWrap(scrollTypeDate)) {
                dateWrapPass = 0;
                dateScrollPos = 0;
                dateState = NP_SCROLL_WRAP;
              } else {
                dateState = NP_SCROLL_RIGHT;
              }
              dateLastScrollMs = now;
            }
            break;
          case NP_SCROLL_WRAP:
            if (now - dateLastScrollMs >= spd(SPD_DATE)) {
              dateLastScrollMs = now;
              dateScrollPos++;
              if (dateScrollPos >= dateColCount) {
                dateWrapPass++;
                if (dateWrapPass >= SCROLL_WRAP_PASSES) {
                  advanceSlot();
                  serviceHttp();
                  if (tileLoopSuspended()) return;
                  return;
                }
                dateScrollPos = -dateEffTextCols;
              }
              dateDrawAtPos(dateScrollPos);
            }
            break;
          case NP_SCROLL_RIGHT:
            if (now - dateLastScrollMs >= spd(SPD_DATE)) {
              dateLastScrollMs = now;
              dateScrollPos++;
              dateDrawAtPos(dateScrollPos);
              if (dateScrollPos >= maxScroll) {
                dateScrollPos = maxScroll;
                dateState = NP_PAUSE_AFTER_RIGHT;
                datePauseMs = now;
              }
            }
            break;
          case NP_PAUSE_AFTER_RIGHT:
            if (now - datePauseMs >= NP_PAUSE_MS) {
              dateState = NP_SCROLL_LEFT;
              dateLastScrollMs = now;
            }
            break;
          case NP_SCROLL_LEFT:
            if (now - dateLastScrollMs >= spd(SPD_DATE)) {
              dateLastScrollMs = now;
              dateScrollPos--;
              if (dateScrollPos <= 0) {
                dateScrollPos = 0;
                dateDrawAtPos(0);
                dateState = NP_PAUSE_BEFORE_NEXT;
                datePauseMs = now;
              } else {
                dateDrawAtPos(dateScrollPos);
              }
            }
            break;
          case NP_PAUSE_BEFORE_NEXT:

            if (now - datePauseMs >= halfSlot) {
              advanceSlot();
              serviceHttp();
              if (tileLoopSuspended()) return;
              return;
            }
            break;
          default: break;
        }

        serviceHttp();
        if (tileLoopSuspended()) return;
        return;
      }
      break;
    }
    case ITEM_TEMP: {

      if (!gStaticDrawDone) {
        float tf;
        if (bmpReadTempC(tf)) lastTemp = (int)round(tf);
        gStaticDrawDone = true;
        gTempShownValue = lastTemp;
        tempInit(lastTemp);
      } else if (gTempShownValue == TEMP_NO_READING && lastTemp != TEMP_NO_READING) {
        // The "--" placeholder is on screen and the sensor just answered.
        // Only re-init in this direction, so a normal reading changing by a
        // degree never restarts a scroll that is already running.
        gTempShownValue = lastTemp;
        tempInit(lastTemp);
      }

      if (tempNeedsScroll) {
        unsigned long now = millis();
        unsigned long halfSlot = (unsigned long)(cur.durationSec / 2) * 1000UL;
        if (halfSlot < 1000UL) halfSlot = 1000UL;
        int tempEffTextCols = (hideIconTemp || scrollIconInBuffer(scrollTypeTemp)) ? 32 : TEMP_TEXT_COLS;
        int maxScroll = tempColCount - tempEffTextCols;
        if (maxScroll < 0) maxScroll = 0;
        switch (tempState) {
          case NP_PAUSE_BEFORE_RIGHT:

            if (now - tempPauseMs >= halfSlot) {
              if (scrollIsWrap(scrollTypeTemp)) {
                tempWrapPass = 0;
                tempScrollPos = 0;
                tempState = NP_SCROLL_WRAP;
              } else {
                tempState = NP_SCROLL_RIGHT;
              }
              tempLastScrollMs = now;
            }
            break;
          case NP_SCROLL_WRAP:
            if (now - tempLastScrollMs >= spd(SPD_TEMP)) {
              tempLastScrollMs = now;
              tempScrollPos++;
              if (tempScrollPos >= tempColCount) {
                tempWrapPass++;
                if (tempWrapPass >= SCROLL_WRAP_PASSES) {
                  advanceSlot();
                  serviceHttp();
                  if (tileLoopSuspended()) return;
                  return;
                }
                tempScrollPos = -tempEffTextCols;
              }
              tempDrawAtPos(tempScrollPos);
            }
            break;
          case NP_SCROLL_RIGHT:
            if (now - tempLastScrollMs >= spd(SPD_TEMP)) {
              tempLastScrollMs = now;
              tempScrollPos++;
              tempDrawAtPos(tempScrollPos);
              if (tempScrollPos >= maxScroll) {
                tempScrollPos = maxScroll;
                tempState = NP_PAUSE_AFTER_RIGHT;
                tempPauseMs = now;
              }
            }
            break;
          case NP_PAUSE_AFTER_RIGHT:
            if (now - tempPauseMs >= NP_PAUSE_MS) {
              tempState = NP_SCROLL_LEFT;
              tempLastScrollMs = now;
            }
            break;
          case NP_SCROLL_LEFT:
            if (now - tempLastScrollMs >= spd(SPD_TEMP)) {
              tempLastScrollMs = now;
              tempScrollPos--;
              if (tempScrollPos <= 0) {
                tempScrollPos = 0;
                tempDrawAtPos(0);
                tempState = NP_PAUSE_BEFORE_NEXT;
                tempPauseMs = now;
              } else {
                tempDrawAtPos(tempScrollPos);
              }
            }
            break;
          case NP_PAUSE_BEFORE_NEXT:

            if (now - tempPauseMs >= halfSlot) {
              advanceSlot();
              serviceHttp();
              if (tileLoopSuspended()) return;
              return;
            }
            break;
          default: break;
        }

        serviceHttp();
        if (tileLoopSuspended()) return;
        return;
      }
      break;
    }
    case ITEM_PRESSURE: {

      if (!gStaticDrawDone) {
        pressureSampleTick();
        gStaticDrawDone = true;
        pressureInit((int)round(lastPressureHpa));
      }

      if (pressureNeedsScroll) {
        unsigned long now = millis();
        unsigned long halfSlot = (unsigned long)(cur.durationSec / 2) * 1000UL;
        if (halfSlot < 1000UL) halfSlot = 1000UL;
        int pressureEffTextCols = (hideIconPressure || scrollIconInBuffer(scrollTypePressure)) ? 32 : PRESSURE_TEXT_COLS;
        int maxScroll = pressureColCount - pressureEffTextCols;
        if (maxScroll < 0) maxScroll = 0;
        switch (pressureState) {
          case NP_PAUSE_BEFORE_RIGHT:
            if (now - pressurePauseMs >= halfSlot) {
              if (scrollIsWrap(scrollTypePressure)) {
                pressureWrapPass = 0;
                pressureScrollPos = 0;
                pressureState = NP_SCROLL_WRAP;
              } else {
                pressureState = NP_SCROLL_RIGHT;
              }
              pressureLastScrollMs = now;
            }
            break;
          case NP_SCROLL_WRAP:
            if (now - pressureLastScrollMs >= spd(SPD_PRESSURE)) {
              pressureLastScrollMs = now;
              pressureScrollPos++;
              if (pressureScrollPos >= pressureColCount) {
                pressureWrapPass++;
                if (pressureWrapPass >= SCROLL_WRAP_PASSES) {
                  advanceSlot();
                  serviceHttp();
                  if (tileLoopSuspended()) return;
                  return;
                }
                pressureScrollPos = -pressureEffTextCols;
              }
              pressureDrawAtPos(pressureScrollPos);
            }
            break;
          case NP_SCROLL_RIGHT:
            if (now - pressureLastScrollMs >= spd(SPD_PRESSURE)) {
              pressureLastScrollMs = now;
              pressureScrollPos++;
              pressureDrawAtPos(pressureScrollPos);
              if (pressureScrollPos >= maxScroll) {
                pressureScrollPos = maxScroll;
                pressureState = NP_PAUSE_AFTER_RIGHT;
                pressurePauseMs = now;
              }
            }
            break;
          case NP_PAUSE_AFTER_RIGHT:
            if (now - pressurePauseMs >= NP_PAUSE_MS) {
              pressureState = NP_SCROLL_LEFT;
              pressureLastScrollMs = now;
            }
            break;
          case NP_SCROLL_LEFT:
            if (now - pressureLastScrollMs >= spd(SPD_PRESSURE)) {
              pressureLastScrollMs = now;
              pressureScrollPos--;
              if (pressureScrollPos <= 0) {
                pressureScrollPos = 0;
                pressureDrawAtPos(0);
                pressureState = NP_PAUSE_BEFORE_NEXT;
                pressurePauseMs = now;
              } else {
                pressureDrawAtPos(pressureScrollPos);
              }
            }
            break;
          case NP_PAUSE_BEFORE_NEXT:
            if (now - pressurePauseMs >= halfSlot) {
              advanceSlot();
              serviceHttp();
              if (tileLoopSuspended()) return;
              return;
            }
            break;
          default: break;
        }
        serviceHttp();
        if (tileLoopSuspended()) return;
        return;
      }
      break;
    }
    case ITEM_SCREENSAVER: {

      if (!ssWaitingAdvance && (nowMs - slotStartMs >= (unsigned long)cur.durationSec * 1000UL)) {
        ssWaitingAdvance = true;
        ssWaitStartMs = nowMs;
      }
      ssTick();
      return;
    }
  }

  if (nowMs - slotStartMs >= (unsigned long)cur.durationSec * 1000UL) {
    advanceSlot();
  }
}
