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
#define DATA_PIN      5   // Pin de datos en el ESP32-C3 para la tira
#define LEDS_POR_SEG  17  // Leds calculados por segmento
#define SEG_POR_DIG   7
#define LEDS_POR_DIG  (LEDS_POR_SEG * SEG_POR_DIG) // 119 leds
#define DOS_PUNTOS    8  // 4 leds por punto
#define GUION         4  // 4 leds guion
#define TOTAL_LEDS    ((LEDS_POR_DIG * 4) + DOS_PUNTOS + GUION) // 488 leds

Adafruit_NeoPixel TiraDeLeds (TOTAL_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

// Colores del Reloj (puedes cambiarlos a gusto, formato RGB)
const uint32_t ColorHora = TiraDeLeds.Color(255, 0, 0);         // Rojo para la hora
const uint32_t ColorSeparador  = TiraDeLeds.Color(255, 100, 0); // Naranja para los separadores
const uint32_t Apagado  = TiraDeLeds.Color(0, 0, 0);            // Apagado

// Representación de dígitos en 7 segmentos (Bitwise: A,B,C,D,E,F,G)
// 1 = Encendido, 0 = Apagado
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

// --- Variables Globales ---
int horas = 0;
int minutos = 0;
unsigned long ultSincNTP = 0;

void setup_wifi() {
  Serial.print("Conectando a Wokwi-GUEST");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.write("\n"); // Nueva línea después de la conexión exitosa
  Serial.println("¡Conectado al WiFi de Wokwi!");
}

void setup() {
  Serial.begin (115200);
  
  TiraDeLeds.begin ();
  TiraDeLeds.show (); // Inicializa todos los leds en apagado
  TiraDeLeds.setBrightness (150); // Ajusta el brillo (0-255) para controlar el consumo

  setup_wifi ();

  // Configuración NTP
  configTime (gmtOffset_sec, daylightOffset_sec, ntpServer);

  Serial.println("Sincronizando hora con NTP...");
  while (!getLocalTime (nullptr)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("Hora sincronizada con NTP.");
}

void loop() {
  // Actualizar el tiempo cada segundo de forma no bloqueante
  if (millis () - ultSincNTP >= 1000) {
    ultSincNTP = millis ();
    actualizaHora ();
    mostrarEnDisplay ();
  }
}

void actualizaHora () {
  struct tm timeinfo;
  if (!getLocalTime (&timeinfo)) return;
  horas = timeinfo.tm_hour;
  minutos = timeinfo.tm_min;
}

// Dibuja un dígito específico en la tira basándose en su índice de inicio
void muestraDigito (int digitIndex, int valor, int ledInicial) {
  byte segmentos = Digitos [valor];

  // Iterar por los 7 segmentos (A a G)
  for (int segActual = 0; segActual < SEG_POR_DIG; segActual++) {
    // Verificar si el segmento actual debe estar encendido (leyendo bit por bit)
    bool encendido = (segmentos >> (6 - segActual)) & 0x01;
    uint32_t color = encendido ? ColorHora : Apagado;

    // Encender todos los leds que pertenecen a este segmento específico
    int ledActual = ledInicial + (segActual * LEDS_POR_SEG);
    for (int led = 0; led < LEDS_POR_SEG; led++) {
      TiraDeLeds.setPixelColor (ledActual + led, color);
    }
  }
}

void mostrarEnDisplay () {
  // Dígito 0 (Decenas de Hora) -> Empieza en led 0
  muestraDigito (0, horas / 10, 0);

  // Dígito 1 (Unidades de Hora) -> Empieza en led 119
  muestraDigito (1, horas % 10, LEDS_POR_DIG);

  // --- SEPARADORES CENTRALES (Led 238 al 246) ---
  int sepStart = LEDS_POR_DIG * 2;
  for (int i = 0; i < DOS_PUNTOS; i++) {
    // Parpadeo de los dos puntos/guion usando los segundos del sistema
    struct tm timeinfo;
    getLocalTime (&timeinfo);
    if (timeinfo.tm_sec % 2 == 0) {
      TiraDeLeds.setPixelColor(sepStart + i, ColorSeparador);
    } else {
      TiraDeLeds.setPixelColor(sepStart + i, Apagado);
    }
  }

  // Dígito 2 (Decenas de Minuto) -> Empieza después de los separadores (led 247)
  int d2Start = (LEDS_POR_DIG * 2) + DOS_PUNTOS + GUION; 
  muestraDigito(2, minutos / 10, d2Start);

  // Dígito 3 (Unidades de Minuto) -> Empieza en led 366
  int d3Start = d2Start + LEDS_POR_DIG;
  muestraDigito(3, minutos % 10, d3Start);

  // Enviar los datos físicos a la tira
  TiraDeLeds.show();
}