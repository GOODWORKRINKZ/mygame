#pragma once
#include <Arduino.h>

// ============================================================
//  Мини-логгер с уровнями и тегами. Пишет в Serial.
//  На ESP32-C3:
//   * Без ARDUINO_USB_CDC_ON_BOOT: Serial = UART0 (GPIO20/21).
//     Если у платы USB-UART мост (CH340/CP2102) — лог идёт в COM.
//     Если у платы нативный USB (DevKitM-1/SuperMini) — пины пустые,
//     лога не будет. В этом случае добавьте флаг ниже.
//   * С ARDUINO_USB_CDC_ON_BOOT=1: Serial = USB-CDC (нужен
//     дополнительный include HWCDC.h, чтобы компилятор видел Serial).
// ============================================================

#if ARDUINO_USB_CDC_ON_BOOT
  #include <HWCDC.h>     // объявление extern HWCDC Serial в USB-CDC режиме
#endif

#ifndef LOG_SERIAL
  #define LOG_SERIAL Serial
#endif

#ifndef LOG_LEVEL
  #define LOG_LEVEL 1   // 0=silent, 1=info, 2=debug, 3=verbose
#endif

#define LOG_I(tag, fmt, ...) do { \
    if (LOG_LEVEL >= 1) { \
      LOG_SERIAL.printf("[I %6lu][%-6s] " fmt "\n", (uint32_t)millis(), tag, ##__VA_ARGS__); \
    } \
  } while (0)

#define LOG_W(tag, fmt, ...) do { \
    if (LOG_LEVEL >= 1) { \
      LOG_SERIAL.printf("[W %6lu][%-6s] " fmt "\n", (uint32_t)millis(), tag, ##__VA_ARGS__); \
    } \
  } while (0)

#define LOG_E(tag, fmt, ...) do { \
    if (LOG_LEVEL >= 1) { \
      LOG_SERIAL.printf("[E %6lu][%-6s] " fmt "\n", (uint32_t)millis(), tag, ##__VA_ARGS__); \
    } \
  } while (0)

#define LOG_D(tag, fmt, ...) do { \
    if (LOG_LEVEL >= 2) { \
      LOG_SERIAL.printf("[D %6lu][%-6s] " fmt "\n", (uint32_t)millis(), tag, ##__VA_ARGS__); \
    } \
  } while (0)

#define LOG_V(tag, fmt, ...) do { \
    if (LOG_LEVEL >= 3) { \
      LOG_SERIAL.printf("[V %6lu][%-6s] " fmt "\n", (uint32_t)millis(), tag, ##__VA_ARGS__); \
    } \
  } while (0)
