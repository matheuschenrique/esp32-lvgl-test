#ifndef LVGL_APP_H
#define LVGL_APP_H

#include <stdio.h>

#include "lvgl.h"
#include "lvgl_helpers.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_timer.h"

#define LV_TICK_PERIOD_MS 1
#define ARC_WIDTH 5

extern lv_obj_t *temp_arc;
extern lv_obj_t *humidity_arc;
extern lv_obj_t *temp_text;
extern lv_obj_t *humidity_text;
extern lv_obj_t *clock_text;

/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
extern SemaphoreHandle_t xGuiSemaphore;

extern char degree_symbol[];

extern void guiTask(void *pvParameter);

#endif