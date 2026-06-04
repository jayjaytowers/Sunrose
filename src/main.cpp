#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
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
#define DATA_PIN          5   // Pin de datos en el ESP32-C3 para la tira
#define LEDS_POR_SEG      17  // Leds calculados por segmento
#define SEG_POR_DIG       7
#define LEDS_POR_DIG      (LEDS_POR_SEG * SEG_POR_DIG) // 119 leds
#define DOS_PUNTOS        8   // 8 leds para los dos puntos
#define GUION             4   // 4 leds para el guion
#define TOTAL_LEDS        ((LEDS_POR_DIG * 4) + DOS_PUNTOS + GUION) // 488 leds

Adafruit_NeoPixel TiraDeLeds (TOTAL_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

// --- Paleta de Colores Independientes ---
const uint32_t ColorHora         = TiraDeLeds.Color(0, 255, 0);     // Verde para los números de la HORA
const uint32_t ColorPuntosHora   = TiraDeLeds.Color(0, 255, 0);     // Verde para los dos puntos de la HORA
const uint32_t ColorGuionHora    = TiraDeLeds.Color(0, 0, 0);       // Apagado para el guion durante la HORA

const uint32_t ColorFecha        = TiraDeLeds.Color(255, 127, 0);   // Naranja para los números de la FECHA
const uint32_t ColorPuntosFecha  = TiraDeLeds.Color(0, 0, 0);       // Apagado para los dos puntos durante la FECHA
const uint32_t ColorGuionFecha   = TiraDeLeds.Color(255, 0, 0);     // Rojo para el guion separador de la FECHA

const uint32_t Apagado           = TiraDeLeds.Color(0, 0, 0);       // Apagado

// Representación de dígitos en 7 segmentos
const byte Digitos [] = {
  0b1111110, // 0
  0b0110000, // 1
  0b1101101, // 2
  0b1111001, // 3
  0b0110011, // 4
  0b1011011, // 5
  0b1011111, // 6
  0b1110000, // 7
  0b1111111, // 8
  0b1111011  // 9
};

// --- Variables Globales de Tiempo ---
int horas = 0;
int minutos = 0;
int segundos = 0; 
int dias = 1;
int meses = 1;
unsigned long ultSincNTP = 0;

void setup_wifi() {
  Serial.print("Conectando a Wokwi-GUEST");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.write("\n"); 
  Serial.println("¡Conectado al WiFi de Wokwi!");
}

void setup() {
  Serial.begin (115200);
  
  TiraDeLeds.begin ();
  TiraDeLeds.show (); 
  TiraDeLeds.setBrightness (150); 

  setup_wifi ();

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
  meses    = timeinfo.tm_mon + 1; // tm_mon va de 0 a 11
}

// Modificada para aceptar un color dinámico como parámetro
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
  // Variables locales para decidir qué datos y qué colores usar
  int primerGrupo = 0;
  int segundoGrupo = 0;
  uint32_t colorActualTexto;
  uint32_t colorActualPuntos;
  uint32_t colorActualGuion;

  // Lógica de alternancia: de los segundos 0-4 (Hora), de los segundos 5-9 (Fecha), etc.
  if (segundos % 10 < 5) {
    // --- MODO HORA ---
    primerGrupo  = horas;
    segundoGrupo = minutos;
    colorActualTexto = ColorHora;
    colorActualGuion = ColorGuionHora;
    
    // Parpadeo de los dos puntos (solo en modo hora)
    colorActualPuntos = (segundos % 2 == 0) ? ColorPuntosHora : Apagado;
  } else {
    // --- MODO FECHA ---
    primerGrupo  = dias;
    segundoGrupo = meses;
    colorActualTexto = ColorFecha;
    colorActualPuntos = ColorPuntosFecha; // Apagados por configuración de paleta
    colorActualGuion = ColorGuionFecha;   // Encendido fijo para separar DD de MM
  }

  // --- RENDERIZADO EN TIRA ---
  
  // Dígito 0 (Decenas del primer grupo) -> Empieza en led 0
  muestraDigito (primerGrupo / 10, 0, colorActualTexto);

  // Dígito 1 (Unidades del primer grupo) -> Empieza en led 119
  muestraDigito (primerGrupo % 10, LEDS_POR_DIG, colorActualTexto);

  // --- SEPARADORES CENTRALES ---
  int inicioPuntos = LEDS_POR_DIG * 2;          // LED 238
  int inicioGuion  = inicioPuntos + DOS_PUNTOS; // LED 246

  // Dibujar Dos Puntos
  for (int i = 0; i < DOS_PUNTOS; i++) {
    TiraDeLeds.setPixelColor(inicioPuntos + i, colorActualPuntos);
  }

  // Dibujar Guion
  for (int i = 0; i < GUION; i++) {
    TiraDeLeds.setPixelColor(inicioGuion + i, colorActualGuion);
  }

  // --- SEGUNDO GRUPO DE DÍGITOS ---
  // Dígito 2 (Decenas del segundo grupo) -> Empieza en LED 250
  int d2Start = inicioGuion + GUION; 
  muestraDigito(segundoGrupo / 10, d2Start, colorActualTexto);

  // Dígito 3 (Unidades del segundo grupo) -> Empieza en led 369
  int d3Start = d2Start + LEDS_POR_DIG;
  muestraDigito(segundoGrupo % 10, d3Start, colorActualTexto);

  // Enviar los datos físicos a la tira de leds
  TiraDeLeds.show();
}