#include "freertos/task.h"

#include <stdlib.h>
#include <pthread.h>

typedef struct {
    TaskFunction_t fn;
    void *arg;
} host_task_boot_t;

static void *task_bootstrap(void *arg)
{
    host_task_boot_t *boot = (host_task_boot_t *)arg;
    TaskFunction_t fn = boot->fn;
    void *fn_arg = boot->arg;
    free(boot);

    fn(fn_arg);
    return NULL;
}

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t task_fn,
                                   const char *name,
                                   uint32_t stack_depth,
                                   void *arg,
                                   UBaseType_t priority,
                                   TaskHandle_t *out_handle,
                                   BaseType_t core_id)
{
    (void)name;
    (void)stack_depth;
    (void)priority;
    (void)core_id;

    if (!task_fn) return pdFAIL;

    host_task_boot_t *boot = calloc(1, sizeof(*boot));
    if (!boot) return pdFAIL;
    boot->fn = task_fn;
    boot->arg = arg;

    pthread_t *thr = calloc(1, sizeof(*thr));
    if (!thr) {
        free(boot);
        return pdFAIL;
    }

    if (pthread_create(thr, NULL, task_bootstrap, boot) != 0) {
        free(thr);
        free(boot);
        return pdFAIL;
    }

    pthread_detach(*thr);

    if (out_handle) {
        *out_handle = (TaskHandle_t)thr;
    } else {
        free(thr);
    }

    return pdPASS;
}

void vTaskDelete(TaskHandle_t task_handle)
{
    (void)task_handle;
    pthread_exit(NULL);
}

int xPortGetCoreID(void)
{
    return 1;
}
