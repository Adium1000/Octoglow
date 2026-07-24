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
#include "mbedtls/sha256.h"

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
void handleApPass();
void handleStopAp();
String getApSsid();
String getApPass();
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
void loadSettings();
void advanceSlot();
void retreatSlot();
int  prevActiveSlot(int from);
void enterSlot();
void registerTouchTap();
void checkPendingTap();
void executeTouchAction(uint8_t action);
void toggleScreenPower();
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
#define BUZZER_PIN 7

// AP CONFIG
#define AP_SSID "Adrian's Octoglow"
#define AP_PASS ""

// TIME

// IANA → POSIX TIMEZONE TABLE

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
    Serial.printf("[TZ] ip-api.com eroare %d — ramane TZ anterior\n", code);
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

#define ITEM_CURRENCY    11
#define NUM_ITEMS        10
#define CANVAS_COLS      32

// SCREEN SAVER animatii disponibile
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

void applyMaxBrightness(uint8_t uiLevel) {
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

#define MAX_SESSIONS 4
static char sessionTokens[MAX_SESSIONS][33];
static uint8_t sessionCount = 0;

static void initSessionPool() {
  for (int i = 0; i < MAX_SESSIONS; i++) sessionTokens[i][0] = '\0';
  sessionCount = 0;
}

static void addSession(const char* tok) {
  if (sessionCount < MAX_SESSIONS) {
    strncpy(sessionTokens[sessionCount], tok, 32);
    sessionTokens[sessionCount][32] = '\0';
    sessionCount++;
  } else {

    for (int i = 0; i < MAX_SESSIONS - 1; i++) {
      strncpy(sessionTokens[i], sessionTokens[i + 1], 32);
      sessionTokens[i][32] = '\0';
    }
    strncpy(sessionTokens[MAX_SESSIONS - 1], tok, 32);
    sessionTokens[MAX_SESSIONS - 1][32] = '\0';
  }
}

static void removeSession(const char* tok) {
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (strncmp(sessionTokens[i], tok, 32) == 0) {

      for (int j = i; j < MAX_SESSIONS - 1; j++) {
        strncpy(sessionTokens[j], sessionTokens[j + 1], 32);
        sessionTokens[j][32] = '\0';
      }
      sessionTokens[MAX_SESSIONS - 1][0] = '\0';
      if (sessionCount > 0) sessionCount--;
      return;
    }
  }
}

static bool isValidSession(const char* tok) {
  if (tok == nullptr || tok[0] == '\0') return false;
  for (int i = 0; i < MAX_SESSIONS; i++) {
    if (sessionTokens[i][0] != '\0' && strncmp(sessionTokens[i], tok, 32) == 0)
      return true;
  }
  return false;
}

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

static void genToken(char out[33]) {
  for (int i = 0; i < 16; i++) {
    uint8_t b = (uint8_t)(esp_random() & 0xFF);
    sprintf(out + i * 2, "%02x", b);
  }
  out[32] = '\0';
}

// AP CONFIG (SSID/parola configurabile de utilizator, persistente)

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
  if (sessionCount == 0) return "";
  String tok = extractCookieToken();
  if (tok.length() == 0) return "";
  if (isValidSession(tok.c_str())) return String(authUser);
  return "";
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
};

// CANVAS TILE

uint8_t canvasBitmap[CANVAS_COLS] = {0};

uint8_t       currentSlot      = 0;
unsigned long slotStartMs      = 0;
bool          inScrollAnim     = false;
bool          inFadeOut        = false;
bool          inFadeIn         = false;
int           lastTemp         = -999;

unsigned long gLastStaticDrawMs = 0;
bool          gStaticDrawDone   = false;

char     nowPlayingBuf[128]    = "";
char     nowArtist[64]         = "";
char     nowTitle[64]          = "";
unsigned long lastNowPlayingMs = 0;
bool     nowPlayingActive      = false;

uint8_t  npDisplayMode         = 0;

// NOTIFICATIONS (tile prioritar intrerupe bucla normala)
char     notifBuf[204]         = "";
bool     notifActive           = false;
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

uint8_t  scrollType            = 0;

uint8_t  scrollTypeDate         = 0;
uint8_t  scrollTypeTemp         = 0;
uint8_t  scrollTypeReminder     = 0;
uint8_t  scrollTypeWeather      = 0;
uint8_t  scrollTypeNotif        = 0;
uint8_t  scrollTypeNowPlaying   = 0;
uint8_t  scrollTypePressure     = 0;
uint8_t  scrollTypeCurrency     = 0;
uint8_t  scrollTypeStopwatch    = 0;
uint8_t  scrollTypeTimer        = 0;
static inline bool scrollIconInBuffer(uint8_t st) { return st == 2 || st == 3; }
static inline bool scrollIsWrap(uint8_t st)       { return st == 1 || st == 3; }

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

// EURO TRUCK SIMULATOR 2 (tile prioritar intrerupe bucla normala)

bool     ets2Active            = false;
bool     ets2Enabled           = true;
bool     ets2OrderFirst        = false;

// PRIORITY TILES ordine generica de precedenta

#define PRIORITY_ID_NOTIF      0
#define PRIORITY_ID_ETS2       1
#define PRIORITY_ID_NOWPLAYING 2
#define PRIORITY_ID_STOPWATCH  3
#define PRIORITY_ID_TIMER      4
#define NUM_PRIORITY_IDS       5
uint8_t  priorityOrder[NUM_PRIORITY_IDS] = { PRIORITY_ID_NOTIF, PRIORITY_ID_ETS2, PRIORITY_ID_NOWPLAYING, PRIORITY_ID_STOPWATCH, PRIORITY_ID_TIMER };
bool     nowPlayingIsPriority  = false;

bool     npPriorityWasActive   = false;
bool     timerWasActive        = false;

int      ets2Speed             = 0;
unsigned long lastEts2Ms       = 0;
#define ETS2_TIMEOUT_MS 5000

static uint8_t  ets2ColBuf[64];
static int      ets2ColCount  = 0;
static int      ets2LastSpeed = -1;

// STOPWATCH variabile de stare (implementare completa mai jos, dupa
// definirea NpState / NP_SCROLL_SPEED_MS, langa restul mecanismelor de scroll)

bool          swRunning       = false;
bool          swWasActive     = false;
unsigned long swStartMs       = 0;
unsigned long swElapsedMs     = 0;

// TIMER variabile de stare
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
    playPresetTone(eventSoundTimer);
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
char     weatherLang[8]        = "en";
float    weatherLat            = 47.6575f;
float    weatherLon            = 23.5689f;
char     weatherBuf[128]       = "";
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

static bool isNight() {
  struct tm ti;
  if (!getLocalTime(&ti)) return false;
  int h = ti.tm_hour;
  return (h >= 21 || h < 6);
}

const uint8_t* getWeatherIcon() {
  if (strcasecmp(weatherCondition, "Clear") == 0)
    return isNight() ? moonIcon : sunnyIcon;
  if (strcasecmp(weatherCondition, "Clouds") == 0)
    return cloudIcon;
  if (strcasecmp(weatherCondition, "Rain") == 0 ||
      strcasecmp(weatherCondition, "Drizzle") == 0)
    return cloudRainIcon;
  if (strcasecmp(weatherCondition, "Thunderstorm") == 0)
    return cloudLightningIcon;
  if (strcasecmp(weatherCondition, "Snow") == 0)
    return cloudSnowIcon;
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
    return windIcon;

  return cloudIcon;
}

uint8_t  tempUnit              = 0;

uint8_t  hourFormat            = 0;

uint8_t  dateFormat            = 2;

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
static uint8_t  notifColBuf[512];
static int      notifColCount  = 0;
static int      notifScrollPos = 0;
static unsigned long notifLastScrollMs = 0;
static int      notifWrapPass  = 0;
static uint8_t  wxColBuf[512];
static int      wxColCount  = 0;
static int      wxScrollPos = 0;
static unsigned long wxLastScrollMs = 0;

bool          provisionMode    = false;

bool          buzzerOn         = true;
uint8_t       buzzerVolume     = 80;
char          buzzerPreset[16] = "calm";

char          eventSoundTile[16]  = "calm";
char          eventSoundWifi[16]  = "urgent";
char          eventSoundNotif[16] = "soft";
char          eventSoundEts2[16]  = "urgent";
char          eventSoundTouch[16] = "soft";
#define ETS2_SPEED_LIMIT 130
bool          ets2SpeedAlertFired = false;
bool          wifiWasConnected    = true;

bool          touchActive      = false;
unsigned long touchStartMs     = 0;
bool          touchShortFired  = false;

uint8_t       touchTapAction       = 0;
uint8_t       touchDoubleTapAction = 0;
bool          tapPending           = false;
unsigned long lastTapReleaseMs     = 0;
bool          touchScreenOff       = false;

char displayBuf[32];

// BUZZER NON BLOCKING (pentru touch)

#define NB_QUEUE_MAX 32
struct NbNote { uint16_t freq; uint16_t dur; };
static NbNote  nbQueue[NB_QUEUE_MAX];
static uint8_t nbHead      = 0;
static uint8_t nbTail      = 0;
static bool    nbPlaying   = false;
static unsigned long nbNoteStart = 0;
static uint16_t nbNoteDur  = 0;

static void nbEnqueue(uint16_t freq, uint16_t dur) {
  uint8_t next = (nbTail + 1) % NB_QUEUE_MAX;
  if (next == nbHead) return;
  nbQueue[nbTail] = {freq, dur};
  nbTail = next;
}

void tickBuzzer() {
  if (nbHead == nbTail) {
    if (nbPlaying) { noTone(BUZZER_PIN); nbPlaying = false; }
    return;
  }
  unsigned long now = millis();
  if (!nbPlaying) {
    NbNote n = nbQueue[nbHead];
    nbHead = (nbHead + 1) % NB_QUEUE_MAX;
    if (n.freq == 0) { noTone(BUZZER_PIN); }
    else             { tone(BUZZER_PIN, n.freq, n.dur); }
    nbNoteStart = now;
    nbNoteDur   = n.dur + 5;
    nbPlaying   = true;
  } else if (now - nbNoteStart >= (unsigned long)nbNoteDur) {
    nbPlaying = false;
  }
}

static void nbPlayPreset(const char* id) {
  if (!buzzerOn) return;
  if (strcmp(id, "none") == 0) return;
  nbHead = nbTail = 0; nbPlaying = false; noTone(BUZZER_PIN);
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
  } else {
    nbEnqueue(1000, 80);
  }
}

// BUZZER BLOCKING (pentru evenimente non touch)

void playPresetTone(const char* id) {
  if (!buzzerOn) return;
  if (strcmp(id, "none") == 0) return;
  if (strcmp(id, "calm") == 0) {
    tone(BUZZER_PIN, 1000, 80); delay(80);
  } else if (strcmp(id, "loud") == 0) {
    tone(BUZZER_PIN, 1500, 130); delay(130);
  } else if (strcmp(id, "urgent") == 0) {
    for (int i = 0; i < 3; i++) { tone(BUZZER_PIN, 1800, 60); delay(75); }
  } else if (strcmp(id, "soft") == 0) {
    tone(BUZZER_PIN, 650, 60); delay(60);
  } else if (strcmp(id, "double") == 0) {
    tone(BUZZER_PIN, 1200, 70); delay(90);
    tone(BUZZER_PIN, 1200, 70); delay(70);
  } else if (strcmp(id, "triple") == 0) {
    for (int i = 0; i < 3; i++) { tone(BUZZER_PIN, 1200, 60); delay(80); }
  } else if (strcmp(id, "chime") == 0) {
    tone(BUZZER_PIN, 523, 90);  delay(95);
    tone(BUZZER_PIN, 659, 90);  delay(95);
    tone(BUZZER_PIN, 784, 130); delay(130);
  } else if (strcmp(id, "bell") == 0) {
    tone(BUZZER_PIN, 1568, 60);  delay(60);
    tone(BUZZER_PIN, 1318, 70);  delay(70);
    tone(BUZZER_PIN, 1046, 160); delay(160);
  } else if (strcmp(id, "doorbell") == 0) {
    tone(BUZZER_PIN, 784, 160); delay(170);
    tone(BUZZER_PIN, 659, 220); delay(220);
  } else if (strcmp(id, "xylophone") == 0) {
    int notes[5] = {659, 784, 880, 988, 1175};
    for (int i = 0; i < 5; i++) { tone(BUZZER_PIN, notes[i], 55); delay(60); }
  } else if (strcmp(id, "harp") == 0) {
    for (int f = 1400; f >= 500; f -= 45) { tone(BUZZER_PIN, f, 18); delay(14); }
  } else if (strcmp(id, "marimba") == 0) {
    tone(BUZZER_PIN, 440, 110); delay(115);
    tone(BUZZER_PIN, 220, 160); delay(160);
  } else if (strcmp(id, "crystal") == 0) {
    tone(BUZZER_PIN, 1568, 45); delay(48);
    tone(BUZZER_PIN, 1976, 45); delay(48);
    tone(BUZZER_PIN, 2349, 90); delay(90);
  } else if (strcmp(id, "wave") == 0) {
    for (int f = 400; f <= 1000; f += 30) { tone(BUZZER_PIN, f, 12); delay(10); }
    for (int f = 1000; f >= 400; f -= 30) { tone(BUZZER_PIN, f, 12); delay(10); }
  } else if (strcmp(id, "lullaby") == 0) {
    int notes[4] = {784, 659, 587, 523};
    for (int i = 0; i < 4; i++) { tone(BUZZER_PIN, notes[i], 140); delay(150); }
  } else if (strcmp(id, "pingpong") == 0) {
    for (int i = 0; i < 4; i++) {
      tone(BUZZER_PIN, i % 2 == 0 ? 1200 : 800, 55);
      delay(65);
    }
  } else {
    tone(BUZZER_PIN, 1000, 80); delay(80);
  }
  noTone(BUZZER_PIN);
}
void beepSwitch() {
  if (!buzzerOn) return;
  playPresetTone(eventSoundTile);
}
void beepTouch() {
  if (!buzzerOn) return;
  nbPlayPreset(eventSoundTouch);
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
      if (apIcon[row] & (0x80 >> col)) colVal |= (1 << row);
    }
    mx.setColumn(31 - col, colVal);
  }

  mx.setColumn(31 - 8, 0x00);

  uint8_t tmpA[8]; uint8_t nA = mx.getChar('A', sizeof(tmpA), tmpA);
  for (int i = 0; i < nA; i++) mx.setColumn(31 - (9 + i), tmpA[i]);
  mx.setColumn(31 - (9 + nA), 0x00);

  uint8_t tmpP[8]; uint8_t nP = mx.getChar('P', sizeof(tmpP), tmpP);
  for (int i = 0; i < nP; i++) mx.setColumn(31 - (9 + nA + 1 + i), tmpP[i]);

  mx.update(MD_MAX72XX::ON);
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
static uint8_t  mementoColBuf[512];
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

static void tempAddGlyphTo(uint8_t* buf, int& count, const uint8_t* glyph) {
  for (int i = 0; i < 5 && count < 62; i++)
    buf[count++] = glyph[i];
  if (count < 63) buf[count++] = 0x00;
}

static void tempAddGlyph(const uint8_t* glyph) {
  for (int i = 0; i < 5 && tempColCount < 126; i++)
    tempColBuf[tempColCount++] = glyph[i];
  if (tempColCount < 127) tempColBuf[tempColCount++] = 0x00;
}

static void dateAddDot() {
  if (dateColCount < 126) dateColBuf[dateColCount++] = CHAR_DOT[0];
  if (dateColCount < 127) dateColBuf[dateColCount++] = 0x00;
}

// SCROLL TYPE: ICON IN BUFFER HELPER

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
  int temp = (tempUnit == 1) ? (tempC * 9 / 5 + 32) : tempC;
  tempColCount = 0;
  if (scrollIconInBuffer(scrollTypeTemp) && !hideIconTemp) {
    scrollBufPrependIcon(tempColBuf, tempColCount, 127, tempIcon, TEMP_ICON_COLS);
  }

  bool negative = (temp < 0);
  int  absTemp  = abs(temp);

  if (negative) {
    for (int i = 0; i < 3 && tempColCount < 126; i++)
      tempColBuf[tempColCount++] = 0x08;
    if (tempColCount < 127) tempColBuf[tempColCount++] = 0x00;
  }

  if (absTemp >= 100) {
    tempAddGlyph(FONT[absTemp / 100]);
    tempAddGlyph(FONT[(absTemp / 10) % 10]);
    tempAddGlyph(FONT[absTemp % 10]);
  } else if (absTemp >= 10) {
    tempAddGlyph(FONT[absTemp / 10]);
    tempAddGlyph(FONT[absTemp % 10]);
  } else {
    tempAddGlyph(FONT[absTemp]);
  }

  tempAddGlyph(CHAR_DEG);
  tempAddGlyph((tempUnit == 1) ? CHAR_F : CHAR_C);

  if (tempColCount > 0) tempColCount--;
}

void tempDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);

  if (!hideIconTemp && !scrollIconInBuffer(scrollTypeTemp)) {

    for (int col = 0; col < TEMP_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (tempIcon[row] & (0x80 >> col)) colVal |= (1 << row);
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

  mx.update(MD_MAX72XX::ON);
}

void tempInit(int tempC) {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mx.update(MD_MAX72XX::ON);

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

void drawTemp(int tempC) {
  tempInit(tempC);
}

// ATMOSPHERIC PRESSURE (tile Circuit)

#define PRESSURE_HISTORY_LEN      6
#define PRESSURE_SAMPLE_MS        (30UL * 60UL * 1000UL)
#define PRESSURE_TREND_THRESH_HPA 1.0f
float         pressureHistory[PRESSURE_HISTORY_LEN] = {NAN, NAN, NAN, NAN, NAN, NAN};
uint8_t       pressureHistCount     = 0;
unsigned long lastPressureSampleMs  = 0;
float         lastPressureHpa       = 1013.25f;
int8_t        pressureTrend         = 0;

static inline const uint8_t* pressureIconForTrend() {
  if (pressureTrend > 0) return pressureIconUp;
  if (pressureTrend < 0) return pressureIconDown;
  return pressureIconFlat;
}

void pressureSampleTick() {
  float hpa = bmp.readPressure() / 100.0F;
  if (hpa < 850 || hpa > 1100) return;
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
  if (absVal >= 1000) {
    tempAddGlyphTo(pressureColBuf, pressureColCount, FONT[(absVal / 1000) % 10]);
    tempAddGlyphTo(pressureColBuf, pressureColCount, FONT[(absVal / 100) % 10]);
    tempAddGlyphTo(pressureColBuf, pressureColCount, FONT[(absVal / 10) % 10]);
    tempAddGlyphTo(pressureColBuf, pressureColCount, FONT[absVal % 10]);
  } else if (absVal >= 100) {
    tempAddGlyphTo(pressureColBuf, pressureColCount, FONT[absVal / 100]);
    tempAddGlyphTo(pressureColBuf, pressureColCount, FONT[(absVal / 10) % 10]);
    tempAddGlyphTo(pressureColBuf, pressureColCount, FONT[absVal % 10]);
  } else if (absVal >= 10) {
    tempAddGlyphTo(pressureColBuf, pressureColCount, FONT[absVal / 10]);
    tempAddGlyphTo(pressureColBuf, pressureColCount, FONT[absVal % 10]);
  } else {
    tempAddGlyphTo(pressureColBuf, pressureColCount, FONT[absVal]);
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

  mx.update(MD_MAX72XX::ON);
}

void pressureInit(int hpaInt) {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mx.update(MD_MAX72XX::ON);

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

// CURRENCY STANDARDS (tile Circuit)

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
  if (currencyTrend > 0) return pressureIconUp;
  if (currencyTrend < 0) return pressureIconDown;
  return pressureIconFlat;
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
  uint8_t tmp[8];
  uint8_t n = mx.getChar(ch, sizeof(tmp), tmp);
  if (n == 0) return;
  for (int i = 0; i < n && currColCount < 126; i++)
    currColBuf[currColCount++] = tmp[i];
  if (currColCount < 127) currColBuf[currColCount++] = 0x00;
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

  mx.update(MD_MAX72XX::ON);
}

void currencyInit() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mx.update(MD_MAX72XX::ON);

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

void currencyFetch() {

  const char* refQuote = currencyCompareEnabled ? currencyQuote
                        : (strcasecmp(currencyBase, "EUR") == 0 ? "USD" : "EUR");

  struct tm ti;
  if (!getLocalTime(&ti)) { return; }

  char todayStr[11];
  snprintf(todayStr, sizeof(todayStr), "%04d-%02d-%02d", ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);

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
    currencyRateNow = rNow;
    currencyValid   = true;
    if (okOld && rOld > 0) {
      currencyRateMonthAgo = rOld;
      float pctDiff = (rNow - rOld) / rOld * 100.0f;
      if (pctDiff >= CURRENCY_TREND_THRESH_PCT)       currencyTrend = 1;
      else if (pctDiff <= -CURRENCY_TREND_THRESH_PCT) currencyTrend = -1;
      else                                            currencyTrend = 0;
    }
  }
  lastCurrencyFetch = millis();
  (void)todayStr;
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

  mx.update(MD_MAX72XX::ON);
}

void ets2Init() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mx.update(MD_MAX72XX::ON);

  ets2BuildBuffer(ets2Speed);
  ets2DrawAtPos();
  ets2LastSpeed = ets2Speed;
}

bool ets2Tick() {
  if (!ets2Active) return false;

  if (millis() - lastEts2Ms > ETS2_TIMEOUT_MS) {
    ets2Active = false;
    CycleItem& cur = items[currentSlot];
    if (cur.id == ITEM_NOW_PLAYING) npInit();
    else if (cur.id == ITEM_WEATHER) weatherInit();
    else if (cur.id == ITEM_MEMENTO) mementoInit();
    else if (cur.id == ITEM_CANVAS) drawCanvas();
    else if (cur.id == ITEM_CURRENCY) currencyInit();
    else { gLastStaticDrawMs = 0; gStaticDrawDone = false; }
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

static void dateAddGlyph(const uint8_t* glyph) {
  for (int i = 0; i < 5 && dateColCount < 126; i++)
    dateColBuf[dateColCount++] = glyph[i];
  if (dateColCount < 127) dateColBuf[dateColCount++] = 0x00;
}

static void dateAddNumber(int value, int digits) {
  int divisor = 1;
  for (int i = 1; i < digits; i++) divisor *= 10;
  for (int i = 0; i < digits; i++) {
    int d = (value / divisor) % 10;
    dateAddGlyph(FONT[d]);
    divisor /= 10;
  }
}

void dateBuildBuffer(int day, int month, int year) {
  dateColCount = 0;
  if (scrollIconInBuffer(scrollTypeDate) && !hideIconDate) {
    scrollBufPrependIcon(dateColBuf, dateColCount, 127, dateIcon, DATE_ICON_COLS);
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

    for (int col = 0; col < DATE_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (dateIcon[row] & (0x80 >> col)) colVal |= (1 << row);
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

  mx.update(MD_MAX72XX::ON);
}

void dateInit() {
  struct tm ti;
  if (!getLocalTime(&ti)) return;
  int day   = ti.tm_mday;
  int month = ti.tm_mon + 1;
  int year  = ti.tm_year + 1900;

  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mx.update(MD_MAX72XX::ON);

  dateBuildBuffer(day, month, year);
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
}

void drawDate() {
  dateInit();
}

void drawHour() {
  struct tm ti;
  if (!getLocalTime(&ti)) return;
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

  bool twoDigitH = (h >= 10);

  int totalWidth = (twoDigitH ? 2 : 1) * 6 + 3 + 11;
  int col = (32 - totalWidth) / 2;

  if (twoDigitH) {
    col = writeDigit(h / 10, col);
  }
  col = writeDigit(h % 10, col);
  if (show) {
    writeCharN(CHAR_COLON, 2, col); col += 2; writeGap(col); col += 1;
  } else {
    mx.setColumn(31 - col, 0x00); col++;
    mx.setColumn(31 - col, 0x00); col++;
    mx.setColumn(31 - col, 0x00); col++;
  }
  col = writeDigit(m / 10, col);
  writeChar(FONT[m % 10], col);

  mx.update(MD_MAX72XX::ON);
}

// NP MANUAL SCROLL
#define NP_SCROLL_SPEED_MS 40
#define NP_DISPLAY_COLS    32

static uint8_t  npColBuf[512];
static int      npColCount  = 0;
static int      npScrollPos = 0;
static unsigned long npLastScrollMs = 0;

void npBuildBuffer() {

  char cleanBuf[128];
  sanitizeUtf8(nowPlayingBuf, cleanBuf, sizeof(cleanBuf));

  npColCount = 0;
  if (scrollIconInBuffer(scrollTypeNowPlaying) && !hideIconNowPlaying) {
    scrollBufPrependIcon(npColBuf, npColCount, 512, musicIcon, NP_ICON_COLS);
  }
  for (int ci = 0; cleanBuf[ci] != '\0' && npColCount < 500; ci++) {
    uint8_t tmp[8];
    uint8_t n = mx.getChar(cleanBuf[ci], sizeof(tmp), tmp);
    if (n == 0) continue;
    for (int i = 0; i < n && npColCount < 512; i++)
      npColBuf[npColCount++] = tmp[i];

    if (npColCount < 512) npColBuf[npColCount++] = 0x00;
  }

  if (npColCount > 0) npColCount--;
}

void npDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);

  if (!hideIconNowPlaying && !scrollIconInBuffer(scrollTypeNowPlaying)) {

    for (int col = 0; col < NP_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (musicIcon[row] & (0x80 >> col)) colVal |= (1 << row);
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

  mx.update(MD_MAX72XX::ON);
}

void npInit() {

  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < NP_DISPLAY_COLS; c++) mx.setColumn(c, 0x00);
  mx.update(MD_MAX72XX::ON);
  npBuildBuffer();
  npScrollPos = 0;
  npWrapPass = 0;
  npDrawAtPos(0);
  npState = NP_PAUSE_BEFORE_RIGHT;
  npPauseStartMs = millis();
}

// STOPWATCH DISPLAY (Priority Tile fix, precum Notificari/ETS2)

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
    uint8_t tmp[8];
    uint8_t n = mx.getChar(txt[ci], sizeof(tmp), tmp);
    if (n == 0) continue;
    for (int i = 0; i < n && swColCount < 160; i++)
      swColBuf[swColCount++] = tmp[i];
    if (swColCount < 160) swColBuf[swColCount++] = 0x00;
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
  mx.update(MD_MAX72XX::ON);
}

void swInit() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mx.update(MD_MAX72XX::ON);
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
    swWasActive = false;
    return false;
  }

  if (!swWasActive) {
    swWasActive = true;
    swInit();
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
      if (now - swLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
      if (now - swLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
      if (now - swLastScrollMs >= NP_SCROLL_SPEED_MS) {
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

// TIMER DISPLAY (Priority Tile fix, precum Stopwatch)

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
    uint8_t tmp[8];
    uint8_t n = mx.getChar(txt[ci], sizeof(tmp), tmp);
    if (n == 0) continue;
    for (int i = 0; i < n && timerColCount < 160; i++)
      timerColBuf[timerColCount++] = tmp[i];
    if (timerColCount < 160) timerColBuf[timerColCount++] = 0x00;
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
  mx.update(MD_MAX72XX::ON);
}

void timerBlankScreen() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < 32; c++) mx.setColumn(c, 0x00);
  mx.update(MD_MAX72XX::ON);
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
      if (now - timerLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
      if (now - timerLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
      if (now - timerLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
    timerWasActive = false;
    return false;
  }
  if (!timerWasActive) {
    timerWasActive = true;
    timerInit();
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

// NOTIF DISPLAY

void notifBuildBuffer() {
  char cleanBuf[204];
  sanitizeUtf8(notifBuf, cleanBuf, sizeof(cleanBuf));
  notifColCount = 0;
  if (scrollIconInBuffer(scrollTypeNotif) && !hideIconNotif) {
    scrollBufPrependIcon(notifColBuf, notifColCount, 512, notifIcon, NP_ICON_COLS);
  }
  for (int ci = 0; cleanBuf[ci] != '\0' && notifColCount < 500; ci++) {
    uint8_t tmp[8];
    uint8_t n = mx.getChar(cleanBuf[ci], sizeof(tmp), tmp);
    if (n == 0) continue;
    for (int i = 0; i < n && notifColCount < 512; i++)
      notifColBuf[notifColCount++] = tmp[i];
    if (notifColCount < 512) notifColBuf[notifColCount++] = 0x00;
  }
  if (notifColCount > 0) notifColCount--;
}

void notifDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);
  if (!hideIconNotif && !scrollIconInBuffer(scrollTypeNotif)) {

    for (int col = 0; col < NP_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (notifIcon[row] & (0x80 >> col)) colVal |= (1 << row);
      }
      mx.setColumn(31 - col, colVal);
    }
    mx.setColumn(31 - NP_ICON_COLS, 0x00);
  }

  bool fullWidth = hideIconNotif || scrollIconInBuffer(scrollTypeNotif);
  int textOffset = fullWidth ? 0 : (NP_ICON_COLS + NP_SEP_COLS);
  int textCols   = fullWidth ? 32 : NP_TEXT_COLS;
  for (int col = 0; col < textCols; col++) {
    int src = pos + col;
    uint8_t val = (src >= 0 && src < notifColCount) ? notifColBuf[src] : 0x00;
    mx.setColumn(31 - (textOffset + col), val);
  }
  mx.update(MD_MAX72XX::ON);
}

void notifInit() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < NP_DISPLAY_COLS; c++) mx.setColumn(c, 0x00);
  mx.update(MD_MAX72XX::ON);
  notifBuildBuffer();
  notifScrollPos = 0;
  notifWrapPass = 0;
  notifDrawAtPos(0);
  notifState    = NP_PAUSE_BEFORE_RIGHT;
  notifPauseMs  = millis();
}

// MEMENTO DISPLAY
void mementoBuildBuffer() {
  char cleanBuf[128];
  sanitizeUtf8(mementoBuf, cleanBuf, sizeof(cleanBuf));
  mementoColCount = 0;
  if (scrollIconInBuffer(scrollTypeReminder) && !hideIconReminder) {
    scrollBufPrependIcon(mementoColBuf, mementoColCount, 512, mementoIcon, NP_ICON_COLS);
  }
  for (int ci = 0; cleanBuf[ci] != '\0' && mementoColCount < 500; ci++) {
    uint8_t tmp[8];
    uint8_t n = mx.getChar(cleanBuf[ci], sizeof(tmp), tmp);
    if (n == 0) continue;
    for (int i = 0; i < n && mementoColCount < 512; i++)
      mementoColBuf[mementoColCount++] = tmp[i];
    if (mementoColCount < 512) mementoColBuf[mementoColCount++] = 0x00;
  }
  if (mementoColCount > 0) mementoColCount--;
}

void mementoDrawAtPos(int pos) {
  mx.update(MD_MAX72XX::OFF);
  if (!hideIconReminder && !scrollIconInBuffer(scrollTypeReminder)) {

    for (int col = 0; col < NP_ICON_COLS; col++) {
      uint8_t colVal = 0;
      for (int row = 0; row < 8; row++) {
        if (mementoIcon[row] & (0x80 >> col)) colVal |= (1 << row);
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
  mx.update(MD_MAX72XX::ON);
}

void mementoInit() {
  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < NP_DISPLAY_COLS; c++) mx.setColumn(c, 0x00);
  mx.update(MD_MAX72XX::ON);
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
  mx.update(MD_MAX72XX::ON);
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
  mx.update(MD_MAX72XX::ON);
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
    mx.update(MD_MAX72XX::ON);
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

// Dispatch principal
void ssInit() {
  mx.update(MD_MAX72XX::OFF);
  for (int i = 0; i < 32; i++) mx.setColumn(i, 0x00);
  mx.update(MD_MAX72XX::ON);
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
  } else {
    mx.update(MD_MAX72XX::OFF);
    for (int i = 0; i < 32; i++) mx.setColumn(i, 0x00);
    mx.update(MD_MAX72XX::ON);
    gLastStaticDrawMs = 0;
    gStaticDrawDone   = false;
  }
}

bool notifTick() {
  if (!notifActive) return false;
  unsigned long now = millis();
  switch (notifState) {
    case NP_SHOW_START:
      break;
    case NP_PAUSE_BEFORE_RIGHT:
      if (now - notifPauseMs >= NP_PAUSE_MS) {
        if (scrollIsWrap(scrollTypeNotif)) {
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
      if (now - notifLastScrollMs >= NP_SCROLL_SPEED_MS) {
        notifLastScrollMs = now;
        notifScrollPos++;
        if (notifScrollPos >= notifColCount) {
          notifWrapPass++;
          if (notifWrapPass >= SCROLL_WRAP_PASSES) {

            notifActive = false;
            notifBuf[0] = '\0';
            resumeAfterNotif();
            return false;
          }
          notifScrollPos = -((hideIconNotif || scrollIconInBuffer(scrollTypeNotif)) ? 32 : NP_TEXT_COLS);
        }
        notifDrawAtPos(notifScrollPos);
      }
      break;
    }
    case NP_SCROLL_RIGHT: {
      int maxPos = notifColCount - ((hideIconNotif || scrollIconInBuffer(scrollTypeNotif)) ? 32 : NP_TEXT_COLS);
      if (maxPos <= 0) {
        notifState = NP_PAUSE_SHORT;
        notifPauseMs = now;
        break;
      }
      if (now - notifLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
      if (now - notifLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
        notifBuf[0] = '\0';
        resumeAfterNotif();
        return false;
      }
      break;
    case NP_PAUSE_SHORT:
      if (now - notifPauseMs >= 4000UL) {
        notifActive = false;
        notifBuf[0] = '\0';
        resumeAfterNotif();
        return false;
      }
      break;
  }
  return true;
}

// NOW PLAYING (Priority Tiles)

bool npPriorityTick() {
  if (!nowPlayingIsPriority || !nowPlayingActive) {
    npPriorityWasActive = false;
    return false;
  }

  int npIdx = -1;
  for (int i = 0; i < NUM_ITEMS; i++) {
    if (items[i].id == ITEM_NOW_PLAYING) { npIdx = i; break; }
  }
  if (npIdx >= 0 && !items[npIdx].enabled) {
    npPriorityWasActive = false;
    return false;
  }

  if (!npPriorityWasActive) {
    npPriorityWasActive = true;
    npInit();
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
      if (now - npLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
      if (now - npLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
      if (now - npLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
  uint8_t n = mx.getChar(ch, sizeof(tmp), tmp);
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

  if (neg) {
    static const uint8_t CHAR_MINUS[3] = {0x08, 0x08, 0x08};
    wxBufAppend(CHAR_MINUS, 3);
  }

  if (twoDigit) {
    wxBufAppend(FONT[absTemp / 10], 5);
    wxBufAppend(FONT[absTemp % 10], 5);
  } else {
    wxBufAppend(FONT[absTemp], 5);
  }

  wxBufAppend(CHAR_DEG, 5);
  wxBufAppend((tempUnit == 1) ? CHAR_F : CHAR_C, 5);

  if (wxColCount < 511) wxColBuf[wxColCount++] = 0x00;
  if (wxColCount < 511) wxColBuf[wxColCount++] = 0x00;
  if (wxColCount < 511) wxColBuf[wxColCount++] = 0x00;

  int hum = weatherHumidity;
  if (hum >= 0) {
    int humTens = hum / 10;
    int humOnes = hum % 10;
    if (hum >= 100) {
      wxBufAppend(FONT[1], 5);
      wxBufAppend(FONT[0], 5);
      wxBufAppend(FONT[0], 5);
    } else if (hum >= 10) {
      wxBufAppend(FONT[humTens], 5);
      wxBufAppend(FONT[humOnes], 5);
    } else {
      wxBufAppend(FONT[humOnes], 5);
    }
    wxBufAppendMD('%');
  }

  if (wxColCount < 511) wxColBuf[wxColCount++] = 0x00;
  if (wxColCount < 511) wxColBuf[wxColCount++] = 0x00;

  char cleanDesc[48];
  sanitizeUtf8(weatherDesc, cleanDesc, sizeof(cleanDesc));
  for (int ci = 0; cleanDesc[ci] != '\0' && wxColCount < 500; ci++)
    wxBufAppendMD(cleanDesc[ci]);

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
  mx.update(MD_MAX72XX::ON);
}

void weatherInit() {

  if (!weatherValid) {
    snprintf(weatherBuf, sizeof(weatherBuf), "No data");
  }

  mx.update(MD_MAX72XX::OFF);
  for (int c = 0; c < NP_DISPLAY_COLS; c++) mx.setColumn(c, 0x00);
  mx.update(MD_MAX72XX::ON);
  weatherBuildBuffer();
  wxScrollPos = 0;
  wxWrapPass = 0;
  weatherDrawAtPos(0);
  wxState = NP_PAUSE_BEFORE_RIGHT;
  wxPauseStartMs = millis();
}

void weatherFetch() {
  if (strlen(weatherApiKey) == 0) return;
  if (WiFi.status() != WL_CONNECTED) return;
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
      weatherTempC   = doc["main"]["temp"] | -999.0f;
      weatherHumidity = doc["main"]["humidity"] | -1;
      const char* desc = doc["weather"][0]["description"] | "";
      strncpy(weatherDesc, desc, sizeof(weatherDesc) - 1);
      weatherDesc[sizeof(weatherDesc)-1] = '\0';

      if (weatherDesc[0] >= 'a' && weatherDesc[0] <= 'z') weatherDesc[0] -= 32;

      const char* cond = doc["weather"][0]["main"] | "";
      strncpy(weatherCondition, cond, sizeof(weatherCondition) - 1);
      weatherCondition[sizeof(weatherCondition)-1] = '\0';
      weatherValid = true;
    }
  }
  http.end();
  lastWeatherFetch = millis();
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
      case PRIORITY_ID_NOTIF:      if (notifActive) return true; break;
      case PRIORITY_ID_ETS2:       if (ets2Active) return true; break;
      case PRIORITY_ID_NOWPLAYING: {
        int npIdx = -1;
        for (int j = 0; j < NUM_ITEMS; j++) if (items[j].id == ITEM_NOW_PLAYING) { npIdx = j; break; }
        if (nowPlayingIsPriority && nowPlayingActive && (npIdx < 0 || items[npIdx].enabled)) return true;
        break;
      }
      case PRIORITY_ID_STOPWATCH:  if (swRunning) return true; break;
      case PRIORITY_ID_TIMER:      if (timerRunning || timerFinished) return true; break;
    }
  }
  return false;
}

String priorityOrderToString() {
  String out = "";
  for (int i = 0; i < NUM_PRIORITY_IDS; i++) {
    if (i > 0) out += ",";
    switch (priorityOrder[i]) {
      case PRIORITY_ID_NOTIF:      out += "notif";      break;
      case PRIORITY_ID_ETS2:       out += "ets2";        break;
      case PRIORITY_ID_NOWPLAYING: out += "nowplaying";  break;
      case PRIORITY_ID_STOPWATCH:  out += "stopwatch";   break;
      case PRIORITY_ID_TIMER:      out += "timer";       break;
    }
  }
  return out;
}

void rebuildPriorityOrderFromEts2Order() {
  if (ets2OrderFirst) {
    priorityOrder[0] = PRIORITY_ID_ETS2;
    priorityOrder[1] = PRIORITY_ID_NOTIF;
  } else {
    priorityOrder[0] = PRIORITY_ID_NOTIF;
    priorityOrder[1] = PRIORITY_ID_ETS2;
  }
  priorityOrder[2] = PRIORITY_ID_NOWPLAYING;
  priorityOrder[3] = PRIORITY_ID_STOPWATCH;
  priorityOrder[4] = PRIORITY_ID_TIMER;
}

void enterSlot() {
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
  } else {

    mx.update(MD_MAX72XX::OFF);
    for (int i = 0; i < 32; i++) mx.setColumn(i, 0x00);
    mx.update(MD_MAX72XX::ON);

    switch (items[currentSlot].id) {
      case ITEM_HOUR:
        gLastStaticDrawMs = millis();
        drawHour();
        break;
      case ITEM_DATE:
        gStaticDrawDone = true;
        dateInit();
        break;
      case ITEM_TEMP: {

        float tf = bmp.readTemperature();
        if (tf >= -40 && tf <= 85) {
          lastTemp = (int)round(tf);
        }
        gStaticDrawDone = true;
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

void advanceSlot() {
  beepSwitch();
  inScrollAnim = false;
  inFadeOut    = false;
  inFadeIn     = false;
  lastTemp     = -999;
  npState      = NP_SHOW_START;
  dateState    = NP_SHOW_START;

  gLastStaticDrawMs = 0;
  gStaticDrawDone   = false;

  int next = nextActiveSlot(currentSlot);
  if (next == -1) return;
  currentSlot = next;
  slotStartMs = millis();
  enterSlot();
}

void retreatSlot() {
  beepSwitch();
  inScrollAnim = false;
  inFadeOut    = false;
  inFadeIn     = false;
  lastTemp     = -999;
  npState      = NP_SHOW_START;
  dateState    = NP_SHOW_START;

  gLastStaticDrawMs = 0;
  gStaticDrawDone   = false;

  int prev = prevActiveSlot(currentSlot);
  if (prev == -1) return;
  currentSlot = prev;
  slotStartMs = millis();
  enterSlot();
}

// SAVE / LOAD

bool isValidCircuitItemId(uint8_t id) {
  switch (id) {
    case ITEM_HOUR: case ITEM_DATE: case ITEM_TEMP: case ITEM_NOW_PLAYING:
    case ITEM_WEATHER: case ITEM_MEMENTO: case ITEM_CANVAS:
    case ITEM_PRESSURE: case ITEM_SCREENSAVER: case ITEM_CURRENCY:
      return true;
    default:
      return false;
  }
}

void repairItemIds() {
  static const uint8_t ALL_IDS[NUM_ITEMS] = {
    ITEM_HOUR, ITEM_DATE, ITEM_TEMP, ITEM_NOW_PLAYING, ITEM_WEATHER,
    ITEM_MEMENTO, ITEM_CANVAS, ITEM_PRESSURE, ITEM_SCREENSAVER, ITEM_CURRENCY
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
  prefs.getString("buzzerPreset", buzzerPreset, sizeof(buzzerPreset));
  if (strlen(buzzerPreset) == 0) strcpy(buzzerPreset, "calm");
  prefs.getString("evSndTile",  eventSoundTile,  sizeof(eventSoundTile));
  if (strlen(eventSoundTile) == 0) strcpy(eventSoundTile, "calm");
  prefs.getString("evSndWifi",  eventSoundWifi,  sizeof(eventSoundWifi));
  if (strlen(eventSoundWifi) == 0) strcpy(eventSoundWifi, "urgent");
  prefs.getString("evSndNotif", eventSoundNotif, sizeof(eventSoundNotif));
  if (strlen(eventSoundNotif) == 0) strcpy(eventSoundNotif, "soft");
  prefs.getString("evSndEts2",  eventSoundEts2,  sizeof(eventSoundEts2));
  if (strlen(eventSoundEts2) == 0) strcpy(eventSoundEts2, "urgent");
  prefs.getString("evSndTouch", eventSoundTouch, sizeof(eventSoundTouch));
  if (strlen(eventSoundTouch) == 0) strcpy(eventSoundTouch, "soft");
  touchTapAction       = prefs.getUChar("touchTap", 0);
  touchDoubleTapAction = prefs.getUChar("touchDbl", 0);
  npDisplayMode = prefs.getUChar("npmode", 0);
  ssAnimSelected = prefs.getUChar("ssAnim", SS_ANIM_RANDOM);
  tempUnit      = prefs.getUChar("tempunit", 0);
  hourFormat    = prefs.getUChar("hourformat", 0);
  dateFormat    = prefs.getUChar("dateformat", 2);
  notifEnabled  = prefs.getBool("notifEn", true);
  ets2Enabled   = prefs.getBool("ets2En", true);
  ets2OrderFirst = prefs.getBool("ets2Ord", false);
  rebuildPriorityOrderFromEts2Order();
  nowPlayingIsPriority = prefs.getBool("npIsPrio", false);
  {
    uint8_t savedOrder[NUM_PRIORITY_IDS];
    size_t got = prefs.getBytes("prioOrder", savedOrder, NUM_PRIORITY_IDS);
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
  hideIconPressure   = prefs.getBool("hideIconPress", hideTileIcons);
  hideIconCurrency   = prefs.getBool("hideIconCurr",  hideTileIcons);
  scrollType     = prefs.getUChar("scrollType", 0);
  scrollTypeDate       = prefs.getUChar("scrollTypeDate",   scrollType);
  scrollTypeTemp       = prefs.getUChar("scrollTypeTemp",   scrollType);
  scrollTypeReminder   = prefs.getUChar("scrollTypeRem",    scrollType);
  scrollTypeWeather    = prefs.getUChar("scrollTypeWx",     scrollType);
  scrollTypeNotif      = prefs.getUChar("scrollTypeNotif",  scrollType);
  scrollTypeNowPlaying = prefs.getUChar("scrollTypeNp",     scrollType);
  scrollTypePressure   = prefs.getUChar("scrollTypePress",  0);
  scrollTypeCurrency   = prefs.getUChar("scrollTypeCurr",   0);
  scrollTypeStopwatch  = prefs.getUChar("scrollTypeSw",     scrollType);
  scrollTypeTimer      = prefs.getUChar("scrollTypeTmr",    0);
  prefs.getString("evSndTimer", eventSoundTimer, sizeof(eventSoundTimer));
  if (strlen(eventSoundTimer) == 0) strcpy(eventSoundTimer, "calm");
  timerDurationSec = prefs.getUInt("timerDurSec", 300);
  if (timerDurationSec == 0) timerDurationSec = 300;
  timerPreset = prefs.getUChar("timerPreset", 0);
  timerRemainingSnapMs = (unsigned long)timerDurationSec * 1000UL;
  timerFinished = false;
  timerWasActive = false;
  for (int i = 0; i < NUM_ITEMS; i++) {
    String base = "it" + String(i);
    items[i].id          = prefs.getUChar((base + "id").c_str(),  items[i].id);
    items[i].enabled     = prefs.getBool((base + "en").c_str(),  items[i].enabled);
    items[i].durationSec = prefs.getUShort((base + "dur").c_str(), items[i].durationSec);
    items[i].order       = prefs.getUChar((base + "ord").c_str(),  items[i].order);
  }

  repairItemIds();
  curBrightness = prefs.getUChar("bright",  BRIGHTNESS);
  dimAutoOn     = prefs.getBool("dimAuto",  false);
  dimFromH      = prefs.getUChar("dimFH",   22);
  dimFromM      = prefs.getUChar("dimFM",   0);
  dimToH        = prefs.getUChar("dimTH",   7);
  dimToM        = prefs.getUChar("dimTM",   0);
  dimLevel      = prefs.getUChar("dimLvl",  1);
  prefs.getString("wxCity",   weatherCity,   sizeof(weatherCity));
  prefs.getString("wxApiKey", weatherApiKey, sizeof(weatherApiKey));
  prefs.getString("wxLang",   weatherLang,   sizeof(weatherLang));
  weatherLat = prefs.getFloat("wxLat", weatherLat);
  weatherLon = prefs.getFloat("wxLon", weatherLon);
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

void saveSettings() {
  prefs.begin("settings", false);
  prefs.putBool("buzzer", buzzerOn);
  prefs.putUChar("buzzerVol", buzzerVolume);
  prefs.putString("buzzerPreset", buzzerPreset);
  prefs.putUChar("npmode", npDisplayMode);
  prefs.putUChar("ssAnim", ssAnimSelected);
  prefs.putUChar("tempunit", tempUnit);
  prefs.putUChar("hourformat", hourFormat);
  prefs.putUChar("dateformat", dateFormat);
  prefs.putUChar("touchTap", touchTapAction);
  prefs.putUChar("touchDbl", touchDoubleTapAction);
  prefs.putBool("notifEn", notifEnabled);
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
  prefs.putBool("hideIconPress", hideIconPressure);
  prefs.putBool("hideIconCurr",  hideIconCurrency);
  prefs.putUChar("scrollType", scrollType);
  prefs.putUChar("scrollTypeDate",  scrollTypeDate);
  prefs.putUChar("scrollTypeTemp",  scrollTypeTemp);
  prefs.putUChar("scrollTypeRem",   scrollTypeReminder);
  prefs.putUChar("scrollTypeWx",    scrollTypeWeather);
  prefs.putUChar("scrollTypeNotif", scrollTypeNotif);
  prefs.putUChar("scrollTypeNp",    scrollTypeNowPlaying);
  prefs.putUChar("scrollTypePress", scrollTypePressure);
  prefs.putUChar("scrollTypeCurr",  scrollTypeCurrency);
  prefs.putUChar("scrollTypeSw",    scrollTypeStopwatch);
  prefs.putUChar("scrollTypeTmr",   scrollTypeTimer);
  prefs.putString("evSndTimer", eventSoundTimer);
  prefs.putUInt("timerDurSec", timerDurationSec);
  prefs.putUChar("timerPreset", timerPreset);
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
  prefs.putFloat("wxLat",    weatherLat);
  prefs.putFloat("wxLon",    weatherLon);
  prefs.putString("currBase",  currencyBase);
  prefs.putString("currQuote", currencyQuote);
  prefs.putBool("currCompare", currencyCompareEnabled);
  prefs.putString("memoText", mementoBuf);
  prefs.putBytes("canvasBmp", canvasBitmap, CANVAS_COLS);
  prefs.end();
}

// WIFI
String scanNetworks() {
  int n = WiFi.scanNetworks();
  String json = "[";
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
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"secured\":" + (secured ? "true" : "false") + ",\"auth\":\"" + authStr + "\""  + "}";
  }
  json += "]";
  WiFi.scanDelete();
  return json;
}

// PORTAL HTML
// PORTAL HTML
const char PORTAL_HTML[] PROGMEM = R"pl0(<!doctype html><html lang=ro><meta charset=UTF-8><meta content="width=device-width,initial-scale=1,viewport-fit=cover" name=viewport><title>Octoglow</title><link href=https://fonts.googleapis.com rel=preconnect><link href="https://fonts.googleapis.com/css2?family=Google+Sans:wght@400;500&family=Roboto:wght@400;500&display=swap" rel=stylesheet><style>:root{--pri:#d0bcff;--on-pri:#381e72;--pri-con:#4f378b;--on-pri-con:#eaddff;--sec:#ccc2dc;--sec-con:#4a4458;--on-sec-con:#e8def8;--bg:#1c1b1f;--on-bg:#e6e1e5;--surf:#1c1b1f;--on-surf:#e6e1e5;--surf-var:#49454f;--on-surf-var:#cac4d0;--surf-low:#1d1b20;--surf-con:#211f26;--surf-high:#2b2930;--surf-highest:#36343b;--outline:#938f99;--outline-var:#49454f;--inv-surf:#e6e1e5;--inv-on-surf:#313033;--err:#f2b8b5;--err-con:#8c1d18;--blue-con:#0d2247;--blue:#aac7ff;--teal-con:#003731;--teal:#6cf9d8;--amber-con:#3b2a00;--amber:#ffdf99;--grn-con:#003824;--grn:#6dd7a1}*{box-sizing:border-box;margin:0;padding:0}body{background:var(--bg);color:var(--on-bg);-webkit-font-smoothing:antialiased;min-height:100vh;font-family:Roboto,sans-serif;overflow-x:hidden}.screen{flex-direction:column;min-height:100vh;display:none}.screen.active{animation:.22s cubic-bezier(.2,0,0,1) slide-in;display:flex}@keyframes slide-in{0%{opacity:0;transform:translate(24px)}to{opacity:1;transform:none}}.top-bar{background:var(--surf);z-index:10;align-items:center;gap:4px;min-height:64px;padding:8px 4px;transition:background .2s,box-shadow .2s;display:flex;position:sticky;top:0}.top-bar.raised{background:var(--surf-high);box-shadow:0 1px 0 var(--outline-var)}.bar-lead{width:48px;height:48px;color:var(--on-surf-var);cursor:pointer;background:0 0;border:none;border-radius:50%;flex-shrink:0;justify-content:center;align-items:center;transition:background .15s;display:flex}.bar-lead:hover{background:color-mix(in srgb,var(--on-surf)8%,transparent)}.bar-lead:active{background:color-mix(in srgb,var(--on-surf)12%,transparent)}.bar-avatar{background:var(--pri-con);width:40px;height:40px;color:var(--on-pri-con);border-radius:50%;flex-shrink:0;justify-content:center;align-items:center;margin-left:4px;display:flex}.bar-text{flex:1;padding-left:4px}.bar-title{color:var(--on-surf);font-family:Google Sans,sans-serif;font-size:22px;font-weight:400}.home-title{color:var(--on-surf);font-family:Google Sans,sans-serif;font-size:30px;font-weight:500;text-align:center;letter-spacing:.1px}.bar-sub{color:var(--on-surf-var);letter-spacing:.3px;margin-top:1px;font-size:12px}.content{flex:1;padding:4px 16px 80px}.sl{letter-spacing:1.5px;text-transform:uppercase;color:var(--pri);padding:20px 4px 8px;font-size:11px;font-weight:500}.sl:first-child{padding-top:8px}.card{background:var(--surf-low);border-radius:12px;margin-bottom:4px;overflow:hidden}.li{cursor:pointer;border-bottom:1px solid var(--outline-var);-webkit-tap-highlight-color:transparent;align-items:center;gap:16px;min-height:72px;padding:0 16px;transition:background .12s;display:flex;position:relative;overflow:hidden}.li:last-child{border-bottom:none}.sel-row{cursor:pointer;border-bottom:1px solid var(--outline-var);-webkit-tap-highlight-color:transparent;align-items:center;gap:16px;min-height:56px;padding:0 16px;transition:background .12s;display:flex;position:relative;overflow:hidden}.sel-row:last-child{border-bottom:none}.sel-row:hover{background:color-mix(in srgb,var(--on-surf)6%,transparent)}.sel-row:active{background:color-mix(in srgb,var(--on-surf)12%,transparent)}.sel-radio{width:20px;height:20px;border-radius:50%;border:2px solid var(--outline);flex:0 0 auto;position:relative;transition:border-color .15s}.sel-radio-on{border-color:var(--pri)}.sel-radio-on:after{content:"";position:absolute;inset:3px;border-radius:50%;background:var(--pri)}.sel-label{font-size:14px;color:var(--on-surf)}.li:after{content:"";background:var(--on-surf);opacity:0;pointer-events:none;transition:opacity .3s;position:absolute;inset:0}.li:active:after{opacity:.12;transition:none}.li:hover{background:color-mix(in srgb,var(--on-surf)6%,transparent)}.li.static{cursor:default}.li.static:hover{background:0 0}.li.static:after{display:none}.lic{border-radius:50%;flex-shrink:0;justify-content:center;align-items:center;width:40px;height:40px;display:flex;overflow:hidden}.lc-pur{background:var(--pri-con);color:var(--pri)}.lc-blu{background:var(--pri-con);color:var(--pri)}.lc-tea{background:var(--pri-con);color:var(--pri)}.lc-amb{background:var(--pri-con);color:var(--pri)}.lc-grn{background:var(--pri-con);color:var(--pri)}.li-body{flex:1;min-width:0}.li-head{color:var(--on-surf);font-family:Google Sans,sans-serif;font-size:16px;font-weight:400}.li-sub{color:var(--on-surf-var);white-space:nowrap;text-overflow:ellipsis;margin-top:2px;font-size:13px;overflow:hidden}.li-trail{color:var(--on-surf-var);flex-shrink:0;align-items:center;gap:8px;display:flex}.chevron{opacity:.6;align-items:center;display:flex}.chip{letter-spacing:.4px;border-radius:8px;align-items:center;padding:4px 12px;font-size:12px;font-weight:500;display:inline-flex}.chip-ok{background:var(--grn-con);color:var(--grn)}.chip-ap{background:var(--amber-con);color:var(--amber)}.ip-val{color:var(--blue);letter-spacing:.5px;font-family:Roboto Mono,monospace;font-size:13px;font-weight:500}.copy-btn{background:var(--surf-high);color:var(--blue);cursor:pointer;letter-spacing:.1px;white-space:nowrap;border:none;border-radius:100px;padding:6px 14px;font-family:Google Sans,sans-serif;font-size:12px;font-weight:500}.sw{cursor:pointer;flex-shrink:0;align-items:center;width:52px;height:32px;display:inline-flex;position:relative}.sw input{opacity:0;pointer-events:none;width:0;height:0;position:absolute}.sw-track{background:var(--surf-var);border:2px solid var(--outline);pointer-events:none;border-radius:16px;transition:background .2s,border-color .2s;position:absolute;inset:0}.sw input:checked~.sw-track{background:var(--pri);border-color:var(--pri)}.sw-thumb{background:var(--outline);pointer-events:none;border-radius:50%;width:16px;height:16px;transition:left .2s cubic-bezier(.4,0,.2,1),width .2s,height .2s,background .2s,margin .2s;position:absolute;top:50%;left:6px;transform:translateY(-50%);box-shadow:0 1px 3px #00000080}.sw input:checked~.sw-thumb{background:var(--on-pri);width:24px;height:24px;margin-top:0;top:50%;left:26px;transform:translateY(-50%)}.btn-row{gap:8px;margin-top:12px;display:flex}.mbtn{letter-spacing:.1px;cursor:pointer;border:none;border-radius:100px;flex:1;justify-content:center;align-items:center;gap:6px;height:40px;padding:0 20px;font-family:Google Sans,sans-serif;font-size:14px;font-weight:500;transition:opacity .15s,transform .1s;display:inline-flex;position:relative;overflow:hidden}.mbtn:disabled{opacity:.38;pointer-events:none}.mbtn:active{transform:scale(.97)}.mbtn-fill{background:var(--pri);color:var(--on-pri)}.mbtn-ton{background:var(--sec-con);color:var(--on-sec-con)}.wlist{background:var(--surf-low);border-radius:12px;max-height:400px;margin-bottom:8px;overflow:hidden auto}.wlist::-webkit-scrollbar{width:4px}.wlist::-webkit-scrollbar-thumb{background:var(--outline-var);border-radius:2px}.wlist .li{min-height:64px}.wlist .cur-net{cursor:pointer}#lang-list::-webkit-scrollbar{width:4px}#lang-list::-webkit-scrollbar-track{background:transparent}#lang-list::-webkit-scrollbar-thumb{background:var(--outline-var);border-radius:2px}#lang-list::-webkit-scrollbar-thumb:hover{background:var(--on-surf-var)}.wi{cursor:pointer;border-bottom:1px solid var(--outline-var);color:var(--on-surf);align-items:center;gap:12px;padding:14px 16px;font-size:14px;transition:background .12s;display:flex}.wi:last-child{border-bottom:none}.wi:hover{background:color-mix(in srgb,var(--on-surf)6%,transparent)}.wi.sel{background:color-mix(in srgb,var(--pri)14%,transparent)}.wi.sel .wi-name{color:var(--pri)}.wi-name{flex:1}.bars{align-items:flex-end;gap:2px;height:14px;display:flex}.bar{background:var(--outline);border-radius:1.5px;width:3px}.bar.on{background:var(--pri)}.scan-hint{text-align:)pl0"
R"pl1(center;color:var(--on-surf-var);padding:24px 16px;font-family:Google Sans,sans-serif;font-size:14px}.spin-ring{width:20px;height:20px;border:3px solid var(--outline-var);border-top-color:var(--pri);border-radius:50%;display:inline-block;vertical-align:middle;margin-right:8px;animation:spin-rot .8s linear infinite}@keyframes spin-rot{to{transform:rotate(360deg)}}.tf-wrap{background:var(--surf-high);border-bottom:2px solid var(--outline);border-radius:4px 4px 0 0;margin-bottom:8px;padding:0 16px;display:none}.tf-wrap.open{display:block}.tf-wrap:focus-within{border-bottom-color:var(--pri)}.tf-wrap label{letter-spacing:.4px;color:var(--on-surf-var);padding-top:8px;font-size:12px;font-weight:500;display:block}.tf-wrap:focus-within label{color:var(--pri)}.tf-wrap input{width:100%;color:var(--on-surf);background:0 0;border:none;outline:none;padding:6px 0 10px;font-family:Roboto,sans-serif;font-size:16px}.msg{border-radius:4px;margin-top:10px;padding:12px 16px;font-size:14px;animation:.2s snk;display:none}@keyframes snk{0%{opacity:0;transform:translateY(4px)}to{opacity:1;transform:none}}.msg.ok{background:var(--grn-con);color:var(--grn)}.msg.err{background:var(--err-con);color:var(--err)}.tile-item{border-bottom:1px solid var(--outline-var);cursor:grab;user-select:none;align-items:center;gap:16px;min-height:72px;padding:0 16px;transition:background .12s;display:flex}.tile-item:last-child{border-bottom:none}.tile-item.drag-over{background:color-mix(in srgb,var(--pri)14%,transparent)}.tile-item.dragging{opacity:.4}.drag-handle{color:var(--outline);cursor:grab;flex-shrink:0;align-items:center;display:flex}.tile-body{flex:1;min-width:0}.tile-head{color:var(--on-surf);font-family:Google Sans,sans-serif;font-size:16px;font-weight:400}.tile-sub{color:var(--on-surf-var);margin-top:2px;font-size:13px}.np-wrap{display:block}.np-expand{background:var(--surf-con);max-height:0;padding:0 16px;transition:max-height .3s cubic-bezier(.4,0,.2,1),padding .3s;overflow:hidden}.np-expand.open{max-height:200px;padding:4px 16px 16px}.dim-sched-wrap{max-height:0;opacity:0;overflow:hidden;transition:max-height .35s cubic-bezier(.4,0,.2,1),opacity .25s ease}.dim-sched-wrap.open{max-height:260px;opacity:1;transition:max-height .35s cubic-bezier(.4,0,.2,1),opacity .3s ease .05s}.np-status-row{color:var(--on-surf-var);align-items:center;gap:8px;padding:10px 0 12px;font-size:13px;display:flex}.seg{border:1px solid var(--outline);border-radius:100px;margin-top:4px;display:flex;overflow:hidden}.sb{color:var(--on-surf-var);border:none;border-right:1px solid var(--outline);cursor:pointer;letter-spacing:.1px;background:0 0;flex:1;padding:8px 6px;font-family:Google Sans,sans-serif;font-size:12px;font-weight:500;transition:background .15s,color .15s}.sb:last-child{border-right:none}.sb.on{background:var(--sec-con);color:var(--on-sec-con)}.dur-row{align-items:center;gap:8px;padding-top:2px;display:flex}.dur-row input[type=range]{accent-color:var(--pri);cursor:pointer;flex:1}.dur-val{color:var(--pri);text-align:right;min-width:32px;font-size:12px;font-weight:500}.drag-hint{text-align:center;color:var(--outline);padding:8px 0 4px;font-size:12px}.bottom-sheet{position:fixed;bottom:0;left:0;right:0;background:var(--surf-high);border-radius:28px 28px 0 0;z-index:101;padding:0 0 env(safe-area-inset-bottom);transform:translateY(100%);transition:transform .35s cubic-bezier(.2,0,0,1);max-width:600px;margin:0 auto}.bottom-sheet.open{transform:translateY(0)}.bs-handle{width:32px;height:4px;background:var(--on-surf-var);border-radius:2px;margin:12px auto 0;opacity:.4}.bs-title{font-family:Google Sans,sans-serif;font-size:18px;color:var(--on-surf);padding:16px 24px 4px;font-weight:500}.bs-body{padding:8px 24px 24px;display:flex;flex-direction:column;gap:20px}.bs-row{display:flex;align-items:center;justify-content:space-between;gap:12px}.bs-label{color:var(--on-surf);font-family:Google Sans,sans-serif;font-size:15px}.bs-sub{color:var(--on-surf-var);font-size:12px;margin-top:2px}.simple-slider{accent-color:var(--pri);cursor:pointer;flex:1}.br-val{color:var(--pri);text-align:right;min-width:32px;font-size:12px;font-weight:500}.time-input{background:var(--surf-con);border:1.5px solid var(--outline-var);border-radius:12px;color:var(--on-surf);font-size:16px;padding:8px 12px 8px 12px;width:110px;text-align:center;outline:none;font-family:Google Sans,sans-serif;color-scheme:dark}.time-input:focus{border-color:var(--pri)}.time-input::-webkit-calendar-picker-indicator{background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='16' height='16' viewBox='0 0 24 24' fill='%23cac4d0'%3E%3Cpath d='M12 2C6.5 2 2 6.5 2 12s4.5 10 10 10 10-4.5 10-10S17.5 2 12 2zm0 18c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8zm.5-13H11v6l5.25 3.15.75-1.23-4.5-2.67V7z'/%3E%3C/svg%3E");cursor:pointer}#s-bright .card .li{border-bottom:none}#s-bright #dim-sched .li{border-bottom:none}#pgrid .tile-item{border-bottom:none}.bs-sep{height:1px;background:var(--outline-var);margin:4px 0}.np-gear-btn{background:0 0;border:none;color:var(--on-surf-var);cursor:pointer;border-radius:50%;width:36px;height:36px;flex-shrink:0;display:inline-flex;align-items:center;justify-content:center;transition:background .15s}.np-gear-btn:hover{background:color-mix(in srgb,var(--on-surf)10%,transparent)}.np-gear-btn:active{background:color-mix(in srgb,var(--on-surf)16%,transparent)}.np-gear-btn{background:0 0;border:none;color:var(--on-surf-var);cursor:pointer;border-radius:50%;width:36px;height:36px;flex-shrink:0;display:inline-flex;align-items:center;justify-content:center;transition:background .15s}.np-gear-btn:hover{background:color-mix(in srgb,var(--on-surf)10%,transparent)}.np-gear-btn:active{background:color-mix(in srgb,var(--on-surf)16%,transparent)}.sw-toggle-btn.playing{color:var(--pri);background:color-mix(in srgb,var(--pri)16%,transparent)}.md-scrim{background:#00000066;pointer-events:none;opacity:0;transition:opacity .25s;position:fixed;inset:0;z-index:100}.md-scrim.open{opacity:1;pointer-events:auto}.md-dialog{background:var(--surf-high);border-radius:28px;box-shadow:0 4px 32px #0006;max-width:360px;width:calc(100% - 48px);position:fixed;left:50%;top:50%;transform:translate(-50%,-50%) scale(.9);opacity:0;pointer-events:none;transition:opacity .25s,transform .25s cubic-bezier(.2,0,0,1);z-index:101;overflow:hidden}.md-dialog.open{opacity:1;pointer-events:auto;transform:translate(-50%,-50%) scale(1)}.mdd-head{align-items:center;gap:16px;padding:24px 24px 16px;display:flex}.mdd-icon{background:var(--pri-con);border-radius:50%;color:var(--pri);flex-shrink:0;justify-content:center;align-items:center;width:40px;height:40px;display:flex}.mdd-title{color:var(--on-surf);font-family:Google Sans,sans-serif;font-size:18px;font-weight:500;flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.mdd-body{padding:0 24px 12px}.mdd-tf-wrap{background:var(--surf-var);border-bottom:2px solid var(--outline);border-radius:4px 4px 0 0;padding:0 16px;margin-top:4px}.mdd-tf-wrap:focus-within{border-bottom-color:var(--pri)}.mdd-label{letter-spacing:.4px;color:var(--on-surf-var);padding-top:8px;font-size:12px;font-weight:500;display:block}.mdd-tf-wrap:focus-within .mdd-label{color:var(--pri)}.mdd-input{width:100%;color:var(--on-surf);background:0 0;border:none;outline:none;padding:6px 0 10px;font-family:Roboto,sans-serif;font-size:16px}.mdd-actions{gap:8px;justify-content:flex-end;padding:8px 16px 20px;display:flex}#home-bar{background-color:var(--bg);background-image:linear-gradient(180deg,color-mix(in srgb,var(--pri-con) 50%,transparent) 0%,var(--bg) 100%)}.wave-hand{display:inline-block;transform-origin:50% 82%;animation:wave-once 1.2s ease-in-out 1}@keyframes wave-once{0%{transform:rotate(0deg)}10%{transform:rotate(14deg)}20%{transform:rotate(-8deg)}30%{transform:rotate(14deg)}40%{transform:rotate(-4deg)}50%{transform:rotate(10deg)}60%,100%{transform:rotate(0deg)}}.greet-wrap{position:relative;display:inline-flex;align-items:center;justify-content:center;width:26px;height:26px;vertical-align:middle;cursor:pointer}.greet-icon{position:absolute;inset:0;display)pl1"
R"pl2(:flex;align-items:center;justify-content:center;opacity:1;transition:opacity .2s}.greet-icon.hidden-icon{opacity:0;pointer-events:none}#display-card .li{border-bottom:none}#periferice-card .li{border-bottom:none}#nlist .li{border-bottom:none}#display-settings-card .li{border-bottom:none}.tile-item:hover{background:color-mix(in srgb,var(--on-surf)6%,transparent)}.cvx-wrap{display:flex;flex-direction:column;gap:10px}.cvx-grid{display:grid;grid-template-columns:repeat(32,1fr);grid-template-rows:repeat(8,1fr);gap:2px;width:100%;aspect-ratio:32/8;background:var(--surf-con);border-radius:12px;padding:8px;touch-action:none;user-select:none;-webkit-user-select:none}.cvx-px{border-radius:50%;background:var(--surf-var);transition:background .06s;cursor:pointer}.cvx-px.on{background:var(--pri)}.cvx-actions{display:flex;gap:8px;justify-content:flex-end}.cvx-clear-btn{background:0 0;border:1px solid var(--outline-var);color:var(--on-surf-var);cursor:pointer;letter-spacing:.1px;border-radius:100px;padding:6px 16px;font-family:Google Sans,sans-serif;font-size:12px;font-weight:500}.cvx-clear-btn:hover{background:color-mix(in srgb,var(--on-surf)8%,transparent)}</style><body><div class="screen active" id=s-home><div class=top-bar id=home-bar style="flex-direction:column;align-items:center;justify-content:center;min-height:0;padding:40px 16px 28px;gap:0"><div class=home-title id=home-title>Octoglow</div><span id=home-sub style=display:none></span></div><div class=content><span id=h-ip style=display:none></span><div class=sl>Conexiune</div><div class=card><div onclick="go('s-wifi')" class=li><div class="lic lc-pur"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4 2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg></div><div class=li-body><div class=li-head>WiFi</div><div class=li-sub id=h-ssid>Se incarca…</div></div><div class=li-trail><span class=chevron><svg viewbox="0 0 24 24" fill=currentColor height=18 width=18><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span></div></div><div onclick="go('s-ap')" class=li style="border-bottom:none"><div class="lic lc-blu"><svg viewbox="0 -960 960 960" fill=currentColor height=20 width=20><path d="M200-120q-33 0-56.5-23.5T120-200v-160q0-33 23.5-56.5T200-440h400v-160h80v160h80q33 0 56.5 23.5T840-360v160q0 33-23.5 56.5T760-120H200Zm0-80h560v-160H200v160Zm108.5-51.5Q320-263 320-280t-11.5-28.5Q297-320 280-320t-28.5 11.5Q240-297 240-280t11.5 28.5Q263-240 280-240t28.5-11.5Zm140 0Q460-263 460-280t-11.5-28.5Q437-320 420-320t-28.5 11.5Q380-297 380-280t11.5 28.5Q403-240 420-240t28.5-11.5Zm140 0Q600-263 600-280t-11.5-28.5Q577-320 560-320t-28.5 11.5Q520-297 520-280t11.5 28.5Q543-240 560-240t28.5-11.5ZM570-630l-58-58q26-24 58-38t70-14q38 0 70 14t58 38l-58 58q-14-14-31.5-22t-38.5-8q-21 0-38.5 8T570-630ZM470-730l-56-56q44-44 102-69t124-25q66 0 124 25t102 69l-56 56q-33-33-76.5-51.5T640-800q-50 0-93.5 18.5T470-730ZM200-200v-160 160Z"/></svg></div><div class=li-body><div class=li-head>AP</div><div class=li-sub id=h-ap-sub>Configureaza reteaua proprie</div></div><div class=li-trail><span class=chevron><svg viewbox="0 0 24 24" fill=currentColor height=18 width=18><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span></div></div></div><div class=sl>Display</div><div class=card id=display-card><div onclick="go('s-tiles')" class=li><div class="lic lc-tea"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M3 13h8V3H3v10zm0 8h8v-6H3v6zm10 0h8V11h-8v10zm0-18v6h8V3h-8z"/></svg></div><div class=li-body><div class=li-head>Tile Manager</div><div class=li-sub id=h-tiles-sub>Ora · Data · Temperatura</div></div><div class=li-trail><span class=chevron><svg viewbox="0 0 24 24" fill=currentColor height=18 width=18><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span></div></div><div onclick="go('s-langmgr')" class=li><div class="lic lc-grn"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M12.87 15.07l-2.54-2.51.03-.03A17.52 17.52 0 0 0 14.07 6H17V4h-7V2H8v2H1v2h11.17C11.5 7.92 10.44 9.75 9 11.35 8.07 10.32 7.3 9.19 6.69 8h-2c.73 1.63 1.73 3.17 2.98 4.56l-5.09 5.02L4 19l5-5 3.11 3.11.76-2.04zM18.5 10h-2L12 22h2l1.12-3h4.75L21 22h2l-4.5-12zm-2.62 7l1.62-4.33L19.12 17h-3.24z"/></svg></div><div class=li-body><div class=li-head>Language Manager</div><div class=li-sub id=h-lang-sub>Limba afisare meteo</div></div><div class=li-trail><span class=chevron><svg viewbox="0 0 24 24" fill=currentColor height=18 width=18><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span></div></div><div onclick="go('s-bright')" class=li style="border-bottom:none"><div class="lic lc-amb"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M12 7c-2.76 0-5 2.24-5 5s2.24 5 5 5 5-2.24 5-5-2.24-5-5-5zM2 13h2c.55 0 1-.45 1-1s-.45-1-1-1H2c-.55 0-1 .45-1 1s.45 1 1 1zm18 0h2c.55 0 1-.45 1-1s-.45-1-1-1h-2c-.55 0-1 .45-1 1s.45 1 1 1zM11 2v2c0 .55.45 1 1 1s1-.45 1-1V2c0-.55-.45-1-1-1s-1 .45-1 1zm0 18v2c0 .55.45 1 1 1s1-.45 1-1v-2c0-.55-.45-1-1-1s-1 .45-1 1zM5.99 4.58c-.39-.39-1.03-.39-1.41 0-.39.39-.39 1.02 0 1.41l1.06 1.06c.39.39 1.03.39 1.41 0 .39-.39.39-1.02 0-1.41L5.99 4.58zm12.37 12.37c-.39-.39-1.03-.39-1.41 0-.39.39-.39 1.02 0 1.41l1.06 1.06c.39.39 1.03.39 1.41 0 .39-.39.39-1.02 0-1.41l-1.06-1.06zm1.06-10.96c.39-.39.39-1.02 0-1.41-.39-.39-1.03-.39-1.41 0l-1.06 1.06c-.39.39-.39 1.02 0 1.41.39.39 1.03.39 1.41 0l1.06-1.06zM5.99 19.41c.39.39 1.03.39 1.41 0l1.06-1.06c.39-.39.39-1.02 0-1.41-.39-.39-1.03-.39-1.41 0l-1.06 1.06c-.38.39-.38 1.03 0 1.41z"/></svg></div><div class=li-body><div class=li-head>Luminozitate</div><div class=li-sub id=h-bright-sub>Nivel 4</div></div><div class=li-trail><span class=chevron><svg viewbox="0 0 24 24" fill=currentColor height=18 width=18><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span></div></div></div><div class=sl>Periferice</div><div class=card id=periferice-card><div onclick="go('s-buzzer')" class=li><div class="lic lc-amb"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M3 9v6h4l5 5V4L7 9H3zm13.5 3c0-1.77-1.02-3.29-2.5-4.03v8.05c1.48-.73 2.5-2.25 2.5-4.02zM14 3.23v2.06c2.89.86 5 3.54 5 6.71s-2.11 5.85-5 6.71v2.06c4.01-.91 7-4.49 7-8.77s-2.99-7.86-7-8.77z"/></svg></div><div class=li-body><div class=li-head>Buzzer</div><div class=li-sub id=h-bsub-home>Volum si tonuri presetate</div></div><div class=li-trail><span class=chevron><svg viewbox="0 0 24 24" fill=currentColor height=18 width=18><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span></div></div><div onclick="go('s-touch')" class=li style="border-bottom:none"><div class="lic lc-grn"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M9 11.24V7.5C9 6.12 10.12 5 11.5 5S14 6.12 14 7.5v3.74c1.21-.81 2-2.18 2-3.74C16 5.01 13.99 3 11.5 3S7 5.01 7 7.5c0 1.56.79 2.93 2 3.74zm9.84 4.63l-4.54-2.26c-.17-.07-.35-.11-.54-.11H13v-6c0-.83-.67-1.5-1.5-1.5S10 6.67 10 7.5v10.74l-3.43-.72c-.08-.01-.15-.03-.24-.03-.31 0-.59.13-.79.33l-.79.8 4.94 4.94c.27.27.65.44 1.06.44h6.79c.75 0 1.33-.55 1.44-1.28l.75-5.27c.01-.07.02-.14.02-.2 0-.62-.38-1.16-.91-1.38z"/></svg></div><div class=li-body><div class=li-head>Touch Sensor</div><div class=li-sub>Tap si Double Tap actions</div></div><div class=li-trail><span class=chevron><svg viewbox="0 0 24 24" fill=currentColor height=18 width=18><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span></div></div></div><div class=sl>Despre</div><div class=card><div onclick="go('s-about')" class=li><div class="lic lc-blu"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M11 7h2v2h-2zm0 4h2v6h-2zm1-9C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 18c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8z"/></svg></div><div class=li-body><div class=li-head>About</div><div class=li-sub>Versiune, IP, uptime</div></div><div class=li-trail><span class=chevron><svg viewbox="0 0 24 24" fill=currentColor height=18 width=18><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span></div></div></div></div></div><div class=screen id=s-about><div class=top-bar><button onclick="go('s-home')" class=bar-lead><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg></button><div class=bar-text><div class=bar-title>About</div></div></div><div class=content style="display:flex;flex-direction:column;align-items:center;text-align:center"><div style="width:84px;height:84px;border-radius:50%;background:var(--pri-con);color:var(--pri);display:flex;align-items:center;justify-content:center;margin:24px 0 16px"><svg viewbox="0 0 24 24" fill=currentColor height=44 width=44><path d="M12 2C6.5 2 2 6.5 2 12s4.5 10 10 10 10-4.5 10-10S17.5 2 12 2zm0 3c1.66 0 3 1.34 3 3s-1.34 3-3 3-3-1.34-3-3 1.34-3 3-3zm0 14.2c-2.5 0-4.71-1.28-6-3.22.03-1.99 4-3.08 6-3.08 1.99 0 5.97 1.09 6 3.08-1.29 1.94-3.5 3.22-6 3.22z"/></svg></div><div style="font-family:Google Sans,sans-serif;font-size:28px;color:var(--on-surf);font-weight:500">Octoglow</div><div style="color:var(--on-surf-var);font-size:13px;margin-top:4px;letter-spacing:.3px">Firmware pentru ceas inteligent)pl2"
R"pl3( ESP32</div><div class=sl style="width:100%;text-align:left;margin-top:24px">Info</div><div class=card style="width:100%;text-align:left"><div class="li static"><div class="lic lc-blu"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M11.99 2C6.47 2 2 6.48 2 12s4.47 10 9.99 10C17.52 22 22 17.52 22 12S17.52 2 11.99 2zm6.93 6h-2.95c-.32-1.25-.78-2.45-1.38-3.56 1.84.63 3.37 1.9 4.33 3.56zM12 4.04c.83 1.2 1.48 2.53 1.91 3.96h-3.82c.43-1.43 1.08-2.76 1.91-3.96zM4.26 14C4.1 13.36 4 12.69 4 12s.1-1.36.26-2h3.38c-.08.66-.14 1.32-.14 2 0 .68.06 1.34.14 2H4.26zm.82 2h2.95c.32 1.25.78 2.45 1.38 3.56-1.84-.63-3.37-1.9-4.33-3.56zm2.95-8H5.08c.96-1.66 2.49-2.93 4.33-3.56C8.81 5.55 8.35 6.75 8.03 8zM12 19.96c-.83-1.2-1.48-2.53-1.91-3.96h3.82c-.43 1.43-1.08 2.76-1.91 3.96zM14.34 14H9.66c-.09-.66-.16-1.32-.16-2 0-.68.07-1.35.16-2h4.68c.09.65.16 1.32.16 2 0 .68-.07 1.34-.16 2zm.25 5.56c.6-1.11 1.06-2.31 1.38-3.56h2.95c-.96 1.66-2.49 2.93-4.33 3.56zM16.36 14c.08-.66.14-1.32.14-2 0-.68-.06-1.34-.14-2h3.38c.16.64.26 1.31.26 2s-.1 1.36-.26 2h-3.38z"/></svg></div><div class=li-body><div class=li-head>Adresa IP</div><div class="li-sub ip-val" id=about-ip>—</div></div></div><div class="li static"><div class="lic lc-pur"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M11.99 2C6.47 2 2 6.48 2 12s4.47 10 9.99 10C17.52 22 22 17.52 22 12S17.52 2 11.99 2zM12 20c-4.42 0-8-3.58-8-8s3.58-8 8-8 8 3.58 8 8-3.58 8-8 8zm.5-13H11v6l5.25 3.15.75-1.23-4.5-2.67V7z"/></svg></div><div class=li-body><div class=li-head>Version</div><div class=li-sub id=about-version>—</div></div></div><div class="li static" style="border-bottom:none"><div class="lic lc-tea"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M12 8a4 4 0 0 0-4 4 1 1 0 0 0 2 0 2 2 0 0 1 2-2 1 1 0 0 0 0-2zm0-6C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 18c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8z"/></svg></div><div class=li-body><div class=li-head>Uptime</div><div class=li-sub id=about-uptime>—</div></div></div></div><div class=sl style="width:100%;text-align:left;margin-top:24px">Software</div><div class=card style="width:100%;text-align:left"><div class="li static"><div class="lic lc-grn"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M11.99 2C6.47 2 2 6.48 2 12s4.47 10 9.99 10C17.52 22 22 17.52 22 12S17.52 2 11.99 2zM12 20c-4.42 0-8-3.58-8-8s3.58-8 8-8 8 3.58 8 8-3.58 8-8 8zm.5-13H11v6l5.25 3.15.75-1.23-4.5-2.67V7z"/></svg></div><div class=li-body><div class=li-head>Version</div><div class=li-sub id=sw-version>—</div></div></div><div onclick="openSwUpdateDlg()" class=li style="border-bottom:none"><div class="lic lc-blu"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M12 4V1L8 5l4 4V6c3.31 0 6 2.69 6 6 0 1.01-.25 1.97-.7 2.8l1.46 1.46C19.54 15.03 20 13.57 20 12c0-4.42-3.58-8-8-8zm0 14c-3.31 0-6-2.69-6-6 0-1.01.25-1.97.7-2.8L5.24 7.74C4.46 8.97 4 10.43 4 12c0 4.42 3.58 8 8 8v3l4-4-4-4v3z"/></svg></div><div class=li-body><div class=li-head>Software Updates</div><div class=li-sub>Verifica daca exista o versiune noua</div></div><div class=li-trail><span class=chevron><svg viewbox="0 0 24 24" fill=currentColor height=18 width=18><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span></div></div></div><div class=sl style="width:100%;text-align:left">User</div><div class=card style="width:100%;text-align:left"><div onclick="openUserAccountDlg()" class=li style="border-bottom:none"><div class="lic lc-pur"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M12 12c2.21 0 4-1.79 4-4s-1.79-4-4-4-4 1.79-4 4 1.79 4 4 4zm0 2c-2.67 0-8 1.34-8 4v2h16v-2c0-2.66-5.33-4-8-4z"/></svg></div><div class=li-body><div class=li-head>User Account</div><div class=li-sub>Schimba utilizatorul sau parola</div></div><div class=li-trail><span class=chevron><svg viewbox="0 0 24 24" fill=currentColor height=18 width=18><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span></div></div></div></div></div><div class=md-scrim id=useracct-scrim onclick=closeUserAccountDlg()></div><div class=md-dialog id=useracct-dlg><div class=mdd-head><div class=mdd-icon><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M12 12c2.21 0 4-1.79 4-4s-1.79-4-4-4-4 1.79-4 4 1.79 4 4 4zm0 2c-2.67 0-8 1.34-8 4v2h16v-2c0-2.66-5.33-4-8-4z"/></svg></div><div class=mdd-title>User Account</div></div><div class=mdd-body><div class=mdd-tf-wrap><label class=mdd-label>User</label><input class=mdd-input id=ua-user-in autocomplete=username maxlength=32></div><div class=mdd-tf-wrap id=ua-curpass-wrap style=display:none><label class=mdd-label>Parola actuala</label><input class=mdd-input id=ua-curpass-in type=password autocomplete=current-password maxlength=64></div><div class=mdd-tf-wrap><label class=mdd-label>New Password</label><input class=mdd-input id=ua-newpass-in type=password autocomplete=new-password maxlength=64 placeholder="Lasa gol pentru a pastra parola actuala"></div><div class=msg id=ua-msg></div></div><div class=mdd-actions><button class="mbtn mbtn-ton" onclick=closeUserAccountDlg()>Anuleaza</button><button class="mbtn mbtn-fill" id=ua-save-btn onclick=saveUserAccount()>Salveaza</button></div></div><div class=md-scrim id=sw-update-scrim onclick=closeSwUpdateDlg()></div><div class=md-dialog id=sw-update-dlg><div class=mdd-head><div class=mdd-icon><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M12 4V1L8 5l4 4V6c3.31 0 6 2.69 6 6 0 1.01-.25 1.97-.7 2.8l1.46 1.46C19.54 15.03 20 13.57 20 12c0-4.42-3.58-8-8-8zm0 14c-3.31 0-6-2.69-6-6 0-1.01.25-1.97.7-2.8L5.24 7.74C4.46 8.97 4 10.43 4 12c0 4.42 3.58 8 8 8v3l4-4-4-4v3z"/></svg></div><div class=mdd-title id=sw-update-title>Software Updates</div></div><div class=mdd-body><div id=sw-searching style="display:flex;flex-direction:column;align-items:center;justify-content:center;padding:24px 0;gap:16px"><span class=spin-ring></span><div style="color:var(--on-surf-var);font-size:14px">Searching for updates...</div></div><div id=sw-uptodate style="display:none;text-align:center;padding:20px 0;color:var(--on-surf);font-family:Google Sans,sans-serif;font-size:15px">You are up to date</div><div id=sw-checkerr style="display:none;text-align:center;padding:20px 0;color:var(--err);font-size:14px">Nu s-a putut verifica actualizarea. Incearca din nou.</div><div id=sw-newupdate style="display:none"><div style="color:var(--on-surf);font-family:Google Sans,sans-serif;font-size:15px;font-weight:500;margin-bottom:6px">There is a new update available</div><div id=sw-ver-compare style="color:var(--pri);font-family:Google Sans,sans-serif;font-size:19px;font-weight:500;margin-bottom:14px">0.1 → 0.2</div><div id=sw-update-desc style="color:var(--on-surf-var);font-size:13px;line-height:1.6;max-height:180px;overflow-y:auto;white-space:pre-wrap"></div></div><div id=sw-installing style="display:none;flex-direction:column;align-items:center;justify-content:center;padding:24px 0;gap:16px"><span class=spin-ring></span><div style="color:var(--on-surf-var);font-size:14px" id=sw-install-status>Se instaleaza update-ul...</div></div></div><div class=mdd-actions id=sw-actions-uptodate style="display:none"><button class="mbtn mbtn-fill" onclick=closeSwUpdateDlg()>OK</button></div><div class=mdd-actions id=sw-actions-newupdate style="display:none"><button class="mbtn mbtn-ton" onclick=closeSwUpdateDlg()>Cancel</button><button class="mbtn mbtn-fill" onclick=installSwUpdate()>Install</button></div></div><div class=screen id=s-bright><div class=top-bar><button onclick="go('s-home')" class=bar-lead><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg></button><div class=bar-text><div class=bar-title>Luminozitate</div></div></div><div class=content><div class=card><div class="li static"><div class="lic lc-amb"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M12 7c-2.76 0-5 2.24-5 5s2.24 5 5 5 5-2.24 5-5-2.24-5-5-5zM2 13h2c.55 0 1-.45 1-1s-.45-1-1-1H2c-.55 0-1 .45-1 1s.45 1 1 1zm18 0h2c.55 0 1-.45 1-1s-.45-1-1-1h-2c-.55 0-1 .45-1 1s.45 1 1 1zM11 2v2c0 .55.45 1 1 1s1-.45 1-1V2c0-.55-.45-1-1-1s-1 .45-1 1zm0 18v2c0 .55.45 1 1 1s1-.45 1-1v-2c0-.55-.45-1-1-1s-1 .45-1 1zM5.99 4.58c-.39-.39-1.03-.39-1.41 0-.39.39-.39 1.02 0 1.41l1.06 1.06c.39.39 1.03.39 1.41 0 .39-.39.39-1.02 0-1.41L5.99 4.58zm12.37 12.37c-.39-.39-1.03-.39-1.41 0-.39.39-.39 1.02 0 1.41l1.06 1.06c.39.39 1.03.39 1.41 0 .39-.39.39-1.02 0-1.41l-1.06-1.06zm1.06-10.96c.39-.39.39-1.02 0-1.41-.39-.39-1.03-.39-1.41 0l-1.06 1.06c-.39.39-.39 1.02 0 1.41.39.39 1.03.39 1.41 0l1.06-1.06zM5.99 19.41c.39.39 1.03.39 1.41 0l1.06-1.06c.39-.39.39-1.02 0-1.41-.39-.39-1.03-.39-1.41 0l-1.06 1.06c-.38.39-.38 1.03 0 1.41z"/></svg></div><div class=li-body><div class=li-head id=bright-screen-lbl>Nivel 4</div><div class=li-sub>0 – 16</div></div></div><div class="li static" style="padding:4px 16px 12px"><div class="dur-row" style="width:100%"><input type=range min=0 max=16 step=1 class=simple-slider id=bright-slider oninput=onBrightInput(this.value)><span class="dur-val" id=br-val>4</span></div></div></div><div class=card><div class="li static"><div class="lic lc-pur"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M12 3a9 9 0 1 0 9 9c0-.46-.04-.92-.1-1.36-.98 1.37-2.58 2.26-4.4 2.26-2.98 0-5.4-2.42-5.4-5.4 0-1.81.89-3.42 2.26-4.4-.44-.06-.9-.1-1.36-.1z"/></svg></div><div class=li-body><div class=li-head>Dim automat</div><div class=li-sub>Reduce luminozitatea noaptea</div></div><div class=li-trail><div class=sw onclick=toggleDimAuto()><input type=checkbox id=dim-auto-cb><span class=sw-track></span><span class=sw-thumb></span></div></div></div><div id=dim-sched class=dim-sched-wrap><div class="li static"><div class=li-body style="padding-left:0"><div class=li-head>Interval</div><div class=li-sub>De la / pana la</div></div><div class=li-trail style="display:flex;align-items:center;gap:8px"><input type=time class=time-input id=dim-from value=22:00><span style="color:var(--on-surf-var)">–</span><input type=time class=time-input id=dim-to value=07:00></div></div><div class="li static" style="padding:4px 16px 8px;flex-direction:column;align-items:flex-start;gap:8px"><div class=li-head>Luminozitate noapte</div><div class="dur-row" style="width:100%"><input type=range min=0 max=16 step=1 class=simple-slider id=dim-level-slider value=1 oninput=onDimLevelInput(this.value)><span class="dur-val" id=dim-level-lbl>1</span></div></div></div></div></div></div><div class=screen id=s-buzzer><div class=top-bar><button onclick="go('s-home')" class=bar-lead><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg></button><div class=bar-text><div class=bar-title>Buzzer Settings</div></div><div class=sw style="margin-right:8px" onclick=toggleBuzzer()><input checked id=btog-cb type=checkbox><span class=sw-track></span><span class=sw-thumb></span></div></div><div class=content><span id=h-bsub style=display:none>Beep la schimbarea tile-ului</span><div class=sl>Event Sound Assignment</div><div class=card id=event-sound-card></div></div></div><div class=screen id=s-touch><div class=top-bar><button onclick="go('s-home')" class=bar-lead><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg></button><div class=bar-text><div class=b)pl3"
R"pl4(ar-title>Touch Sensor Actions</div></div></div><div class=content><div class=sl>Gesture Actions</div><div class=card id=touch-action-card></div></div></div><div class=screen id=s-wifi><div class=top-bar><button onclick="go('s-home')" class=bar-lead><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg></button><div class=bar-text><div class=bar-title>WiFi</div></div><button class=bar-lead onclick=doScan() title=Scan id=scan-btn><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M17.65 6.35A7.96 7.96 0 0012 4C7.58 4 4 7.58 4 12s3.58 8 8 8 8-3.58 8-8h-2c0 3.31-2.69 6-6 6s-6-2.69-6-6 2.69-6 6-6c1.66 0 3.14.69 4.22 1.78L13 11h7V4l-2.35 2.35z"/></svg></button><button class=bar-lead onclick=openDisconDialog() title="Comuta in modul AP" id=switch-ap-btn style=display:none><svg viewbox="0 -960 960 960" fill=currentColor height=24 width=24><path d="M200-120q-33 0-56.5-23.5T120-200v-160q0-33 23.5-56.5T200-440h400v-160h80v160h80q33 0 56.5 23.5T840-360v160q0 33-23.5 56.5T760-120H200Zm0-80h560v-160H200v160Zm108.5-51.5Q320-263 320-280t-11.5-28.5Q297-320 280-320t-28.5 11.5Q240-297 240-280t11.5 28.5Q263-240 280-240t28.5-11.5Zm140 0Q460-263 460-280t-11.5-28.5Q437-320 420-320t-28.5 11.5Q380-297 380-280t11.5 28.5Q403-240 420-240t28.5-11.5Zm140 0Q600-263 600-280t-11.5-28.5Q577-320 560-320t-28.5 11.5Q520-297 520-280t11.5 28.5Q543-240 560-240t28.5-11.5ZM570-630l-58-58q26-24 58-38t70-14q38 0 70 14t58 38l-58 58q-14-14-31.5-22t-38.5-8q-21 0-38.5 8T570-630ZM470-730l-56-56q44-44 102-69t124-25q66 0 124 25t102 69l-56 56q-33-33-76.5-51.5T640-800q-50 0-93.5 18.5T470-730ZM200-200v-160 160Z"/></svg></button><button class=bar-lead onclick=openSwitchWifiDialog() title="Comuta in modul WiFi" id=switch-wifi-btn style=display:none><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4 2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg></button></div><div class=content><span id=w-ssid style=display:none></span><span id=w-ip style=display:none></span><div class=sl>Retele WiFi</div><div class=wlist id=nlist><div class=scan-hint><span class=spin-ring></span>Se cauta retele…</div></div><div class=msg id=wifi-msg></div><div class=md-scrim id=md-scrim onclick=closeDialog()></div><div class=md-dialog id=conn-dlg><div class=mdd-head><div class=mdd-icon><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4 2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg></div><div class=mdd-title id=dlg-ssid-name>Conecteaza la retea</div></div><div class=mdd-body id=dlg-pass-body><div class=mdd-tf-wrap><label class=mdd-label>Parola WiFi</label><input class=mdd-input placeholder="Introdu parola" autocomplete=off id=pass-in type=password></div></div><div class=mdd-actions><button class="mbtn mbtn-ton" onclick=closeDialog()>Anuleaza</button><button class="mbtn mbtn-fill" id=conn-btn onclick=doConnect()>Conecteaza</button></div></div></div></div><div class=screen id=s-ap><div class=top-bar><button onclick="go('s-home')" class=bar-lead><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg></button><div class=bar-text><div class=bar-title>Access Point</div></div><button class=bar-lead onclick=openDisconDialog() title="Comuta in modul AP" id=ap-switch-btn style=display:none><svg viewbox="0 -960 960 960" fill=currentColor height=24 width=24><path d="M200-120q-33 0-56.5-23.5T120-200v-160q0-33 23.5-56.5T200-440h400v-160h80v160h80q33 0 56.5 23.5T840-360v160q0 33-23.5 56.5T760-120H200Zm0-80h560v-160H200v160Zm108.5-51.5Q320-263 320-280t-11.5-28.5Q297-320 280-320t-28.5 11.5Q240-297 240-280t11.5 28.5Q263-240 280-240t28.5-11.5Zm140 0Q460-263 460-280t-11.5-28.5Q437-320 420-320t-28.5 11.5Q380-297 380-280t11.5 28.5Q403-240 420-240t28.5-11.5Zm140 0Q600-263 600-280t-11.5-28.5Q577-320 560-320t-28.5 11.5Q520-297 520-280t11.5 28.5Q543-240 560-240t28.5-11.5ZM570-630l-58-58q26-24 58-38t70-14q38 0 70 14t58 38l-58 58q-14-14-31.5-22t-38.5-8q-21 0-38.5 8T570-630ZM470-730l-56-56q44-44 102-69t124-25q66 0 124 25t102 69l-56 56q-33-33-76.5-51.5T640-800q-50 0-93.5 18.5T470-730ZM200-200v-160 160Z"/></svg></button></div><div class=content><div class=sl>Configurare Retea AP</div><div class=card style="padding:6px 16px 18px"><div class=mdd-tf-wrap><label class=mdd-label>Nume retea (SSID)</label><input class=mdd-input id=ap-ssid-in placeholder="ex: Octoglow" maxlength=32 autocomplete=off></div><div class=mdd-tf-wrap style="margin-top:10px;display:flex;align-items:center;padding-right:8px"><div style="flex:1"><label class=mdd-label>Parola (minim 8 caractere; lasa gol pentru a pastra parola curenta)</label><input class=mdd-input id=ap-pass-in type=password placeholder="Parola retea" maxlength=64 autocomplete=off oninput=onApPassInput()></div><button type=button id=ap-pass-eye onclick=toggleApPassVis() style="background:none;border:none;cursor:pointer;padding:4px;color:var(--on-surf-var);flex-shrink:0;display:flex;align-items:center;align-self:flex-end;margin-bottom:6px" title="Arata/ascunde parola"><svg id=ap-pass-eye-icon width=20 height=20 viewBox="0 0 24 24" fill="currentColor"><path d="M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zM12 17c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z"/></svg></button></div><div class=msg id=ap-msg></div><button class="mbtn mbtn-fill" style="width:100%;margin-top:16px" onclick=saveApSettings() id=ap-save-btn>Salveaza</button></div></div></div><div class=md-scrim id=discon-scrim onclick=closeDisconDialog()></div><div class=md-dialog id=discon-dlg><div class=mdd-head><div class=mdd-icon style="background:var(--err-con);color:var(--err)"><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4 2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg></div><div class=mdd-title id=discon-ssid-name>Comutare in modul AP</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:14px;padding:4px 0 8px">Doresti sa opresti conexiunea WiFi si sa pornesti modul Access Point?</div></div><div class=mdd-actions><button class="mbtn mbtn-ton" onclick=closeDisconDialog()>Anuleaza</button><button class="mbtn" style="background:var(--err-con);color:var(--err);flex:1;justify-content:center;align-items:center;gap:6px;height:40px;padding:0 20px;font-family:Google Sans,sans-serif;font-size:14px;font-weight:500;display:inline-flex;border:none;border-radius:100px;cursor:pointer" onclick=doDisconnect()>Confirma</button></div></div><div class=md-scrim id=switch-wifi-scrim onclick=closeSwitchWifiDialog()></div><div class=md-dialog id=switch-wifi-dlg><div class=mdd-head><div class=mdd-icon><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4 2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg></div><div class=mdd-title>Comutare in modul WiFi</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:14px;padding:4px 0 8px">Doresti sa opresti modul Access Point si sa te reconectezi la reteaua WiFi salvata anterior?</div><div class=msg id=switch-wifi-msg></div></div><div class=mdd-actions><button class="mbtn mbtn-ton" onclick=closeSwitchWifiDialog()>Anuleaza</button><button class="mbtn mbtn-fill" id=switch-wifi-confirm-btn onclick=doSwitchToWifi()>Confirma</button></div></div><div class=screen id=s-tiles><div class=top-bar><button onclick="go('s-home')" class=bar-lead><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg></button><div class=bar-text><div class=bar-title>Tile Manager</div></div></div><div class=content><div class=sl>Setari Afisare</div><div class=card id=display-settings-card><div onclick="go('s-hideicons')" class=li><div class="lic lc-tea"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zM12 17c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z"/></svg></div><div class=li-body><div class=li-head>Disable Tile Icons</div><div class=li-sub>Ascunde iconitele din stanga tile-urilor</div></div><div class=li-trail><div class=sw onclick="event.stopPropagation();toggleHideIcons()"><input type=checkbox id=hide-icons-cb><span class=sw-track></span><span class=sw-thumb></span></div><span class=chevron><svg viewbox="0 0 24 24" fill=currentColor height=18 width=18><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span></div></div><div class="li" style="border-bottom:none" onclick="go('s-scrolltype')"><div class="lic lc-tea"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M13 3c-4.42 0-8 3.58-8 8H2l3.89 3.89.07.14L10 11H7c0-3.31 2.69-6 6-6s6 2.69 6 6-2.69 6-6 6c-1.66 0-3.14-.69-4.22-1.78l-1.42 1.42C8.68 18.11 10.75 19 13 19c4.42 0 8-3.58 8-8s-3.58-8-8-8zm-1 5v5l4.28 2.54.72-1.21-3.5-2.08V8H12z"/></svg></div><div class=li-body><div class=li-head>Scroll Type</div><div class=li-sub>Modul de scroll al textului pe tile-uri</div></div><div class=li-trail><select class=evsnd-select onclick="event.stopPropagation()" onchange=setScrollType(this.value) id=scroll-type-sel style="background:var(--surf-high);color:var(--on-surf);border:1px solid var(--outline-var);border-radius:8px;padding:6px 10px;font-family:Google Sans,sans-serif;font-size:13px;max-width:150px"><option value=0 selected>Bounce</option><option value=1>Wrap</option><option value=2>Bounce + Icon</option><option value=3>Wrap + Icon</option></select><button class="np-gear-btn" onclick="event.stopPropagation();go('s-scrolltype')" title="Configureaza scroll individual per tile"><svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></button></div></div></div><div class=sl>Circuit Tiles</div><div class=card id=tgrid></div><div class=sl>Priority Tiles</div><div class=card id=pgrid></div></div><div class=md-scrim id=sett-scrim onclick=closeSettingsDlg()></div><div class=md-dialog id=sett-dlg><div class=mdd-head><div class=mdd-icon><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M11.99 2C6.47 2 2 6.48 2 12s4.47 10 9.99 10C17.52 22 22 17.52 22 12S17.52 2 11.99 2zM12 20c-4.42 0-8-3.58-8-8s3.58-8 8-8 8 3.58 8 8-3.58 8-8 8zm.5-13H11v6l5.25 3.15.75-1.23-4.5-2.67V7z"/></svg></div><div class=mdd-title>Setari Display</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:4px 0 12px;font-weight:500">Ce sa afiseze ceasul</div><div style="display:flex;flex-direction:column;gap:8px" id=sett-items-list></div><div style="height:16px"></div><div style="display:flex;justify-content:space-between;align-items:center;padding:12px 0 4px;border-top:1px solid var(--outline-var)"><div><div style="color:var(--on-surf);font-family:Google Sans,sans-serif;font-size:15px">Buzzer</div><div style="color:var(--on-surf-var);font-size:12px;margin-top:2px">Beep la schimbarea tile-ului</div></div><div class=sw onclick=toggleBuzzerSett()><input id=sett-buz-cb type=checkbox><span class=sw-track></span><span class=sw-thumb></span></div></div></div><div class=mdd-actions><button class="mbtn mbtn-fill" onclick=closeSettingsDlg()>Gata</button></div></div></div><div class=md-scrim id=np-sett-scrim onclick=closeNpSettingsDlg()></div><div class=md-dialog id=np-sett-dlg><div class=mdd-head><div class=mdd-icon><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M12 3v10.55c-.59-.34-1.27-.55-2-.55-2.21 0-4 1.79-4 4s1.79 4 4 4 4-1.79 4-4V7h4V3h-6z"/></svg></div><div class=mdd-title>Now Playing</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 12px;font-weight:500">Ce sa afiseze pe ceas</div><div class=seg style="border-radius:12px;overflow:hidden"><button class="sb on" id=npMode0 onclick="setNpMode(0)">Artist + Titlu</button><button class=sb id=npMode1 onclick="setNpMode(1)">Artist</button><button class=sb id=npMode2 onclick="setNpMode(2)">Titlu</button></div></div><div class=mdd-actions><button class="mbtn mbtn-fill" onclick=closeNp)pl4"
R"pl5(SettingsDlg()>Gata</button></div></div><div class=md-scrim id=temp-sett-scrim onclick=closeTempSettingsDlg()></div><div class=md-dialog id=temp-sett-dlg><div class=mdd-head><div class=mdd-icon><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M15 13V5c0-1.66-1.34-3-3-3S9 3.34 9 5v8c-1.21.91-2 2.37-2 4 0 2.76 2.24 5 5 5s5-2.24 5-5c0-1.63-.79-3.09-2-4z"/></svg></div><div class=mdd-title>Temperatura</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 12px;font-weight:500">Unitate de masura</div><div class=seg style="border-radius:12px;overflow:hidden"><button class="sb on" id=tempUnitC onclick="setTempUnit(0)">°C  Celsius</button><button class=sb id=tempUnitF onclick="setTempUnit(1)">°F  Fahrenheit</button></div><div style="height:16px"></div><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">Timp Afisare</div><div class=dur-row><input type=range min=2 max=30 step=1 id=temp-dur-slider oninput="onTempDurInput(this.value)" onchange="saveSettings()"><span class=dur-val id=temp-dur-val>10s</span></div></div><div class=mdd-actions><button class="mbtn mbtn-fill" onclick=closeTempSettingsDlg()>Gata</button></div></div><div class=md-scrim id=hour-sett-scrim onclick=closeHourSettingsDlg()></div><div class=md-dialog id=hour-sett-dlg><div class=mdd-head><div class=mdd-icon><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M11.99 2C6.47 2 2 6.48 2 12s4.47 10 9.99 10C17.52 22 22 17.52 22 12S17.52 2 11.99 2zM12 20c-4.42 0-8-3.58-8-8s3.58-8 8-8 8 3.58 8 8-3.58 8-8 8zm.5-13H11v6l5.25 3.15.75-1.23-4.5-2.67V7z"/></svg></div><div class=mdd-title>Ora</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 12px;font-weight:500">Format afisare</div><div class=seg style="border-radius:12px;overflow:hidden"><button class="sb on" id=hourFmt24 onclick="setHourFormat(0)">24h (13:45)</button><button class=sb id=hourFmt12 onclick="setHourFormat(1)">12h (1:45)</button></div><div style="height:16px"></div><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">Timp Afisare</div><div class=dur-row><input type=range min=2 max=30 step=1 id=hour-dur-slider oninput="onHourDurInput(this.value)" onchange="saveSettings()"><span class=dur-val id=hour-dur-val>10s</span></div></div><div class=mdd-actions><button class="mbtn mbtn-fill" onclick=closeHourSettingsDlg()>Gata</button></div></div><div class=md-scrim id=date-sett-scrim onclick=closeDateSettingsDlg()></div><div class=md-dialog id=date-sett-dlg><div class=mdd-head><div class=mdd-icon><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M20 3h-1V1h-2v2H7V1H5v2H4c-1.1 0-2 .9-2 2v16c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 18H4V8h16v13z"/></svg></div><div class=mdd-title>Data</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 12px;font-weight:500">Format afisare</div><div style="display:flex;flex-direction:column;gap:8px"><div class=seg style="border-radius:12px;overflow:hidden"><button class=sb id=dateFmt0 onclick="setDateFormat(0)">2026.06.18</button><button class=sb id=dateFmt1 onclick="setDateFormat(1)">26.06.18</button></div><div class=seg style="border-radius:12px;overflow:hidden"><button class="sb on" id=dateFmt2 onclick="setDateFormat(2)">18.06.2026</button><button class=sb id=dateFmt3 onclick="setDateFormat(3)">18.06</button></div></div><div style="height:16px"></div><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">Timp Afisare</div><div class=dur-row><input type=range min=2 max=30 step=1 id=date-dur-slider oninput="onDateDurInput(this.value)" onchange="saveSettings()"><span class=dur-val id=date-dur-val>10s</span></div></div><div class=mdd-actions><button class="mbtn mbtn-fill" onclick=closeDateSettingsDlg()>Gata</button></div></div><div class=md-scrim id=memo-sett-scrim onclick=closeMemoSettingsDlg()></div><div class=md-dialog id=memo-sett-dlg><div class=mdd-head><div class=mdd-icon><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04a1 1 0 0 0 0-1.41l-2.34-2.34a1 1 0 0 0-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/></svg></div><div class=mdd-title>Memento</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 12px;font-weight:500">Mesaj de afisat</div><div class=mdd-tf-wrap><label class=mdd-label>Text memento (max 120 caractere)</label><input class=mdd-input id=memo-text-in placeholder="ex: Ia medicamentele!" autocomplete=off maxlength=120 oninput=onMemoInput()></div><div id=memo-char-count style="color:var(--on-surf-var);font-size:11px;text-align:right;margin-top:4px">0 / 120</div></div><div class=mdd-actions><button class="mbtn mbtn-ton" onclick=closeMemoSettingsDlg()>Anuleaza</button><button class="mbtn mbtn-fill" id=memo-save-btn onclick=saveMemoSettings()>Salveaza</button></div></div><div class=md-scrim id=canvas-sett-scrim onclick=closeCanvasSettingsDlg()></div><div class=md-dialog id=canvas-sett-dlg style="max-width:480px"><div class=mdd-head><div class=mdd-icon><svg viewBox="0 0 24 24" fill=currentColor height=24 width=24><path d="M4 4h6v6H4V4zm0 10h6v6H4v-6zm10-10h6v6h-6V4zm0 10h6v6h-6v-6z"/></svg></div><div class=mdd-title>Canvas</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 10px;font-weight:500">Deseneaza pe grila ceasului (8&times;32)</div><div class=cvx-wrap><div class=cvx-grid id=cvx-grid></div><div class=cvx-actions><button type=button class=cvx-clear-btn onclick=clearCanvasGrid()>Sterge tot</button></div><div style="height:6px"></div><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">Timp Afisare</div><div class=dur-row><input type=range min=2 max=30 step=1 id=canvas-dur-slider oninput="onCanvasDurInput(this.value)" onchange="saveSettings()"><span class=dur-val id=canvas-dur-val>10s</span></div></div></div><div class=mdd-actions><button class="mbtn mbtn-ton" onclick=closeCanvasSettingsDlg()>Anuleaza</button><button class="mbtn mbtn-fill" onclick=saveCanvasSettings()>Salveaza</button></div></div><div class=md-scrim id=sw-result-scrim onclick=closeSwResultDlg()></div><div class=md-dialog id=sw-result-dlg><div class=mdd-head><div class=mdd-icon><svg viewBox="0 0 24 24" fill=currentColor height=24 width=24><path d="M15 1H9v2h6V1zm-4 13h2V8h-2v6zm8.03-6.61l1.42-1.42c-.43-.51-.9-.99-1.41-1.41l-1.42 1.42A8.962 8.962 0 0012 4c-4.97 0-9 4.03-9 9s4.02 9 9 9a8.994 8.994 0 007.03-14.61zM12 20c-3.87 0-7-3.13-7-7s3.13-7 7-7 7 3.13 7 7-3.13 7-7 7z"/></svg></div><div class=mdd-title>Stopwatch</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 10px;font-weight:500">Counted time</div><div id=sw-result-time style="color:var(--on-surf);font-family:Google Sans,sans-serif;font-size:32px;font-weight:500;letter-spacing:.5px;padding:4px 0 8px">00:00:00:00</div></div><div class=mdd-actions><button class="mbtn mbtn-fill" onclick=closeSwResultDlg()>OK</button></div></div><style>.wx-suggest{background:var(--surf-con);border-radius:0 0 12px 12px;overflow:hidden;margin-top:2px;display:none}.wx-suggest.open{display:block}.wx-si{cursor:pointer;color:var(--on-surf);padding:12px 16px;font-size:14px;border-bottom:1px solid var(--outline-var);transition:background .12s}.wx-si:last-child{border-bottom:none}.wx-si:hover{background:color-mix(in srgb,var(--on-surf)8%,transparent)}.wx-si-main{font-family:Google Sans,sans-serif}.wx-si-sub{color:var(--on-surf-var);font-size:12px;margin-top:2px}.wx-sel-badge{background:var(--grn-con);color:var(--grn);border-radius:8px;padding:4px 12px;font-size:12px;font-weight:500;margin-top:8px;display:none}.wx-no-key-hint{color:var(--on-surf-var);font-size:12px;margin-top:6px;display:none}</style><div class=md-scrim id=wx-sett-scrim onclick=closeWxSettingsDlg()></div><div class=md-dialog id=wx-sett-dlg style="max-width:420px"><div class=mdd-head><div class=mdd-icon><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M6.76 4.84l-1.8-1.79-1.41 1.41 1.79 1.79 1.42-1.41zM4 10.5H1v2h3v-2zm9-9.95h-2V3.5h2V.55zm7.45 3.91l-1.41-1.41-1.79 1.79 1.41 1.41 1.79-1.79zm-3.21 13.7l1.79 1.8 1.41-1.41-1.8-1.79-1.4 1.4zM20 10.5v2h3v-2h-3zm-8-5c-3.31 0-6 2.69-6 6s2.69 6 6 6 6-2.69 6-6-2.69-6-6-6zm-1 16.95h2V19.5h-2v2.95zm-7.45-3.91l1.41 1.41 1.79-1.8-1.41-1.41-1.79 1.8z"/></svg><)pl5"
R"pl6(/div><div class=mdd-title>Setari Meteo</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">OpenWeatherMap API Key</div><div class=mdd-tf-wrap style="display:flex;align-items:center;padding-right:8px"><div style="flex:1"><label class=mdd-label>API Key (openweathermap.org)</label><input class=mdd-input id=wx-key-in placeholder="Cola cheie aici" type=password autocomplete=off oninput=onWxKeyInput()></div><button type=button id=wx-key-vis onclick=toggleWxKeyVis() style="background:none;border:none;cursor:pointer;padding:4px;color:var(--on-surf-var);flex-shrink:0;display:flex;align-items:center;align-self:flex-end;margin-bottom:6px" title="Arata/ascunde cheia"><svg id=wx-eye-icon width=20 height=20 viewBox="0 0 24 24" fill="currentColor"><path d="M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zM12 17c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z"/></svg></button></div><div class=wx-no-key-hint id=wx-no-key-hint>Introdu mai intai cheia API — e necesara pentru cautare</div><div style="height:14px"></div><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">Cauta Localitate</div><div class=mdd-tf-wrap><label class=mdd-label>Scrie numele localitatii</label><input class=mdd-input id=wx-search-in placeholder="ex: Moisei,RO sau Baia Mare,RO" autocomplete=off oninput=onWxSearch() onfocus=onWxSearch()></div><div class=wx-suggest id=wx-suggest></div><div class=wx-sel-badge id=wx-sel-badge></div><div id=wx-status style="color:var(--on-surf-var);font-size:13px;margin-top:10px;min-height:18px"></div><div style="height:14px"></div><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">Unitate temperatura</div><div class=seg style="border-radius:12px;overflow:hidden"><button class="sb" id=wxTempUnitC onclick="setWxTempUnit(0)">°C  Celsius</button><button class=sb id=wxTempUnitF onclick="setWxTempUnit(1)">°F  Fahrenheit</button></div></div><div id=wx-saving style="display:none;flex-direction:column;align-items:center;justify-content:center;padding:32px 24px 40px;gap:18px;transition:opacity .25s"><div style="width:40px;height:40px;border:3px solid var(--surf-var);border-top-color:var(--pri);border-radius:50%;animation:wx-spin .8s linear infinite"></div><div style="color:var(--on-surf-var);font-size:15px;font-family:Google Sans,sans-serif">Se salveaza datele...</div></div><style>@keyframes wx-spin{to{transform:rotate(360deg)}}#wx-sett-dlg .mdd-body,#wx-sett-dlg .mdd-actions{transition:opacity .2s}</style><div class=mdd-actions><button class="mbtn mbtn-ton" onclick=closeWxSettingsDlg()>Anuleaza</button><button class="mbtn mbtn-fill" id=wx-save-btn onclick=saveWxSettings() disabled>Gata</button></div></div><div class=md-scrim id=pressure-sett-scrim onclick=closePressureSettingsDlg()></div><div class=md-dialog id=pressure-sett-dlg><div class=mdd-head><div class=mdd-icon><svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor"><path d="M20.38 8.57l-1.23 1.85a8 8 0 0 1-.22 7.58H5.07A8 8 0 0 1 15.58 6.85l1.85-1.23A10 10 0 0 0 3.35 19a2 2 0 0 0 1.72 1h13.85a2 2 0 0 0 1.74-1a10 10 0 0 0-.27-10.44zm-9.79 6.84a2 2 0 0 0 2.83 0l5.66-8.49l-8.49 5.66a2 2 0 0 0 0 2.83z"/></svg></div><div class=mdd-title>Atmospheric Pressure</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">Timp Afisare</div><div class=dur-row><input type=range min=2 max=30 step=1 id=pressure-dur-slider oninput="onPressureDurInput(this.value)" onchange="saveSettings()"><span class=dur-val id=pressure-dur-val>10s</span></div></div><div class=mdd-actions><button class="mbtn mbtn-fill" onclick=closePressureSettingsDlg()>Gata</button></div></div><div class=md-scrim id=ss-sett-scrim onclick=closeSsSettingsDlg()></div><div class=md-dialog id=ss-sett-dlg><div class=mdd-head><div class=mdd-icon><svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor"><path d="M21 3H3c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h7l-2 3v1h8v-1l-2-3h7c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 14H3V5h18v12zM8 13l2.03-2.03 1.5 1.5L15.5 8.5 17 10l-5.5 5.5z"/></svg></div><div class=mdd-title>Screen Saver</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">Animatie</div><div class=card id=ss-anim-list></div><div style="height:14px"></div><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">Timp Afisare</div><div class=dur-row><input type=range min=2 max=30 step=1 id=ss-dur-slider oninput="onSsDurInput(this.value)" onchange="saveSettings()"><span class=dur-val id=ss-dur-val>8s</span></div></div><div class=mdd-actions><button class="mbtn mbtn-fill" onclick=closeSsSettingsDlg()>Gata</button></div></div><div class=md-scrim id=currency-sett-scrim onclick=closeCurrencySettingsDlg()></div><div class=md-dialog id=currency-sett-dlg><div class=mdd-head><div class=mdd-icon><svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor"><path d="M11.8 10.9c-2.27-.59-3-1.2-3-2.15 0-1.09 1.01-1.85 2.7-1.85 1.78 0 2.44.85 2.5 2.1h2.21c-.07-1.72-1.12-3.3-3.21-3.81V3h-3v2.16c-1.94.42-3.5 1.68-3.5 3.61 0 2.31 1.91 3.46 4.7 4.13 2.5.6 3 1.48 3 2.41 0 .69-.49 1.79-2.7 1.79-2.06 0-2.87-.92-2.98-2.1h-2.2c.12 2.19 1.76 3.42 3.68 3.83V21h3v-2.15c1.95-.37 3.5-1.5 3.5-3.55 0-2.84-2.43-3.81-4.7-4.4z"/></svg></div><div class=mdd-title>Currency Standards</div></div><div class=mdd-body><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">Moneda principala</div><select id=currency-base-sel style="width:100%;background:var(--surf-high);color:var(--on-surf);border:1px solid var(--outline-var);border-radius:8px;padding:8px 10px;font-family:Google Sans,sans-serif;font-size:14px"><option value=EUR>EUR</option><option value=USD>USD</option><option value=GBP>GBP</option><option value=CHF>CHF</option><option value=JPY>JPY</option><option value=CAD>CAD</option><option value=AUD>AUD</option><option value=RON>RON</option></select><div style="height:14px"></div><div class="li static" style="padding:0 0 4px"><div class=li-body><div class=li-head>Enable Comparison</div><div class=li-sub>Compara cu o a doua moneda</div></div><div class=li-trail><div class=sw onclick=toggleCurrencyCompare()><input type=checkbox id=currency-compare-cb><span class=sw-track></span><span class=sw-thumb></span></div></div></div><div id=currency-quote-wrap style="display:none"><div style="height:14px"></div><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">Moneda secundara</div><select id=currency-quote-sel style="width:100%;background:var(--surf-high);color:var(--on-surf);border:1px solid var(--outline-var);border-radius:8px;padding:8px 10px;font-family:Google Sans,sans-serif;font-size:14px"><option value=EUR>EUR</option><option value=USD>USD</option><option value=GBP>GBP</option><option value=CHF>CHF</option><option value=JPY>JPY</option><option value=CAD>CAD</option><option value=AUD>AUD</option><option value=RON>RON</option></select></div><div style="height:14px"></div><div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:0 0 6px;font-weight:500">Timp Afisare</div><div class=dur-row><input type=range min=2 max=30 step=1 id=currency-dur-slider oninput="onCurrencyDurInput(this.value)" onchange="saveSettings()"><span class=dur-val id=currency-dur-val>10s</span></div></div><div class=mdd-actions><button class="mbtn mbtn-ton" onclick=closeCurrencySettingsDlg()>Anuleaza</button><button class="mbtn mbtn-fill" onclick=saveCurrencySettings()>Gata</button></div></div><div class=screen id=s-scrolltype><div class=top-bar><button onclick="go('s-tiles')" class=bar-lead><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg></button><div class=bar-text><div class=bar-title>Scroll Type</div></div></div><div class=content><div class=sl>Scroll individual per tile</div><div class=card id=scrolltype-list></div></div></div><div class=screen id=s-hideicons><div class=top-bar><button onclick="go('s-tiles')" class=bar-lead><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg></button><div class=bar-text><div class=bar-title>Disable Tile Icons</div></div></div><div class=content><div class=sl>Iconite individuale per tile</div><div class=card id=hideicons-list></div></div></div><div class=screen id=s-langmgr><div class=top-bar><button onclick="go('s-home')" class=bar-lead><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M20 11H7.83l5.59-5.59L12 4l-8 8 8 8 1.41-1.41L7.83 13H20v-2z"/></svg></button><div class=bar-text><div class=bar-title>Language Manager</div></div></div><div class=content><div class=sl>Categorii</div><div class=card><div onclick="openLangPicker()" class=li><div class="lic lc-grn"><svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M6.76 4.84l-1.8-1.79-1.41 1.41 1.79 1.79 1.42-1.41zM4 10.5H1v2h3v-2zm9-9.95h-2V3.5h2V.55zm7.45 3.91l-1.41-1.41-1.79 1.79 1.41 1.41 1.79-1.79zm-3.21 13.7l1.79 1.8 1.41-1.41-1.8-1.79-1.4 1.4zM20 10.5v2h3v-2h-3zm-8-5c-3.31 0-6 2.69-6 6s2.69 6 6 6 6-2.69 6-6-2.69-6-6-6zm-1 16.95h2V19.5h-2v2.95zm-7.45-3.91l1.41 1.41 1.79-1.8-1.41-1.41-1.79 1.8z"/></svg></div><div class=li-body><div class=li-head>OpenWeatherMap Language</div><div class=li-sub id=owm-lang-sub>English (en)</div></div><div class=li-trail><span class=chevron><svg viewbox="0 0 24 24" fill=currentColor height=18 width=18><path d="M10 6L8.59 7.41 13.17 12l-4.58 4.59L10 18l6-6z"/></svg></span></div></div></div></div></div><div class=md-scrim id=lang-scrim onclick=closeLangPicker()></div><div class=md-dialog id=lang-dlg style="max-width:400px"><div class=mdd-head><div class=mdd-icon><svg viewbox="0 0 24 24" fill=currentColor height=24 width=24><path d="M12.87 15.07l-2.54-2.51.03-.03A17.52 17.52 0 0 0 14.07 6H17V4h-7V2H8v2H1v2h11.17C11.5 7.92 10.44 9.75 9 11.35 8.07 10.32 7.3 9.19 6.69 8h-2c.73 1.63 1.73 3.17 2.98 4.56l-5.09 5.02L4 19l5-5 3.11 3.11.76-2.04zM18.5 10h-2L12 22h2l1.12-3h4.75L21 22h2l-4.5-12zm-2.62 7l1.62-4.33L19.12 17h-3.24z"/></svg></div><div class=mdd-title>Alege Limba</div></div><div class=mdd-body style="padding:0 24px 8px"><div class=mdd-tf-wrap style="margin-bottom:10px"><label class=mdd-label>Cauta limba</label><input class=mdd-input id=lang-search placeholder="ex: Romanian, English..." oninput=filterLangs()></div><div id=lang-list style="max-height:320px;overflow-y:auto;margin:0 -24px;border-top:1px solid var(--outline-var);scrollbar-width:thin;scrollbar-color:var(--outline-var) transparent"></div></div><div class=mdd-actions><button class="mbtn mbtn-ton" onclick=closeLangPicker()>Anuleaza</button></div></div><script>var apSsidCache=``,selSSID=``,selSec=!1,bOn=!0,items=[],npM=0,tempUnitV=0,pressureHpaV=0,pressureTrendV=0,currencyBaseV=`EUR`,currencyQuoteV=`RON`,currencyCompareV=!1,currencyRateV=0,currencyTrendV=0,currencyValidV=!1,dragSrc=null,brightLevel=4,dimAuto=!1,dimFrom=`22:00`,dimTo=`07:00`,dimLevel=1,notifEn=!0,ets2En=!0,isApMode=!1,dragSrc2=null,dragCtx=null,hideTileIcons=!1,indivHideIcons={date:!1,temp:!1,reminder:!1,weather:!1,notif:!1,nowplaying:!1,press)pl6"
R"pl7(ure:!1,currency:!1},scrollTypeV=0,indivScroll={date:0,temp:0,reminder:0,weather:0,notif:0,nowplaying:0,pressure:0,stopwatch:0,currency:0},SCROLL_TYPE_OPTIONS=[{v:0,n:`Bounce`},{v:1,n:`Wrap`},{v:2,n:`Bounce + Icon`},{v:3,n:`Wrap + Icon`}],buzzerVolume=80,buzzerPreset=`calm`,BUZZER_PRESETS=[{id:`calm`,name:`Calm`},{id:`loud`,name:`Loud`},{id:`urgent`,name:`Urgent`},{id:`soft`,name:`Soft`},{id:`double`,name:`Double Beep`},{id:`triple`,name:`Triple Beep`}],evSndTile=`calm`,evSndWifi=`urgent`,evSndNotif=`soft`,evSndEts2=`urgent`,evSndTouch=`soft`,touchTapAction=0,touchDoubleTapAction=0,TOUCH_ACTIONS=[{id:0,name:`Do nothing`},{id:1,name:`Previous tile`},{id:2,name:`Next tile`},{id:3,name:`Turn screen ON/OFF`},{id:4,name:`Increase brightness`},{id:5,name:`Decrease brightness`},{id:6,name:`Mute / Unmute Buzzer`},{id:7,name:`Restart ESP32`}],EVENT_SOUND_OPTIONS=[{id:`none`,name:`None`},{id:`calm`,name:`Calm`},{id:`loud`,name:`Loud`},{id:`urgent`,name:`Urgent`},{id:`soft`,name:`Soft`},{id:`double`,name:`Double Beep`},{id:`triple`,name:`Triple Beep`},{id:`chime`,name:`Chime`},{id:`bell`,name:`Bell`},{id:`doorbell`,name:`Doorbell`},{id:`xylophone`,name:`Xylophone`},{id:`harp`,name:`Harp Glissando`},{id:`marimba`,name:`Marimba`},{id:`crystal`,name:`Crystal Sparkle`},{id:`wave`,name:`Gentle Wave`},{id:`lullaby`,name:`Lullaby`},{id:`pingpong`,name:`Ping Pong`}],priorityItems=[{id:`notif`,enabled:!0},{id:`ets2`,enabled:!0},{id:`stopwatch`,enabled:!0}],swRunning=!1,swElapsedMs=0,swLastText=`00:00:00:00`,ssAnimV=0,SS_ANIM_OPTIONS=[`Random`,`Pong`,`Fireworks`,`Equalizer`],NAMES=[`Ora`,`Data`,`Temperatura`,`Now Playing`,`Meteo`,`Memento`,`Canvas`,``,`Atmospheric Pressure`,`Screen Saver`,``,`Currency Standards`],ISVG=[`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M11.99 2C6.47 2 2 6.48 2 12s4.47 10 9.99 10C17.52 22 22 17.52 22 12S17.52 2 11.99 2zM12 20c-4.42 0-8-3.58-8-8s3.58-8 8-8 8 3.58 8 8-3.58 8-8 8zm.5-13H11v6l5.25 3.15.75-1.23-4.5-2.67V7z"/></svg>`,`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M20 3h-1V1h-2v2H7V1H5v2H4c-1.1 0-2 .9-2 2v16c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 18H4V8h16v13z"/></svg>`,`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M15 13V5c0-1.66-1.34-3-3-3S9 3.34 9 5v8c-1.21.91-2 2.37-2 4 0 2.76 2.24 5 5 5s5-2.24 5-5c0-1.63-.79-3.09-2-4zm-3 7c-1.65 0-3-1.35-3-3 0-1.3.84-2.4 2-2.82V5c0-.55.45-1 1-1s1 .45 1 1v9.18c1.16.42 2 1.52 2 2.82 0 1.65-1.35 3-3 3z"/></svg>`,`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M12 3v10.55c-.59-.34-1.27-.55-2-.55-2.21 0-4 1.79-4 4s1.79 4 4 4 4-1.79 4-4V7h4V3h-6z"/></svg>`,`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M6.76 4.84l-1.8-1.79-1.41 1.41 1.79 1.79 1.42-1.41zM4 10.5H1v2h3v-2zm9-9.95h-2V3.5h2V.55zm7.45 3.91l-1.41-1.41-1.79 1.79 1.41 1.41 1.79-1.79zm-3.21 13.7l1.79 1.8 1.41-1.41-1.8-1.79-1.4 1.4zM20 10.5v2h3v-2h-3zm-8-5c-3.31 0-6 2.69-6 6s2.69 6 6 6 6-2.69 6-6-2.69-6-6-6zm-1 16.95h2V19.5h-2v2.95zm-7.45-3.91l1.41 1.41 1.79-1.8-1.41-1.41-1.79 1.8z"/></svg>`,`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04a1 1 0 0 0 0-1.41l-2.34-2.34a1 1 0 0 0-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"/></svg>`,`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M4 4h6v6H4V4zm0 10h6v6H4v-6zm10-10h6v6h-6V4zm0 10h6v6h-6v-6z"/></svg>`,`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 18c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8z"/></svg>`,`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M20.38 8.57l-1.23 1.85a8 8 0 0 1-.22 7.58H5.07A8 8 0 0 1 15.58 6.85l1.85-1.23A10 10 0 0 0 3.35 19a2 2 0 0 0 1.72 1h13.85a2 2 0 0 0 1.74-1a10 10 0 0 0-.27-10.44zm-9.79 6.84a2 2 0 0 0 2.83 0l5.66-8.49l-8.49 5.66a2 2 0 0 0 0 2.83z"/></svg>`,`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M21 3H3c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h7l-2 3v1h8v-1l-2-3h7c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 14H3V5h18v12zM8 13l2.03-2.03 1.5 1.5L15.5 8.5 17 10l-5.5 5.5z"/></svg>`,``,`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M11.8 10.9c-2.27-.59-3-1.2-3-2.15 0-1.09 1.01-1.85 2.7-1.85 1.78 0 2.44.85 2.5 2.1h2.21c-.07-1.72-1.12-3.3-3.21-3.81V3h-3v2.16c-1.94.42-3.5 1.68-3.5 3.61 0 2.31 1.91 3.46 4.7 4.13 2.5.6 3 1.48 3 2.41 0 .69-.49 1.79-2.7 1.79-2.06 0-2.87-.92-2.98-2.1h-2.2c.12 2.19 1.76 3.42 3.68 3.83V21h3v-2.15c1.95-.37 3.5-1.5 3.5-3.55 0-2.84-2.43-3.81-4.7-4.4z"/></svg>`],ILCLS=[`lc-pur`,`lc-blu`,`lc-tea`,`lc-amb`,`lc-grn`,`lc-tea`,`lc-blu`,`lc-pur`,`lc-blu`,`lc-pur`,`lc-pur`,`lc-amb`];var SCROLL_TILES=[{key:`date`,label:`Date`,cls:`lc-blu`,svg:ISVG[1]},{key:`temp`,label:`Temperature`,cls:`lc-tea`,svg:ISVG[2]},{key:`reminder`,label:`Reminder`,cls:`lc-tea`,svg:ISVG[5]},{key:`weather`,label:`Weather`,cls:`lc-grn`,svg:ISVG[4]},{key:`notif`,label:`PC Notifications`,cls:`lc-pur`,svg:`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M12 22c1.1 0 2-.9 2-2h-4c0 1.1.9 2 2 2zm6-6v-5c0-3.07-1.64-5.64-4.5-6.32V4c0-.83-.67-1.5-1.5-1.5s-1.5.67-1.5 1.5v.68C7.63 5.36 6 7.92 6 11v5l-2 2v1h16v-1l-2-2z"/></svg>`},{key:`nowplaying`,label:`Now Playing`,cls:`lc-amb`,svg:ISVG[3]},{key:`pressure`,label:`Atmospheric Pressure`,cls:`lc-blu`,svg:ISVG[8]},{key:`currency`,label:`Currency Standards`,cls:`lc-amb`,svg:ISVG[11]},{key:`stopwatch`,label:`Stopwatch`,cls:`lc-grn`,hasIcon:!1,svg:`<svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M15 1H9v2h6V1zm-4 13h2V8h-2v6zm8.03-6.61l1.42-1.42c-.43-.51-.9-.99-1.41-1.41l-1.42 1.42A8.962 8.962 0 0012 4c-4.97 0-9 4.03-9 9s4.02 9 9 9a8.994 8.994 0 007.03-14.61zM12 20c-3.87 0-7-3.13-7-7s3.13-7 7-7 7 3.13 7 7-3.13 7-7 7z"/></svg>`}];var fwVersion=`0.1`,bootUptimeSec=0,uptimeFetchedAt=0,aboutTickTimer=null;function pad2(n){return(n<10?`0`:``)+n}function formatUptime(sec){sec=Math.max(0,Math.floor(sec));var h=Math.floor(sec/3600),m=Math.floor((sec%3600)/60),s=sec%60;return h+`h `+pad2(m)+`m `+pad2(s)+`s`}function refreshAboutState(){fetch(`/state`).then(function(r){return r.json()}).then(function(s){var ipEl=document.getElementById(`about-ip`);if(ipEl)ipEl.textContent=s.ip||`—`;var verEl=document.getElementById(`about-version`);if(verEl)verEl.textContent=s.version||fwVersion;var swVerEl=document.getElementById(`sw-version`);if(swVerEl)swVerEl.textContent=s.version||fwVersion;if(s.version)fwVersion=s.version;if(s.uptime!==void 0){bootUptimeSec=s.uptime;uptimeFetchedAt=Date.now();var upEl=document.getElementById(`about-uptime`);if(upEl)upEl.textContent=formatUptime(bootUptimeSec)}}).catch(function(){})}function tickAboutUptime(){if(uptimeFetchedAt===0)return;var elapsed=(Date.now()-uptimeFetchedAt)/1000;var upEl=document.getElementById(`about-uptime`);if(upEl)upEl.textContent=formatUptime(bootUptimeSec+elapsed)}function startAboutTick(){stopAboutTick();aboutTickTimer=setInterval(tickAboutUptime,1000)}function stopAboutTick(){if(aboutTickTimer){clearInterval(aboutTickTimer);aboutTickTimer=null}}var SW_REPO_BASE=`https://raw.githubusercontent.com/Adium1000/Octoglow/main/Updater/`,SW_VER_URL=SW_REPO_BASE+`UpdaterNewVersion.MD`,SW_DESC_URL=SW_REPO_BASE+`UpdaterVersionDescription.MD`,SW_BIN_URL=SW_REPO_BASE+`Update.bin`,swServerVersion=``,swUpdating=!1;function swCompareVersions(a,b){var pa=String(a).trim().split(`.`).map(function(x){return parseInt(x,10)||0});var pb=String(b).trim().split(`.`).map(function(x){return parseInt(x,10)||0});var len=Math.max(pa.length,pb.length);for(var i=0;i<len;i++){var na=pa[i]||0,nb=pb[i]||0;if(na>nb)return 1;if(na<nb)return -1}return 0}function swShowState(state){[`sw-searching`,`sw-uptodate`,`sw-checkerr`,`sw-newupdate`,`sw-installing`].forEach(function(id){var el=document.getElementById(id);if(el)el.style.display=`none`});[`sw-actions-uptodate`,`sw-actions-newupdate`].forEach(function(id){var el=document.getElementById(id);if(el)el.style.display=`none`});var el=document.getElementById(state);if(el)el.style.display=(state===`sw-newupdate`?`block`:`flex`);if(state===`sw-uptodate`)document.getElementById(`sw-actions-uptodate`).style.display=`flex`;if(state===`sw-newupdate`)document.getElementById(`sw-actions-newupdate`).style.display=`flex`}function openSwUpdateDlg(){document.getElementById(`sw-update-scrim`).classList.add(`open`);document.getElementById(`sw-update-dlg`).classList.add(`open`);swShowState(`sw-searching`);swCheckForUpdates()}function closeSwUpdateDlg(){if(swUpdating)return;document.getElementById(`sw-update-scrim`).classList.remove(`open`);document.getElementById(`sw-update-dlg`).classList.remove(`open`)}function swCheckForUpdates(){fetch(SW_VER_URL,{cache:`no-store`}).then(function(r){if(!r.ok)throw new Error(`ver`);return r.text()}).then(function(txt){swServerVersion=txt.trim();if(swCompareVersions(swServerVersion,fwVersion)>0){return fetch(SW_DESC_URL,{cache:`no-store`}).then(function(r){return r.ok?r.text():``}).then(function(desc){document.getElementById(`sw-ver-compare`).textContent=fwVersion+` → `+swServerVersion;document.getElementById(`sw-update-desc`).textContent=desc.trim();swShowState(`sw-newupdate`)})}else{swShowState(`sw-uptodate`)}}).catch(function(){swShowState(`sw-checkerr`)})}function installSwUpdate(){swUpdating=!0;swShowState(`sw-installing`);document.getElementById(`sw-install-status`).textContent=`Se descarca update-ul...`;fetch(SW_BIN_URL,{cache:`no-store`}).then(function(r){if(!r.ok)throw new Error(`bin`);return r.arrayBuffer()}).then(function(buf){document.getElementById(`sw-install-status`).textContent=`Se instaleaza update-ul...`;var blob=new Blob([buf],{type:`application/octet-stream`});var fd=new FormData();fd.append(`update`,blob,`update.bin`);return fetch(`/swupdateupload`,{method:`POST`,body:fd})}).then(function(r){return r.json()}).then(function(d){if(d&&d.ok){document.getElementById(`sw-install-status`).textContent=`Update instalat. Se reporneste...`}else{document.getElementById(`sw-install-status`).textContent=`Eroare la instalare: `+((d&&d.err)||`necunoscuta`);swUpdating=!1}}).catch(function(){document.getElementById(`sw-install-status`).textContent=`Eroare la descarcarea/instalarea update-ului.`;swUpdating=!1})}function go(id){if(id===`s-bright`){var sl=document.getElementById(`bright-slider`);if(sl)sl.value=brightLevel;var bv=document.getElementById(`br-val`);if(bv)bv.textContent=brightLevel;var lbl=document.getElementById(`bright-screen-lbl`);if(lbl)lbl.textContent=`Nivel `+brightLevel;var cb=document.getElementById(`dim-auto-cb`);if(cb)cb.checked=dimAuto;document.getElementById(`dim-sched`).classList.toggle(`open`,dimAuto);var df=document.getElementById(`dim-from`);if(df)df.value=dimFrom;var dt=document.getElementById(`dim-to`);if(dt)dt.value=dimTo;var ds=document.getElementById(`dim-level-slider`);if(ds)ds.value=dimLevel;var dl=document.getElementById(`dim-level-lbl`);if(dl)dl.textContent=dimLevel}if(id===`s-buzzer`){var bcb=document.getElementById(`btog-cb`);if(bcb)bcb.checked=bOn;var bs=document.getElementById(`buzzer-vol-slider`);if(bs)bs.value=buzzerVolume;var bv=document.getElementById(`buzzer-vol-val`);if(bv)bv.textContent=buzzerVolume;var bl=document.getElementById(`buzzer-vol-lbl`);if(bl)bl.textContent=`Volum `+buzzerVolume+`%`;buildBuzzerPresets();buildEventSoundCard()}if(id===`s-touch`){buildTouchActionCard()}if(id===`s-scrolltype`){buildScrollTypeList()}if(id===`s-hideicons`){buildHideIconsList()}if(id===`s-ap`){loadApSettings()}document.querySelectorAll(`.screen`).forEach(function(s){s.classList.remove(`active`)}),document.getElementById(id).classList.add(`active`),window.scrollTo(0,0),id===`s-tiles`&&(buildGrid(),buildPriorityGrid());if(id===`s-wifi`){startWifiAutoScan()}else{stopWifiAutoScan()}if(id===`s-about`){refreshAboutState();startAboutTick()}else{stopAboutTick()}document.getElementById(`lang-scrim`).classList.remove(`open`);document.getElementById(`lang-dlg`).classList.remove(`open`);}function rssi2b(r){return r>=-55?4:r>=-70?3:r>=-80?2:1}function bHtml(n){var h=`<div class="bars">`;)pl7"
R"pl8(return[3,5,9,13].forEach(function(px,i){h+=`<div class="bar`+(i<n?` on`:``)+`" style="height:`+px+`px"></div>`}),h+`</div>`}var wifiAutoScanTimer=null;var wifiScanRunning=!1;function startWifiAutoScan(){stopWifiAutoScan();doScan();wifiAutoScanTimer=setInterval(function(){if(document.getElementById(`s-wifi`).classList.contains(`active`)&&!document.getElementById(`conn-dlg`).classList.contains(`open`)){doScanSilent()}},15000)}function stopWifiAutoScan(){if(wifiAutoScanTimer){clearInterval(wifiAutoScanTimer);wifiAutoScanTimer=null}}function wifiSvgForBars(bars){if(bars>=4){return`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4 2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg>`}else if(bars===3){return`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9z" opacity=".3"/><path d="M5 13l2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13zm4 4 3 3 3-3c-1.65-1.66-4.34-1.66-6 0z"/></svg>`}else if(bars===2){return`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm4 4 2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z" opacity=".3"/><path d="M9 17l3 3 3-3c-1.65-1.66-4.34-1.66-6 0z"/></svg>`}else{return`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm4 4 2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13zm4 4 3 3 3-3c-1.65-1.66-4.34-1.66-6 0z" opacity=".3"/></svg>`}}function buildNetItem(n,curSSID){var isCur=(n.ssid===curSSID&&curSSID!==`—`&&curSSID!==``);var bars=rssi2b(n.rssi);var wifiSvg=wifiSvgForBars(bars);var lockSvg=n.secured?`<svg width="18" height="18" viewBox="0 0 24 24" fill="var(--on-surf-var)"><path d="M18 8h-1V6c0-2.76-2.24-5-5-5S7 3.24 7 6v2H6c-1.1 0-2 .9-2 2v10c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V10c0-1.1-.9-2-2-2zm-6 9c-1.1 0-2-.9-2-2s.9-2 2-2 2 .9 2 2-.9 2-2 2zm3.1-9H8.9V6c0-1.71 1.39-3.1 3.1-3.1 1.71 0 3.1 1.39 3.1 3.1v2z"/></svg>`:``;var authTxt=n.auth||``;var d=document.createElement(`div`);d.className=`li`+(isCur?` cur-net`:``);d.dataset.ssid=n.ssid;d.innerHTML=`<div class="lic lc-pur">`+wifiSvg+`</div><div class="li-body"><div class="li-head" style="`+(isCur?`color:var(--pri)`:``)+`">`+n.ssid+`</div><div class="li-sub">`+authTxt+`</div></div><div class="li-trail">`+lockSvg+`</div>`;d.onclick=function(){if(isCur){openDisconDialog(n.ssid);return;}document.querySelectorAll(`#nlist .li`).forEach(function(x){x.classList.remove(`sel`)}),d.classList.add(`sel`),selSSID=n.ssid,selSec=n.secured,openConnDialog(n.ssid,n.secured)};return d}function renderNetsList(nets){var nl=document.getElementById(`nlist`);var curSSID=document.getElementById(`w-ssid`).textContent||``;nets.sort(function(a,b){return b.rssi-a.rssi});var existingMap={};nl.querySelectorAll(`.li[data-ssid]`).forEach(function(el){existingMap[el.dataset.ssid]=el});var newSsids=nets.map(function(n){return n.ssid});Object.keys(existingMap).forEach(function(ssid){if(newSsids.indexOf(ssid)<0){var el=existingMap[ssid];el.style.transition=`opacity .3s,transform .3s`;el.style.opacity=`0`;el.style.transform=`translateX(-16px)`;setTimeout(function(){if(el.parentNode)el.parentNode.removeChild(el)},300)}});nets.forEach(function(n,i){var existing=nl.querySelector(`.li[data-ssid="`+n.ssid+`"]`);if(existing){var isCur=(n.ssid===curSSID&&curSSID!==`—`&&curSSID!==``);var bars=rssi2b(n.rssi);existing.querySelector(`.lic`).innerHTML=wifiSvgForBars(bars);var head=existing.querySelector(`.li-head`);if(head)head.style.color=isCur?`var(--pri)`:``;var newParent=nl;var items=newParent.querySelectorAll(`.li[data-ssid]`);if(items[i]&&items[i]!==existing){newParent.insertBefore(existing,items[i])}}else{var d=buildNetItem(n,curSSID);d.style.opacity=`0`;d.style.transform=`translateX(16px)`;d.style.transition=`none`;var items=nl.querySelectorAll(`.li[data-ssid]`);if(items[i])nl.insertBefore(d,items[i]);else nl.appendChild(d);requestAnimationFrame(function(){d.style.transition=`opacity .3s,transform .3s`;d.style.opacity=`1`;d.style.transform=``})}})}function doScanSilent(){if(wifiScanRunning)return;wifiScanRunning=!0;fetch(`/scan`).then(function(r){return r.json()}).then(function(nets){wifiScanRunning=!1;var nl=document.getElementById(`nlist`);var hint=nl.querySelector(`.scan-hint`);if(hint){nl.innerHTML=``}if(!nets.length){if(!nl.querySelector(`.li[data-ssid]`))nl.innerHTML=`<div class="scan-hint">Nicio retea gasita</div>`;return}renderNetsList(nets)}).catch(function(){wifiScanRunning=!1})}function doScan(){var nl=document.getElementById(`nlist`);var scanBtn=document.getElementById(`scan-btn`);if(scanBtn){scanBtn.disabled=!0;scanBtn.style.opacity=`.5`;scanBtn.style.pointerEvents=`none`}nl.innerHTML=`<div class="scan-hint"><span class="spin-ring"></span>Se cauta retele…</div>`,selSSID=``,wifiScanRunning=!1,fetch(`/scan`).then(function(r){return r.json()}).then(function(nets){if(scanBtn){scanBtn.disabled=!1;scanBtn.style.opacity=``;scanBtn.style.pointerEvents=``}if(!nets.length){nl.innerHTML=`<div class="scan-hint">Nicio retea gasita</div>`;return};var curSSID=document.getElementById(`w-ssid`).textContent||``;nets.sort(function(a,b){return b.rssi-a.rssi});nl.innerHTML=``;nets.forEach(function(n){var d=buildNetItem(n,curSSID);d.style.opacity=`0`;d.style.transition=`none`;nl.appendChild(d);requestAnimationFrame(function(){d.style.transition=`opacity .25s`;d.style.opacity=`1`})})}).catch(function(){if(scanBtn){scanBtn.disabled=!1;scanBtn.style.opacity=``;scanBtn.style.pointerEvents=``}nl.innerHTML=`<div class="scan-hint">Eroare la scan</div>`})}function openConnDialog(ssid,secured){document.getElementById(`dlg-ssid-name`).textContent=ssid;var b=document.getElementById(`dlg-pass-body`);b.style.display=secured?`block`:`none`;if(!secured)document.getElementById(`pass-in`).value=``;document.getElementById(`md-scrim`).classList.add(`open`);document.getElementById(`conn-dlg`).classList.add(`open`);if(secured)setTimeout(function(){document.getElementById(`pass-in`).focus()},250)}function closeDialog(){document.getElementById(`md-scrim`).classList.remove(`open`);document.getElementById(`conn-dlg`).classList.remove(`open`)}function doConnect(){if(!selSSID)return;var pass=document.getElementById(`pass-in`).value,btn=document.getElementById(`conn-btn`);btn.disabled=!0,btn.textContent=`…`;fetch(`/connect`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`ssid=`+encodeURIComponent(selSSID)+`&pass=`+encodeURIComponent(pass)}).then(function(r){return r.text()}).then(function(){closeDialog(),showMsg(`Salvat! Ceasul se reconecteaza…`,`ok`),btn.disabled=!1,btn.textContent=`Conecteaza`,document.getElementById(`h-ssid`).textContent=selSSID,document.getElementById(`w-ssid`).textContent=selSSID,document.getElementById(`h-ip`).textContent=`…`,document.getElementById(`w-ip`).textContent=`…`;var att=0,poll=setInterval(function(){att++,fetch(`/state`).then(function(r){return r.json()}).then(function(s){s.ip&&s.ip!==`0.0.0.0`&&!s.ap&&(isApMode=!1,clearInterval(poll),document.getElementById(`h-ip`).textContent=s.ip,document.getElementById(`w-ip`).textContent=s.ip,setChips(selSSID,!1),showMsg(`Conectat! IP: `+s.ip,`ok`))}).catch(function(){}),att>=20&&clearInterval(poll)},1500)}).catch(function(){showMsg(`Eroare conexiune`,`err`),btn.disabled=!1,btn.textContent=`Conecteaza`})}function showMsg(t,c){var m=document.getElementById(`wifi-msg`);m.textContent=t,m.className=`msg `+c,m.style.display=`block`}function setChips(ssid,isAp){var b=document.getElementById(`switch-ap-btn`);if(b)b.style.display=isAp?`none`:``;var b2=document.getElementById(`ap-switch-btn`);if(b2)b2.style.display=isAp?`none`:``;var b3=document.getElementById(`switch-wifi-btn`);if(b3)b3.style.display=isAp?``:`none`}function copyIP(){var ip=document.getElementById(`h-ip`).textContent;ip===`…`||ip===`—`||navigator.clipboard&&navigator.clipboard.writeText(`http://`+ip+`/`).then(function(){var b=document.getElementById(`copy-btn`);b.textContent=`Copiat!`,setTimeout(funct)pl8"
R"pl9(ion(){b.textContent=`Copiaza`},2e3)})}function toggleBuzzer(){bOn=!bOn;var cb=document.getElementById(`btog-cb`);cb.checked=bOn,document.getElementById(`h-bsub`).textContent=bOn?`Beep la schimbarea tile-ului`:`Silentios`,saveSettings()}function onBuzzerVolumeInput(v){buzzerVolume=parseInt(v);var lbl=document.getElementById(`buzzer-vol-lbl`);if(lbl)lbl.textContent=`Volum `+v+`%`;var val=document.getElementById(`buzzer-vol-val`);if(val)val.textContent=v;fetch(`/buzzersett`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`volume=`+v})}function buildBuzzerPresets(){var c=document.getElementById(`buzzer-presets`);if(!c)return;c.innerHTML=``;BUZZER_PRESETS.forEach(function(p){var isSel=(p.id===buzzerPreset);var d=document.createElement(`div`);d.className=`li`;d.style.cursor=`pointer`;d.innerHTML=`<div class="li-body"><div class="li-head" style="`+(isSel?`color:var(--pri)`:``)+`">`+p.name+`</div></div><div class="li-trail">`+(isSel?`<svg width="20" height="20" viewBox="0 0 24 24" fill="var(--pri)"><path d="M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z"/></svg>`:``)+`</div>`;d.onclick=function(){selectBuzzerPreset(p.id)};c.appendChild(d)})}function selectBuzzerPreset(id){buzzerPreset=id;buildBuzzerPresets();fetch(`/buzzersett`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`preset=`+encodeURIComponent(id)})}function buildTouchActionCard(){var c=document.getElementById(`touch-action-card`);if(!c)return;c.innerHTML=``;var touchSvg=`<svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M9 11.24V7.5C9 6.12 10.12 5 11.5 5S14 6.12 14 7.5v3.74c1.21-.81 2-2.18 2-3.74C16 5.01 13.99 3 11.5 3S7 5.01 7 7.5c0 1.56.79 2.93 2 3.74zm9.84 4.63l-4.54-2.26c-.17-.07-.35-.11-.54-.11H13v-6c0-.83-.67-1.5-1.5-1.5S10 6.67 10 7.5v10.74l-3.43-.72c-.08-.01-.15-.03-.24-.03-.31 0-.59.13-.79.33l-.79.8 4.94 4.94c.27.27.65.44 1.06.44h6.79c.75 0 1.33-.55 1.44-1.28l.75-5.27c.01-.07.02-.14.02-.2 0-.62-.38-1.16-.91-1.38z"/></svg>`;var gestures=[{key:`tap`,label:`Tap`,desc:`Atingere scurta a senzorului`,val:touchTapAction},{key:`dbl`,label:`Double Tap`,desc:`Doua atingeri rapide, consecutive`,val:touchDoubleTapAction}];gestures.forEach(function(g,i){var row=document.createElement(`div`);row.className=`li`;row.style.borderBottom=`none`;var opts=``;TOUCH_ACTIONS.forEach(function(a){opts+=`<option value="`+a.id+`"`+(a.id===g.val?` selected`:``)+`>`+a.name+`</option>`});row.innerHTML=`<div class="lic lc-grn">`+touchSvg+`</div><div class="li-body"><div class="li-head">`+g.label+`</div><div class="li-sub">`+g.desc+`</div></div><div class="li-trail"><select class="evsnd-select" onchange="setTouchAction(\'`+g.key+`\',this.value)" style="background:var(--surf-high);color:var(--on-surf);border:1px solid var(--outline-var);border-radius:8px;padding:6px 10px;font-family:Google Sans,sans-serif;font-size:13px;max-width:150px">`+opts+`</select></div>`;c.appendChild(row)})}function setTouchAction(key,val){var v=parseInt(val);if(key===`tap`)touchTapAction=v;else touchDoubleTapAction=v;fetch(`/touchsett`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`tap=`+touchTapAction+`&dbl=`+touchDoubleTapAction})}function buildEventSoundCard(){var c=document.getElementById(`event-sound-card`);if(!c)return;c.innerHTML=``;var evs=[{key:`tile`,label:`Tile Switching`,desc:`Beep la schimbarea tile-ului`,val:evSndTile,enabled:!0,cls:`lc-tea`,svg:`<svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M3 13h8V3H3v10zm0 8h8v-6H3v6zm10 0h8V11h-8v10zm0-18v6h8V3h-8z"/></svg>`},{key:`wifi`,label:`WiFi Connection Lost`,desc:`Sunet la pierderea conexiunii WiFi`,val:evSndWifi,enabled:!0,cls:`lc-blu`,svg:`<svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M1 9l2 2c4.97-4.97 13.03-4.97 18 0l2-2C16.93 2.93 7.08 2.93 1 9zm8 8l3 3 3-3c-1.65-1.66-4.34-1.66-6 0zm-4-4 2 2c2.76-2.76 7.24-2.76 10 0l2-2C15.14 9.14 8.87 9.14 5 13z"/></svg>`},{key:`notif`,label:`New Notification`,desc:`Sunet la notificare noua de la PC`,val:evSndNotif,enabled:notifEn,cls:`lc-pur`,svg:`<svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M12 22c1.1 0 2-.9 2-2h-4c0 1.1.9 2 2 2zm6-6v-5c0-3.07-1.64-5.64-4.5-6.32V4c0-.83-.67-1.5-1.5-1.5s-1.5.67-1.5 1.5v.68C7.63 5.36 6 7.92 6 11v5l-2 2v1h16v-1l-2-2z"/></svg>`},{key:`ets2`,label:`ETS2 Speeding`,desc:`Sunet la depasirea limitei de viteza`,val:evSndEts2,enabled:ets2En,cls:`lc-amb`,svg:`<svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M20 8h-3V4H3c-1.1 0-2 .9-2 2v11h2c0 1.66 1.34 3 3 3s3-1.34 3-3h6c0 1.66 1.34 3 3 3s3-1.34 3-3h2v-5l-3-4zM6 18.5c-.83 0-1.5-.67-1.5-1.5s.67-1.5 1.5-1.5 1.5.67 1.5 1.5-.67 1.5-1.5 1.5zm12 0c-.83 0-1.5-.67-1.5-1.5s.67-1.5 1.5-1.5 1.5.67 1.5 1.5-.67 1.5-1.5 1.5zm-1-9.5h2.5l2.07 2.5H17V9z"/></svg>`},{key:`touch`,label:`Touch Sensor`,desc:`Sunet la atingerea senzorului`,val:evSndTouch,enabled:!0,cls:`lc-grn`,svg:`<svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M9 11.24V7.5C9 6.12 10.12 5 11.5 5S14 6.12 14 7.5v3.74c1.21-.81 2-2.18 2-3.74C16 5.01 13.99 3 11.5 3S7 5.01 7 7.5c0 1.56.79 2.93 2 3.74zm9.84 4.63l-4.54-2.26c-.17-.07-.35-.11-.54-.11H13v-6c0-.83-.67-1.5-1.5-1.5S10 6.67 10 7.5v10.74l-3.43-.72c-.08-.01-.15-.03-.24-.03-.31 0-.59.13-.79.33l-.79.8 4.94 4.94c.27.27.65.44 1.06.44h6.79c.75 0 1.33-.55 1.44-1.28l.75-5.27c.01-.07.02-.14.02-.2 0-.62-.38-1.16-.91-1.38z"/></svg>`}];evs.forEach(function(ev,i){var row=document.createElement(`div`);row.className=`li`;row.style.borderBottom=`none`;if(!ev.enabled){row.style.opacity=`.38`;row.style.pointerEvents=`none`}var opts=``;EVENT_SOUND_OPTIONS.forEach(function(o){opts+=`<option value="`+o.id+`"`+(o.id===ev.val?` selected`:``)+`>`+o.name+`</option>`});var descTxt=ev.enabled?ev.desc:`Activeaza mai intai in Tile Manager`;row.innerHTML=`<div class="lic `+ev.cls+`">`+ev.svg+`</div><div class="li-body"><div class="li-head">`+ev.label+`</div><div class="li-sub">`+descTxt+`</div></div><div class="li-trail"><select class="evsnd-select" `+(ev.enabled?``:`disabled`)+` onchange="setEventSound(\'`+ev.key+`\',this.value)" style="background:var(--surf-high);color:var(--on-surf);border:1px solid var(--outline-var);border-radius:8px;padding:6px 10px;font-family:Google Sans,sans-serif;font-size:13px;max-width:150px">`+opts+`</select></div>`;c.appendChild(row)})}function setEventSound(key,val){if(key===`tile`)evSndTile=val;else if(key===`wifi`)evSndWifi=val;else if(key===`notif`)evSndNotif=val;else if(key===`ets2`)evSndEts2=val;else if(key===`touch`)evSndTouch=val;fetch(`/eventsoundsett`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`event=`+key+`&preset=`+encodeURIComponent(val)})}function toggleHideIcons(){hideTileIcons=!hideTileIcons;var cb=document.getElementById(`hide-icons-cb`);if(cb)cb.checked=hideTileIcons;Object.keys(indivHideIcons).forEach(function(k){indivHideIcons[k]=hideTileIcons});applyHideIcons();refreshHideIconSwitches();saveSettings()}function applyHideIcons(){var cb=document.getElementById(`hide-icons-cb`);if(cb)cb.checked=hideTileIcons}function refreshHideIconSwitches(){Object.keys(indivHideIcons).forEach(function(k){var cb=document.getElementById(`hideicon-cb-`+k);if(cb)cb.checked=indivHideIcons[k]})}function setScrollType(v){scrollTypeV=parseInt(v);var sel=document.getElementById(`scroll-type-sel`);if(sel)sel.value=scrollTypeV;Object.keys(indivScroll).forEach(function(k){indivScroll[k]=scrollTypeV});refreshScrollTypeSelects();saveSettings()}function applyScrollType(){var sel=document.getElementById(`scroll-type-sel`);if(sel)sel.value=scrollTypeV}function refreshScrollTypeSelects(){Object.keys(indivScroll).forEach(function(k){var s=document.getElementById(`scrolltype-sel-`+k);if(s)s.value=indivScroll[k]})}function buildScrollTypeList(){var c=document.getElementById(`scrolltype-list`);if(!c)return;c.innerHTML=``;SCROLL_TILES.forEach(function(t,i){var row=document)pl9"
R"pl10(.createElement(`div`);row.className=`li static`;row.style.borderBottom=`none`;var opts=``;SCROLL_TYPE_OPTIONS.forEach(function(o){opts+=`<option value="`+o.v+`"`+(o.v===indivScroll[t.key]?` selected`:``)+`>`+o.n+`</option>`});row.innerHTML=`<div class="lic `+t.cls+`">`+t.svg+`</div><div class="li-body"><div class="li-head">`+t.label+`</div></div><div class="li-trail"><select class="evsnd-select" id="scrolltype-sel-`+t.key+`" onchange="setIndividualScrollType(\'`+t.key+`\',this.value)" style="background:var(--surf-high);color:var(--on-surf);border:1px solid var(--outline-var);border-radius:8px;padding:6px 10px;font-family:Google Sans,sans-serif;font-size:13px;max-width:150px">`+opts+`</select></div>`;c.appendChild(row)})}function setIndividualScrollType(key,val){indivScroll[key]=parseInt(val);saveSettings()}function buildHideIconsList(){var c=document.getElementById(`hideicons-list`);if(!c)return;c.innerHTML=``;var tiles=SCROLL_TILES.filter(function(t){return t.hasIcon!==!1});tiles.forEach(function(t,i){var row=document.createElement(`div`);row.className=`li static`;row.style.borderBottom=`none`;row.innerHTML=`<div class="lic `+t.cls+`">`+t.svg+`</div><div class="li-body"><div class="li-head">`+t.label+`</div></div><div class="li-trail"><div class="sw" onclick="setIndividualHideIcon(\'`+t.key+`\')"><input type="checkbox" id="hideicon-cb-`+t.key+`"`+(indivHideIcons[t.key]?` checked`:``)+`><span class="sw-track"></span><span class="sw-thumb"></span></div></div>`;c.appendChild(row)})}function setIndividualHideIcon(key){indivHideIcons[key]=!indivHideIcons[key];var cb=document.getElementById(`hideicon-cb-`+key);if(cb)cb.checked=indivHideIcons[key];saveSettings()}function buildGrid(){var g=document.getElementById(`tgrid`);g.innerHTML=``;items.forEach(function(item,idx){if(item.id===3&&npIsPriority())return;var isNp=item.id===3,isTemp=item.id===2,isWx=item.id===4,isHour=item.id===0,isDate=item.id===1,isMemoTile=item.id===5,isCanvas=item.id===6,isPressure=item.id===8,isScreensaver=item.id===9,isCurrency=item.id===11,d=document.createElement(`div`);d.className=`tile-item`;var swHtml=`<div class=\"sw\" onclick=\"toggleTile(`+idx+`)\"><input type=\"checkbox\" id=\"tsw`+idx+`\"`+(item.enabled?` checked`:``)+`><span class=\"sw-track\"></span><span class=\"sw-thumb\"></span></div>`;var gearSvg=`<svg width=\"18\" height=\"18\" viewBox=\"0 0 24 24\" fill=\"currentColor\"><path d=\"M19.14 12.94c.04-.3.06-.61.06-.94s-.02-.64-.07-.94l2.03-1.58a.49.49 0 0 0 .12-.61l-1.92-3.32a.488.488 0 0 0-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54a.484.484 0 0 0-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87a.49.49 0 0 0 .12.61l2.03 1.58c-.05.3-.07.62-.07.94s.02.64.07.94l-2.03 1.58a.49.49 0 0 0-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32a.49.49 0 0 0-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z\"/></svg>`;var npGearHtml=isNp?`<button class=\"np-gear-btn\" onclick=\"openNpSettingsDlg(event)\" title=\"Setari Now Playing\">`+gearSvg+`</button>`:`` ;var wxGearHtml=isWx?`<button class=\"np-gear-btn\" onclick=\"openWxSettingsDlg(event)\" title=\"Setari Meteo\">`+gearSvg+`</button>`:``;var tempGearHtml=isTemp?`<button class=\"np-gear-btn\" onclick=\"openTempSettingsDlg(event)\" title=\"Setari Temperatura\">`+gearSvg+`</button>`:``; var hourGearHtml=isHour?`<button class=\"np-gear-btn\" onclick=\"openHourSettingsDlg(event)\" title=\"Setari Ora\">`+gearSvg+`</button>`:``; var dateGearHtml=isDate?`<button class=\"np-gear-btn\" onclick=\"openDateSettingsDlg(event)\" title=\"Setari Data\">`+gearSvg+`</button>`:``; var isMemo=item.id===5,pencilSvg=`<svg width=\"18\" height=\"18\" viewBox=\"0 0 24 24\" fill=\"currentColor\"><path d=\"M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04a1 1 0 0 0 0-1.41l-2.34-2.34a1 1 0 0 0-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z\"/></svg>`,memoGearHtml=isMemo?`<button class=\"np-gear-btn\" onclick=\"openMemoSettingsDlg(event)\" title=\"Setari Memento\">`+pencilSvg+`</button>`:``,canvasGearHtml=isCanvas?`<button class=\"np-gear-btn\" onclick=\"openCanvasSettingsDlg(event)\" title=\"Setari Canvas\">`+gearSvg+`</button>`:``; var pressureGearHtml=isPressure?`<button class=\"np-gear-btn\" onclick=\"openPressureSettingsDlg(event)\" title=\"Setari Presiune\">`+gearSvg+`</button>`:``; var ssGearHtml=isScreensaver?`<button class=\"np-gear-btn\" onclick=\"openSsSettingsDlg(event)\" title=\"Setari Screen Saver\">`+gearSvg+`</button>`:``; var currencyGearHtml=isCurrency?`<button class=\"np-gear-btn\" onclick=\"openCurrencySettingsDlg(event)\" title=\"Setari Currency Standards\">`+gearSvg+`</button>`:``; d.innerHTML=`<span class="drag-handle"><svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M11 18c0 1.1-.9 2-2 2s-2-.9-2-2 .9-2 2-2 2 .9 2 2zm-2-8c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2zm0-6c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2zm6 4c1.1 0 2-.9 2-2s-.9-2-2-2-2 .9-2 2 .9 2 2 2zm0 2c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2zm0 6c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2z"/></svg></span><div class="lic `+ILCLS[item.id]+`">`+ISVG[item.id]+`</div><div class="tile-body"><div class="tile-head">`+NAMES[item.id]+`</div>`+(isNp?`<div class="tile-sub" id="nptxt-`+idx+`">Astept date de la PC...</div>`:isWx?`<div class="tile-sub" id="wxtxt-`+idx+`">`+(wxCityLabel||`Se incarca...`)+`</div>`:isCanvas?`<div class="tile-sub">Desen personalizat 8×32</div>`:isMemoTile?`<div class="tile-sub" id="memotxt-`+idx+`">`+(memoText||`Niciun text configurat`)+`</div>`:isHour?`<div class="tile-sub" id="hourtxt-`+idx+`">`+hourPreviewText()+`</div>`:isDate?`<div class="tile-sub" id="datetxt-`+idx+`">`+datePreviewText()+`</div>`:isTemp?`<div class="tile-sub" id="temptxt-`+idx+`">`+tempPreviewText()+`</div>`:isPressure?`<div class="tile-sub" id="pressuretxt-`+idx+`">`+pressurePreviewText()+`</div>`:isScreensaver?`<div class="tile-sub" id="sstxt-`+idx+`">`+ssAnimPreviewText()+`</div>`:isCurrency?`<div class="tile-sub" id="currencytxt-`+idx+`">`+currencyPreviewText()+`</div>`:``)+`</div>`+tempGearHtml+hourGearHtml+dateGearHtml+npGearHtml+wxGearHtml+memoGearHtml+canvasGearHtml+pressureGearHtml+ssGearHtml+currencyGearHtml+swHtml;var container=document.createElement(`div`);container.style.position=`relative`;container.dataset.idx=String(idx);if(isNp){container.appendChild(d)}else{container.appendChild(d)}g.appendChild(container);var dragTarget=d;dragTarget.draggable=!0;var rng=d.querySelector(`input[type=range]`);rng&&(rng.addEventListener(`mousedown`,function(e){dragTarget.draggable=!1}),rng.addEventListener(`touchstart`,function(e){dragTarget.draggable=!1},{passive:!0}),document.addEventListener(`mouseup`,function(){dragTarget.draggable=!0},{once:!1,passive:!0}),document.addEventListener(`touchend`,function(){dragTarget.draggable=!0},{once:!1,passive:!0}));dragTarget.addEventListener(`dragstart`,function(e){if(!dragTarget.draggable)return e.preventDefault();dragSrc=idx;dragCtx={list:`circuit`,idx:idx};dragTarget.classList.add(`dragging`);e.dataTransfer.effectAllowed=`move`});dragTarget.addEventListener(`dragend`,function(){document.querySelectorAll(`.tile-item`).forEach(function(el){el.classList.remove(`dragging`,`drag-over`)});dragTarget.draggable=!0});dragTarget.addEventListener(`dragover`,function(e){e.preventDefault();dragTarget.classList.add(`drag-over`)});dragTarget.addEventListener(`dragleave`,function(e){if(!dragTarget.contains(e.relatedTarget))dragTarget.classList.remove(`drag-over`)});dragTarget.addEventListener(`drop`,function(e){e.preventDefault();dragTarget.classList.remove(`drag-over`);if(dragCtx&&dragCtx.list===`priority`){var moved=priorityItems[dragCtx.idx];if(moved&&moved.id===`nowplaying`){priorityItems.splice(dragCtx.idx,1);var npIdx2=items.findIndex(function(it){return it.id===3});if(npIdx2>=0){var npItem=items.splice(npIdx2,1)[0];var insertAt=idx>npIdx2?idx-1:idx;items.splice(insertAt,0,npItem)}dragCtx=null;dragSrc=null;dragSrc2=null;buildGrid();buildPriorityGrid();saveSettings();savePriorityOrder()}else{dragCtx=null}return}if(dragSrc===null||dragSrc==)pl10"
R"pl11(=idx)return;var mv=items.splice(dragSrc,1)[0];items.splice(idx,0,mv);dragSrc=null;dragCtx=null;buildGrid();saveSettings()})}),updSub(),applyHideIcons(),applyScrollType()}function buildPriorityGrid(){var g=document.getElementById(`pgrid`);g.innerHTML=``;priorityItems.forEach(function(item,idx){var isEts2=item.id===`ets2`,isNowPlaying=item.id===`nowplaying`,isStopwatch=item.id===`stopwatch`,npIdx=isNowPlaying?items.findIndex(function(it){return it.id===3}):-1,gearSvg=`<svg width=\"18\" height=\"18\" viewBox=\"0 0 24 24\" fill=\"currentColor\"><path d=\"M19.14 12.94c.04-.3.06-.61.06-.94s-.02-.64-.07-.94l2.03-1.58a.49.49 0 0 0 .12-.61l-1.92-3.32a.488.488 0 0 0-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54a.484.484 0 0 0-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87a.49.49 0 0 0 .12.61l2.03 1.58c-.05.3-.07.62-.07.94s.02.64.07.94l-2.03 1.58a.49.49 0 0 0-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32a.49.49 0 0 0-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z\"/></svg>`,icon=isNowPlaying?ISVG[3]:isEts2?`<svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M20 8h-3V4H3c-1.1 0-2 .9-2 2v11h2c0 1.66 1.34 3 3 3s3-1.34 3-3h6c0 1.66 1.34 3 3 3s3-1.34 3-3h2v-5l-3-4zM6 18.5c-.83 0-1.5-.67-1.5-1.5s.67-1.5 1.5-1.5 1.5.67 1.5 1.5-.67 1.5-1.5 1.5zm12 0c-.83 0-1.5-.67-1.5-1.5s.67-1.5 1.5-1.5 1.5.67 1.5 1.5-.67 1.5-1.5 1.5zm-1-9.5h2.5l2.07 2.5H17V9z"/></svg>`:isStopwatch?`<svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M15 1H9v2h6V1zm-4 13h2V8h-2v6zm8.03-6.61l1.42-1.42c-.43-.51-.9-.99-1.41-1.41l-1.42 1.42A8.962 8.962 0 0012 4c-4.97 0-9 4.03-9 9s4.02 9 9 9a8.994 8.994 0 007.03-14.61zM12 20c-3.87 0-7-3.13-7-7s3.13-7 7-7 7 3.13 7 7-3.13 7-7 7z"/></svg>`:`<svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M12 22c1.1 0 2-.9 2-2h-4c0 1.1.9 2 2 2zm6-6v-5c0-3.07-1.64-5.64-4.5-6.32V4c0-.83-.67-1.5-1.5-1.5s-1.5.67-1.5 1.5v.68C7.63 5.36 6 7.92 6 11v5l-2 2v1h16v-1l-2-2z"/></svg>`,cls=isNowPlaying?`lc-amb`:isEts2?`lc-blu`:isStopwatch?`lc-grn`:`lc-pur`,name=isNowPlaying?`Now Playing`:isEts2?`Euro Truck Simulator 2`:isStopwatch?`Stopwatch`:`Notificari PC`,subHtml=isNowPlaying?`<div class=\"tile-sub\" id=\"nptxt-p`+idx+`\">Astept date de la PC...</div>`:isStopwatch?`<div class=\"tile-sub\" id=\"sw-sub\">`+(swRunning?`Ruleaza — `+(swLastText||`00:00:00:00`):`Oprit`)+`</div>`:`<div class=\"tile-sub\" id=\"`+(isEts2?`ets2-sub`:`notif-sub`)+`\">`+(isEts2?`Inactiv`:`Afisare instantanee, intrerup bucla normala`)+`</div>`,gearHtml=isNowPlaying?`<button class=\"np-gear-btn\" onclick=\"openNpSettingsDlg(event)\" title=\"Setari Now Playing\">`+gearSvg+`</button>`:``,swHtml=isNowPlaying?`<div class=\"sw\" onclick=\"toggleTile(`+npIdx+`)\"><input type=\"checkbox\" id=\"npPrioCb`+idx+`\"`+(npIdx>=0&&items[npIdx].enabled?` checked`:``)+`><span class=\"sw-track\"></span><span class=\"sw-thumb\"></span></div>`:isStopwatch?`<button class=\"np-gear-btn sw-toggle-btn`+(swRunning?` playing`:``)+`\" id=\"sw-toggle-btn\" onclick=\"toggleStopwatch()\" title=\"Start / Stop\">`+(swRunning?`<svg width=\"20\" height=\"20\" viewBox=\"0 0 24 24\" fill=\"currentColor\"><path d=\"M6 19h4V5H6v14zm8-14v14h4V5h-4z\"/></svg>`:`<svg width=\"20\" height=\"20\" viewBox=\"0 0 24 24\" fill=\"currentColor\"><path d=\"M8 5v14l11-7z\"/></svg>`)+`</button>`:`<div class=\"sw\" onclick=\"`+(isEts2?`toggleEts2()`:`toggleNotif()`)+`\"><input type=\"checkbox\" id=\"`+(isEts2?`ets2-cb`:`notif-cb`)+`\"`+((isEts2?ets2En:notifEn)?` checked`:``)+`><span class=\"sw-track\"></span><span class=\"sw-thumb\"></span></div>`,d=document.createElement(`div`);d.className=`tile-item`;d.innerHTML=`<span class=\"drag-handle\"><svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M11 18c0 1.1-.9 2-2 2s-2-.9-2-2 .9-2 2-2 2 .9 2 2zm-2-8c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2zm0-6c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2zm6 4c1.1 0 2-.9 2-2s-.9-2-2-2-2 .9-2 2 .9 2 2 2zm0 2c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2zm0 6c-1.1 0-2 .9-2 2s.9 2 2 2 2-.9 2-2-.9-2-2-2z"/></svg></span><div class=\"lic `+cls+`\">`+icon+`</div><div class=\"tile-body\"><div class=\"tile-head\">`+name+`</div>`+subHtml+`</div>`+gearHtml+swHtml;g.appendChild(d);d.draggable=!0;d.addEventListener(`dragstart`,function(e){dragSrc2=idx;dragCtx={list:`priority`,idx:idx};d.classList.add(`dragging`);e.dataTransfer.effectAllowed=`move`});d.addEventListener(`dragend`,function(){document.querySelectorAll(`.tile-item`).forEach(function(el){el.classList.remove(`dragging`,`drag-over`)})});d.addEventListener(`dragover`,function(e){e.preventDefault();d.classList.add(`drag-over`)});d.addEventListener(`dragleave`,function(e){if(!d.contains(e.relatedTarget))d.classList.remove(`drag-over`)});d.addEventListener(`drop`,function(e){e.preventDefault();d.classList.remove(`drag-over`);if(dragCtx&&dragCtx.list===`circuit`){var srcItem=items[dragCtx.idx];if(srcItem&&srcItem.id===3){priorityItems.splice(idx,0,{id:`nowplaying`,enabled:!0});dragCtx=null;dragSrc=null;dragSrc2=null;buildGrid();buildPriorityGrid();saveSettings();savePriorityOrder()}else{dragCtx=null}return}if(dragSrc2===null||dragSrc2===idx)return;var mv=priorityItems.splice(dragSrc2,1)[0];priorityItems.splice(idx,0,mv);dragSrc2=null;dragCtx=null;buildPriorityGrid();savePriorityOrder()})})}function saveEts2Order(){var ord=priorityItems[0].id===`ets2`?1:0;fetch(`/ets2order`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`first=`+ord})}function npIsPriority(){return priorityItems.some(function(it){return it.id===`nowplaying`})}function savePriorityOrder(){var ord=priorityItems.map(function(it){return it.id}).join(`,`);fetch(`/priorityorder`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`order=`+encodeURIComponent(ord)})}function updDur(idx,v){items[idx].dur=parseInt(v);var e=document.getElementById(`dv`+idx);e&&(e.textContent=v+`s`)}function hourPreviewText(){return hourFormatV===1?`Format 12h (1:45)`:`Format 24h (13:45)`}function datePreviewText(){var arr=[`2026.06.18`,`26.06.18`,`18.06.2026`,`18.06`];return arr[dateFormatV]||arr[2]}function tempPreviewText(){return tempUnitV===1?`Fahrenheit (°F)`:`Celsius (°C)`}function pressureTrendArrow(){return pressureTrendV>0?`↑`:pressureTrendV<0?`↓`:`—`}function pressurePreviewText(){return pressureHpaV+` hPa `+pressureTrendArrow()}function currencyTrendArrow(){return currencyTrendV>0?`↑`:currencyTrendV<0?`↓`:`—`}function currencyPreviewText(){if(!currencyValidV)return `Se incarca...`;if(currencyCompareV){var r=currencyRateV;var rs=Math.abs(r)>=1?r.toFixed(2):r.toFixed(4);return `1 `+currencyBaseV+` = `+rs+` `+currencyQuoteV+`  `+currencyTrendArrow()}return currencyBaseV+`  `+currencyTrendArrow()}function refreshCurrencyTilePreview(){var el=document.getElementById(`currencytxt-`+items.findIndex(function(i){return i.id===11}));if(el)el.textContent=currencyPreviewText()}function onCurrencyDurInput(v){var idx=items.findIndex(function(it){return it.id===11});if(idx<0)return;items[idx].dur=parseInt(v);var vl=document.getElementById(`currency-dur-val`);if(vl)vl.textContent=v+`s`}function toggleCurrencyCompare(){var cb=document.getElementById(`currency-compare-cb`);if(!cb)return;cb.checked=!cb.checked;var w=document.getElementById(`currency-quote-wrap`);if(w)w.style.display=cb.checked?`block`:`none`}function openCurrencySettingsDlg(e){e&&e.stopPropagation();var idx=items.findIndex(function(it){return it.id===11});if(idx>=0){var it=items[idx];var sl=document.getElementById(`currency-dur-slider`);if(sl){sl.value=it.dur;sl.disabled=!it.enabled}var vl=document.getElementById(`currency-dur-val`);if(vl)vl.textContent=it.dur+`s`}var bsel=document.getElementById(`currency-base-sel`);if(bsel)bsel.value=currencyBaseV;var qsel=document.getElementById(`currency-quote-sel`);if(qsel)qsel.value=currencyQuoteV;var ccb=document.getElementById(`currency-compare-cb`);if(ccb)ccb.checked=currencyCompareV;var w=document.getElementById(`currency-quote-wrap`);if(w)w.style.display=currencyCompareV?`block`:`none`;document.getElementById(`currency-sett-scrim`).classList.add(`open`);document.getElementById(`currency-sett-dlg`).classList.add(`open`)}function closeCurrencySettingsDlg(){document.getElementById(`currency-sett-scrim`).classList.remove(`open`);document.getElementById(`currency-sett-dlg`).classList.remove(`open`)}function saveCurrencySettings(){var bsel=document.getElementById(`currency-base-sel`);var qsel=document.getElementById(`currency-quote-sel`);var ccb=document.getElementById(`currency-compare-cb`);currencyBaseV=bsel?bsel.value:currencyBaseV;currencyCompareV=ccb?ccb.checked:currencyCompareV;currencyQuoteV=qsel?qsel.value:currencyQuoteV;currencyValidV=!1;refreshCurrencyTilePreview();saveSettings();var f=new FormData();f.append(`base`,currencyBaseV);f.append(`compare`,currencyCompareV?1:0);f.append(`quote`,currencyQuoteV);fetch(`/currencysett`,{method:`POST`,body:f}).then(function(){closeCurrencySettingsDlg();setTimeout(function(){fetch(`/state`).then(function(r){return r.json()}).then(function(s){if(s.currencyBase!==void 0){currencyBaseV=s.currencyBase;currencyQuoteV=s.currencyQuote;currencyCompareV=!!s.currencyCompare;currencyValidV=!!s.currencyValid;currencyRateV=s.currencyRate!==void 0?s.currencyRate:0;currencyTrendV=s.currencyTrend!==void 0?s.currencyTrend:0;refreshCurrencyTilePreview()}}).catch(function(){})},2500)}).catch(function(){closeCurrencySettingsDlg()})}
function ssAnimPreviewText(){return SS_ANIM_OPTIONS[ssAnimV]||`Random`}
function refreshSsTilePreview(){var el=document.getElementById(`sstxt-`+items.findIndex(function(i){return i.id===9}));if(el)el.textContent=ssAnimPreviewText()}
function setSsAnim(v){ssAnimV=v;buildScreensaverList();refreshSsTilePreview();var f=new FormData();f.append(`anim`,v);fetch(`/screensaversett`,{method:`POST`,body:f})}
function buildScreensaverList(){var c=document.getElementById(`ss-anim-list`);if(!c)return;c.innerHTML=``;SS_ANIM_OPTIONS.forEach(function(name,i){var row=document.createElement(`div`);row.className=`sel-row`;row.onclick=function(){setSsAnim(i)};var radio=`<span class="sel-radio`+(ssAnimV===i?` sel-radio-on`:``)+`"></span>`;row.innerHTML=radio+`<span class="sel-label">`+name+`</span>`;c.appendChild(row)})}
function refreshPressureTilePreview(){document.querySelectorAll(`[id^=pressuretxt-]`).forEach(function(el){el.textContent=pressurePreviewText()})}function refreshHourTilePreview(){document.querySelectorAll(`[id^=hourtxt-]`).forEach(function(el){el.textContent=hourPreviewText()})}function refreshDateTilePreview(){document.querySelectorAll(`[id^=datetxt-]`).forEach(function(el){el.textContent=datePreviewText()})}function refreshTempTilePreview(){document.querySelectorAll(`[id^=temptxt-]`).forEach(function(el){el.textContent=tempPreviewText()})}function onHourDurInput(v){var idx=items.findIndex(function(it){return it.id===0});if(idx<0)return;items[idx].dur=parseInt(v);var e=document.getElementById(`hour-dur-val`);e&&(e.textContent=v+`s`)}function onDateDurInput(v){var idx=items.findIndex(function(it){return it.id===1});if(idx<0)return;items[idx].dur=parseInt(v);var e=document.getElementById(`date-dur-val`);e&&(e.textContent=v+`s`)}function onTempDurInput(v){var idx=items.findIndex(function(it){return it.id===2});if(idx<0)return;items[idx].dur=parseInt(v);var e=document.getElementById(`temp-dur-val`);e&&(e.textContent=v+`s`)}function openPressureSettingsDlg(e){e&&e.stopPropagation();var idx=items.findIndex(function(it){return it.id===8});if(idx>=0){var it=items[idx];var sl=document.getElementById(`pressure-dur-slider`);if(sl){sl.value=it.dur;sl.disabled=!it.enabled}var vl=document.getElementById(`pressure-dur-val`);if(vl)vl.textContent=it.dur+`s`}document.getElementById(`pressure-sett-scrim`).class)pl11"
R"pl12(List.add(`open`);document.getElementById(`pressure-sett-dlg`).classList.add(`open`)}function closePressureSettingsDlg(){document.getElementById(`pressure-sett-scrim`).classList.remove(`open`);document.getElementById(`pressure-sett-dlg`).classList.remove(`open`)}function openSsSettingsDlg(e){e&&e.stopPropagation();var idx=items.findIndex(function(it){return it.id===9});if(idx>=0){var it=items[idx];var sl=document.getElementById(`ss-dur-slider`);if(sl){sl.value=it.dur;sl.disabled=!it.enabled}var vl=document.getElementById(`ss-dur-val`);if(vl)vl.textContent=it.dur+`s`}buildScreensaverList();document.getElementById(`ss-sett-scrim`).classList.add(`open`);document.getElementById(`ss-sett-dlg`).classList.add(`open`)}function closeSsSettingsDlg(){document.getElementById(`ss-sett-scrim`).classList.remove(`open`);document.getElementById(`ss-sett-dlg`).classList.remove(`open`)}function onSsDurInput(v){var idx=items.findIndex(function(it){return it.id===9});if(idx<0)return;items[idx].dur=parseInt(v);var vl=document.getElementById(`ss-dur-val`);if(vl)vl.textContent=v+`s`}function onPressureDurInput(v){var idx=items.findIndex(function(it){return it.id===8});if(idx<0)return;items[idx].dur=parseInt(v);var vl=document.getElementById(`pressure-dur-val`);if(vl)vl.textContent=v+`s`}function toggleTile(idx){items[idx].enabled=!items[idx].enabled;var cb=document.getElementById(`tsw`+idx);if(cb)cb.checked=items[idx].enabled;var rng=document.querySelector(`[data-idx="`+idx+`"] input[type=range]`);if(rng){rng.disabled=!items[idx].enabled}var npEx=document.getElementById(`np-expand`);if(npEx)npEx.className=`np-expand`+(items[idx].enabled?` open`:``);if(items[idx].id===3&&npIsPriority()){var pIdx=priorityItems.findIndex(function(it){return it.id===`nowplaying`});if(pIdx>=0){var pcb=document.getElementById(`npPrioCb`+pIdx);if(pcb)pcb.checked=items[idx].enabled}}updSub();saveSettings()}function updSub(){var a=items.filter(function(it){return it.enabled&&!(it.id===3&&npIsPriority())}).map(function(it){return NAMES[it.id]});document.getElementById(`h-tiles-sub`).textContent=a.join(` ·`)||`Nimic activ`}function setNpMode(m){npM=m,[0,1,2].forEach(function(i){var e=document.getElementById(`npMode`+i);e&&(e.className=`sb`+(i===m?` on`:``))}),fetch(`/npmode`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`mode=`+m})}function saveSettings(){var p=`buzzer=`+(bOn?1:0)+`&hideIcons=`+(hideTileIcons?1:0)+`&hideIconDate=`+(indivHideIcons.date?1:0)+`&hideIconTemp=`+(indivHideIcons.temp?1:0)+`&hideIconReminder=`+(indivHideIcons.reminder?1:0)+`&hideIconWeather=`+(indivHideIcons.weather?1:0)+`&hideIconNotif=`+(indivHideIcons.notif?1:0)+`&hideIconNowPlaying=`+(indivHideIcons.nowplaying?1:0)+`&hideIconPressure=`+(indivHideIcons.pressure?1:0)+`&hideIconCurrency=`+(indivHideIcons.currency?1:0)+`&scrollType=`+scrollTypeV+`&scrollTypeDate=`+indivScroll.date+`&scrollTypeTemp=`+indivScroll.temp+`&scrollTypeReminder=`+indivScroll.reminder+`&scrollTypeWeather=`+indivScroll.weather+`&scrollTypeNotif=`+indivScroll.notif+`&scrollTypeNowPlaying=`+indivScroll.nowplaying+`&scrollTypePressure=`+indivScroll.pressure+`&scrollTypeStopwatch=`+indivScroll.stopwatch+`&scrollTypeCurrency=`+indivScroll.currency;items.forEach(function(it,i){p+=`&id`+i+`=`+it.id+`&en`+i+`=`+(it.enabled?1:0)+`&dur`+i+`=`+it.dur}),fetch(`/settings`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:p})}function openSettingsDlg(){var list=document.getElementById(`sett-items-list`);list.innerHTML=``;items.forEach(function(item,idx){var row=document.createElement(`div`);row.style.cssText=`display:flex;justify-content:space-between;align-items:center;padding:10px 0`;var nameEl=document.createElement(`div`);nameEl.innerHTML=`<div style="color:var(--on-surf);font-family:Google Sans,sans-serif;font-size:15px">`+NAMES[item.id]+`</div>`;var sw=document.createElement(`div`);sw.className=`sw`;sw.onclick=function(){toggleTile(idx);buildSettItemsList()};sw.innerHTML=`<input type="checkbox" `+(item.enabled?`checked`:``)+`><span class="sw-track"></span><span class="sw-thumb"></span>`;row.appendChild(nameEl);row.appendChild(sw);list.appendChild(row);if(idx<items.length-1){var sep=document.createElement(`div`);sep.style.cssText=`height:1px;background:var(--outline-var)`;list.appendChild(sep)}});document.getElementById(`sett-buz-cb`).checked=bOn;document.getElementById(`sett-scrim`).classList.add(`open`);document.getElementById(`sett-dlg`).classList.add(`open`)}function buildSettItemsList(){var list=document.getElementById(`sett-items-list`);if(!list)return;list.innerHTML=``;items.forEach(function(item,idx){var row=document.createElement(`div`);row.style.cssText=`display:flex;justify-content:space-between;align-items:center;padding:10px 0`;var nameEl=document.createElement(`div`);nameEl.innerHTML=`<div style="color:var(--on-surf);font-family:Google Sans,sans-serif;font-size:15px">`+NAMES[item.id]+`</div>`;var sw=document.createElement(`div`);sw.className=`sw`;sw.onclick=function(){toggleTile(idx);buildSettItemsList()};sw.innerHTML=`<input type="checkbox" `+(item.enabled?`checked`:``)+`><span class="sw-track"></span><span class="sw-thumb"></span>`;row.appendChild(nameEl);row.appendChild(sw);list.appendChild(row);if(idx<items.length-1){var sep=document.createElement(`div`);sep.style.cssText=`height:1px;background:var(--outline-var)`;list.appendChild(sep)}})}function closeSettingsDlg(){document.getElementById(`sett-scrim`).classList.remove(`open`);document.getElementById(`sett-dlg`).classList.remove(`open`)}function toggleBuzzerSett(){bOn=!bOn;document.getElementById(`sett-buz-cb`).checked=bOn;document.getElementById(`btog-cb`).checked=bOn;document.getElementById(`h-bsub`).textContent=bOn?`Beep la schimbarea tile-ului`:`Silentios`;saveSettings()}function onBrightInput(v){brightLevel=parseInt(v);var lbl=document.getElementById(`bright-screen-lbl`);if(lbl)lbl.textContent=`Nivel `+v;document.getElementById(`h-bright-sub`).textContent=`Nivel `+v;var bv=document.getElementById(`br-val`);if(bv)bv.textContent=v;fetch(`/brightness`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`level=`+v+`&dimAuto=`+(dimAuto?1:0)+`&dimFrom=`+dimFrom+`&dimTo=`+dimTo+`&dimLevel=`+dimLevel})}function onDimLevelInput(v){dimLevel=parseInt(v);document.getElementById(`dim-level-lbl`).textContent=v;fetch(`/brightness`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`level=`+brightLevel+`&dimAuto=`+(dimAuto?1:0)+`&dimFrom=`+dimFrom+`&dimTo=`+dimTo+`&dimLevel=`+v})}var dimInputsBound=false;function bindDimScheduleInputs(){if(dimInputsBound)return;dimInputsBound=true;var df=document.getElementById(`dim-from`);if(df)df.addEventListener(`change`,function(){dimFrom=this.value;sendBrightSettings()});var dt=document.getElementById(`dim-to`);if(dt)dt.addEventListener(`change`,function(){dimTo=this.value;sendBrightSettings()})}function toggleDimAuto(){dimAuto=!dimAuto;document.getElementById(`dim-auto-cb`).checked=dimAuto;document.getElementById(`dim-sched`).classList.toggle(`open`,dimAuto);sendBrightSettings()}function sendBrightSettings(){fetch(`/brightness`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`level=`+brightLevel+`&dimAuto=`+(dimAuto?1:0)+`&dimFrom=`+dimFrom+`&dimTo=`+dimTo+`&dimLevel=`+dimLevel})}function openNpSettingsDlg(e){e&&e.stopPropagation();var npIdx=items.findIndex(function(it){return it.id===3});if(npIdx<0)return;[0,1,2].forEach(function(i){var b=document.getElementById(`npMode`+i);b&&(b.className=`sb`+(i===npM?` on`:``))});document.getElementById(`np-sett-scrim`).classList.add(`open`);document.getElementById(`np-sett-dlg`).classList.add(`open`)}function closeNpSettingsDlg(){document.getElementById(`np-sett-scrim`).classList.remove(`open`);document.getElementById(`np-sett-dlg`).classList.remove(`open`)}function pollNP(){fetch(`/npstate`).then(function(r){return r.json()}).then(function(s){var all=document.querySelectorAll(`[id^=nptxt-]`);all.forEach(function(el){el.textContent=s.active?`▶  `+s.text:`Astept date de la PC…`});s.tempunit!==void 0&&(tempUnitV=s.tempunit,[`tempUnitC`,`tempUnitF`,`wxTempUnitC`,`wxTempUnitF`].forEach(function(bid,bi){var b=document.getElementById(bid);b&&(b.className=`sb`+(bi%2===tempUnitV?` on`:``))}),refreshTempTilePreview());s.npmode!==void 0&&(npM=s.npmode,[0,1,2].forEach(function(i){var e=document.getElementById(`npMode`+i);e&&(e.className=`sb`+(i===s.npmode?` on`:``))}));s.hourformat!==void 0&&(hourFormatV=s.hourformat,[`hourFmt24`,`hourFmt12`].forEach(function(b,i){var el=document.getElementById(b);el&&(el.className=`sb`+(i===hourFormatV?` on`:``))}),refreshHourTilePreview()),s.dateformat!==void 0&&(dateFormatV=s.dateformat,[`dateFmt0`,`dateFmt1`,`dateFmt2`,`dateFmt3`].forEach(function(b,i){var el=document.getElementById(b);el&&(el.className=`sb`+(i===dateFormatV?` on`:``))}),refreshDateTilePreview())}).catch(functio)pl12"
R"pl13(n(){}),setTimeout(pollNP,3e3)}function openTempSettingsDlg(e){e&&e.stopPropagation();[`tempUnitC`,`tempUnitF`].forEach(function(bid,bi){var b=document.getElementById(bid);b&&(b.className=`sb`+(bi===tempUnitV?` on`:``))});var tIdx=items.findIndex(function(it){return it.id===2});if(tIdx>=0){var it=items[tIdx];var sl=document.getElementById(`temp-dur-slider`);if(sl){sl.value=it.dur;sl.disabled=!it.enabled}var vl=document.getElementById(`temp-dur-val`);if(vl)vl.textContent=it.dur+`s`}document.getElementById(`temp-sett-scrim`).classList.add(`open`);document.getElementById(`temp-sett-dlg`).classList.add(`open`)}function closeTempSettingsDlg(){document.getElementById(`temp-sett-scrim`).classList.remove(`open`);document.getElementById(`temp-sett-dlg`).classList.remove(`open`)}function setTempUnit(u){tempUnitV=u;[`tempUnitC`,`tempUnitF`].forEach(function(bid,bi){var b=document.getElementById(bid);b&&(b.className=`sb`+(bi===u?` on`:``))});[`wxTempUnitC`,`wxTempUnitF`].forEach(function(bid,bi){var b=document.getElementById(bid);b&&(b.className=`sb`+(bi===u?` on`:``))});refreshTempTilePreview();fetch(`/tempunit`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`unit=`+u})}function setWxTempUnit(u){setTempUnit(u)}var wxSelLat=null,wxSelLon=null,wxSelName=``,wxSearchTimer=null;function toggleWxKeyVis(){var ki=document.getElementById(`wx-key-in`);var icon=document.getElementById(`wx-eye-icon`);var eyeOpen=`<path d=\"M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zM12 17c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z\"\/>`;var eyeClosed=`<path d=\"M12 7c2.76 0 5 2.24 5 5 0 .65-.13 1.26-.36 1.83l2.92 2.92c1.51-1.26 2.7-2.89 3.43-4.75-1.73-4.39-6-7.5-11-7.5-1.4 0-2.74.25-3.98.7l2.16 2.16C10.74 7.13 11.35 7 12 7zM2 4.27l2.28 2.28.46.46C3.08 8.3 1.78 10.02 1 12c1.73 4.39 6 7.5 11 7.5 1.55 0 3.03-.3 4.38-.84l.42.42L19.73 22 21 20.73 3.27 3 2 4.27zM7.53 9.8l1.55 1.55c-.05.21-.08.43-.08.65 0 1.66 1.34 3 3 3 .22 0 .44-.03.65-.08l1.55 1.55c-.67.33-1.41.53-2.2.53-2.76 0-5-2.24-5-5 0-.79.2-1.53.53-2.2zm4.31-.78l3.15 3.15.02-.16c0-1.66-1.34-3-3-3l-.17.01z\"\/>`;if(ki.dataset.masked===`1`){ki.disabled=!0;ki.style.opacity=`0.5`;fetch(`/weatherkey`).then(function(r){if(!r.ok)throw new Error(r.status);return r.json()}).then(function(d){ki.value=d.key||``;ki.dataset.masked=`0`;ki.type=`text`;ki.style.webkitTextSecurity=``;ki.style.fontFamily=``;ki.disabled=!1;ki.style.opacity=``;icon.innerHTML=eyeClosed;ki.focus()}).catch(function(){ki.dataset.masked=`0`;ki.value=``;ki.type=`password`;ki.style.webkitTextSecurity=``;ki.style.fontFamily=``;ki.disabled=!1;ki.style.opacity=``;icon.innerHTML=eyeOpen});return}var isPass=(ki.type===`password`);ki.type=isPass?`text`:`password`;ki.style.webkitTextSecurity=``;ki.style.fontFamily=``;icon.innerHTML=isPass?eyeClosed:eyeOpen}function onWxKeyInput(){var ki=document.getElementById(`wx-key-in`);if(ki.dataset.masked===`1`){ki.value=``;ki.dataset.masked=`0`}var k=ki.value.trim();var hint=document.getElementById(`wx-no-key-hint`);if(hint)hint.style.display=k?`none`:`block`}function onWxSearch(){var ki=document.getElementById(`wx-key-in`);var k=(ki.dataset.masked===`1`)?`__masked__`:ki.value.trim();var hasKey=wxHasStoredKey||(k.length>0&&k!==`__masked__`);var q=document.getElementById(`wx-search-in`).value.trim();var sug=document.getElementById(`wx-suggest`);if(!hasKey){document.getElementById(`wx-no-key-hint`).style.display=`block`;return}document.getElementById(`wx-no-key-hint`).style.display=`none`;if(q.length<2){sug.innerHTML=``;sug.className=`wx-suggest`;return}clearTimeout(wxSearchTimer);wxSearchTimer=setTimeout(function(){var apiK=(k===`__masked__`||k.length===0)?``:k;fetch(`/weathersearch?q=`+encodeURIComponent(q)+`&key=`+encodeURIComponent(apiK)).then(function(r){if(!r.ok)return r.text().then(function(t){throw new Error(t||r.status)});return r.json()}).then(function(res){sug.innerHTML=``;if(!res||!res.length){sug.innerHTML=`<div class=\"wx-si\" style=\"cursor:default;color:var(--on-surf-var)\">Nicio localitate gasita</div>`;sug.className=`wx-suggest open`;return}res.forEach(function(loc){var d=document.createElement(`div`);d.className=`wx-si`;var country=loc.country||``;var state=loc.state?loc.state+`, `:``;d.innerHTML=`<div class=\"wx-si-main\">`+loc.name+`</div><div class=\"wx-si-sub\">`+state+country+`</div>`;d.onclick=function(){wxSelLat=loc.lat;wxSelLon=loc.lon;wxSelName=loc.name+(loc.state?`, `+loc.state:``)+`, `+country;document.getElementById(`wx-search-in`).value=loc.name;var badge=document.getElementById(`wx-sel-badge`);badge.style.display=`none`;sug.innerHTML=``;sug.className=`wx-suggest`;document.getElementById(`wx-save-btn`).disabled=!1};sug.appendChild(d)});sug.className=`wx-suggest open`}).catch(function(err){sug.innerHTML=`<div class=\"wx-si\" style=\"cursor:default;color:var(--err)\">Eroare: `+err.message+`</div>`;sug.className=`wx-suggest open`})},400)}var wxHasStoredKey=!1;var wxCityLabel=``;var owmLangCode=`en`;var owmLangs=[{c:`af`,n:`Afrikaans`},{c:`al`,n:`Albanian`},{c:`ar`,n:`Arabic`},{c:`az`,n:`Azerbaijani`},{c:`bg`,n:`Bulgarian`},{c:`ca`,n:`Catalan`},{c:`cz`,n:`Czech`},{c:`da`,n:`Danish`},{c:`de`,n:`German`},{c:`el`,n:`Greek`},{c:`en`,n:`English`},{c:`eu`,n:`Basque`},{c:`fa`,n:`Persian`},{c:`fi`,n:`Finnish`},{c:`fr`,n:`French`},{c:`gl`,n:`Galician`},{c:`he`,n:`Hebrew`},{c:`hi`,n:`Hindi`},{c:`hr`,n:`Croatian`},{c:`hu`,n:`Hungarian`},{c:`id`,n:`Indonesian`},{c:`it`,n:`Italian`},{c:`ja`,n:`Japanese`},{c:`kr`,n:`Korean`},{c:`la`,n:`Latvian`},{c:`lt`,n:`Lithuanian`},{c:`mk`,n:`Macedonian`},{c:`nl`,n:`Dutch`},{c:`no`,n:`Norwegian`},{c:`pl`,n:`Polish`},{c:`pt`,n:`Portuguese`},{c:`pt_br`,n:`Portuguese Brazil`},{c:`ro`,n:`Romanian`},{c:`ru`,n:`Russian`},{c:`sk`,n:`Slovak`},{c:`sl`,n:`Slovenian`},{c:`sp`,n:`Spanish`},{c:`sr`,n:`Serbian`},{c:`sv`,n:`Swedish`},{c:`th`,n:`Thai`},{c:`tr`,n:`Turkish`},{c:`ua`,n:`Ukrainian`},{c:`vi`,n:`Vietnamese`},{c:`zh_cn`,n:`Chinese Simplified`},{c:`zh_tw`,n:`Chinese Traditional`},{c:`zu`,n:`Zulu`}];function owmLangLabel(c){var f=owmLangs.find(function(l){return l.c===c});return f?f.n+` (`+c+`)`:c}function openLangPicker(){buildLangList(``);document.getElementById(`lang-search`).value=``;document.getElementById(`lang-scrim`).classList.add(`open`);document.getElementById(`lang-dlg`).classList.add(`open`);setTimeout(function(){document.getElementById(`lang-search`).focus()},220)}function closeLangPicker(){document.getElementById(`lang-scrim`).classList.remove(`open`);document.getElementById(`lang-dlg`).classList.remove(`open`)}function filterLangs(){var q=document.getElementById(`lang-search`).value.trim().toLowerCase();buildLangList(q)}function buildLangList(q){var list=document.getElementById(`lang-list`);list.innerHTML=``;var src=q?owmLangs.filter(function(l){return l.n.toLowerCase().includes(q)||l.c.toLowerCase().includes(q)}):owmLangs;src.forEach(function(l){var isSel=(l.c===owmLangCode);var d=document.createElement(`div`);d.style.cssText=`display:flex;align-items:center;justify-content:space-between;padding:14px 24px;border-bottom:1px solid var(--outline-var);cursor:pointer;transition:background .12s;background:`+(isSel?`color-mix(in srgb,var(--pri)14%,transparent)`:``);d.innerHTML=`<div><div style="font-family:Google Sans,sans-serif;font-size:15px;color:`+(isSel?`var(--pri)`:`var(--on-surf)`)+`">`+l.n+`</div><div style="font-size:12px;color:var(--on-surf-var);margin-top:2px">`+l.c+`</div></div>`+(isSel?`<svg width="20" height="20" viewBox="0 0 24 24" fill="var(--pri)"><path d="M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z"/></svg>`:``);d.onmouseenter=function(){if(!isSel)d.style.background=`color-mix(in srgb,var(--on-surf)6%,transparent)`};d.onmouseleave=function(){if(!isSel)d.style.background=``};d.onclick=function(){owmLangCode=l.c;fetch(`/wxlang`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`lang=`+encodeURICom)pl13"
R"pl14(ponent(l.c)});var s1=document.getElementById(`owm-lang-sub`);if(s1)s1.textContent=owmLangLabel(l.c);var s2=document.getElementById(`h-lang-sub`);if(s2)s2.textContent=owmLangLabel(l.c);closeLangPicker()};list.appendChild(d)})}function openWxSettingsDlg(e){e&&e.stopPropagation();wxSelLat=null;wxSelLon=null;wxSelName=``;var si=document.getElementById(`wx-search-in`);if(si)si.value=``;var sug=document.getElementById(`wx-suggest`);if(sug){sug.innerHTML=``;sug.className=`wx-suggest`}var badge=document.getElementById(`wx-sel-badge`);if(badge)badge.style.display=`none`;var ki=document.getElementById(`wx-key-in`);if(ki)ki.value=``;wxHasStoredKey=!1;document.getElementById(`wx-save-btn`).disabled=!0;fetch(`/weatherstate`).then(function(r){return r.json()}).then(function(s){var st=document.getElementById(`wx-status`);if(s.valid&&st)st.textContent=`Ultima valoare: `+s.temp.toFixed(1)+`°  `+s.humidity+`%  `+s.desc;else if(st)st.textContent=s.hasKey?`Cheie configurata, se asteapta fetch…`:``;if(s.hasKey){wxHasStoredKey=!0;var ki2=document.getElementById(`wx-key-in`);if(ki2){ki2.value=`•`.repeat(s.keyLen||32);ki2.dataset.masked=`1`;ki2.type=`text`;ki2.style.webkitTextSecurity=`disc`;ki2.style.fontFamily=`monospace`;ki2.placeholder=``}document.getElementById(`wx-no-key-hint`).style.display=`none`}if(s.city){var si2=document.getElementById(`wx-search-in`);if(si2){si2.value=s.city;wxSelLat=s.lat||null;wxSelLon=s.lon||null;wxSelName=s.city}if(s.hasKey){document.getElementById(`wx-save-btn`).disabled=!1}}}).catch(function(){});[`wxTempUnitC`,`wxTempUnitF`].forEach(function(bid,bi){var b=document.getElementById(bid);b&&(b.className=`sb`+(bi===tempUnitV?` on`:``))});document.getElementById(`wx-sett-scrim`).classList.add(`open`);document.getElementById(`wx-sett-dlg`).classList.add(`open`)}function closeWxSettingsDlg(){document.getElementById(`wx-sett-scrim`).classList.remove(`open`);document.getElementById(`wx-sett-dlg`).classList.remove(`open`);var sug=document.getElementById(`wx-suggest`);if(sug){sug.innerHTML=``;sug.className=`wx-suggest`}setTimeout(function(){var body=document.querySelector(`#wx-sett-dlg .mdd-body`);var act=document.querySelector(`#wx-sett-dlg .mdd-actions`);var spin=document.getElementById(`wx-saving`);if(body){body.style.display=``;body.style.opacity=`1`}if(act){act.style.display=``;act.style.opacity=`1`}if(spin){spin.style.display=`none`;spin.style.opacity=`0`}},400)}function wxShowSpinner(on,cb){var body=document.querySelector(`#wx-sett-dlg .mdd-body`);var act=document.querySelector(`#wx-sett-dlg .mdd-actions`);var spin=document.getElementById(`wx-saving`);if(on){if(body){body.style.opacity=`0`}if(act){act.style.opacity=`0`}setTimeout(function(){if(body)body.style.display=`none`;if(act)act.style.display=`none`;if(spin){spin.style.opacity=`0`;spin.style.display=`flex`;requestAnimationFrame(function(){spin.style.opacity=`1`})}if(cb)cb()},220)}else{if(spin){spin.style.opacity=`0`}if(cb)setTimeout(cb,200)}}function saveWxSettings(){var key=document.getElementById(`wx-key-in`).value.trim();if(!wxSelLat&&!key){document.getElementById(`wx-status`).textContent=`Selecteaza o localitate si introdu cheia API`;return}wxShowSpinner(!0);var body=``;if(key)body+=`apikey=`+encodeURIComponent(key);if(wxSelLat!==null){if(body)body+=`&`;body+=`lat=`+wxSelLat+`&lon=`+wxSelLon+`&name=`+encodeURIComponent(wxSelName)}fetch(`/weathersett`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:body}).then(function(r){return r.text()}).then(function(){var loc=wxSelName||document.getElementById(`wx-search-in`).value.trim();if(loc){wxCityLabel=loc;document.querySelectorAll(`[id^=wxtxt-]`).forEach(function(el){el.textContent=loc})}setTimeout(function(){wxShowSpinner(!1,function(){closeWxSettingsDlg()})},1000)}).catch(function(){var body=document.querySelector(`#wx-sett-dlg .mdd-body`);var act=document.querySelector(`#wx-sett-dlg .mdd-actions`);var spin=document.getElementById(`wx-saving`);if(spin)spin.style.display=`none`;if(body){body.style.display=``;requestAnimationFrame(function(){body.style.opacity=`1`})}if(act){act.style.display=``;requestAnimationFrame(function(){act.style.opacity=`1`})}document.getElementById(`wx-status`).textContent=`Eroare la salvare`})}var hourFormatV=0;function openHourSettingsDlg(e){e&&e.stopPropagation();[`hourFmt24`,`hourFmt12`].forEach(function(b,i){var el=document.getElementById(b);el&&(el.className=`sb`+(i===hourFormatV?` on`:``))});var hIdx=items.findIndex(function(it){return it.id===0});if(hIdx>=0){var it=items[hIdx];var sl=document.getElementById(`hour-dur-slider`);if(sl){sl.value=it.dur;sl.disabled=!it.enabled}var vl=document.getElementById(`hour-dur-val`);if(vl)vl.textContent=it.dur+`s`}document.getElementById(`hour-sett-scrim`).classList.add(`open`);document.getElementById(`hour-sett-dlg`).classList.add(`open`)}function closeHourSettingsDlg(){document.getElementById(`hour-sett-scrim`).classList.remove(`open`);document.getElementById(`hour-sett-dlg`).classList.remove(`open`)}function setHourFormat(f){hourFormatV=f;[`hourFmt24`,`hourFmt12`].forEach(function(b,i){var el=document.getElementById(b);el&&(el.className=`sb`+(i===f?` on`:``))});refreshHourTilePreview();fetch(`/hourformat`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`fmt=`+f})}var dateFormatV=2;function openDateSettingsDlg(e){e&&e.stopPropagation();[`dateFmt0`,`dateFmt1`,`dateFmt2`,`dateFmt3`].forEach(function(b,i){var el=document.getElementById(b);el&&(el.className=`sb`+(i===dateFormatV?` on`:``))});var dIdx=items.findIndex(function(it){return it.id===1});if(dIdx>=0){var it=items[dIdx];var sl=document.getElementById(`date-dur-slider`);if(sl){sl.value=it.dur;sl.disabled=!it.enabled}var vl=document.getElementById(`date-dur-val`);if(vl)vl.textContent=it.dur+`s`}document.getElementById(`date-sett-scrim`).classList.add(`open`);document.getElementById(`date-sett-dlg`).classList.add(`open`)}function closeDateSettingsDlg(){document.getElementById(`date-sett-scrim`).classList.remove(`open`);document.getElementById(`date-sett-dlg`).classList.remove(`open`)}function setDateFormat(f){dateFormatV=f;[`dateFmt0`,`dateFmt1`,`dateFmt2`,`dateFmt3`].forEach(function(b,i){var el=document.getElementById(b);el&&(el.className=`sb`+(i===f?` on`:``))});refreshDateTilePreview();fetch(`/dateformat`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`fmt=`+f})}var memoText=``;function openMemoSettingsDlg(e){e&&e.stopPropagation();var inp=document.getElementById(`memo-text-in`);if(inp){inp.value=memoText;document.getElementById(`memo-char-count`).textContent=memoText.length+` / 120`}document.getElementById(`memo-sett-scrim`).classList.add(`open`);document.getElementById(`memo-sett-dlg`).classList.add(`open`);setTimeout(function(){if(inp)inp.focus()},220)}function closeMemoSettingsDlg(){document.getElementById(`memo-sett-scrim`).classList.remove(`open`);document.getElementById(`memo-sett-dlg`).classList.remove(`open`)}function onMemoInput(){var inp=document.getElementById(`memo-text-in`);var cnt=document.getElementById(`memo-char-count`);if(cnt)cnt.textContent=inp.value.length+` / 120`}function saveMemoSettings(){var inp=document.getElementById(`memo-text-in`);var txt=inp?inp.value.trim():``;memoText=txt;fetch(`/mementosett`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`text=`+encodeURIComponent(txt)}).then(function(){document.querySelectorAll(`[id^=memotxt-]`).forEach(function(el){el.textContent=txt||`Niciun text configurat`});closeMemoSettingsDlg()}).catch(function(){})}var canvasBitmap=new Array(32).fill(0),canvasBmpHex=``,cvxPainting=!1,cvxPaintVal=!1,cvxBound=!1;function hexToCanvasBitmap(hex){var arr=new Array(32).fill(0);if(hex&&hex.length===64){for(var i=0;i<32;i++){arr[i]=parseInt(hex.substr(i*2,2),16)||0}}return arr}function canvasBitmapToHexStr(){var s=``;for(var i=0;i<32;i++){var h=canvasBitmap[i].toString(16);if(h.length<2)h=`0`+h;s+=h}return s}function renderCanvasG)pl14"
R"pl15(rid(){var wrap=document.getElementById(`cvx-grid`);if(!wrap)return;wrap.innerHTML=``;for(var r=0;r<8;r++){for(var c=0;c<32;c++){var cell=document.createElement(`div`);cell.className=`cvx-px`+(((canvasBitmap[c]>>r)&1)?` on`:``);cell.dataset.r=r;cell.dataset.c=c;wrap.appendChild(cell)}}}function setCanvasCell(el,on){var r=parseInt(el.dataset.r),c=parseInt(el.dataset.c);if(on){canvasBitmap[c]|=(1<<r);el.classList.add(`on`)}else{canvasBitmap[c]&=~(1<<r);el.classList.remove(`on`)}}function canvasCellAtPoint(x,y){var el=document.elementFromPoint(x,y);return(el&&el.classList.contains(`cvx-px`))?el:null}function bindCanvasGridEvents(){if(cvxBound)return;cvxBound=!0;var wrap=document.getElementById(`cvx-grid`);if(!wrap)return;wrap.addEventListener(`pointerdown`,function(e){var el=canvasCellAtPoint(e.clientX,e.clientY);if(!el)return;cvxPainting=!0;cvxPaintVal=!el.classList.contains(`on`);setCanvasCell(el,cvxPaintVal);e.preventDefault()});wrap.addEventListener(`pointermove`,function(e){if(!cvxPainting)return;var el=canvasCellAtPoint(e.clientX,e.clientY);if(!el)return;setCanvasCell(el,cvxPaintVal)});window.addEventListener(`pointerup`,function(){cvxPainting=!1});wrap.addEventListener(`contextmenu`,function(e){e.preventDefault()})}function clearCanvasGrid(){for(var i=0;i<32;i++)canvasBitmap[i]=0;renderCanvasGrid()}function openCanvasSettingsDlg(e){e&&e.stopPropagation();canvasBitmap=hexToCanvasBitmap(canvasBmpHex);renderCanvasGrid();bindCanvasGridEvents();var idx=items.findIndex(function(it){return it.id===6});if(idx>=0){var it=items[idx];var sl=document.getElementById(`canvas-dur-slider`);if(sl){sl.value=it.dur;sl.disabled=!it.enabled}var vl=document.getElementById(`canvas-dur-val`);if(vl)vl.textContent=it.dur+`s`}document.getElementById(`canvas-sett-scrim`).classList.add(`open`);document.getElementById(`canvas-sett-dlg`).classList.add(`open`)}function closeCanvasSettingsDlg(){document.getElementById(`canvas-sett-scrim`).classList.remove(`open`);document.getElementById(`canvas-sett-dlg`).classList.remove(`open`)}function onCanvasDurInput(v){var idx=items.findIndex(function(it){return it.id===6});if(idx<0)return;items[idx].dur=parseInt(v);var vl=document.getElementById(`canvas-dur-val`);if(vl)vl.textContent=v+`s`}function saveCanvasSettings(){canvasBmpHex=canvasBitmapToHexStr();fetch(`/canvassett`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`bmp=`+canvasBmpHex}).then(function(){saveSettings();closeCanvasSettingsDlg()}).catch(function(){})}function openSwitchWifiDialog(){var m=document.getElementById(`switch-wifi-msg`);if(m){m.style.display=`none`;m.textContent=``}document.getElementById(`switch-wifi-scrim`).classList.add(`open`);document.getElementById(`switch-wifi-dlg`).classList.add(`open`)}function closeSwitchWifiDialog(){document.getElementById(`switch-wifi-scrim`).classList.remove(`open`);document.getElementById(`switch-wifi-dlg`).classList.remove(`open`)}function doSwitchToWifi(){var btn=document.getElementById(`switch-wifi-confirm-btn`);if(btn){btn.disabled=!0;btn.textContent=`…`}fetch(`/stopap`,{method:`POST`}).then(function(r){return r.json()}).then(function(d){if(btn){btn.disabled=!1;btn.textContent=`Confirma`}if(d&&d.ok){closeSwitchWifiDialog();isApMode=!1;showMsg(`Comutare la WiFi... Ceasul se reconecteaza.`,`ok`)}else{var m=document.getElementById(`switch-wifi-msg`);if(m){m.textContent=(d&&d.err)||`Eroare`;m.className=`msg err`;m.style.display=`block`}}}).catch(function(){if(btn){btn.disabled=!1;btn.textContent=`Confirma`}var m=document.getElementById(`switch-wifi-msg`);if(m){m.textContent=`Eroare conexiune`;m.className=`msg err`;m.style.display=`block`}})}function openDisconDialog(ssid){document.getElementById(`discon-scrim`).classList.add(`open`);document.getElementById(`discon-dlg`).classList.add(`open`)}function closeDisconDialog(){document.getElementById(`discon-scrim`).classList.remove(`open`);document.getElementById(`discon-dlg`).classList.remove(`open`)}function doDisconnect(){var btn=document.querySelector(`#discon-dlg button[onclick=\"doDisconnect()\"]`);if(btn)btn.disabled=!0;fetch(`/startap`,{method:`POST`}).then(function(){isApMode=!0;closeDisconDialog();var apSsid=apSsidCache||`AP`;showMsg(`Mod AP pornit! Conecteaza-te la reteaua `+apSsid+`.`,`ok`);document.getElementById(`w-ssid`).textContent=``;document.getElementById(`h-ssid`).textContent=`Mod AP`;document.getElementById(`h-ip`).textContent=`192.168.4.1`;document.getElementById(`home-sub`).textContent=`Mod AP — 192.168.4.1`;document.getElementById(`h-ap-sub`).textContent=`Activ — `+apSsid;setChips(apSsid,!0);if(btn)btn.disabled=!1}).catch(function(){if(btn)btn.disabled=!1;showMsg(`Eroare la deconectare`,`err`)})}function onApPassInput(){var pi=document.getElementById(`ap-pass-in`);if(pi.dataset.masked===`1`){pi.value=``;pi.dataset.masked=`0`;pi.type=`password`;pi.style.webkitTextSecurity=``;pi.style.fontFamily=``}}function toggleApPassVis(){var pi=document.getElementById(`ap-pass-in`);var icon=document.getElementById(`ap-pass-eye-icon`);var eyeOpen=`<path d=\"M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zM12 17c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z\"/>`;var eyeClosed=`<path d=\"M12 7c2.76 0 5 2.24 5 5 0 .65-.13 1.26-.36 1.83l2.92 2.92c1.51-1.26 2.7-2.89 3.43-4.75-1.73-4.39-6-7.5-11-7.5-1.4 0-2.74.25-3.98.7l2.16 2.16C10.74 7.13 11.35 7 12 7zM2 4.27l2.28 2.28.46.46C3.08 8.3 1.78 10.02 1 12c1.73 4.39 6 7.5 11 7.5 1.55 0 3.03-.3 4.38-.84l.42.42L19.73 22 21 20.73 3.27 3 2 4.27zM7.53 9.8l1.55 1.55c-.05.21-.08.43-.08.65 0 1.66 1.34 3 3 3 .22 0 .44-.03.65-.08l1.55 1.55c-.67.33-1.41.53-2.2.53-2.76 0-5-2.24-5-5 0-.79.2-1.53.53-2.2zm4.31-.78l3.15 3.15.02-.16c0-1.66-1.34-3-3-3l-.17.01z\"/>`;if(pi.dataset.masked===`1`){pi.disabled=!0;pi.style.opacity=`0.5`;fetch(`/appass`).then(function(r){if(!r.ok)throw new Error(r.status);return r.json()}).then(function(d){pi.value=d.pass||``;pi.dataset.masked=`0`;pi.type=`text`;pi.style.webkitTextSecurity=``;pi.style.fontFamily=``;pi.disabled=!1;pi.style.opacity=``;if(icon)icon.innerHTML=eyeClosed;pi.focus()}).catch(function(){pi.dataset.masked=`0`;pi.value=``;pi.type=`password`;pi.style.webkitTextSecurity=``;pi.style.fontFamily=``;pi.disabled=!1;pi.style.opacity=``;if(icon)icon.innerHTML=eyeOpen});return}var isPass=(pi.type===`password`);pi.type=isPass?`text`:`password`;pi.style.webkitTextSecurity=``;pi.style.fontFamily=``;if(icon)icon.innerHTML=isPass?eyeClosed:eyeOpen}function loadApSettings(){var badge=document.getElementById(`ap-mode-text`);var desc=document.getElementById(`ap-mode-desc`);var badgeWrap=document.getElementById(`ap-mode-badge`);fetch(`/apstate`).then(function(r){return r.json()}).then(function(d){var si=document.getElementById(`ap-ssid-in`);if(si)si.value=d.ssid||``;if(d.ssid)apSsidCache=d.ssid;var pi=document.getElementById(`ap-pass-in`);if(pi){if(d.hasPass){pi.value=`•`.repeat(d.passLen||8);pi.dataset.masked=`1`;pi.type=`text`;pi.style.webkitTextSecurity=`disc`;pi.style.fontFamily=`monospace`;pi.placeholder=``}else{pi.value=``;pi.dataset.masked=`0`;pi.type=`password`;pi.style.webkitTextSecurity=``;pi.style.fontFamily=``;pi.placeholder=`Parola retea (optional)`}}var isAp=(d.mode===`ap`);if(badge)badge.textContent=isAp?`AP Mode`:`WiFi Mode`;if(badgeWrap){badgeWrap.style.background=isAp?`var(--pri-con)`:`var(--surf-var)`;badgeWrap.style.color=isAp?`var(--pri)`:`var(--on-surf-var)`}if(desc)desc.textContent=isAp?`Ceasul emite propria retea WiFi. Conecteaza-te la ea pentru a-l accesa.`:`Ceasul este conectat la un router (mod WiFi). Setarile de mai jos se aplica data viitoare cand pornesti modul AP.`;var apSwBtn=document.getElementById(`ap-switch-btn`);if(apSwBtn)apSwBtn.style.display=isAp?`none`:``}).catch(function(){if(badge)badge.textContent=`Necunoscut`})}function saveApSettings(){var ssid=document.getElementById(`ap-ssid-in`).value.trim();var passEl=document.getElementById(`ap-pass-in`);var pass=(passEl.dataset.masked===`1`)?``:passEl.value;var btn=document.getElementById(`ap-save-btn`);var msg=document.getElementById(`ap-msg`);msg.style.display=`none`;if(!ssid){msg.textContent=`Introdu un SSID`;msg.className=`msg err`;msg.style.display=`block`;return}if(pass.length>0&&pass.length<8){msg.textContent=`Parola trebuie sa aiba minim 8 caractere`;msg.className=`msg err`;msg.style.display=`block`;return}btn.disabled=!0;btn.textContent=`…`;var body=`ssid=`+encodeURIComponent(ssid);if(pass.length>0)body+=`&pass=`+encodeURIComponent(pass);fetch(`/apsett`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:body}).then(function(r){return r.json()}).then(function(d){btn.disabled=!1;btn.textContent=`Salveaza`;if(d.ok){msg.textContent=`Setari AP salvate!`;msg.className=`msg ok`;msg.style.display=`block`;var hs=document.getElementById(`h-ap-sub`);if(hs)hs.textContent=isApMode?`Activ — `+ssid:`SSID: `+ssid;loadApSettings()}else{msg.textContent=(d&&d.err)||`Eroare`;msg.className=`msg err`;msg.style.display=`block`}}).catch(function(){btn.disabled=!1;btn.textContent=`Salveaza`;msg.textContent=`Eroare conexiune`;msg.className=`msg err`;msg.style.display=`block`})}function doLogout(){fetch(`/logout`,{method:`POST`}).then(function(){window.location.href=`/`}).catch(function(){window.location.href=`/`})}var WAVE_SVG=`<svg class="wave-hand" xmlns="http://www.w3.org/2000/svg" height="22px" viewBox="0 -960 960 960" width="22px" fill="currentColor"><path d="M880-759q0-51-35-86t-86-35v-60q75 0 128 53t53 128h-60ZM240-40q-83 0-141.5-58.5T40-240h60q0 58 41 99t99 41v60Zm162 0q-30 0-56-13.5T303-92L48-465l24-23q19-19 45-22t47 12l116 81v-383q0-17 11.5-28.5T320-840q17 0 28.5 11.5T360-800v537L212-367l157 229q5 8 14 13t19 5h278q33 0 56.5-23.5T760-200v-560q0-17 11.5-28.5T800-800q17 0 28.5 11.5T840-760v560q0 66-47 113T680-40H402Zm38-440v-400q0-17 11.5-28.5T480-920q17 0 28.5 11.5T520-880v400h-80Zm160 0v-360q0-17 11.5-28.5T640-880q17 0 28.5 11.5T680-840v360h-80ZM486-300Z"/></svg>`;var LOGOUT_SVG=`<svg viewBox="0 0 24 24" width="20" height="20" fill="currentColor"><path d="M17 7l-1.41 1.41L17.17 10H8v2h9.17l-1.58 1.59L17 15l4-4zM4 5h8V3H4c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h8v-2H4V5z"/></svg>`;function setupGreeting(){var wrap=document.getElementById(`greet-wrap`);var wave=document.getElementById(`greet-wave`);var lo=document.getElementById(`greet-logout`);if(!wrap||!wave||!lo)return;wrap.addEventListener(`mouseenter`,function(){wave.classList.add(`hidden-icon`);lo.classList.remove(`hidden-icon`)});wrap.addEventListener(`mouseleave`,function(){wave.classList.remove(`hidden-icon`);lo.classList.add(`hidden-icon`)});wrap.addEventListener(`click`,function(e){if(!lo.classList.contains(`hidden-icon`)){e.stopPropagation();doLogout()}})}function openUserAccountDlg(){var ui=document.getElementById(`ua-user-in`);fetch(`/whoami`).then(function(r){return r.json()}).then(function(d){if(ui)ui.value=d&&d.user?d.user:``}).catch(function(){});var np=document.getElementById(`ua-newpass-in`);if(np)np.value=``;var cp=document.getElementById(`ua-curpass-in`);if(cp)cp.value=``;var cpWrap=document.getElementById(`ua-curpass-wrap`);if(cpWrap)cpWrap.style.display=isApMode?`none`:`block`;var m=document.getElementById(`ua-msg`);if(m){m.style.display=`none`;m.textContent=``}document.getElementById(`useracct-scrim`).classList.add(`open`);document.getElementById(`useracct-dlg`).classList.add(`open`)}function closeUserAccountDlg(){document.getElementById(`useracct-scrim`).classList.remove(`open`);document.getElementById(`useracct-dlg`).classList.remove(`open`)}function showUaMsg(t,c){var m=document.getElementById(`ua-msg`);if(!m)return;m.textContent=t;m.className=`msg `+c;m.style.display=`block`}function saveUserAccount(){var u=document.getElementById(`ua-user-in`).value.trim();var np=document.getElementById(`ua-newpass-in`).value;var cp=isApMode?``:document.getElementById(`ua-curpass-in`).value;var btn=document.getElementById(`ua-save-btn`);btn.disabled=!0;btn.textContent=`…`;fetch(`/updateaccount`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`user=`+encodeURIComponent(u)+`&newpass=`+encodeURIComponent(np)+`&curpass=`+encodeURIComponent(cp)}).then(function(r){return r.json()}).then(function(d){btn.disabled=!1;btn.textContent=`Salveaza`;if(d.ok){showUaMsg(`Cont actualizat cu succes`,`ok`);setTimeout(function(){closeUserAccountDlg()},900)}else{showUaMsg((d&&d.err)||`Eroare`,`err`)}}).catch(function(){btn.disabled=!1;btn.textContent=`Salveaza`;showUaMsg(`Eroare conexiune`,`err`)})}bindDimScheduleInputs();window.onload=function(){fetch(`/whoami`).then(function(r){if(r.status===401){window.location.href=`/`;return}return r.json()}).then(function(d){if(!d)return;var t=document.getElementById(`home-title`);if(t){t.innerHTML=`<span class="greet-wrap" id="greet-wrap" title="Logout"><span class="greet-icon" id="greet-wave">`+WAVE_SVG+`</span><span class="greet-icon hidden-icon" id="greet-logout">`+LOGOUT_SVG+`</span></span> Welcome, `+d.user+`!`;setupGreeting()}});
// Top bar elevation on scroll
fetch(`/state`).then(function(r){return r.json()}).then(function(s){bOn=s.buzzer;document.getElementById(`btog-cb`).checked=bOn;document.getElementById(`h-bsub`).textContent=bOn?`Beep la schimbarea tile-ului`:`Silentios`;if(s.bright!==undefined){brightLevel=s.bright;document.getElementById(`h-bright-sub`).textContent=`Nivel `+brightLevel}if(s.buzzerVolume!==undefined){buzzerVolume=s.buzzerVolume}if(s.buzzerPreset){buzzerPreset=s.buzzerPreset}if(s.dimAuto!==undefined){dimAuto=s.dimAuto;dimFrom=s.dimFrom||dimFrom;dimTo=s.dimTo||dimTo;dimLevel=s.dimLevel!==undefined?s.dimLevel:dimLevel}var ssid=s.ssid||`—`,ip=s.ip||`—`;document.getElementById()pl15"
R"pl16(`h-ssid`).textContent=ssid,document.getElementById(`h-ip`).textContent=ip,document.getElementById(`w-ssid`).textContent=ssid,document.getElementById(`w-ip`).textContent=ip,document.getElementById(`home-sub`).textContent=ssid===`—`?`Neconectat`:s.ap?`Mod AP — `+ip:ssid+` — `+ip,isApMode=!!s.ap;setChips(ssid,!!s.ap);var _hap=document.getElementById(`h-ap-sub`);if(s.apSsid)apSsidCache=s.apSsid;if(_hap&&s.apSsid){_hap.textContent=s.ap?`Activ — `+s.apSsid:`SSID: `+s.apSsid};items=s.items;if(s.wxCity){wxCityLabel=s.wxCity+(s.wxHasKey?` — API OK`:`  (fara cheie API)`);}if(s.canvasBmp){canvasBmpHex=s.canvasBmp;}buildGrid();if(s.tempunit!==void 0){tempUnitV=s.tempunit;[`tempUnitC`,`tempUnitF`,`wxTempUnitC`,`wxTempUnitF`].forEach(function(bid,bi){var b=document.getElementById(bid);b&&(b.className=`sb`+(bi%2===tempUnitV?` on`:``))})}if(s.hourformat!==void 0){hourFormatV=s.hourformat;[`hourFmt24`,`hourFmt12`].forEach(function(bid,bi){var b=document.getElementById(bid);b&&(b.className=`sb`+(bi===hourFormatV?` on`:``))})};if(s.dateformat!==void 0){dateFormatV=s.dateformat;[`dateFmt0`,`dateFmt1`,`dateFmt2`,`dateFmt3`].forEach(function(bid,bi){var b=document.getElementById(bid);b&&(b.className=`sb`+(bi===dateFormatV?` on`:``))})};if(s.wxLang){owmLangCode=s.wxLang;var _ls1=document.getElementById(`owm-lang-sub`);if(_ls1)_ls1.textContent=owmLangLabel(s.wxLang);var _ls2=document.getElementById(`h-lang-sub`);if(_ls2)_ls2.textContent=owmLangLabel(s.wxLang)};if(s.hideIcons!==void 0){hideTileIcons=s.hideIcons;applyHideIcons()}if(s.hideIconDate!==void 0){indivHideIcons.date=s.hideIconDate;indivHideIcons.temp=s.hideIconTemp;indivHideIcons.reminder=s.hideIconReminder;indivHideIcons.weather=s.hideIconWeather;indivHideIcons.notif=s.hideIconNotif;indivHideIcons.nowplaying=s.hideIconNowPlaying;indivHideIcons.pressure=s.hideIconPressure;indivHideIcons.currency=s.hideIconCurrency!==void 0?s.hideIconCurrency:!1;refreshHideIconSwitches()}if(s.scrollType!==void 0){scrollTypeV=s.scrollType;applyScrollType()}if(s.scrollTypeDate!==void 0){indivScroll.date=s.scrollTypeDate;indivScroll.temp=s.scrollTypeTemp;indivScroll.reminder=s.scrollTypeReminder;indivScroll.weather=s.scrollTypeWeather;indivScroll.notif=s.scrollTypeNotif;indivScroll.nowplaying=s.scrollTypeNowPlaying;indivScroll.pressure=s.scrollTypePressure;indivScroll.stopwatch=s.scrollTypeStopwatch!==void 0?s.scrollTypeStopwatch:0;indivScroll.currency=s.scrollTypeCurrency!==void 0?s.scrollTypeCurrency:0;refreshScrollTypeSelects()}if(s.notifEnabled!==void 0){notifEn=s.notifEnabled;var nc=document.getElementById(`notif-cb`);if(nc)nc.checked=notifEn}if(s.pressureHpa!==void 0){pressureHpaV=s.pressureHpa;pressureTrendV=s.pressureTrend!==void 0?s.pressureTrend:0;refreshPressureTilePreview()}if(s.currencyBase!==void 0){currencyBaseV=s.currencyBase;currencyQuoteV=s.currencyQuote;currencyCompareV=!!s.currencyCompare;currencyValidV=!!s.currencyValid;currencyRateV=s.currencyRate!==void 0?s.currencyRate:0;currencyTrendV=s.currencyTrend!==void 0?s.currencyTrend:0;refreshCurrencyTilePreview()}if(s.ssAnim!==void 0){ssAnimV=s.ssAnim;refreshSsTilePreview()}
if(s.ets2Enabled!==void 0){ets2En=s.ets2Enabled;var ec=document.getElementById(`ets2-cb`);if(ec)ec.checked=ets2En}
if(s.priorityOrder){var _po=s.priorityOrder.split(`,`);priorityItems=_po.filter(function(id){return id!==`nowplaying`||!!s.nowPlayingIsPriority}).map(function(id){return{id:id,enabled:!0}})}else if(s.ets2OrderFirst!==void 0&&s.ets2OrderFirst){priorityItems=[{id:`ets2`,enabled:!0},{id:`notif`,enabled:!0},{id:`stopwatch`,enabled:!0}]}
if(s.mementoText!==void 0){memoText=s.mementoText}if(s.evSndTile)evSndTile=s.evSndTile;if(s.evSndWifi)evSndWifi=s.evSndWifi;if(s.evSndNotif)evSndNotif=s.evSndNotif;if(s.evSndEts2)evSndEts2=s.evSndEts2;if(s.evSndTouch)evSndTouch=s.evSndTouch;if(s.touchTapAction!==void 0)touchTapAction=s.touchTapAction;if(s.touchDoubleTapAction!==void 0)touchDoubleTapAction=s.touchDoubleTapAction;}).catch(function(){items=[{id:0,enabled:!0,dur:5},{id:1,enabled:!0,dur:4},{id:2,enabled:!0,dur:4},{id:3,enabled:!1,dur:8},{id:4,enabled:!1,dur:8},{id:5,enabled:!1,dur:8},{id:6,enabled:!1,dur:8}],bOn=!0,buildGrid()}),pollNP(),pollEts2(),pollSwState(),window.addEventListener(`scroll`,function(){document.querySelectorAll(`.top-bar`).forEach(function(b){b.classList.toggle(`raised`,window.scrollY>4)})},{passive:!0})};function toggleNotif(){notifEn=!notifEn;var nc=document.getElementById(`notif-cb`);if(nc)nc.checked=notifEn;fetch(`/notiftoggle`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`enabled=`+(notifEn?1:0)})}function toggleEts2(){ets2En=!ets2En;var ec=document.getElementById(`ets2-cb`);if(ec)ec.checked=ets2En;fetch(`/ets2toggle`,{method:`POST`,headers:{"Content-Type":`application/x-www-form-urlencoded`},body:`enabled=`+(ets2En?1:0)})}function pollEts2(){fetch(`/ets2state`).then(function(r){return r.json()}).then(function(s){var sub=document.getElementById(`ets2-sub`);if(!sub)return;if(!s.enabled){sub.textContent=`Dezactivat`}else if(s.active){sub.textContent=`Activ — `+s.speed+` km/h`}else{sub.textContent=`Inactiv`}}).catch(function(){}),setTimeout(pollEts2,3e3)}function setSwToggleBtnIcon(){var b=document.getElementById(`sw-toggle-btn`);if(!b)return;b.classList.toggle(`playing`,swRunning);b.innerHTML=swRunning?`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M6 19h4V5H6v14zm8-14v14h4V5h-4z"/></svg>`:`<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M8 5v14l11-7z"/></svg>`}function setSwSubText(){var sub=document.getElementById(`sw-sub`);if(!sub)return;sub.textContent=swRunning?`Ruleaza — `+swLastText:`Oprit`}function toggleStopwatch(){if(swRunning){fetch(`/swstop`,{method:`POST`}).then(function(r){return r.json()}).then(function(s){swRunning=!1;swLastText=s.text||swLastText;setSwToggleBtnIcon();setSwSubText();openSwResultDlg(s.text)}).catch(function(){swRunning=!1;setSwToggleBtnIcon();setSwSubText()})}else{fetch(`/swstart`,{method:`POST`}).then(function(){swRunning=!0;swElapsedMs=0;swLastText=`00:00:00:00`;setSwToggleBtnIcon();setSwSubText()})}}function pollSwState(){fetch(`/swstate`).then(function(r){return r.json()}).then(function(s){swRunning=!!s.running;swLastText=s.text||swLastText;setSwToggleBtnIcon();setSwSubText()}).catch(function(){}),setTimeout(pollSwState,swRunning?1e3:4e3)}function openSwResultDlg(txt){var el=document.getElementById(`sw-result-time`);if(el)el.textContent=txt;document.getElementById(`sw-result-scrim`).classList.add(`open`);document.getElementById(`sw-result-dlg`).classList.add(`open`)}function closeSwResultDlg(){document.getElementById(`sw-result-scrim`).classList.remove(`open`);document.getElementById(`sw-result-dlg`).classList.remove(`open`)}</script>)pl16"
R"pl17(<script>
// ─── TIMER TILE — integrare completa in Tile Manager ───────────────────────
// Adaugat aditiv, fara sa modifice codul minificat de mai sus. Timer e un
// Priority Tile fix (precum Stopwatch) — nu poate fi mutat in Circuit Tiles.
// Impacheteaza buildPriorityGrid()/saveSettings()/buildEventSoundCard()
// pastrandu-le comportamentul original neschimbat.
(function(){
  var TIMER_SVG = '<svg viewbox="0 0 24 24" fill=currentColor height=20 width=20><path d="M15 1H9v2h6V1zm-4 13h2V8h-2v6zm8.03-6.61l1.42-1.42c-.43-.51-.9-.99-1.41-1.41l-1.42 1.42A8.962 8.962 0 0012 4c-4.97 0-9 4.03-9 9s4.02 9 9 9a8.994 8.994 0 007.03-14.61zM12 20c-3.87 0-7-3.13-7-7s3.13-7 7-7 7 3.13 7 7-3.13 7-7 7z"/></svg>';
  var GEAR_SVG = '<svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor"><path d="M19.14 12.94c.04-.3.06-.61.06-.94s-.02-.64-.07-.94l2.03-1.58a.49.49 0 0 0 .12-.61l-1.92-3.32a.488.488 0 0 0-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54a.484.484 0 0 0-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96c-.22-.08-.47 0-.59.22L2.74 8.87a.49.49 0 0 0 .12.61l2.03 1.58c-.05.3-.07.62-.07.94s.02.64.07.94l-2.03 1.58a.49.49 0 0 0-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32a.49.49 0 0 0-.12-.61l-2.01-1.58zM12 15.6c-1.98 0-3.6-1.62-3.6-3.6s1.62-3.6 3.6-3.6 3.6 1.62 3.6 3.6-1.62 3.6-3.6 3.6z"/></svg>';
  var PLAY_SVG = '<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M8 5v14l11-7z"/></svg>';
  var PAUSE_SVG = '<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M6 19h4V5H6v14zm8-14v14h4V5h-4z"/></svg>';
  var STOP_SVG = '<svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor"><path d="M6 6h12v12H6z"/></svg>';

  // SCROLL_TILES/indivScroll.timer raman — folosite de pagina generica Scroll
  // Type (Timer poate avea totusi nevoie de scroll cand durata are ore).
  SCROLL_TILES.push({ key: 'timer', label: 'Timer', cls: 'lc-pur', svg: TIMER_SVG, hasIcon: false });
  indivScroll.timer = 0;

  // ── stare locala ──────────────────────────────────────────────────────
  var timerDurationSec = 300, timerPresetV = 0, timerRunningV = false, timerFinishedV = false, timerRemSecV = 300;
  var evSndTimer = 'calm';

  function fmtTimerSec(sec){
    sec = Math.max(0, sec|0);
    var h = Math.floor(sec/3600), m = Math.floor((sec%3600)/60), s = sec%60;
    function p(n){ return (n<10?'0':'')+n; }
    return h>0 ? (p(h)+':'+p(m)+':'+p(s)) : (p(m)+':'+p(s));
  }
  function timerPreviewText(){
    if (timerFinishedV) return 'Suna! Apasa pentru a opri';
    return timerRunningV ? ('Ruleaza — ' + fmtTimerSec(timerRemSecV)) : ('Oprit — ' + fmtTimerSec(timerDurationSec));
  }
  function updateAllTimerSubtitles(){
    document.querySelectorAll('[id^=timertxt-]').forEach(function(el){ el.textContent = timerPreviewText(); });
  }
  function refreshTimerPlayButtons(){
    document.querySelectorAll('.timer-toggle-btn').forEach(function(b){
      b.classList.toggle('playing', timerRunningV || timerFinishedV);
      b.innerHTML = timerFinishedV ? STOP_SVG : (timerRunningV ? PAUSE_SVG : PLAY_SVG);
      b.title = timerFinishedV ? 'Opreste alarma' : 'Play / Pause';
    });
  }

  // ── patch randul din Priority Tiles dupa randarea originala (buildPriorityGrid
  // nu stie nativ de tile-ul Timer, il randeaza gresit ca pe Notificari PC) ──
  function patchTimerPriorityRow(row, idx){
    var lic = row.querySelector('.lic');
    if (lic) { lic.className = 'lic lc-pur'; lic.innerHTML = TIMER_SVG; }
    var head = row.querySelector('.tile-head');
    if (head) head.textContent = 'Timer';
    var body = row.querySelector('.tile-body');
    var subEl = row.querySelector('.tile-sub');
    if (!subEl && body) { subEl = document.createElement('div'); subEl.className = 'tile-sub'; body.appendChild(subEl); }
    if (subEl) { subEl.id = 'timertxt-p' + idx; subEl.textContent = timerPreviewText(); }
    // scoate butoanele generice gresite (legate implicit de Notificari PC)
    if (body) {
      var n = body.nextSibling;
      while (n) { var nx = n.nextSibling; row.removeChild(n); n = nx; }
    }
    // Play stanga, Settings dreapta
    var playBtn = document.createElement('button');
    playBtn.className = 'np-gear-btn timer-toggle-btn' + ((timerRunningV || timerFinishedV) ? ' playing' : '');
    playBtn.title = timerFinishedV ? 'Opreste alarma' : 'Play / Pause';
    playBtn.innerHTML = timerFinishedV ? STOP_SVG : (timerRunningV ? PAUSE_SVG : PLAY_SVG);
    playBtn.onclick = function(e){ e.stopPropagation(); toggleTimerRun(); };
    var gearBtn = document.createElement('button');
    gearBtn.className = 'np-gear-btn'; gearBtn.title = 'Setari Timer'; gearBtn.innerHTML = GEAR_SVG;
    gearBtn.onclick = function(e){ e.stopPropagation(); openTimerSettingsDlg(); };
    row.appendChild(playBtn);
    row.appendChild(gearBtn);
  }

  var _origBuildPriorityGrid = buildPriorityGrid;
  buildPriorityGrid = function(){
    _origBuildPriorityGrid();
    var g = document.getElementById('pgrid');
    if (!g) return;
    priorityItems.forEach(function(item, idx){
      if (item.id !== 'timer') return;
      var row = g.children[idx];
      if (row) patchTimerPriorityRow(row, idx);
    });
  };

  // ── saveSettings(): override complet (identic cu originalul + scrollTypeTimer) ──
  saveSettings = function(){
    var p = 'buzzer=' + (bOn ? 1 : 0)
      + '&hideIcons=' + (hideTileIcons ? 1 : 0)
      + '&hideIconDate=' + (indivHideIcons.date ? 1 : 0)
      + '&hideIconTemp=' + (indivHideIcons.temp ? 1 : 0)
      + '&hideIconReminder=' + (indivHideIcons.reminder ? 1 : 0)
      + '&hideIconWeather=' + (indivHideIcons.weather ? 1 : 0)
      + '&hideIconNotif=' + (indivHideIcons.notif ? 1 : 0)
      + '&hideIconNowPlaying=' + (indivHideIcons.nowplaying ? 1 : 0)
      + '&hideIconPressure=' + (indivHideIcons.pressure ? 1 : 0)
      + '&scrollType=' + scrollTypeV
      + '&scrollTypeDate=' + indivScroll.date
      + '&scrollTypeTemp=' + indivScroll.temp
      + '&scrollTypeReminder=' + indivScroll.reminder
      + '&scrollTypeWeather=' + indivScroll.weather
      + '&scrollTypeNotif=' + indivScroll.notif
      + '&scrollTypeNowPlaying=' + indivScroll.nowplaying
      + '&scrollTypePressure=' + indivScroll.pressure
      + '&scrollTypeStopwatch=' + indivScroll.stopwatch
      + '&scrollTypeTimer=' + (indivScroll.timer || 0);
    items.forEach(function(it, i){
      p += '&id' + i + '=' + it.id + '&en' + i + '=' + (it.enabled ? 1 : 0) + '&dur' + i + '=' + it.dur;
    });
    fetch('/settings', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: p });
  };

  // ── Buzzer Settings -> Event Sound Assignment: randul Timer ──────────────
  var _origBuildEventSoundCard = buildEventSoundCard;
  buildEventSoundCard = function(){
    _origBuildEventSoundCard();
    var c = document.getElementById('event-sound-card');
    if (!c) return;
    var opts = '';
    EVENT_SOUND_OPTIONS.forEach(function(o){ opts += '<option value="' + o.id + '"' + (o.id === evSndTimer ? ' selected' : '') + '>' + o.name + '</option>'; });
    var row = document.createElement('div');
    row.className = 'li'; row.style.borderBottom = 'none';
    row.innerHTML = '<div class="lic lc-pur">' + TIMER_SVG + '</div><div class="li-body"><div class="li-head">Timer</div><div class="li-sub">Sunet redat cand countdown-ul ajunge la 0</div></div><div class="li-trail"><select class="evsnd-select" onchange="setTimerEventSound(this.value)" style="background:var(--surf-high);color:var(--on-surf);border:1px solid var(--outline-var);border-radius:8px;padding:6px 10px;font-family:Google Sans,sans-serif;font-size:13px;max-width:150px">' + opts + '</select></div>';
    c.appendChild(row);
  };
  window.setTimerEventSound = function(v){
    evSndTimer = v;
    fetch('/eventsoundsett', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'event=timer&preset=' + encodeURIComponent(v) });
  };

  // ── Play / Pause / Dismiss (un singur buton, 3 stari) ────────────────────
  window.toggleTimerRun = function(){
    if (timerFinishedV) {
      // alarma suna — apasarea o opreste (dismiss), reseteaza la durata configurata
      fetch('/timerpause', { method: 'POST' }).then(function(){ timerFinishedV = false; timerRunningV = false; refreshTimerPlayButtons(); pollTimerState(); });
    } else if (timerRunningV) {
      fetch('/timerpause', { method: 'POST' }).then(function(){ timerRunningV = false; refreshTimerPlayButtons(); pollTimerState(); });
    } else {
      fetch('/timerstart', { method: 'POST' }).then(function(){ timerRunningV = true; refreshTimerPlayButtons(); pollTimerState(); });
    }
  };
  function pollTimerState(){
    fetch('/timerstate').then(function(r){ return r.json(); }).then(function(s){
      timerRunningV = !!s.running;
      timerFinishedV = !!s.finished;
      timerRemSecV = s.remainingSec;
      timerDurationSec = s.durationSec;
      updateAllTimerSubtitles();
      refreshTimerPlayButtons();
      var pv = document.getElementById('timer-picker-preview');
      if (pv) pv.textContent = (timerRunningV || timerFinishedV) ? fmtTimerSec(timerRemSecV) : fmtTimerSec(timerDurationSec);
    }).catch(function(){});
    setTimeout(pollTimerState, (timerRunningV || timerFinishedV) ? 1000 : 4000);
  }

  // ── Dialog Setari Timer (creat dinamic — nu necesita markup HTML static) ──
  var PRESETS = [
    { id: 0, label: '5 Minutes', sec: 300 },
    { id: 1, label: '10 Minutes', sec: 600 },
    { id: 2, label: '25 Minutes', sec: 1500 },
    { id: 3, label: '1 Hour', sec: 3600 },
    { id: 4, label: 'Custom', sec: -1 }
  ];

  function ensureTimerDialog(){
    if (document.getElementById('timer-sett-dlg')) return;
    var scrim = document.createElement('div');
    scrim.className = 'md-scrim'; scrim.id = 'timer-sett-scrim';
    scrim.onclick = closeTimerSettingsDlg;
    var dlg = document.createElement('div');
    dlg.className = 'md-dialog'; dlg.id = 'timer-sett-dlg';
    dlg.innerHTML =
      '<div class="mdd-head"><div class="mdd-icon">' + TIMER_SVG + '</div><div class="mdd-title">Timer</div></div>' +
      '<div class="mdd-body">' +
        '<div id="timer-picker-preview" style="text-align:center;font-family:Google Sans,sans-serif;font-size:32px;color:var(--pri);padding:8px 0 16px;letter-spacing:1px">05:00</div>' +
        '<div id="timer-running-hint" style="display:none;color:var(--on-surf-var);font-size:12px;text-align:center;padding-bottom:12px">Opreste countdown-ul pentru a schimba durata</div>' +
        '<div style="color:var(--on-surf-var);font-size:12px;letter-spacing:1.5px;text-transform:uppercase;padding:4px 0 8px;font-weight:500">Preseturi</div>' +
        '<div id="timer-preset-list" style="display:flex;flex-direction:column;gap:4px"></div>' +
        '<div id="timer-custom-wrap" style="display:none;gap:8px;padding-top:12px;justify-content:center">' +
          '<div class="mdd-tf-wrap" style="flex:1;margin-bottom:0"><label class="mdd-label">Ore</label><input class="mdd-input" type="number" min="0" max="99" id="timer-h-in" value="0" oninput="onTimerCustomInput()"></div>' +
          '<div class="mdd-tf-wrap" style="flex:1;margin-bottom:0"><label class="mdd-label">Minute</label><input class="mdd-input" type="number" min="0" max="59" id="timer-m-in" value="5" oninput="onTimerCustomInput()"></div>' +
          '<div class="mdd-tf-wrap" style="flex:1;margin-bottom:0"><label class="mdd-label">Secunde</label><input class="mdd-input" type="number" min="0" max="59" id="timer-s-in" value="0" oninput="onTimerCustomInput()"></div>' +
        '</div>' +
      '</div>' +
      '<div class="mdd-actions"><button class="mbtn mbtn-fill" onclick="closeTimerSettingsDlg()">Gata</button></div>';
    document.body.appendChild(scrim);
    document.body.appendChild(dlg);
  }

  function buildTimerPresetList(){
    var c = document.getElementById('timer-preset-list');
    if (!c) return;
    c.innerHTML = '';
    PRESETS.forEach(function(p){
      var isSel = (p.id === timerPresetV);
      var d = document.createElement('div');
      d.className = 'li'; d.style.cursor = 'pointer'; d.style.minHeight = '52px';
      d.innerHTML = '<div class="li-body"><div class="li-head" style="' + (isSel ? 'color:var(--pri)' : '') + '">' + p.label + '</div></div><div class="li-trail">' + (isSel ? '<svg width="20" height="20" viewBox="0 0 24 24" fill="var(--pri)"><path d="M9 16.17L4.83 12l-1.42 1.41L9 19 21 7l-1.41-1.41z"/></svg>' : '') + '</div>';
      d.onclick = function(){ selectTimerPreset(p.id); };
      c.appendChild(d);
    });
  }

  window.selectTimerPreset = function(id){
    timerPresetV = id;
    buildTimerPresetList();
    var customWrap = document.getElementById('timer-custom-wrap');
    var preset = PRESETS[id];
    if (id === 4) {
      if (customWrap) customWrap.style.display = 'flex';
      var h = parseInt(document.getElementById('timer-h-in').value, 10) || 0;
      var m = parseInt(document.getElementById('timer-m-in').value, 10) || 0;
      var s = parseInt(document.getElementById('timer-s-in').value, 10) || 0;
      timerDurationSec = h * 3600 + m * 60 + s;
    } else {
      if (customWrap) customWrap.style.display = 'none';
      timerDurationSec = preset.sec;
    }
    var pv = document.getElementById('timer-picker-preview');
    if (pv) pv.textContent = fmtTimerSec(timerDurationSec);
    saveTimerSett();
  };

  window.onTimerCustomInput = function(){
    var h = parseInt(document.getElementById('timer-h-in').value, 10) || 0;
    var m = Math.min(59, parseInt(document.getElementById('timer-m-in').value, 10) || 0);
    var s = Math.min(59, parseInt(document.getElementById('timer-s-in').value, 10) || 0);
    timerDurationSec = h * 3600 + m * 60 + s;
    var pv = document.getElementById('timer-picker-preview');
    if (pv) pv.textContent = fmtTimerSec(timerDurationSec);
    saveTimerSett();
  };

  function saveTimerSett(){
    if (timerRunningV || timerFinishedV) return; // backend respinge oricum schimbarea duratei cat timp ruleaza/suna
    fetch('/timersett', {
      method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'durationSec=' + timerDurationSec + '&preset=' + timerPresetV
    }).then(function(){ updateAllTimerSubtitles(); });
  }

  window.openTimerSettingsDlg = function(e){
    if (e && e.stopPropagation) e.stopPropagation();
    ensureTimerDialog();
    buildTimerPresetList();
    var customWrap = document.getElementById('timer-custom-wrap');
    var locked = timerRunningV || timerFinishedV;
    if (timerPresetV === 4) {
      customWrap.style.display = 'flex';
      var rem = timerDurationSec;
      document.getElementById('timer-h-in').value = Math.floor(rem / 3600);
      document.getElementById('timer-m-in').value = Math.floor((rem % 3600) / 60);
      document.getElementById('timer-s-in').value = rem % 60;
    } else {
      customWrap.style.display = 'none';
    }
    document.getElementById('timer-picker-preview').textContent = locked ? fmtTimerSec(timerRemSecV) : fmtTimerSec(timerDurationSec);
    var hint = document.getElementById('timer-running-hint');
    var list = document.getElementById('timer-preset-list');
    if (hint) hint.style.display = locked ? 'block' : 'none';
    if (list) list.style.opacity = locked ? '.4' : '1';
    if (list) list.style.pointerEvents = locked ? 'none' : 'auto';
    if (customWrap) customWrap.style.pointerEvents = locked ? 'none' : 'auto';
    if (customWrap) customWrap.style.opacity = locked ? '.4' : '1';
    document.getElementById('timer-sett-scrim').classList.add('open');
    document.getElementById('timer-sett-dlg').classList.add('open');
  };
  window.closeTimerSettingsDlg = function(){
    var scrim = document.getElementById('timer-sett-scrim'), dlg = document.getElementById('timer-sett-dlg');
    if (scrim) scrim.classList.remove('open');
    if (dlg) dlg.classList.remove('open');
  };

  // ── init: preia campurile specifice Timer din /state, apoi reface randarea ──
  // (Timer e mereu prezent in priorityItems — vine deja asa din constructia
  // originala pe baza priorityOrder, la fel ca Notif/ETS2/Stopwatch.)
  function initTimerFeature(){
    fetch('/state').then(function(r){ return r.json(); }).then(function(s){
      if (s.timerDurationSec !== undefined) timerDurationSec = s.timerDurationSec;
      if (s.timerPreset !== undefined) timerPresetV = s.timerPreset;
      if (s.scrollTypeTimer !== undefined) indivScroll.timer = s.scrollTypeTimer;
      if (s.evSndTimer) evSndTimer = s.evSndTimer;
      buildPriorityGrid();
      pollTimerState();
    }).catch(function(){});
  }
  if (document.readyState === 'complete') initTimerFeature();
  else window.addEventListener('load', initTimerFeature);
})();
</script>)pl17";
// SERVER HANDLERS
// AUTH HANDLERS

void handleAuthState() {
  server.send(200, "application/json",
    String("{\"configured\":") + (authConfigured ? "true" : "false") + "}");
}

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

  char newTok[33];
  genToken(newTok);
  addSession(newTok);
  server.sendHeader("Set-Cookie", String("sc_tok=") + newTok + "; Path=/; HttpOnly");
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

  String existingTok = extractCookieToken();
  if (existingTok.length() > 0 && isValidSession(existingTok.c_str())) {

    server.sendHeader("Set-Cookie", String("sc_tok=") + existingTok + "; Path=/; HttpOnly");
    server.send(200, "application/json", String("{\"ok\":true,\"user\":\"") + u + "\"}");
    return;
  }

  char newTok[33];
  genToken(newTok);
  addSession(newTok);
  server.sendHeader("Set-Cookie", String("sc_tok=") + newTok + "; Path=/; HttpOnly");
  server.send(200, "application/json", String("{\"ok\":true,\"user\":\"") + u + "\"}");
}

void handleLogout() {
  String tok = extractCookieToken();
  if (tok.length() > 0) removeSession(tok.c_str());
  server.sendHeader("Set-Cookie", "sc_tok=; Path=/; HttpOnly; Max-Age=0");
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
  server.setContentLength(strlen_P(PORTAL_HTML));
  server.send(200, "text/html", "");
  server.sendContent_P(PORTAL_HTML);
}

static const char AUTH_SHELL[] PROGMEM = R"rawliteral(<!doctype html><html lang=ro><meta charset=UTF-8><meta content="width=device-width,initial-scale=1,viewport-fit=cover" name=viewport><title>Octoglow</title><link href="https://fonts.googleapis.com/css2?family=Google+Sans:wght@400;500&family=Roboto:wght@400;500&display=swap" rel=stylesheet><style>:root{--pri:#d0bcff;--on-pri:#381e72;--pri-con:#4f378b;--on-pri-con:#eaddff;--sec:#ccc2dc;--sec-con:#4a4458;--on-sec-con:#e8def8;--bg:#1c1b1f;--on-bg:#e6e1e5;--surf:#1c1b1f;--on-surf:#e6e1e5;--surf-var:#49454f;--on-surf-var:#cac4d0;--surf-high:#2b2930;--outline:#938f99;--err:#f2b8b5;--err-con:#8c1d18;--grn:#6dd7a1;--grn-con:#003824}*{box-sizing:border-box;margin:0;padding:0}body{background:var(--bg);color:var(--on-bg);font-family:Roboto,sans-serif;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:24px}.auth-card{background:var(--surf-high);border-radius:28px;padding:40px 28px 32px;width:100%;max-width:360px;display:flex;flex-direction:column;align-items:center;gap:0}.auth-logo{width:72px;height:72px;border-radius:50%;background:var(--pri-con);color:var(--pri);display:flex;align-items:center;justify-content:center;margin-bottom:20px}.auth-title{font-family:Google Sans,sans-serif;font-size:24px;color:var(--on-surf);font-weight:500;margin-bottom:4px;text-align:center}.auth-sub{font-size:13px;color:var(--on-surf-var);margin-bottom:28px;text-align:center}.tf{background:var(--surf-var);border-bottom:2px solid var(--outline);border-radius:4px 4px 0 0;padding:0 16px;margin-bottom:12px;width:100%}.tf:focus-within{border-bottom-color:var(--pri)}.tf label{display:block;font-size:11px;letter-spacing:.4px;font-weight:500;color:var(--on-surf-var);padding-top:8px}.tf:focus-within label{color:var(--pri)}.tf input{width:100%;background:0 0;border:none;outline:none;color:var(--on-surf);font-family:Roboto,sans-serif;font-size:16px;padding:6px 0 10px}.mbtn{cursor:pointer;border:none;border-radius:100px;width:100%;height:44px;font-family:Google Sans,sans-serif;font-size:15px;font-weight:500;letter-spacing:.1px;background:var(--pri);color:var(--on-pri);margin-top:8px;transition:opacity .15s,transform .1s}.mbtn:active{transform:scale(.97)}.mbtn:disabled{opacity:.38;pointer-events:none}.btn-spin{width:18px;height:18px;border:2.5px solid color-mix(in srgb,var(--on-pri) 30%,transparent);border-top-color:var(--on-pri);border-radius:50%;display:inline-block;animation:btn-spin-rot .7s linear infinite}@keyframes btn-spin-rot{to{transform:rotate(360deg)}}.err-msg{background:var(--err-con);color:var(--err);border-radius:8px;padding:10px 14px;font-size:13px;margin-top:10px;width:100%;display:none}@keyframes fade-in{from{opacity:0;transform:translateY(8px)}to{opacity:1;transform:none}}.auth-card{animation:.3s cubic-bezier(.2,0,0,1) fade-in}</style><body><div class=auth-card id=auth-card><div class=auth-logo><svg viewBox="0 0 24 24" fill=currentColor width=36 height=36><path d="M12 2C6.5 2 2 6.5 2 12s4.5 10 10 10 10-4.5 10-10S17.5 2 12 2zm0 3c1.66 0 3 1.34 3 3s-1.34 3-3 3-3-1.34-3-3 1.34-3 3-3zm0 14.2c-2.5 0-4.71-1.28-6-3.22.03-1.99 4-3.08 6-3.08 1.99 0 5.97 1.09 6 3.08-1.29 1.94-3.5 3.22-6 3.22z"/></svg></div><div class=auth-title id=auth-title>Octoglow</div><div class=auth-sub id=auth-sub>Se verifica...</div><div id=auth-form style="width:100%;display:none"><div class=tf id=tf-user><label>Utilizator</label><input id=inp-user type=text autocomplete=username placeholder="ex: admin" maxlength=32></div><div class=tf><label>Parola</label><input id=inp-pass type=password autocomplete=current-password placeholder="Parola" maxlength=64></div><button class=mbtn id=auth-btn onclick=doAuth()>Continua</button><div class=err-msg id=err-msg></div></div></div><script>var isSetup=false;function init(){fetch('/authstate').then(function(r){return r.json()}).then(function(d){isSetup=!d.configured;var sub=document.getElementById('auth-sub');var form=document.getElementById('auth-form');var title=document.getElementById('auth-title');if(isSetup){title.textContent='Bun venit!';sub.textContent='Creeaza un cont pentru a proteja dashboard-ul.';document.getElementById('tf-user').style.display='';document.getElementById('auth-btn').textContent='Creeaza cont'}else{title.textContent='Octoglow';sub.textContent='Autentifica-te pentru a continua.';document.getElementById('tf-user').style.display='';document.getElementById('auth-btn').textContent='Autentificare'}form.style.display='';document.getElementById('inp-user').focus()}).catch(function(){document.getElementById('auth-sub').textContent='Eroare conexiune. Reincearca.'})}function doAuth(){var u=document.getElementById('inp-user').value.trim();var p=document.getElementById('inp-pass').value;var btn=document.getElementById('auth-btn');var err=document.getElementById('err-msg');if(isSetup&&u.length<1){showErr('Introdu un username.');return}if(p.length<4){showErr('Parola trebuie sa aiba cel putin 4 caractere.');return}var origLabel=btn.textContent;btn.disabled=true;btn.innerHTML='<span class="btn-spin"></span>';var url=isSetup?'/authsetup':'/login';var body='user='+encodeURIComponent(u)+'&pass='+encodeURIComponent(p);fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body}).then(function(r){return r.json()}).then(function(d){if(d.ok){window.location.href='/dashboard'}else{showErr(d.err||'Eroare');btn.disabled=false;btn.innerHTML=origLabel}}).catch(function(){showErr('Eroare conexiune.');btn.disabled=false;btn.innerHTML=origLabel})}function showErr(msg){var e=document.getElementById('err-msg');e.textContent=msg;e.style.display='block'}var authUser='';init()</script>)rawliteral";

void handleRoot() {
  if (provisionMode) {
    server.setContentLength(strlen_P(PORTAL_HTML));
    server.send(200, "text/html", "");
    server.sendContent_P(PORTAL_HTML);
    return;
  }
  server.setContentLength(strlen_P(AUTH_SHELL));
  server.send(200, "text/html", "");
  server.sendContent_P(AUTH_SHELL);
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
  bool scrollTypeTouched = hideIconTouched;
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
  if (scrollTypeTouched) {

    if (notifActive) {
      notifBuildBuffer();
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
    }
  }

  int count = 0;
  CycleItem newItems[NUM_ITEMS];
  for (int i = 0; i < NUM_ITEMS; i++) {
    String idKey  = "id"  + String(i);
    String enKey  = "en"  + String(i);
    String durKey = "dur" + String(i);
    if (!server.hasArg(idKey)) break;
    newItems[i].id          = (uint8_t)server.arg(idKey).toInt();
    newItems[i].enabled     = (server.arg(enKey) == "1");
    newItems[i].durationSec = (uint16_t)server.arg(durKey).toInt();
    newItems[i].order       = i;
    count++;
  }
  for (int i = 0; i < count; i++) items[i] = newItems[i];

  repairItemIds();
  saveSettings();
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
  saveSettings();
  server.send(200, "text/plain", "OK");
}

void handleBuzzerSett() {
  if (!checkAuth()) return;
  if (server.hasArg("volume")) {
    buzzerVolume = (uint8_t)constrain(server.arg("volume").toInt(), 0, 100);
  }
  if (server.hasArg("preset")) {
    String p = server.arg("preset");
    p.toCharArray(buzzerPreset, sizeof(buzzerPreset));
  }
  saveSettings();
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
  server.send(200, "text/plain", "OK");
}

void handleTouchSett() {
  if (!checkAuth()) return;
  if (server.hasArg("tap")) {
    touchTapAction = (uint8_t)constrain(server.arg("tap").toInt(), 0, 7);
  }
  if (server.hasArg("dbl")) {
    touchDoubleTapAction = (uint8_t)constrain(server.arg("dbl").toInt(), 0, 7);
  }
  saveSettings();
  server.send(200, "text/plain", "OK");
}

void handleState() {
  if (!checkAuth()) return;
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  prefs.end();

  String localIP = provisionMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();

  String json = "{\"buzzer\":" + String(buzzerOn ? "true" : "false") +
                ",\"buzzerVolume\":" + String(buzzerVolume) +
                ",\"buzzerPreset\":\"" + String(buzzerPreset) + "\"" +
                ",\"ssid\":\"" + ssid + "\"" +
                ",\"ip\":\"" + localIP + "\"" +
                ",\"ap\":" + String(provisionMode ? "true" : "false") +
                ",\"apSsid\":\"" + getApSsid() + "\"" +
                ",\"version\":\"" + FW_VERSION + "\"" +
                ",\"uptime\":" + String(millis() / 1000) +
                ",\"items\":[";
  for (int i = 0; i < NUM_ITEMS; i++) {
    if (i > 0) json += ",";
    json += "{\"id\":" + String(items[i].id) +
            ",\"enabled\":" + (items[i].enabled ? "true" : "false") +
            ",\"dur\":" + String(items[i].durationSec) + "}";
  }
  json += "]";
  char tFrom[6], tTo[6];
  snprintf(tFrom, 6, "%02d:%02d", dimFromH, dimFromM);
  snprintf(tTo,   6, "%02d:%02d", dimToH,   dimToM);
  json += ",\"bright\":" + String(curBrightness) +
          ",\"dimAuto\":" + String(dimAutoOn ? "true" : "false") +
          ",\"dimFrom\":\"" + String(tFrom) + "\"" +
          ",\"dimTo\":\"" + String(tTo) + "\"" +
          ",\"dimLevel\":" + String(dimLevel) +
          ",\"tempunit\":" + String(tempUnit) +
          ",\"hourformat\":" + String(hourFormat) +
          ",\"dateformat\":" + String(dateFormat) +
          ",\"wxCity\":\"" + String(weatherCity) + "\"" +
          ",\"wxLang\":\"" + String(weatherLang) + "\"" +
          ",\"wxHasKey\":" + String(strlen(weatherApiKey) > 0 ? "true" : "false") +
          ",\"currencyBase\":\"" + String(currencyBase) + "\"" +
          ",\"currencyQuote\":\"" + String(currencyQuote) + "\"" +
          ",\"currencyCompare\":" + String(currencyCompareEnabled ? "true" : "false") +
          ",\"notifEnabled\":" + String(notifEnabled ? "true" : "false") +
          ",\"ets2Enabled\":" + String(ets2Enabled ? "true" : "false") +
          ",\"ets2OrderFirst\":" + String(ets2OrderFirst ? "true" : "false") +
          ",\"nowPlayingIsPriority\":" + String(nowPlayingIsPriority ? "true" : "false") +
          ",\"priorityOrder\":\"" + priorityOrderToString() + "\"" +
          ",\"hideIcons\":" + String(hideTileIcons ? "true" : "false") +
          ",\"hideIconDate\":" + String(hideIconDate ? "true" : "false") +
          ",\"hideIconTemp\":" + String(hideIconTemp ? "true" : "false") +
          ",\"hideIconReminder\":" + String(hideIconReminder ? "true" : "false") +
          ",\"hideIconWeather\":" + String(hideIconWeather ? "true" : "false") +
          ",\"hideIconNotif\":" + String(hideIconNotif ? "true" : "false") +
          ",\"hideIconNowPlaying\":" + String(hideIconNowPlaying ? "true" : "false") +
          ",\"hideIconPressure\":" + String(hideIconPressure ? "true" : "false") +
          ",\"hideIconCurrency\":" + String(hideIconCurrency ? "true" : "false") +
          ",\"scrollType\":" + String(scrollType) +
          ",\"scrollTypeDate\":" + String(scrollTypeDate) +
          ",\"scrollTypeTemp\":" + String(scrollTypeTemp) +
          ",\"scrollTypeReminder\":" + String(scrollTypeReminder) +
          ",\"scrollTypeWeather\":" + String(scrollTypeWeather) +
          ",\"scrollTypeNotif\":" + String(scrollTypeNotif) +
          ",\"scrollTypeNowPlaying\":" + String(scrollTypeNowPlaying) +
          ",\"scrollTypePressure\":" + String(scrollTypePressure) +
          ",\"scrollTypeStopwatch\":" + String(scrollTypeStopwatch) +
          ",\"scrollTypeCurrency\":" + String(scrollTypeCurrency) +
          ",\"scrollTypeTimer\":" + String(scrollTypeTimer) +
          ",\"timerDurationSec\":" + String(timerDurationSec) +
          ",\"timerPreset\":" + String(timerPreset) +
          ",\"pressureHpa\":" + String((int)round(lastPressureHpa)) +
          ",\"pressureTrend\":" + String(pressureTrend) +
          ",\"currencyValid\":" + String(currencyValid ? "true" : "false") +
          ",\"currencyRate\":" + String(currencyRateNow, 4) +
          ",\"currencyTrend\":" + String(currencyTrend) +
          ",\"mementoText\":\"" + String(mementoBuf) + "\"" +
          ",\"canvasBmp\":\"" + canvasBitmapToHex() + "\"" +
          ",\"evSndTile\":\""  + String(eventSoundTile)  + "\"" +
          ",\"evSndWifi\":\""  + String(eventSoundWifi)  + "\"" +
          ",\"evSndNotif\":\"" + String(eventSoundNotif) + "\"" +
          ",\"evSndEts2\":\""  + String(eventSoundEts2)  + "\"" +
          ",\"evSndTouch\":\"" + String(eventSoundTouch) + "\"" +
          ",\"evSndTimer\":\"" + String(eventSoundTimer) + "\"" +
          ",\"touchTapAction\":" + String(touchTapAction) +
          ",\"touchDoubleTapAction\":" + String(touchDoubleTapAction) +
          ",\"ssAnim\":" + String(ssAnimSelected) + "}";
  server.send(200, "application/json", json);
}

// MEMENTO HANDLER
void handleMementoSett() {
  if (!checkAuth()) return;
  if (server.hasArg("text")) {
    String txt = server.arg("text");
    txt.trim();

    if (txt.length() > 120) txt = txt.substring(0, 120);
    txt.toCharArray(mementoBuf, sizeof(mementoBuf));
    saveSettings();

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
  if (server.hasArg("apikey") && server.arg("apikey").length() > 0) {
    server.arg("apikey").toCharArray(weatherApiKey, sizeof(weatherApiKey));
    changed = true;
  }
  if (server.hasArg("lat") && server.hasArg("lon") && server.hasArg("name")) {
    weatherLat = server.arg("lat").toFloat();
    weatherLon = server.arg("lon").toFloat();
    server.arg("name").toCharArray(weatherCity, sizeof(weatherCity));
    changed = true;
  }
  if (changed) {
    saveSettings();
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
      saveSettings();
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
    saveSettings();
    lastCurrencyFetch = 0;
    currencyValid = false;

    if (items[currentSlot].id == ITEM_CURRENCY) currencyInit();
  }
  server.send(200, "text/plain", "OK");
}

void handleCurrencyState() {
  if (!checkAuth()) return;
  String json = "{\"base\":\"" + String(currencyBase) + "\"" +
                ",\"quote\":\"" + String(currencyQuote) + "\"" +
                ",\"compare\":" + String(currencyCompareEnabled ? "true" : "false") +
                ",\"valid\":" + String(currencyValid ? "true" : "false") +
                ",\"rate\":" + String(currencyRateNow, 4) +
                ",\"trend\":" + String(currencyTrend) + "}";
  server.send(200, "application/json", json);
}

void handleNowPlaying() {
  if (!checkAuth()) return;
  bool updated = false;

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
                ",\"text\":\"" + String(nowPlayingBuf) + "\"" +
                ",\"npmode\":" + String(npDisplayMode) +
                ",\"hourformat\":" + String(hourFormat) +
                ",\"dateformat\":" + String(dateFormat) + "}";
  server.send(200, "application/json", json);
}

// SCREEN SAVER HANDLER
void handleScreensaverSett() {
  if (!checkAuth()) return;
  if (server.hasArg("anim")) {
    uint8_t a = (uint8_t)server.arg("anim").toInt();
    if (a <= SS_ANIM_COUNT) {
      ssAnimSelected = a;
      saveSettings();

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
      saveSettings();

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
    notifInit();
    playPresetTone(eventSoundNotif);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste text");
  }
}

void handleNotifToggle() {
  if (!checkAuth()) return;
  if (server.hasArg("enabled")) {
    notifEnabled = (server.arg("enabled") == "1");
    if (!notifEnabled && notifActive) {

      notifActive = false;
      notifBuf[0] = '\0';
      CycleItem& cur = items[currentSlot];
      if (cur.id == ITEM_NOW_PLAYING) npInit();
      else if (cur.id == ITEM_WEATHER) weatherInit();
      else if (cur.id == ITEM_CURRENCY) currencyInit();
      else { gLastStaticDrawMs = 0; gStaticDrawDone = false; }
    }
    saveSettings();
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
      ets2Init();
    }

    if (spd > ETS2_SPEED_LIMIT) {
      if (!ets2SpeedAlertFired) {
        ets2SpeedAlertFired = true;
        playPresetTone(eventSoundEts2);
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
      CycleItem& cur = items[currentSlot];
      if (cur.id == ITEM_NOW_PLAYING) npInit();
      else if (cur.id == ITEM_WEATHER) weatherInit();
      else if (cur.id == ITEM_CURRENCY) currencyInit();
      else { gLastStaticDrawMs = 0; gStaticDrawDone = false; }
    }
    saveSettings();
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
    saveSettings();
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
  saveSettings();
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
      startIdx = i + 1;
    }
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
  if (count != NUM_PRIORITY_IDS) {
    server.send(400, "text/plain", "Ordine invalida");
    return;
  }
  for (int i = 0; i < NUM_PRIORITY_IDS; i++) priorityOrder[i] = newOrder[i];
  nowPlayingIsPriority = sawNowPlaying;
  if (!nowPlayingIsPriority) npPriorityWasActive = false;

  int notifRank = -1, ets2Rank = -1;
  for (int i = 0; i < NUM_PRIORITY_IDS; i++) {
    if (priorityOrder[i] == PRIORITY_ID_NOTIF) notifRank = i;
    if (priorityOrder[i] == PRIORITY_ID_ETS2)  ets2Rank  = i;
  }
  ets2OrderFirst = (ets2Rank < notifRank);
  saveSettings();
  server.send(200, "text/plain", "OK");
}

void handleHourFormat() {
  if (!checkAuth()) return;
  if (server.hasArg("fmt")) {
    uint8_t f = (uint8_t)server.arg("fmt").toInt();
    if (f <= 1) {
      hourFormat = f;
      saveSettings();
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste fmt");
  }
}

void handleDateFormat() {
  if (!checkAuth()) return;
  if (server.hasArg("fmt")) {
    uint8_t f = (uint8_t)server.arg("fmt").toInt();
    if (f <= 3) {
      dateFormat = f;
      saveSettings();

      if (items[currentSlot].id == ITEM_DATE) {
        dateInit();
      }
    }
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Lipseste fmt");
  }
}

void handleTempUnit() {
  if (!checkAuth()) return;
  if (server.hasArg("unit")) {
    uint8_t u = (uint8_t)server.arg("unit").toInt();
    if (u <= 1) {
      tempUnit = u;
      lastTemp = -999;
      saveSettings();
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
  WiFi.disconnect(true);
  delay(200);
  drawApScreen();
  WiFi.mode(WIFI_AP);
  String apSsid = getApSsid();
  String apPass = getApPass();
  WiFi.softAP(apSsid.c_str(), apPass.length() > 0 ? apPass.c_str() : "");
  Serial.print("AP pornit: ");
  Serial.print(apSsid);
  Serial.print(" — IP: ");
  Serial.println(WiFi.softAPIP());
  server.on("/",           HTTP_GET,  handleRoot);
  server.on("/scan",       HTTP_GET,  handleScan);
  server.on("/connect",    HTTP_POST, handleConnect);
  server.on("/settings",   HTTP_POST, handleSettings);
  server.on("/brightness",  HTTP_POST, handleBrightness);
  server.on("/buzzersett", HTTP_POST, handleBuzzerSett);
  server.on("/state",      HTTP_GET,  handleState);
  server.on("/nowplaying", HTTP_POST, handleNowPlaying);
  server.on("/npstate",    HTTP_GET,  handleNpState);
  server.on("/npmode",     HTTP_POST, handleNpMode);
  server.on("/screensaversett", HTTP_POST, handleScreensaverSett);
  server.on("/tempunit",   HTTP_POST, handleTempUnit);
  server.on("/hourformat",  HTTP_POST, handleHourFormat);
  server.on("/dateformat",  HTTP_POST, handleDateFormat);
  server.on("/weathersett",  HTTP_POST, handleWeatherSett);
  server.on("/weathersearch", HTTP_GET,  handleWeatherSearch);
  server.on("/weatherstate", HTTP_GET,  handleWeatherState);
  server.on("/weatherkey",   HTTP_GET,  handleWeatherKey);
  server.on("/wxlang",       HTTP_POST, handleWxLang);
  server.on("/currencysett",  HTTP_POST, handleCurrencySett);
  server.on("/currencystate", HTTP_GET,  handleCurrencyState);
  server.on("/notification", HTTP_POST, handleNotification);
  server.on("/notiftoggle",  HTTP_POST, handleNotifToggle);
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
  server.on("/mementosett",   HTTP_POST, handleMementoSett);
  server.on("/canvassett",    HTTP_POST, handleCanvasSett);
  server.on("/eventsoundsett", HTTP_POST, handleEventSoundSett);
  server.on("/touchsett",     HTTP_POST, handleTouchSett);
  server.on("/startap",       HTTP_POST, handleStartAp);
  server.on("/apstate",       HTTP_GET,  handleApState);
  server.on("/apsett",        HTTP_POST, handleApSett);
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
  const char* hdrs[] = {"Cookie"};
  server.collectHeaders(hdrs, 1);
  server.begin();
}

void handleStartAp() {
  if (!checkAuth()) return;
  server.send(200, "text/plain", "ok");
  delay(100);

  WiFi.disconnect(true);
  delay(300);
  startProvisionMode();
}

void handleApState() {
  if (!checkAuth()) return;
  String ssid = getApSsid();
  String pass = getApPass();
  String json = "{\"ssid\":\"" + ssid + "\"" +
                ",\"hasPass\":" + String(pass.length() > 0 ? "true" : "false") +
                ",\"passLen\":" + String(pass.length()) +
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
  delay(500);
  if (provisionMode) {
    stopProvisionMode();
  }
  connectToWiFi(ssid.c_str(), pass.c_str());
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

  const char* wifiLabel = "Wifi";
  int wCol = 0;
  for (int ci = 0; wifiLabel[ci] != '\0'; ci++) {
    uint8_t tmp[8]; uint8_t n = mx.getChar(wifiLabel[ci], sizeof(tmp), tmp);
    for (int i = 0; i < n; i++) mx.setColumn(31 - (wCol + i), tmp[i]);
    wCol += n;
    mx.setColumn(31 - wCol, 0x00); wCol++;
  }

  for (int col = 0; col < 8; col++) {
    uint8_t colVal = 0;
    for (int row = 0; row < 8; row++) {
      if (wifiLogo[row] & (0x80 >> col)) colVal |= (1 << row);
    }
    mx.setColumn(31 - (24 + col), colVal);
  }
  mx.update(MD_MAX72XX::ON);
  Serial.printf("Conectare la: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); Serial.print("."); attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConectat! IP: " + WiFi.localIP().toString());
    P.setTextAlignment(PA_CENTER);
    P.print("OK");
    delay(1000);

    server.on("/",           HTTP_GET,  handleRoot);
    server.on("/scan",       HTTP_GET,  handleScan);
    server.on("/connect",    HTTP_POST, handleConnect);
    server.on("/settings",   HTTP_POST, handleSettings);
  server.on("/brightness",  HTTP_POST, handleBrightness);
    server.on("/buzzersett", HTTP_POST, handleBuzzerSett);
    server.on("/state",      HTTP_GET,  handleState);
    server.on("/nowplaying", HTTP_POST, handleNowPlaying);
    server.on("/npstate",    HTTP_GET,  handleNpState);
    server.on("/npmode",     HTTP_POST, handleNpMode);
    server.on("/screensaversett", HTTP_POST, handleScreensaverSett);
    server.on("/tempunit",   HTTP_POST, handleTempUnit);
    server.on("/hourformat",  HTTP_POST, handleHourFormat);
    server.on("/dateformat",  HTTP_POST, handleDateFormat);
    server.on("/weathersett",  HTTP_POST, handleWeatherSett);
  server.on("/weathersearch", HTTP_GET,  handleWeatherSearch);
    server.on("/weatherstate", HTTP_GET,  handleWeatherState);
    server.on("/weatherkey",   HTTP_GET,  handleWeatherKey);
  server.on("/wxlang",       HTTP_POST, handleWxLang);
  server.on("/currencysett",  HTTP_POST, handleCurrencySett);
  server.on("/currencystate", HTTP_GET,  handleCurrencyState);
    server.on("/notification", HTTP_POST, handleNotification);
    server.on("/notiftoggle",  HTTP_POST, handleNotifToggle);
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
    server.on("/mementosett",   HTTP_POST, handleMementoSett);
    server.on("/canvassett",    HTTP_POST, handleCanvasSett);
    server.on("/eventsoundsett", HTTP_POST, handleEventSoundSett);
    server.on("/touchsett",     HTTP_POST, handleTouchSett);
    server.on("/startap",       HTTP_POST, handleStartAp);
    server.on("/apstate",       HTTP_GET,  handleApState);
    server.on("/apsett",        HTTP_POST, handleApSett);
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
    const char* hdrs2[] = {"Cookie"};
    server.collectHeaders(hdrs2, 1);
    server.begin();

    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", "UTC0", 1); tzset();
    autoDetectTimezone();
    struct tm ti_init;
    int retry = 0;
    while (!getLocalTime(&ti_init) && retry < 10) { delay(500); retry++; }
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

// TOUCH

void executeTouchAction(uint8_t action) {
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
      saveSettings();
      break;
    case 5:
      touchScreenOff = false;
      curBrightness = (uint8_t)constrain((int)curBrightness - 1, 0, 16);
      applyBrightness();
      saveSettings();
      break;
    case 6:
      buzzerOn = !buzzerOn;
      saveSettings();
      break;
    case 7:
      ESP.restart();
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
  } else if (!touched && touchActive) {
    if (!touchShortFired) {
      beepTouch();
      registerTouchTap();
    }
    touchActive = false; touchShortFired = false;
  }
  if (touchActive && !touchShortFired && (millis() - touchStartMs >= TOUCH_HOLD_MS)) {
    touchShortFired = true;
    touchActive = false;
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
  initSessionPool();
  loadAuth();
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);
  Wire.begin(BMP_SDA, BMP_SCL);
  mx.begin();
  P.begin();
  if (!bmp.begin(0x76)) {
    Serial.println("BMP280 negasit!");
  } else {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
    delay(50);
    pressureSampleTick();
  }
  loadSettings();
  applyMaxBrightness(curBrightness);
  prefs.begin("wifi", true);
  String savedSSID = prefs.getString("ssid", "");
  String savedPass = prefs.getString("pass", "");
  prefs.end();
  if (savedSSID.length() > 0) {
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
  if (touchScreenOff) {
    target = 0;
  } else if (!dimAutoOn) {
    target = curBrightness;
  } else {
    struct tm ti;
    if (!getLocalTime(&ti)) {
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

void loop() {
  tickBuzzer();
  handleTouch();

  if (provisionMode) {
    server.handleClient();
    return;
  }

  // WiFi Connection Lost detecteaza tranzitia conectat > deconectat
  bool wifiNowConnected = (WiFi.status() == WL_CONNECTED);
  if (wifiWasConnected && !wifiNowConnected) {
    playPresetTone(eventSoundWifi);
  }
  wifiWasConnected = wifiNowConnected;

  if (nowPlayingActive && (millis() - lastNowPlayingMs > NOW_PLAYING_TIMEOUT_MS)) {
    nowPlayingActive = false;
    nowPlayingBuf[0] = '\0';
    npPriorityWasActive = false;

    if (!nowPlayingIsPriority && items[currentSlot].id == ITEM_NOW_PLAYING) {
      advanceSlot();
      server.handleClient();
      if (provisionMode) return;
      return;
    }
  }

  // Priority Tiles ruleaza in ordinea configurata de utilizator (drag&drop
  // in Tile Manager); primul tile activ din ordine intrerupe bucla normala
  for (int pi = 0; pi < NUM_PRIORITY_IDS; pi++) {
    bool handled = false;
    switch (priorityOrder[pi]) {
      case PRIORITY_ID_NOTIF:      handled = notifTick();      break;
      case PRIORITY_ID_ETS2:       handled = ets2Tick();       break;
      case PRIORITY_ID_NOWPLAYING: handled = npPriorityTick(); break;
      case PRIORITY_ID_STOPWATCH:  handled = swPriorityTick(); break;
      case PRIORITY_ID_TIMER:      handled = timerPriorityTick(); break;
    }
    if (handled) {
      server.handleClient();
      if (provisionMode) return;
      return;
    }
  }

  CycleItem& cur = items[currentSlot];

  if (cur.id == ITEM_NOW_PLAYING) {

    if (notifActive) {
      server.handleClient();
      if (provisionMode) return;
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
        if (now - npLastScrollMs >= NP_SCROLL_SPEED_MS) {
          npLastScrollMs = now;
          npScrollPos++;
          if (npScrollPos >= npColCount) {
            npWrapPass++;
            if (npWrapPass >= SCROLL_WRAP_PASSES) {
              advanceSlot();
              server.handleClient();
              if (provisionMode) return;
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
        if (now - npLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
        if (now - npLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
          server.handleClient();
          if (provisionMode) return;
          return;
        }
        break;

      case NP_PAUSE_SHORT:
        if (now - npPauseStartMs >= 4000UL) {
          advanceSlot();
          server.handleClient();
          if (provisionMode) return;
          return;
        }
        break;
    }

    server.handleClient();
    if (provisionMode) return;
    return;
  }

  // WEATHER SCROLL (identic cu NP)
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
        if (now - wxLastScrollMs >= NP_SCROLL_SPEED_MS) {
          wxLastScrollMs = now;
          wxScrollPos++;
          if (wxScrollPos >= wxColCount) {
            wxWrapPass++;
            if (wxWrapPass >= SCROLL_WRAP_PASSES) {
              advanceSlot();
              server.handleClient();
              if (provisionMode) return;
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
        if (now - wxLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
        if (now - wxLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
          server.handleClient();
          if (provisionMode) return;
          return;
        }
        break;
      case NP_PAUSE_SHORT:
        if (now - wxPauseStartMs >= 4000UL) {
          advanceSlot();
          server.handleClient();
          if (provisionMode) return;
          return;
        }
        break;
    }
    server.handleClient();
    if (provisionMode) return;
    return;
  }

  // CURRENCY STANDARDS SCROLL (identic cu Weather/NP)
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
        if (now - currLastScrollMs >= NP_SCROLL_SPEED_MS) {
          currLastScrollMs = now;
          currScrollPos++;
          if (currScrollPos >= currColCount) {
            currWrapPass++;
            if (currWrapPass >= SCROLL_WRAP_PASSES) {
              advanceSlot();
              server.handleClient();
              if (provisionMode) return;
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
        if (now - currLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
        if (now - currLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
          server.handleClient();
          if (provisionMode) return;
          return;
        }
        break;
      case NP_PAUSE_SHORT:
        if (now - currPauseMs >= 4000UL) {
          advanceSlot();
          server.handleClient();
          if (provisionMode) return;
          return;
        }
        break;
    }
    server.handleClient();
    if (provisionMode) return;
    return;
  }

  // MEMENTO SCROLL (identic cu NP)
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
        if (now - mementoLastScrollMs >= NP_SCROLL_SPEED_MS) {
          mementoLastScrollMs = now;
          mementoScrollPos++;
          if (mementoScrollPos >= mementoColCount) {
            mementoWrapPass++;
            if (mementoWrapPass >= SCROLL_WRAP_PASSES) {
              advanceSlot();
              server.handleClient();
              if (provisionMode) return;
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
        if (now - mementoLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
        if (now - mementoLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
          server.handleClient();
          if (provisionMode) return;
          return;
        }
        break;
      case NP_PAUSE_SHORT:
        if (now - mementoPauseMs >= 4000UL) {
          advanceSlot();
          server.handleClient();
          if (provisionMode) return;
          return;
        }
        break;
    }
    server.handleClient();
    if (provisionMode) return;
    return;
  }

  server.handleClient();
  if (provisionMode) return;

  unsigned long nowMs = millis();

  static unsigned long lastBrightCheck = 0;
  if (nowMs - lastBrightCheck >= 1000UL) {
    lastBrightCheck = nowMs;
    applyBrightness();
  }

  if (strlen(weatherApiKey) > 0 &&
      (lastWeatherFetch == 0 || nowMs - lastWeatherFetch >= WEATHER_FETCH_INTERVAL_MS)) {
    weatherFetch();
    nowMs = millis();
  }

  if (lastCurrencyFetch == 0 || nowMs - lastCurrencyFetch >= CURRENCY_FETCH_INTERVAL_MS) {
    currencyFetch();
    nowMs = millis();
  }

  pressureSampleTick();

  nowMs = millis();

  switch (cur.id) {
    case ITEM_HOUR:

      if (nowMs - gLastStaticDrawMs >= 200) {
        gLastStaticDrawMs = nowMs;
        drawHour();
      }
      break;
    case ITEM_DATE: {

      if (!gStaticDrawDone) {
        gStaticDrawDone = true;
        dateInit();
      }

      if (dateNeedsScroll) {
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
            if (now - dateLastScrollMs >= NP_SCROLL_SPEED_MS) {
              dateLastScrollMs = now;
              dateScrollPos++;
              if (dateScrollPos >= dateColCount) {
                dateWrapPass++;
                if (dateWrapPass >= SCROLL_WRAP_PASSES) {
                  advanceSlot();
                  server.handleClient();
                  if (provisionMode) return;
                  return;
                }
                dateScrollPos = -dateEffTextCols;
              }
              dateDrawAtPos(dateScrollPos);
            }
            break;
          case NP_SCROLL_RIGHT:
            if (now - dateLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
            if (now - dateLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
              server.handleClient();
              if (provisionMode) return;
              return;
            }
            break;
          default: break;
        }

        server.handleClient();
        if (provisionMode) return;
        return;
      }
      break;
    }
    case ITEM_TEMP: {

      if (!gStaticDrawDone) {
        float tf = bmp.readTemperature();
        if (tf >= -40 && tf <= 85) {
          lastTemp = (int)round(tf);
          gStaticDrawDone = true;
          tempInit(lastTemp);
        }
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
            if (now - tempLastScrollMs >= NP_SCROLL_SPEED_MS) {
              tempLastScrollMs = now;
              tempScrollPos++;
              if (tempScrollPos >= tempColCount) {
                tempWrapPass++;
                if (tempWrapPass >= SCROLL_WRAP_PASSES) {
                  advanceSlot();
                  server.handleClient();
                  if (provisionMode) return;
                  return;
                }
                tempScrollPos = -tempEffTextCols;
              }
              tempDrawAtPos(tempScrollPos);
            }
            break;
          case NP_SCROLL_RIGHT:
            if (now - tempLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
            if (now - tempLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
              server.handleClient();
              if (provisionMode) return;
              return;
            }
            break;
          default: break;
        }

        server.handleClient();
        if (provisionMode) return;
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
            if (now - pressureLastScrollMs >= NP_SCROLL_SPEED_MS) {
              pressureLastScrollMs = now;
              pressureScrollPos++;
              if (pressureScrollPos >= pressureColCount) {
                pressureWrapPass++;
                if (pressureWrapPass >= SCROLL_WRAP_PASSES) {
                  advanceSlot();
                  server.handleClient();
                  if (provisionMode) return;
                  return;
                }
                pressureScrollPos = -pressureEffTextCols;
              }
              pressureDrawAtPos(pressureScrollPos);
            }
            break;
          case NP_SCROLL_RIGHT:
            if (now - pressureLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
            if (now - pressureLastScrollMs >= NP_SCROLL_SPEED_MS) {
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
              server.handleClient();
              if (provisionMode) return;
              return;
            }
            break;
          default: break;
        }
        server.handleClient();
        if (provisionMode) return;
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
