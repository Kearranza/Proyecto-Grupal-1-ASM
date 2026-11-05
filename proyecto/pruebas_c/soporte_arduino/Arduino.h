#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <string>
using std::isfinite;
using std::isinf;
using std::isnan;
#define IRAM_ATTR
#define SERIAL_8N1 0
#define portMUX_INITIALIZER_UNLOCKED 0
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(x) (x)
using portMUX_TYPE = int;
using BaseType_t = int;
using TickType_t = uint32_t;
using QueueHandle_t = void *;
using TaskHandle_t = void *;
struct hw_timer_t
{
};
inline void portENTER_CRITICAL(portMUX_TYPE *) {}
inline void portEXIT_CRITICAL(portMUX_TYPE *) {}
inline void portYIELD_FROM_ISR() {}
inline void vTaskNotifyGiveFromISR(TaskHandle_t, BaseType_t *) {}
inline uint32_t ulTaskNotifyTake(int, uint32_t) { return 1; }
inline void vTaskDelay(uint32_t) {}
inline void vTaskDelayUntil(TickType_t *, TickType_t) {}
inline TickType_t xTaskGetTickCount() { return 0; }
inline int xQueueSend(QueueHandle_t, const void *, uint32_t) { return pdTRUE; }
inline int xQueueReceive(QueueHandle_t, void *, uint32_t) { return pdFALSE; }
inline void xQueueReset(QueueHandle_t) {}
inline QueueHandle_t xQueueCreate(int, size_t) { return reinterpret_cast<void *>(1); }
inline int xTaskCreatePinnedToCore(void (*)(void *), const char *, unsigned, void *, unsigned, TaskHandle_t *, int) { return pdPASS; }
inline hw_timer_t *timerBegin(unsigned)
{
    static hw_timer_t t;
    return &t;
}
inline void timerAttachInterrupt(hw_timer_t *, void (*)()) {}
inline void timerAlarm(hw_timer_t *, unsigned, bool, unsigned) {}
inline void dacWrite(int, uint8_t) {}
inline uint32_t micros() { return 0; }
inline void delay(int) {}
struct SerialPrueba
{
    std::string entrada, salida;
    void begin(int, int = 0, int = 0, int = 0) {}
    void setRxBufferSize(int) {}
    int available() { return (int)entrada.size(); }
    int read()
    {
        char c = entrada.front();
        entrada.erase(0, 1);
        return (unsigned char)c;
    }
    void print(const char *s) { salida += s; }
    void print(char c) { salida += c; }
    void println(const char *s)
    {
        salida += s;
        salida += '\n';
    }
    void println(float v, int decimales)
    {
        char b[80];
        std::snprintf(b, sizeof b, "%.*f\n", decimales, (double)v);
        salida += b;
    }
    void printf(const char *formato, ...)
    {
        char b[1024];
        va_list args;
        va_start(args, formato);
        std::vsnprintf(b, sizeof b, formato, args);
        va_end(args);
        salida += b;
    }
};
inline SerialPrueba Serial, Serial2;
