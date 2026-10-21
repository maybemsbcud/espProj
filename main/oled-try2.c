#include <stdint.h>
#include <stdio.h>
#include "driver/i2c_master.h"
#include "driver/i2c_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "hal/i2c_types.h"
#include "portmacro.h"
#include "soc/clk_tree_defs.h"
#include "soc/gpio_num.h"
#include "u8g2.h"
#include "string.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include <stdbool.h>
#include "esp_log.h"
#include <stddef.h>
#include "freertos/queue.h"

#define OLED_TAG "OLED"
#define MAIN_TAG "MAIN"
#define KEYPAD_TAG "KEYPAD"

QueueHandle_t keypad_queue;
QueueHandle_t oled_queue;
QueueHandle_t main_queue;


typedef enum{
    STATE_PAGE1,
    STATE_PAGE2,
    
}SystemState;

SystemState app_state = STATE_PAGE1;
const gpio_num_t ROW_PINS[] = {GPIO_NUM_9, GPIO_NUM_10};
const gpio_num_t COL_PINS[] = {GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_8};

const int NUM_ROWS = 2;
const int NUM_COLS = 3;


const char* stateToString(int state);




void init_Cols(){
    uint64_t col_pins = 0;
    for(int i = 0; i < NUM_COLS; i++){
        col_pins |= (1ULL << COL_PINS[i]);
    }
    gpio_config_t col_conf = {
        .pin_bit_mask = col_pins,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&col_conf);
}
void init_Rows(){
    uint64_t row_pins_map = 0;
    for(int i = 0; i<NUM_ROWS; i++){
        row_pins_map |= (1ULL << ROW_PINS[i]);
    }
    gpio_config_t row_conf = {
        .pin_bit_mask = row_pins_map,
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&row_conf);
    for(int i = 0; i< NUM_ROWS; i++){
        gpio_set_level(ROW_PINS[i],1);
    }
}
bool buttonstatus[2][3];

void scan_matrix(){
    for (int row = 0; row< NUM_ROWS; row++){
        gpio_set_level(ROW_PINS[row], 0);
        esp_rom_delay_us(2);
        for(int col = 0; col<NUM_COLS; col++){
            if(gpio_get_level(COL_PINS[col]) == 0){
                if(buttonstatus[row][col] == false){
                    buttonstatus[row][col] = true;
                    int id_button = (row*NUM_COLS) +col +1;
                    if(xQueueSend(keypad_queue,&id_button,pdMS_TO_TICKS(10)) != pdPASS){
                        ESP_LOGW(KEYPAD_TAG, "queue is full");
                    }
                }
            }
            else{
                buttonstatus[row][col] = false;
            }
        }  
        gpio_set_level(ROW_PINS[row], 1);      
    }
}
void keybord_init(){
    init_Rows();
    init_Cols();
}

void task_keypad(void *pvParameters){
    keybord_init();
    while(1){
        vTaskDelay(pdMS_TO_TICKS(20));
        scan_matrix();
        //ESP_LOGI(KYEPAD_TAG, "its working");
        //uint8_t dummy_key;
        //for(int i = 0; i < 6; i++){
            //dummy_key = i;
            
        //vTaskDelay(pdMS_TO_TICKS(1000));
    }
    //vTaskDelay(pdMS_TO_TICKS(100));
}

void task_oled(void *pvParameters){
    while(1){
        //ESP_LOGI(OLED_TAG, "its working");
        vTaskDelay(pdMS_TO_TICKS(1000));
        int received_status;
        if((xQueueReceive(oled_queue, &received_status, portMAX_DELAY)) == pdTRUE){
            ESP_LOGI(OLED_TAG,"received state %d", received_status);
        
            switch (received_status) {
                case STATE_PAGE1:
                ESP_LOGI(OLED_TAG, "showing status %s", stateToString(received_status));
                break;

                case STATE_PAGE2:
                ESP_LOGI(OLED_TAG, "showing status %s", stateToString(received_status));
                break;
            
            }
        }
    }
}
void task_main(void *pvParameters){
    while(1){
        //ESP_LOGI(MAIN_TAG, "it work");
        //vTaskDelay(pdMS_TO_TICKS(500));
        uint8_t received_key;
        if((xQueueReceive(keypad_queue, &received_key, portMAX_DELAY)) == pdTRUE){
            ESP_LOGI(MAIN_TAG, "received key %d", received_key);
        }
        
        switch (app_state) {
            case STATE_PAGE1:
                switch (received_key) {
                    case 1:
                    app_state = STATE_PAGE2;
                    ESP_LOGI(MAIN_TAG, "you move from STATE_PAGE1 to %s", stateToString(app_state));
                    if(xQueueSend(oled_queue, &app_state, pdMS_TO_TICKS(100)) != pdPASS){
                        ESP_LOGW(MAIN_TAG, "queue is full");
                    }
                    break;
                    
                    default:
                    break;
                
                }
            break;
            
            case STATE_PAGE2:
                switch (received_key) {
                    case 1:
                    app_state = STATE_PAGE1;
                    ESP_LOGI(MAIN_TAG, "you move from STATE_PAGE2 to %s", stateToString(app_state));
                    if(xQueueSend(oled_queue, &app_state, pdMS_TO_TICKS(100)) != pdPASS){
                        ESP_LOGW(MAIN_TAG, "queue is full");
                    }
                    break;
                
                }
            default:
            break;
        }
    }
}

const char* stateToString(int state){
    switch (state) {
        case 0: return "STATE_PAGE1";
        case 1: return "STATE_PAGE2";
        default: return "UNKNOWN";
    }
}

// 1. Глобальний хендл для екрана, щоб callback міг його бачити
i2c_master_dev_handle_t oled_dev_handle;

// 2. Callback для роботи з часом та затримками
uint8_t u8g2_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            break;
        case U8X8_MSG_DELAY_10MICRO:
            esp_rom_delay_us(10);
            break;
        case U8X8_MSG_DELAY_100NANO:
            esp_rom_delay_us(1);
            break;
    }
    return 1; // 1 означає успіх
}

// 3. Callback для передачі байтів через твій новий I2C драйвер
uint8_t u8g2_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    static uint8_t buffer[128]; // Тимчасовий буфер для пакету даних
    static uint8_t buf_idx = 0;

    switch(msg) {
        case U8X8_MSG_BYTE_SEND: {
            // Бібліотека дає шматок даних, ми складаємо його в буфер
            uint8_t *data = (uint8_t *)arg_ptr;
            for (int i = 0; i < arg_int; i++) {
                buffer[buf_idx++] = data[i];
            }
            break;
        }
        case U8X8_MSG_BYTE_START_TRANSFER: {
            // Початок транзакції - скидаємо індекс буфера
            buf_idx = 0;
            break;
        }
        case U8X8_MSG_BYTE_END_TRANSFER: {
            // Бібліотека каже "відправляй". Використовуємо твій драйвер.
            // oled_dev_handle вже знає адресу 0x3C, тому передаємо тільки дані.
            i2c_master_transmit(oled_dev_handle, buffer, buf_idx, -1);
            break;
        }
    }
    return 1;
}
void app_main(void) {
    /*
    // === ТВОЯ ІНІЦІАЛІЗАЦІЯ I2C ===
    i2c_master_bus_config_t bus_config = {
        .i2c_port = -1,
        .sda_io_num = 16,
        .scl_io_num = 15,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    i2c_new_master_bus(&bus_config, &bus_handle);

    i2c_device_config_t dev_conf = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x3C,
        .scl_speed_hz = 400000,
    };
    // Записуємо хендл у глобальну змінну, яку бачить HAL
    i2c_master_bus_add_device(bus_handle, &dev_conf, &oled_dev_handle);

    // === ІНІЦІАЛІЗАЦІЯ U8G2 ===
    u8g2_t u8g2; // Головна структура стану екрана

    // Кажемо u8g2: "У тебе екран SSD1306 по I2C, і ось функції, якими ти маєш користуватися"
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8g2_i2c_byte_cb, u8g2_gpio_and_delay_cb);
    
    u8g2_InitDisplay(&u8g2);     // Відправляє команди ввімкнення екрана
    u8g2_SetPowerSave(&u8g2, 0); // Виводить з режиму сну

    // === МАЛЮВАННЯ ===
    //u8g2_ClearBuffer(&u8g2); // Очищаємо відеобуфер у пам'яті ESP32
    
    // Вибираємо шрифт (їх там тисячі, це стандартний середній)
    u8g2_SetFont(&u8g2, u8g2_font_10x20_t_cyrillic); 
    
    // Пишемо текст. (Координати: X=15 від лівого краю, Y=35 від верху до базової лінії тексту)
    //u8g2_DrawUTF8(&u8g2, 0, 35, "твоя мать");
    //u8g2_DrawUTF8(&u8g2,20,45, "шлюха");

    // Виштовхуємо намальований кадр на фізичний екран
    //u8g2_SendBuffer(&u8g2);
    bool screen = false;
    keybordScan();
    while(1) {
        scan_matrix();
        if(buttonstatus[0][0]){
            printf("hui");
            if(!screen){
            u8g2_ClearBuffer(&u8g2);
            u8g2_DrawUTF8(&u8g2,0,35,"сосі");
            u8g2_SendBuffer(&u8g2);
            screen = true;
            }
        }
        else{
            u8g2_ClearBuffer(&u8g2);
            u8g2_SendBuffer(&u8g2);
            screen = false;
        }
             
        vTaskDelay(pdMS_TO_TICKS(20));
    }*/
    keypad_queue = xQueueCreate(10, sizeof(uint8_t));
    if(keypad_queue == NULL){
        ESP_LOGE(KEYPAD_TAG,"failed to create queue, out of chep memory");
        return;
    }
    oled_queue = xQueueCreate(5, sizeof(int));
    if(oled_queue == NULL){
        ESP_LOGE(KEYPAD_TAG,"failed to create queue, out of chep memory");
        return;
    }
    /*
     main_queue = xQueueCreate(10, sizeof(uint8_t));
    if(main_queue == NULL){
        ESP_LOGE(KYEPAD_TAG,"failed to create queue, out of cheap memory");
        return;
    }   */



   BaseType_t keypad_task_cr = xTaskCreate(task_keypad, "keypad_task_name", 2048, NULL, 5, NULL);
   if(keypad_task_cr != pdPASS){
        ESP_LOGE(KEYPAD_TAG,"task wasn't created, memory overflow");
   }
    BaseType_t main_task_cr = xTaskCreate(task_main, "main_task_name)", 2048, NULL, 5, NULL);
    if(main_task_cr != pdPASS){
         ESP_LOGE(MAIN_TAG, "task wasn't created, memory overflow");
   }
    BaseType_t oled_task_cr= xTaskCreate(task_oled, "oled_task_name", 2048, NULL, 5, NULL);
    if(oled_task_cr != pdPASS){
         ESP_LOGE(OLED_TAG, "task wasn't created, memory overflow");
   }
}