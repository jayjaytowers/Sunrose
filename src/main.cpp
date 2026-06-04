#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "time.h"
#include <Adafruit_NeoPixel.h>

// --- Prototipos ---
void mostrarEnDisplay ();
void actualizaHora ();

// --- Configuración Wi-Fi ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// --- Configuración NTP ---
const char* ntpServer = "ar.pool.ntp.org";
const long  gmtOffset_sec = -10800; // GMT-3
const int   daylightOffset_sec = 0;

// --- Configuración Neopixel ---
#define DATA_PIN          5   
#define LEDS_POR_SEG      17  
#define SEG_POR_DIG       7
#define LEDS_POR_DIG      (LEDS_POR_SEG * SEG_POR_DIG) 
#define DOS_PUNTOS        8   
#define GUION             4   
#define TOTAL_LEDS        ((LEDS_POR_DIG * 4) + DOS_PUNTOS + GUION) 

Adafruit_NeoPixel TiraDeLeds (TOTAL_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

// --- Servidor Web Async ---
AsyncWebServer server(80);

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

// --- Código HTML / CSS / JS de la Página Web ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Panel de Control - Reloj RGB</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #121212; color: #e0e0e0; text-align: center; margin: 0; padding: 20px; }
        .container { max-width: 500px; margin: auto; background: #1e1e1e; padding: 30px; border-radius: 15px; box-shadow: 0 4px 15px rgba(0,0,0,0.5); }
        h1 { color: #00ff66; margin-bottom: 25px; font-size: 24px; }
        .card { background: #292929; padding: 15px; border-radius: 10px; margin-bottom: 20px; border-left: 5px solid #00ff66; }
        .label { font-size: 12px; text-transform: uppercase; color: #888; letter-spacing: 1px; }
        .value { font-size: 32px; font-weight: bold; font-family: monospace; color: #fff; margin-top: 5px; }
        .slider-container { margin-top: 30px; }
        input[type=range] { -webkit-appearance: none; width: 100%; background: linear-gradient(to right, red, yellow, green, cyan, blue, magenta, red); hieght: 15px; border-radius: 8px; outline: none; height: 12px; }
        input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; width: 25px; height: 25px; border-radius: 50%; background: #ffffff; cursor: pointer; box-shadow: 0 0 5px #000; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Reloj de Pared WS2812B</h1>
        
        <div class="card">
            <div id="web-modo" class="label">Cargando...</div>
            <div id="web-valor" class="value">--:--</div>
        </div>

        <div class="slider-container">
            <div class="label" style="margin-bottom: 10px;">Color de los Dígitos (Tono HSV)</div>
            <input type="range" id="colorSlider" min="0" max="359" value="%SLIDER_VAL%" onchange="enviarColor(this.value)">
        </div>
    </div>

    <script>
        // Función para pedir datos en tiempo real al ESP32 (Cada 1 segundo)
        setInterval(function() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('web-modo').innerText = data.modo;
                    document.getElementById('web-valor').innerText = data.valor;
                });
        }, 1000);

        // Función para enviar el valor del slider sin recargar la página
        function enviarColor(val) {
            fetch('/setcolor?value=' + val);
        }
    </script>
</body>
</html>
)rawliteral";

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

void setup_wifi() {
  Serial.print("Conectando a Wokwi-GUEST");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.write("\n"); 
  Serial.println("¡Conectado al WiFi de Wokwi!");
  Serial.print("Dirección IP: http://");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin (115200);
  
  TiraDeLeds.begin ();
  TiraDeLeds.show (); 
  TiraDeLeds.setBrightness (150); 

  actualizarPaletaColores(tonoElegido); 

  setup_wifi ();

  // --- CONFIGURACIÓN DE RUTAS WEB ---
  
  // 1. Ruta Principal (Carga la interfaz gráfica)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, procesadorTemplates);
  });

  // 2. Ruta de Estado (Devuelve un JSON ligero con lo que se ve en pantalla)
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    char jsonResponse[64];
    sprintf(jsonResponse, "{\"modo\":\"%s\",\"valor\":\"%s\"}", stringModoWeb, stringValorWeb);
    request->send(200, "application/json", jsonResponse);
  });

  // 3. Ruta para recibir el cambio de color desde el Slider
  server.on("/setcolor", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("value")) {
      tonoElegido = request->getParam("value")->value().toInt();
      actualizarPaletaColores(tonoElegido);
      Serial.printf("Web nativa actualizó tono a: %d°\n", tonoElegido);
    }
    request->send(200, "text/plain", "OK");
  });

  server.begin();
  configTime (gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
  if (millis () - ultSincNTP >= 1000) {
    ultSincNTP = millis ();
    actualizaHora ();
    mostrarEnDisplay ();
  }
}

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