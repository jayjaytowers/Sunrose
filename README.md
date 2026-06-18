# Sunrose

Interfaz web de control para timbre escolar, ejecutable en **VSCode + PlatformIO + Wokwi**.

---

## Estructura del proyecto

```
Sunrose/
├── src/
│   └── main.cpp          ← código principal
├── diagram.json          ← circuito Wokwi (ESP32 + LED simulando timbre)
├── wokwi.toml            ← enlace firmware ↔ Wokwi
├── platformio.ini        ← dependencias y plataforma
└── README.md
```

---

## Requisitos

- **VSCode** con extensiones:
  - PlatformIO IDE
  - Wokwi Simulator (requiere licencia gratuita en wokwi.com)

---

## Pasos para correr

1. Abrí la carpeta `wb06pro/` en VSCode.
2. PlatformIO descargará automáticamente las librerías al compilar.
3. Compilá con **PlatformIO: Build** (ícono ✓ o `Ctrl+Alt+B`).
4. Iniciá la simulación con **Wokwi: Start Simulator** (`F1` → "Wokwi").
5. En el Serial Monitor verás:

```
[WIFI] AP iniciado  SSID: ESP32-Bell
[WIFI] IP: 192.168.4.1
[SERVER] Servidor HTTP iniciado en el puerto 80
[INFO] Abrí http://192.168.4.1 en tu navegador
```

> ⚠️ En Wokwi, el Wi-Fi es simulado. Usá el **navegador integrado de Wokwi**
> (ícono 🌐 en la barra de la simulación) e ingresá `http://192.168.4.1`.

---

## Endpoints HTTP

| Método | Ruta             | Descripción                        |
|--------|------------------|------------------------------------|
| GET    | `/`              | Página HTML de la interfaz         |
| GET    | `/status`        | JSON con hora actual               |
| POST   | `/save-datetime` | Guarda `date` y `time`             |
| POST   | `/ring`          | Activa el timbre manual            |
| POST   | `/save-schedule` | Guarda `day`, `t1`, `t2`           |
| POST   | `/save-bellsetup`| Guarda `duration` y `volume`       |

---

## Hardware real (fuera de Wokwi)

- **GPIO 2** → resistor 220Ω → LED (o relé 5V con transistor NPN)
- Cambiar `#define BELL_PIN 2` al GPIO que uses para el relé
- Para horario automático real, agregar módulo **RTC DS3231** por I2C

---

## Próximos pasos sugeridos

- [ ] Integrar RTC DS3231 para hora real
- [ ] Guardar configuración en NVS (Preferences.h) para sobrevivir reinicios
- [ ] Agregar múltiples timbres por día en el horario
- [ ] Modo Station (conectarse a Wi-Fi existente en lugar de AP)
