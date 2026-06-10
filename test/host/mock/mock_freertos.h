#pragma once

#include <stdint.h>

// Tests PC = mono-thread : pas de vrai RTOS, mutex = no-op
typedef int *SemaphoreHandle_t;

#define xSemaphoreCreateMutex()              ((SemaphoreHandle_t)0)
#define xSemaphoreCreateRecursiveMutex()     ((SemaphoreHandle_t)0)
#define xSemaphoreTake(m, t)                 ((void)0)
#define xSemaphoreGive(m)                    ((void)0)
#define xSemaphoreTakeRecursive(m, t)        ((void)0)
#define xSemaphoreGiveRecursive(m)           ((void)0)

#define portMAX_DELAY         0xFFFFFFFFUL
#define vTaskDelay(x)         ((void)0)
#define xTaskCreate(f,n,s,p,pr,h) ((void)0)
#define pdMS_TO_TICKS(x)      (x)
#define portMUX_TYPE          int
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(m) ((void)0)
#define portEXIT_CRITICAL(m)  ((void)0)
#define portENTER_CRITICAL_ISR(m) ((void)0)
#define portEXIT_CRITICAL_ISR(m)  ((void)0)
#define IRAM_ATTR
