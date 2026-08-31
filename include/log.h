#pragma once
#include <Arduino.h>

// ============================================================
//  Мини-логгер с уровнями и тегами. Пишет в Serial.
//  На ESP32-C3 DevKitM-1 / SuperMini обычный Serial уже выходит
//  по USB (UART0 подключён к встроенному USB-UART или USB-CDC мосту).
//  Если у вас плата БЕЗ USB (чистый модуль), подключите UART0
//  (GPIO20/21) к USB-TTL адаптеру и поправьте platformio.ini:
//      build_flags = -DLOG_SERIAL=Serial0
//      monitor_port = COM<номер>
// ============================================================

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
