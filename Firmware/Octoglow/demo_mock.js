// OCTOGLOW - BACKEND FALS PENTRU DEMO
//
// portal.html vorbeste cu ceasul prin ~60 de endpoint-uri HTTP. Fisierul asta
// le inlocuieste pe toate cu raspunsuri sintetice, ca interfata sa poata fi
// deschisa si testata pe orice dispozitiv, fara placa si fara retea.
//
// Se injecteaza in <head>, inaintea scripturilor paginii, ca window.fetch sa
// fie deja inlocuit cand porneste loadHomeState().
//
// Ce e cu adevarat viu: cronometrul, temporizatorul, scanarea WiFi, monitorul
// hardware si alarmele. Restul salvarilor raspund OK si raman in DOM pana la o
// reincarcare - suficient ca sa poti umbla prin toata interfata.
(function () {
  "use strict";

  var BOOT = Date.now();

  // ---------------------------------------------------------------- /state
  var S = {
    buzzer: true, buzzerVolume: 80, buzzerPreset: "clasic",
    buzzerVolNotif: 80, buzzerVolAuto: 80, buzzerVolAlarm: 80, buzzerVolTouch: 80,
    ssid: "Octoglow-Demo", ip: "192.168.1.57", ap: false, apSsid: "Octoglow-A1B2",
    version: "1.9.0", uptime: 93784, lastTemp: 23.4, bmpOk: true,
    items: [
      { id: 0,  enabled: true,  dur: 5 },
      { id: 1,  enabled: true,  dur: 4 },
      { id: 2,  enabled: true,  dur: 4 },
      { id: 3,  enabled: true,  dur: 8 },
      { id: 4,  enabled: true,  dur: 8 },
      { id: 5,  enabled: false, dur: 8 },
      { id: 6,  enabled: false, dur: 8 },
      { id: 8,  enabled: false, dur: 8 },
      { id: 9,  enabled: false, dur: 15 },
      { id: 11, enabled: true,  dur: 8 },
      { id: 12, enabled: false, dur: 8 },
      { id: 10, enabled: false, dur: 5 }
    ],
    bright: 4, dimAuto: true, dimFrom: "22:00", dimTo: "07:00", dimLevel: 1,
    tempunit: 0, hourformat: 0, hourLeadingZero: true, netTimeSync: true,
    hwFormat: 0, hwLeadZero: false, hwBarMode: 1, hwBarPos: 0, hwSwap: false,
    defaultStartMode: 0, dateformat: 0, datelang: 0, customdatefmt: "d MMM",
    wxCity: "Cluj-Napoca", wxPreset: 0, wxLang: "ro", wxHasKey: true,
    iconSelYoutube: 0,
    currencyBase: "EUR", currencyQuote: "RON", currencyCompare: true,
    liveHl: true, uiDark: true, uiLang: "ro", showGrayed: true,
    autoSleep: false, autoSleepSec: 1800, sleeping: false,
    tileHidden: 0, prioHidden: 0,
    webEnabled: true, notifEnabled: true, ets2Enabled: false,
    ets2OrderFirst: false, nowPlayingIsPriority: true,
    priorityOrder: "alarm,notif,ets2,nowplaying,stopwatch,timer,webaccess",
    hideIcons: false, npAdaptiveIcon: true,
    scrollType: 0, scrollSpeed: 40, fontType: 0,
    tileTransGlobal: 0, tileTransC2P: 0, tileTransP2C: 0, tileTransSpd: 100,
    timerDurationSec: 300, timerPreset: 0,
    alarms: [
      { h: 7, m: 30, d: 31,  en: true,  p: true,  tone: "classic" },
      { h: 9, m: 15, d: 96,  en: false, p: true,  tone: "chimes"  },
      { h: 9, m: 0,  d: 127, en: false, p: false, tone: "morning" }
    ],
    pressureHpa: 1014, pressureTrend: 1,
    currencyValid: true, currencyRate: 5.0764, currencyTrend: 1,
    mementoText: "Bea apa",
    canvasBmp: "",
    evSndTile: "none", evSndWifi: "blip", evSndWeb: "blip",
    evSndNotif: "ding", evSndEts2: "none", evSndTouch: "tick", evSndTimer: "alarm",
    touchTapAction: 1, touchDoubleTapAction: 2,
    accentColor: "#d0bcff", uiShape: 0, uiColors: "", ssAnim: 0
  };

  // Cheile pe grupuri: acelasi lucru pe care firmware-ul il scrie in bucle.
  ["Hour","Date","Temp","Np","Wx","Rem","Canvas","Press","Ss","Curr","Yt","Hw",
   "Web","Notif","Ets2","Sw","Tmr","Alarm","Ip","C2P","P2C"].forEach(function (k) {
    S["tileTransSpd" + k] = 100;
  });
  ["Date","Temp","Reminder","Weather","Notif","NowPlaying","Pressure","Stopwatch",
   "Currency","Youtube","WebAccess","Timer","Ip"].forEach(function (k) {
    S["scrollType" + k] = 0;
    S["fontType" + k] = 0;
    S["scrollSpeed" + k] = 40;
  });
  ["Date","Temp","Reminder","Weather","Notif","NowPlaying","Pressure","Currency",
   "Youtube","WebAccess","Ip"].forEach(function (k) { S["hideIcon" + k] = false; });
  ["Date","Temp","Reminder","Notif","NpMusic","NpVideo","Pressure","Currency","Ip"]
    .forEach(function (k) { S["iconSel" + k] = 0; });
  ["Sunny","Cloud","Rain","Storm","Snow","Wind","Night"]
    .forEach(function (k) { S["iconWx" + k] = 0; });
  ["Hour","Date","Temp","Np","Wx","Rem","Canvas","Press","Ss","Curr","Yt","Web",
   "Notif","Ets2","Sw","Tmr","Alarm","Ip"].forEach(function (k) { S["tileTrans" + k] = 0; });
  [0, 1, 2].forEach(function (i) {
    S["soc" + i + "Handle"] = i === 0 ? "@octoglow" : "";
    S["soc" + i + "ShowName"] = true;
    S["soc" + i + "HasKey"] = i === 0;
    S["soc" + i + "KeyLen"] = i === 0 ? 39 : 0;
    S["soc" + i + "Count"] = i === 0 ? 1240 : 0;
    S["soc" + i + "Valid"] = i === 0;
  });

  // ------------------------------------------------- cronometru si timer viu
  var sw = { running: false, base: 0, since: 0 };
  var tmr = { running: false, finished: false, left: 300, since: 0 };

  function swMs() { return sw.base + (sw.running ? Date.now() - sw.since : 0); }
  function tmrLeft() {
    if (!tmr.running) return tmr.left;
    var l = tmr.left - Math.floor((Date.now() - tmr.since) / 1000);
    if (l <= 0) { tmr.running = false; tmr.finished = true; tmr.left = 0; return 0; }
    return l;
  }
  function pad(n) { return (n < 10 ? "0" : "") + n; }
  function swText(ms) {
    var t = Math.floor(ms / 1000);
    return pad(Math.floor(t / 60)) + ":" + pad(t % 60) + "." + Math.floor((ms % 1000) / 100);
  }
  function tmrText(s) {
    return s >= 3600
      ? Math.floor(s / 3600) + ":" + pad(Math.floor(s / 60) % 60) + ":" + pad(s % 60)
      : pad(Math.floor(s / 60)) + ":" + pad(s % 60);
  }

  // ------------------------------------------------------------ retele WiFi
  var NETS = [
    { ssid: "Octoglow-Demo", rssi: -46, secured: true,  auth: "WPA2" },
    { ssid: "Vecinul_5G",    rssi: -62, secured: true,  auth: "WPA2/WPA3" },
    { ssid: "TP-Link_9C40",  rssi: -71, secured: true,  auth: "WPA2" },
    { ssid: "Cafenea Free",  rssi: -78, secured: false, auth: "Open" },
    { ssid: "DIGI-8fA2",     rssi: -84, secured: true,  auth: "WPA/WPA2" }
  ];
  var scanCalls = 0;

  // --------------------------------------------------------------- alarme
  function alarmNext() {
    var now = new Date(), best = null, bestIdx = -1;
    for (var d = 0; d < 8; d++) {
      for (var i = 0; i < S.alarms.length; i++) {
        var a = S.alarms[i];
        if (!a.p || !a.en) continue;
        var when = new Date(now.getFullYear(), now.getMonth(), now.getDate() + d, a.h, a.m, 0, 0);
        if (when <= now) continue;
        var bit = 1 << ((when.getDay() + 6) % 7);
        if (!(a.d & bit)) continue;
        if (best === null || when < best) { best = when; bestIdx = i; }
      }
      if (best) break;
    }
    if (!best) return { nextMin: -1, nextIdx: -1, nextTime: "" };
    return {
      nextMin: Math.round((best - now) / 60000),
      nextIdx: bestIdx,
      nextTime: pad(best.getHours()) + ":" + pad(best.getMinutes())
    };
  }

  // ---------------------------------------------------- raspunsuri sintetice
  function resp(body, status) {
    var isStr = typeof body === "string";
    var txt = isStr ? body : JSON.stringify(body);
    return Promise.resolve({
      ok: (status || 200) < 400,
      status: status || 200,
      statusText: "OK",
      headers: { get: function () { return isStr ? "text/plain" : "application/json"; } },
      json: function () { return Promise.resolve(isStr ? { ok: true } : body); },
      text: function () { return Promise.resolve(txt); },
      blob: function () { return Promise.resolve(new Blob([txt])); },
      clone: function () { return this; }
    });
  }

  function args(url, init) {
    var out = {};
    var q = url.indexOf("?");
    function eat(s) {
      String(s || "").split("&").forEach(function (kv) {
        if (!kv) return;
        var i = kv.indexOf("=");
        var k = decodeURIComponent((i < 0 ? kv : kv.slice(0, i)).replace(/\+/g, " "));
        var v = i < 0 ? "" : decodeURIComponent(kv.slice(i + 1).replace(/\+/g, " "));
        out[k] = v;
      });
    }
    if (q >= 0) eat(url.slice(q + 1));
    if (init && typeof init.body === "string") eat(init.body);
    return out;
  }

  var ROUTES = {
    "/whoami": function () { return resp({ user: "Admin" }); },

    "/state": function () {
      S.uptime = 93784 + Math.floor((Date.now() - BOOT) / 1000);
      return resp(S);
    },

    "/hwstate": function () {
      var up = 93784 + Math.floor((Date.now() - BOOT) / 1000);
      var jitter = function (n, p) { return Math.round(n * (1 + (Math.random() - 0.5) * p)); };
      return resp({
        cpuFreqMhz: 240, cores: 2, chip: "ESP32-S3",
        loopsPerSec: jitter(1180, 0.08),
        loopMaxUs: jitter(4200, 0.3), loopMaxUsEver: 21400,
        httpMaxUs: jitter(38000, 0.2), httpMaxUri: "/state",
        pageUs: 141000, stateUs: jitter(26000, 0.15), stateBuildUs: jitter(9000, 0.15),
        pageFull: 3, page304: 27,
        heapFree: jitter(198000, 0.04), heapSize: 327680,
        heapMinFree: 171200, heapMaxAlloc: 110592,
        psramSize: 0, psramFree: 0,
        sketchSize: 1478099, sketchFree: 487981, flashSize: 8388608,
        uptime: up, hasTemp: true, tempC: 43.2 + Math.random() * 2, tempUnit: 0
      });
    },

    "/npstate": function () {
      return resp({
        active: false, text: "", npmode: 0,
        hourformat: S.hourformat, dateformat: S.dateformat, lastTemp: S.lastTemp,
        wxValid: true, wxTemp: 21.6, wxHumidity: 54, wxDesc: "cer senin"
      });
    },

    "/apstate": function () {
      var d = new Date();
      return resp({
        ssid: S.apSsid, hasPass: true, passLen: 9,
        netTime: S.netTimeSync,
        devTime: d.getFullYear() + "-" + pad(d.getMonth() + 1) + "-" + pad(d.getDate()) +
                 "T" + pad(d.getHours()) + ":" + pad(d.getMinutes()) + ":" + pad(d.getSeconds()),
        timeValid: true, mode: S.ap ? "ap" : "sta"
      });
    },

    "/appass": function () { return resp({ pass: "octoglow" }); },

    "/scan": function () {
      // Prima cerere raspunde ca scaneaza, ca sa vezi si starea de asteptare.
      scanCalls++;
      if (scanCalls % 4 === 1) return resp({ scanning: true, networks: [] });
      return resp({ scanning: false, networks: NETS });
    },

    "/wifiinfo": function () {
      return resp({
        connected: !S.ap, ssid: S.ssid, bssid: "A4:2B:B0:9C:14:03",
        rssi: -46 + Math.round(Math.random() * 6 - 3), channel: 6,
        ip: S.ip, gw: "192.168.1.1", mask: "255.255.255.0", dns: "192.168.1.1",
        mac: "3C:84:27:E1:9F:5A", phy: "11n", bw: 20, maxMbps: 72
      });
    },

    "/ets2state": function () {
      return resp({ enabled: S.ets2Enabled, active: false, speed: 0, elapsedMs: 0, text: "", running: false });
    },

    "/swstate": function () {
      var ms = swMs();
      return resp({ running: sw.running, elapsedMs: ms, text: swText(ms) });
    },
    "/swstart": function () {
      if (!sw.running) { sw.since = Date.now(); sw.running = true; }
      else { sw.base = swMs(); sw.running = false; }
      return resp("OK");
    },
    "/swstop": function () { sw.base = 0; sw.running = false; return resp("OK"); },

    "/timerstate": function () {
      var l = tmrLeft();
      return resp({
        running: tmr.running, finished: tmr.finished,
        remainingSec: l, durationSec: S.timerDurationSec, text: tmrText(l)
      });
    },
    "/timerstart": function () {
      tmr.finished = false;
      if (!tmr.running) { tmr.left = tmr.left || S.timerDurationSec; tmr.since = Date.now(); tmr.running = true; }
      return resp("OK");
    },
    "/timerpause": function (u, i) {
      var a = args(u, i);
      if (a.reset === "1" || a.action === "reset") {
        tmr.running = false; tmr.finished = false; tmr.left = S.timerDurationSec;
      } else if (tmr.running) { tmr.left = tmrLeft(); tmr.running = false; }
      else { tmr.since = Date.now(); tmr.running = true; }
      return resp("OK");
    },
    "/timersett": function (u, i) {
      var a = args(u, i);
      if (a.durationSec) {
        S.timerDurationSec = parseInt(a.durationSec, 10) || S.timerDurationSec;
        if (!tmr.running) { tmr.left = S.timerDurationSec; tmr.finished = false; }
      }
      if (a.preset !== undefined) S.timerPreset = parseInt(a.preset, 10) || 0;
      return resp("OK");
    },

    "/alarmstate": function () {
      var n = alarmNext();
      return resp({ ringing: false, idx: -1, nextMin: n.nextMin, nextIdx: n.nextIdx, nextTime: n.nextTime });
    },
    "/alarmsett": function (u, i) {
      var a = args(u, i), k = parseInt(a.idx, 10);
      if (!isNaN(k) && S.alarms[k]) {
        var t = S.alarms[k];
        if (a.hour !== undefined) t.h = parseInt(a.hour, 10) || 0;
        if (a.minute !== undefined) t.m = parseInt(a.minute, 10) || 0;
        if (a.days !== undefined) t.d = parseInt(a.days, 10) || 0;
        if (a.enabled !== undefined) t.en = a.enabled === "1";
        if (a.present !== undefined) t.p = a.present === "1";
        if (a.tone) t.tone = a.tone;
      }
      return resp("OK");
    },
    "/alarmstop": function () { return resp("OK"); },

    "/weatherstate": function () {
      return resp({
        valid: true, city: S.wxCity, lat: 46.77, lon: 23.6,
        hasKey: S.wxHasKey, keyLen: 32, lang: S.wxLang,
        temp: 21.6, humidity: 54, desc: "cer senin"
      });
    },
    "/weathersearch": function (u) {
      var q = (args(u).q || "").toLowerCase();
      var all = [
        { name: "Cluj-Napoca", country: "RO", state: "Cluj", lat: 46.77, lon: 23.60 },
        { name: "Bucuresti", country: "RO", state: "Bucuresti", lat: 44.43, lon: 26.10 },
        { name: "Timisoara", country: "RO", state: "Timis", lat: 45.75, lon: 21.23 },
        { name: "Iasi", country: "RO", state: "Iasi", lat: 47.16, lon: 27.59 },
        { name: "Londra", country: "GB", state: "", lat: 51.51, lon: -0.13 }
      ];
      return resp(q ? all.filter(function (c) { return c.name.toLowerCase().indexOf(q) === 0; }) : all);
    },

    "/tilehidden": function (u, i) {
      var a = args(u, i);
      if (a.circuit !== undefined) S.tileHidden = parseInt(a.circuit, 10) || 0;
      if (a.prio !== undefined) S.prioHidden = parseInt(a.prio, 10) || 0;
      return resp("OK");
    },
    "/priorityorder": function (u, i) {
      var a = args(u, i);
      if (a.order) S.priorityOrder = a.order;
      return resp("OK");
    },
    "/brightness": function (u, i) {
      var a = args(u, i);
      if (a.level !== undefined) S.bright = parseInt(a.level, 10);
      return resp("OK");
    },
    "/accentsett": function (u, i) {
      var a = args(u, i);
      if (a.hex) S.accentColor = a.hex;
      if (a.shape !== undefined) S.uiShape = parseInt(a.shape, 10) || 0;
      if (a.colors !== undefined) S.uiColors = a.colors;
      if (a.dark !== undefined) S.uiDark = a.dark === "1";
      return resp("OK");
    },
    "/power": function (u, i) {
      var a = args(u, i);
      return resp("Demo: actiunea " + (a.action || "?") + " nu opreste nimic aici.");
    },
    "/logout": function () { return resp("OK"); }
  };

  var realFetch = window.fetch ? window.fetch.bind(window) : null;

  window.fetch = function (input, init) {
    var url = typeof input === "string" ? input : (input && input.url) || "";
    // Doar cererile catre ceas sunt interceptate; restul (fonturi etc.) trec.
    if (url.charAt(0) !== "/") return realFetch ? realFetch(input, init) : resp("OK");
    var path = url.split("?")[0];
    var h = ROUTES[path];
    if (h) return h(url, init);
    // Restul salvarilor: raspunde OK, ca interfata sa isi vada schimbarea.
    return resp("OK");
  };

  // -------------------------------------------------------------- eticheta
  function badge() {
    if (document.getElementById("demo-badge")) return;
    // Eticheta pluteste peste pagina, deci ultimul rand din fiecare lista are
    // nevoie de loc sub el ca sa nu ramana ascuns cand ai derulat pana jos.
    var css = document.createElement("style");
    css.textContent = ".content{padding-bottom:64px}";
    document.head.appendChild(css);
    var b = document.createElement("div");
    b.id = "demo-badge";
    b.textContent = "DEMO · fara ceas conectat";
    b.title = "Apasa pentru a ascunde";
    b.setAttribute("style", [
      "position:fixed", "left:50%", "transform:translateX(-50%)",
      "bottom:calc(12px + env(safe-area-inset-bottom,0px))", "z-index:99999",
      "padding:7px 14px", "border-radius:999px",
      "font:500 12px/1 Roboto,system-ui,sans-serif", "letter-spacing:.4px",
      "background:rgba(20,20,24,.86)", "color:#fff",
      "border:1px solid rgba(255,255,255,.18)",
      "box-shadow:0 4px 16px rgba(0,0,0,.35)",
      "cursor:pointer", "-webkit-backdrop-filter:blur(6px)", "backdrop-filter:blur(6px)"
    ].join(";"));
    b.onclick = function () { b.remove(); };
    document.body.appendChild(b);
  }
  if (document.readyState === "loading")
    document.addEventListener("DOMContentLoaded", badge);
  else badge();

  // Inlocuieste navigarile care ar parasi pagina (logout, reset din fabrica).
  window.__demoNav = function () {
    var b = document.getElementById("demo-badge");
    if (!b) { badge(); b = document.getElementById("demo-badge"); }
    b.textContent = "DEMO · nu exista unde naviga";
    setTimeout(function () { b.textContent = "DEMO · fara ceas conectat"; }, 2000);
  };
})();
