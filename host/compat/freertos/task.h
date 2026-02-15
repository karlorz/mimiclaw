#pragma once

#include "freertos/FreeRTOS.h"

typedef void (*TaskFunction_t)(void *);

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t task_fn,
                                   const char *name,
                                   uint32_t stack_depth,
                                   void *arg,
                                   UBaseType_t priority,
                                   TaskHandle_t *out_handle,
                                   BaseType_t core_id);

void vTaskDelete(TaskHandle_t task_handle);
int xPortGetCoreID(void);
