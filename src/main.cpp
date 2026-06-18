/*
 * SUNROSE
 * Interfaz de control para reloj/timbre escolar
 *
 * Plataforma : ESP32-C3 mini
 * Framework  : Arduino (PlatformIO)
 * Simulador  : Wokwi
 *
  */

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "time.h"
#include <Adafruit_NeoPixel.h>

// ─── Configuración Wi-Fi  ─────────────────────────────────
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const int WIFI_CHANNEL = 6;

// ─── Configuración NTP  ─────────────────────────────────
const char* ntpServer = "ar.pool.ntp.org";
const long  gmtOffset_sec = -10800; // GMT-3
const int   daylightOffset_sec = 0;

// ─── Prototipos ─────────────────────────────────
void mostrarEnDisplay ();
void actualizaHora ();
void configurarWiFi();
void actualizarPaletaColores(int hueGrados);
void ringBell(int durationSec);
void saveDateTime(AsyncWebServerRequest* req);
void saveSchedule(AsyncWebServerRequest* req);
void saveBellSetup(AsyncWebServerRequest* req);

// ─── Pines de conexión ────────────────────────────────────────────────────
#define DATA_PIN          5   
#define LEDS_POR_SEG      17  
#define SEG_POR_DIG       7
#define LEDS_POR_DIG      (LEDS_POR_SEG * SEG_POR_DIG) 
#define DOS_PUNTOS        8   
#define GUION             4   
#define TOTAL_LEDS        ((LEDS_POR_DIG * 4) + DOS_PUNTOS + GUION) 
#define BELL_PIN          2

// ─── Estado global ────────────────────────────────────────────────────────────
struct BellConfig {
  String  date      = "";
  String  time      = "";
  int     duration  = 3;    // segundos
  String  volume    = "Medio";
  // Horario simple: hasta 8 timbres por día (hora:minuto)
  String  schedDay  = "Lunes";
  String  schedT1   = "07:00";
  String  schedT2   = "13:00";
};

// ─── Instancias  ─────────────────────────────────
BellConfig cfg;
AsyncWebServer server(80);
Adafruit_NeoPixel TiraDeLeds (TOTAL_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

// ─── Variables de estado  ─────────────────────────────────
// --- Variables de Estado y Color ---
int tonoElegido = 120; // Guardamos el valor del slider (0-359 grados)
uint32_t ColorHora;
uint32_t ColorPuntosHora;
uint32_t ColorGuionHora;
uint32_t ColorFecha;
uint32_t ColorPuntosFecha;
uint32_t ColorGuionFecha;
const uint32_t Apagado = TiraDeLeds.Color(0, 0, 0);

char stringModoWeb[20] = "INICIALIZANDO...";
char stringValorWeb[20] = "00:00";

// Mapeo 7 segmentos
const byte Digitos [] = {
  0b1111110, 0b0110000, 0b1101101, 0b1111001, 0b0110011, 
  0b1011011, 0b1011111, 0b1110000, 0b1111111, 0b1111011  
};

int horas = 0, minutos = 0, segundos = 0, dias = 1, meses = 1;
unsigned long ultSincNTP = 0;

// ─── HTML de la interfaz (PROGMEM para ahorrar RAM) ──────────────────────────
const char HTML_PAGE[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>SUNROSE</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: 'Segoe UI', sans-serif; background: #f0f0f5; min-height: 100vh; display: flex; justify-content: center; padding: 20px 0 40px; }
    .app { width: 100%; max-width: 400px; }

    .status-bar { background: #2e7d32; color: #fff; padding: 10px 14px; font-size: 13px; border-radius: 12px 12px 0 0; display: flex; align-items: center; gap: 8px; }
    .dot { width: 8px; height: 8px; border-radius: 50%; background: #a5d6a7; flex-shrink: 0; }

    .body { background: #fff; border: 1px solid #ddd; border-top: none; border-radius: 0 0 12px 12px; padding: 20px 16px; }
    h1 { text-align: center; font-size: 22px; font-weight: 600; color: #111; margin-bottom: 2px; }
    .subtitle { text-align: center; font-size: 13px; color: #666; margin-bottom: 20px; display: flex; align-items: center; gap: 8px; }
    .subtitle::before, .subtitle::after { content: ''; flex: 1; height: 1px; background: #ddd; }

    .btn { display: block; width: 100%; padding: 14px; background: #c62828; color: #fff; border: none; border-radius: 8px; font-size: 15px; font-weight: 500; cursor: pointer; margin-bottom: 10px; text-align: center; transition: background .15s; }
    .btn:hover { background: #b71c1c; }
    .btn:active { background: #8b0000; }
    .btn-manual { background: #1565c0; }
    .btn-manual:hover { background: #0d47a1; }

    .submenu { border: 2px dashed #c62828; border-radius: 8px; padding: 14px; margin-top: -4px; margin-bottom: 10px; background: #fffde7; display: none; }
    .submenu.open { display: block; }
    label { font-size: 13px; color: #555; display: block; margin: 10px 0 4px; }
    label:first-child { margin-top: 0; }
    select, input[type=text] { width: 100%; padding: 8px 10px; border: 1px solid #ccc; border-radius: 6px; font-size: 14px; background: #fff; color: #111; }
    .btn-save { display: block; width: 100%; padding: 11px; background: #5c7a9e; color: #fff; border: none; border-radius: 6px; font-size: 14px; font-weight: 500; cursor: pointer; margin-top: 12px; }
    .btn-save:hover { background: #4a6480; }

    .toast { display: none; background: #2e7d32; color: #fff; font-size: 13px; padding: 9px 14px; border-radius: 8px; text-align: center; margin-bottom: 10px; }
    .toast.show { display: block; }
    .toast.error { background: #c62828; }

    .bell-anim { display: none; font-size: 32px; text-align: center; padding: 6px; animation: ring .3s infinite alternate; }
    .bell-anim.active { display: block; }
    @keyframes ring { from { transform: rotate(-15deg); } to { transform: rotate(15deg); } }
  </style>
</head>
<body>
<div class="app">
  <div class="status-bar">
    <div class="dot"></div>
    <span id="status-text">Conectando...</span>
  </div>

  <div class="body">
    <h1>SUNROSE</h1>
    <p class="subtitle">SuperSync Technologies</p>

    <!-- ── Fecha y Hora ──────────────────────────────── -->
    <button class="btn" onclick="toggle('datetime')">📅 Configurar Fecha y Hora</button>
    <div class="submenu" id="menu-datetime">
      <label>Fecha (YYYY-MM-DD):</label>
      <input type="text" id="inp-date" placeholder="2026-06-16">
      <label>Hora (HH:MM):</label>
      <select id="inp-time"></select>
      <button class="btn-save" onclick="saveDateTime()">💾 Guardar</button>
    </div>
    <div class="toast" id="toast-datetime"></div>

    <!-- ── Timbre Manual ─────────────────────────────── -->
    <button class="btn btn-manual" onclick="ringBell()">🔔 Timbre Manual</button>
    <div class="bell-anim" id="bell-anim">🔔</div>
    <div class="toast" id="toast-bell"></div>

    <!-- ── Horario Diario ────────────────────────────── -->
    <button class="btn" onclick="toggle('schedule')">📆 Horario Diario</button>
    <div class="submenu" id="menu-schedule">
      <label>Día:</label>
      <select id="sched-day">
        <option>Lunes</option><option>Martes</option><option>Miércoles</option>
        <option>Jueves</option><option>Viernes</option><option>Sábado</option>
      </select>
      <label>Timbre 1:</label>
      <select id="sched-t1"></select>
      <label>Timbre 2:</label>
      <select id="sched-t2"></select>
      <button class="btn-save" onclick="saveSchedule()">💾 Guardar Horario</button>
    </div>
    <div class="toast" id="toast-schedule"></div>

    <!-- ── Configuración del Timbre ──────────────────── -->
    <button class="btn" onclick="toggle('bellsetup')">⚙️ Configuración del Timbre</button>
    <div class="submenu" id="menu-bellsetup">
      <label>Duración del toque (seg):</label>
      <select id="bell-dur">
        <option value="1">1</option><option value="2">2</option>
        <option value="3" selected>3</option><option value="5">5</option>
        <option value="10">10</option>
      </select>
      <label>Volumen:</label>
      <select id="bell-vol">
        <option>Bajo</option><option selected>Medio</option><option>Alto</option>
      </select>
      <button class="btn-save" onclick="saveBellSetup()">💾 Guardar Configuración</button>
    </div>
    <div class="toast" id="toast-bellsetup"></div>
  </div>
</div>

<script>
// Poblar selects de hora
function populateTimes(selId, defaultVal) {
  const sel = document.getElementById(selId);
  for (let h = 0; h < 24; h++) {
    for (let m = 0; m < 60; m += 15) {
      const v = String(h).padStart(2,'0') + ':' + String(m).padStart(2,'0');
      const o = document.createElement('option');
      o.value = o.textContent = v;
      if (v === defaultVal) o.selected = true;
      sel.appendChild(o);
    }
  }
}
populateTimes('inp-time',  '07:00');
populateTimes('sched-t1', '07:00');
populateTimes('sched-t2', '13:00');

// Estado del servidor
function updateStatus() {
  fetch('/status')
    .then(r => r.json())
    .then(d => {
      document.getElementById('status-text').textContent =
        'Online ✔ [Wi-Fi: ESP32-Bell] | ' + d.time;
    })
    .catch(() => {
      document.getElementById('status-text').textContent = 'Sin conexión';
    });
}
updateStatus();
setInterval(updateStatus, 5000);

// Toggle submenús
function toggle(id) {
  const el = document.getElementById('menu-' + id);
  const open = el.classList.contains('open');
  document.querySelectorAll('.submenu').forEach(m => m.classList.remove('open'));
  if (!open) el.classList.add('open');
}

// Toast helper
function toast(id, msg, isError) {
  const t = document.getElementById('toast-' + id);
  t.textContent = msg;
  t.className = 'toast show' + (isError ? ' error' : '');
  setTimeout(() => t.className = 'toast', 3000);
}

// POST helper
async function post(url, data) {
  const params = new URLSearchParams(data);
  const r = await fetch(url, { method: 'POST', body: params,
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' } });
  return r.json();
}

// Guardar fecha/hora
async function saveDateTime() {
  const date = document.getElementById('inp-date').value.trim();
  const time = document.getElementById('inp-time').value;
  if (!date) { toast('datetime', '⚠ Ingresá una fecha', true); return; }
  const r = await post('/save-datetime', { date, time });
  toast('datetime', r.ok ? '✔ ' + r.msg : '✘ Error: ' + r.msg, !r.ok);
}

// Timbre manual
async function ringBell() {
  document.getElementById('bell-anim').classList.add('active');
  const r = await post('/ring', {});
  setTimeout(() => document.getElementById('bell-anim').classList.remove('active'), 3500);
  toast('bell', r.ok ? '✔ ' + r.msg : '✘ Error', !r.ok);
}

// Guardar horario
async function saveSchedule() {
  const day = document.getElementById('sched-day').value;
  const t1  = document.getElementById('sched-t1').value;
  const t2  = document.getElementById('sched-t2').value;
  const r = await post('/save-schedule', { day, t1, t2 });
  toast('schedule', r.ok ? '✔ ' + r.msg : '✘ Error', !r.ok);
}

// Guardar config timbre
async function saveBellSetup() {
  const duration = document.getElementById('bell-dur').value;
  const volume   = document.getElementById('bell-vol').value;
  const r = await post('/save-bellsetup', { duration, volume });
  toast('bellsetup', r.ok ? '✔ ' + r.msg : '✘ Error', !r.ok);
}
</script>
</body>
</html>
)rawhtml";

// ─── Helpers ──────────────────────────────────────────────────────────────────
void actualizaHora () {
  struct tm timeinfo;
  if (!getLocalTime (&timeinfo)) return;
  horas    = timeinfo.tm_hour;
  minutos  = timeinfo.tm_min;
  segundos = timeinfo.tm_sec; 
  dias     = timeinfo.tm_mday;
  meses    = timeinfo.tm_mon + 1;
}

void muestraDigito (int valor, int ledInicial, uint32_t colorTexto) {
  byte segmentos = Digitos [valor];

  for (int segActual = 0; segActual < SEG_POR_DIG; segActual++) {
    bool encendido = (segmentos >> (6 - segActual)) & 0x01;
    uint32_t color = encendido ? colorTexto : Apagado;

    int ledActual = ledInicial + (segActual * LEDS_POR_SEG);
    for (int led = 0; led < LEDS_POR_SEG; led++) {
      TiraDeLeds.setPixelColor (ledActual + led, color);
    }
  }
}

void mostrarEnDisplay () {
  int primerGrupo = 0;
  int segundoGrupo = 0;
  uint32_t colorActualTexto;
  uint32_t colorActualPuntos;
  uint32_t colorActualGuion;

  if (segundos % 10 < 5) {
    // --- MODO HORA ---
    primerGrupo  = horas;
    segundoGrupo = minutos; 
    colorActualTexto = ColorHora;
    colorActualGuion = ColorGuionHora;
    colorActualPuntos = (segundos % 2 == 0) ? ColorPuntosHora : Apagado;

    // Guardar copia para la petición Web
    strcpy(stringModoWeb, "MOSTRANDO: HORA");
    sprintf(stringValorWeb, "%02d:%02d", horas, minutos);
  } else {
    // --- MODO FECHA ---
    primerGrupo  = dias;
    segundoGrupo = meses;
    colorActualTexto = ColorFecha;
    colorActualPuntos = ColorPuntosFecha; 
    colorActualGuion = ColorGuionFecha;   

    // Guardar copia para la petición Web
    strcpy(stringModoWeb, "MOSTRANDO: FECHA");
    sprintf(stringValorWeb, "%02d-%02d", dias, meses);
  }

  // --- RENDERIZADO EN TIRA FÍSICA ---
  muestraDigito (primerGrupo / 10, 0, colorActualTexto);
  muestraDigito (primerGrupo % 10, LEDS_POR_DIG, colorActualTexto);

  int inicioPuntos = LEDS_POR_DIG * 2;          
  int inicioGuion  = inicioPuntos + DOS_PUNTOS; 

  for (int i = 0; i < DOS_PUNTOS; i++) {
    TiraDeLeds.setPixelColor(inicioPuntos + i, colorActualPuntos);
  }
  for (int i = 0; i < GUION; i++) {
    TiraDeLeds.setPixelColor(inicioGuion + i, colorActualGuion);
  }

  int d2Start = inicioGuion + GUION; 
  muestraDigito(segundoGrupo / 10, d2Start, colorActualTexto);

  int d3Start = d2Start + LEDS_POR_DIG;
  muestraDigito(segundoGrupo % 10, d3Start, colorActualTexto);

  TiraDeLeds.show();
}

void ringBell(int durationSec) {
  Serial.printf("[BELL] Activando %d seg\n", durationSec);
  digitalWrite(BELL_PIN, HIGH);
  delay(durationSec * 1000);
  digitalWrite(BELL_PIN, LOW);
}

String jsonOk(const String& msg) {
  return "{\"ok\":true,\"msg\":\"" + msg + "\"}";
}

String jsonErr(const String& msg) {
  return "{\"ok\":false,\"msg\":\"" + msg + "\"}";
}

void actualizarPaletaColores(int hueGrados) {
  uint32_t hueLeds = map(hueGrados, 0, 359, 0, 65535);
  uint32_t colorUsuario = TiraDeLeds.ColorHSV(hueLeds, 255, 255);
  
  uint32_t hueFecha = (hueLeds + 32768) % 65536;
  uint32_t colorContraste = TiraDeLeds.ColorHSV(hueFecha, 255, 255);

  ColorHora         = colorUsuario;
  ColorPuntosHora   = colorUsuario;
  ColorGuionHora    = TiraDeLeds.Color(40, 40, 40); 
  
  ColorFecha        = colorContraste; 
  ColorPuntosFecha  = Apagado;
  ColorGuionFecha   = colorContraste;
}

// Reemplaza los marcadores (%SLIDER_VAL%) del HTML estático con variables reales del microcontrolador
String procesadorTemplates(const String& var) {
  if (var == "SLIDER_VAL") {
    return String(tonoElegido);
  }
  return String();
}

void configurarWiFi () {
  Serial.print ("[INFO] Conectando a " + String(ssid) + "...");
  WiFi.begin (ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay (500);
    Serial.print (".");
  }
  Serial.println();
  Serial.println ("[INFO] ¡Conectado a WiFi " + String(ssid) + "!");
  Serial.println("[INFO] Dirección IP: " + WiFi.localIP().toString());
}

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Inicialización de tira de LEDs
  TiraDeLeds.begin ();
  TiraDeLeds.show (); 
  TiraDeLeds.setBrightness (150); 
  actualizarPaletaColores(tonoElegido); 

  configurarWiFi();

  // ── Rutas del servidor ────────────────────────────────────────────────────
  // Página principal
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", HTML_PAGE);
  });

  // Estado (JSON)
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    String body = "{\"ok\":true,\"time\":\"";
    // En producción, usar RTC real. Aquí devolvemos millis() como ejemplo.
    unsigned long s = millis() / 1000;
    unsigned long h = (s / 3600) % 24;
    unsigned long m = (s / 60) % 60;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02lu:%02lu", h, m);
    body += String(buf) + "\"}";
    req->send(200, "application/json", body);
  });

  // Guardar fecha y hora
  server.on("/save-datetime", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("date", true) || !req->hasParam("time", true)) {
      req->send(400, "application/json", jsonErr("Parámetros faltantes"));
      return;
    }
    cfg.date = req->getParam("date", true)->value();
    cfg.time = req->getParam("time", true)->value();
    Serial.printf("[CFG] Fecha: %s  Hora: %s\n", cfg.date.c_str(), cfg.time.c_str());
    req->send(200, "application/json", jsonOk("Fecha " + cfg.date + " / " + cfg.time + " guardada"));
  });

  // Timbre manual
  server.on("/ring", HTTP_POST, [](AsyncWebServerRequest* req) {
    Serial.println("[BELL] Timbre manual activado");
    ringBell(cfg.duration);
    req->send(200, "application/json", jsonOk("Timbre activado por " + String(cfg.duration) + " seg"));
  });

  // Guardar horario
  server.on("/save-schedule", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (!req->hasParam("day", true)) {
      req->send(400, "application/json", jsonErr("Parámetros faltantes"));
      return;
    }
    cfg.schedDay = req->getParam("day",  true)->value();
    cfg.schedT1  = req->getParam("t1",   true)->value();
    cfg.schedT2  = req->getParam("t2",   true)->value();
    Serial.printf("[CFG] Horario %s: %s / %s\n",
      cfg.schedDay.c_str(), cfg.schedT1.c_str(), cfg.schedT2.c_str());
    req->send(200, "application/json",
      jsonOk(cfg.schedDay + ": " + cfg.schedT1 + " y " + cfg.schedT2));
  });

  // Configuración del timbre
  server.on("/save-bellsetup", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (req->hasParam("duration", true))
      cfg.duration = req->getParam("duration", true)->value().toInt();
    if (req->hasParam("volume", true))
      cfg.volume = req->getParam("volume", true)->value();
    Serial.printf("[CFG] Duración: %d seg  Volumen: %s\n", cfg.duration, cfg.volume.c_str());
    req->send(200, "application/json",
      jsonOk("Duración " + String(cfg.duration) + "s / Vol: " + cfg.volume));
  });

  // 404
  server.onNotFound([](AsyncWebServerRequest* req) {
    req->send(404, "application/json", jsonErr("Ruta no encontrada"));
  });

  configTime (gmtOffset_sec, daylightOffset_sec, ntpServer);
  server.begin();
  Serial.println("[SUNROSE] Servidor HTTP iniciado en el puerto 80");
  Serial.println("[SUNROSE] Accede a http://" + WiFi.localIP().toString() + " en tu navegador");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  // ESPAsyncWebServer maneja todo en interrupciones.
  if (millis () - ultSincNTP >= 1000) {
  ultSincNTP = millis ();
  actualizaHora ();
  mostrarEnDisplay ();
  }
}