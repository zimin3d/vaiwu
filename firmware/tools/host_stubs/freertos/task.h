/* Host-check stub. */
#pragma once
#include "freertos/FreeRTOS.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef void* TaskHandle_t;
typedef void (*TaskFunction_t)(void*);
BaseType_t xTaskCreate(TaskFunction_t fn, const char* name, uint32_t stackDepth,
    void* params, UBaseType_t priority, TaskHandle_t* handle);
void vTaskDelete(TaskHandle_t handle);
void vTaskDelay(TickType_t ticks);
#ifdef __cplusplus
}
#endif
