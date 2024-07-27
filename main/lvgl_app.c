#include "lvgl_app.h"

lv_style_t style;
lv_style_t clock_style;
lv_style_t header_style;

char degree_symbol[] = "\u00B0C";

static void lv_tick_task(void *arg) {
    (void) arg;

    lv_tick_inc(LV_TICK_PERIOD_MS);
}

static void arc_create(void) {
    temp_arc = lv_arc_create(lv_scr_act(), NULL);
    lv_obj_reset_style_list(temp_arc, LV_ARC_PART_BG);
    lv_obj_reset_style_list(temp_arc, LV_ARC_PART_INDIC);

    lv_arc_set_range(temp_arc, 0, 50);

    lv_obj_set_size(temp_arc, 80, 64);
    lv_obj_add_style(temp_arc, LV_ARC_PART_INDIC, &style);
    lv_obj_set_style_local_line_color(temp_arc, LV_ARC_PART_INDIC, LV_STATE_DEFAULT, lv_color_hex(0xa9b7be));

    lv_obj_align(temp_arc, NULL, LV_ALIGN_IN_LEFT_MID, 10, 10);

    humidity_arc = lv_arc_create(lv_scr_act(), NULL);
    lv_obj_reset_style_list(humidity_arc, LV_ARC_PART_BG);
    lv_obj_reset_style_list(humidity_arc, LV_ARC_PART_INDIC);

    lv_arc_set_range(humidity_arc, 0, 100);

    lv_obj_set_size(humidity_arc, 80, 64);
    lv_obj_add_style(humidity_arc, LV_ARC_PART_INDIC, &style);
    lv_obj_set_style_local_line_color(humidity_arc, LV_ARC_PART_INDIC, LV_STATE_DEFAULT, lv_color_hex(0xa9b7be));

    lv_obj_align(humidity_arc, NULL, LV_ALIGN_IN_RIGHT_MID, 0, 10);
}

static void text_create(void) {
    temp_text = lv_label_create(lv_scr_act(), NULL);
    lv_obj_set_size(temp_text, 80, 64);
    lv_style_set_text_font(&header_style, LV_STATE_DEFAULT, &lv_font_montserrat_10);
    lv_style_set_text_color(&header_style, LV_STATE_DEFAULT, lv_color_hex(0x00));
    lv_obj_add_style(temp_text, LV_OBJ_PART_MAIN, &header_style);
    
    lv_label_set_text(temp_text, "--,--");
    lv_obj_align(temp_text, NULL, LV_ALIGN_IN_LEFT_MID, 22, 10);

    humidity_text = lv_label_create(lv_scr_act(), NULL);
    lv_obj_set_size(humidity_text, 80, 64);
    lv_style_set_text_font(&header_style, LV_STATE_DEFAULT, &lv_font_montserrat_10);
    lv_style_set_text_color(&header_style, LV_STATE_DEFAULT, lv_color_hex(0x00));
    lv_obj_add_style(humidity_text, LV_OBJ_PART_MAIN, &header_style);
    
    lv_label_set_text(humidity_text, "--,--");
    lv_obj_align(humidity_text, NULL, LV_ALIGN_IN_RIGHT_MID, -48, 10);

    clock_text = lv_label_create(lv_scr_act(), NULL);
    lv_style_set_text_font(&clock_style, LV_STATE_DEFAULT, &lv_font_montserrat_8);
    lv_style_set_text_color(&clock_style, LV_STATE_DEFAULT, lv_color_hex(0x00));
    lv_obj_add_style(clock_text, LV_OBJ_PART_MAIN, &clock_style);

    lv_label_set_text(clock_text, "HH:MM:SS");
    lv_obj_align(clock_text, NULL, LV_ALIGN_IN_TOP_RIGHT, 10, 0);
}

static void header_text(void) {

    lv_obj_t *temp = lv_label_create(lv_scr_act(), NULL);
    lv_style_set_text_font(&header_style, LV_STATE_DEFAULT, &lv_font_montserrat_8);
    // lv_obj_set_size(temp, 80, 160);
    lv_obj_set_width(temp, 80);
    lv_obj_add_style(temp, LV_OBJ_PART_MAIN, &header_style);

    char buf[50];

    snprintf(buf, sizeof(buf), "Temperature[%s]", degree_symbol);

    lv_label_set_text(temp, buf);
    lv_obj_align(temp, NULL, LV_ALIGN_IN_TOP_LEFT, 5, 20);

    lv_obj_t *hum = lv_label_create(lv_scr_act(), NULL);
    // lv_obj_set_size(hum, 80, 160);
    lv_obj_add_style(hum, LV_OBJ_PART_MAIN, &header_style);

    snprintf(buf, sizeof(buf), "Humidity[%%]");

    lv_label_set_text(hum, buf);
    lv_obj_align(hum, NULL, LV_ALIGN_IN_TOP_RIGHT, -20, 20);
}

static void new_app(void) {
    lv_style_init(&style);
    lv_style_init(&clock_style);
    lv_style_init(&header_style);

    lv_style_set_line_width(&style, LV_STATE_DEFAULT, ARC_WIDTH);
    lv_style_set_line_opa(&style, LV_STATE_DEFAULT, LV_OPA_MAX);
    lv_style_set_line_blend_mode(&style, LV_STATE_DEFAULT, LV_BLEND_MODE_NORMAL);

    header_text();

    arc_create();

    text_create();
}

void guiTask(void *pvParameter) {
    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    lvgl_driver_init();

    lv_color_t* buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 != NULL);
    /* Use double buffered when not working with monochrome displays */
    lv_color_t* buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2 != NULL);
    static lv_disp_buf_t disp_buf;

    uint32_t size_in_px = DISP_BUF_SIZE;

    lv_disp_buf_init(&disp_buf, buf1, buf2, size_in_px);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };

    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));

    /* Create the demo application */
    new_app();

    while (1) {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));

        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
       }
    }

    /* A task should NEVER return */
    free(buf1);
    free(buf2);
    vTaskDelete(NULL);
}