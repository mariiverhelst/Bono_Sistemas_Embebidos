
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/adc.h"
#include "esp_timer.h"


#define PIN_LED_GREEN   2
#define PIN_LED_RED     5

#define PIN_BTN_RIGHT   0
#define PIN_BTN_LEFT    22

#define PIN_DIR_A       19
#define PIN_DIR_B       21

#define PIN_PWM_LOW     18   // NPN (2N2222A)
#define PIN_PWM_HIGH    23   // PNP (2N3906)

#define PIN_POTENTIOMETER ADC1_CHANNEL_6

// display
int segment_pins[7]   = {12,13,14,15,25,26,27};
int digit_select_pins[3] = {4,32,33};

// variables
volatile int current_display_digit = 0;
volatile int display_value = 0;
volatile int pwm_duty = 0;

// Tabla de números (7 segmentos)
const uint8_t digit_map[10][7] = {
    {1,1,1,1,1,1,0},{0,1,1,0,0,0,0},{1,1,0,1,1,0,1},
    {1,1,1,1,0,0,1},{0,1,1,0,0,1,1},{1,0,1,1,0,1,1},
    {1,0,1,1,1,1,1},{1,1,1,0,0,0,0},{1,1,1,1,1,1,1},
    {1,1,1,1,0,1,1}
};

// funciones
void display_multiplex_callback(void* arg)
{
    // Apagar todos los dígitos
    for(int i = 0; i < 3; i++)
        gpio_set_level(digit_select_pins[i], 0);

    int digit_value;

    if(current_display_digit == 0) 
        digit_value = display_value % 10;
    else if(current_display_digit == 1) 
        digit_value = (display_value / 10) % 10;
    else 
        digit_value = (display_value / 100) % 10;

    // Encender segmentos
    for(int i = 0; i < 7; i++)
        gpio_set_level(segment_pins[i], !digit_map[digit_value][i]);

    // Activar dígito actual
    gpio_set_level(digit_select_pins[current_display_digit], 1);

    current_display_digit = (current_display_digit + 1) % 3;
}

void pwm_update_callback(void* arg)
{
    static int pwm_counter = 0;

    pwm_counter++;
    if(pwm_counter >= 255) pwm_counter = 0;

    // Transistor NPN (directo)
    gpio_set_level(PIN_PWM_LOW, (pwm_counter < pwm_duty));

    // Transistor PNP (invertido)
    gpio_set_level(PIN_PWM_HIGH, !(pwm_counter < pwm_duty));
}


void app_main(void)
{
    // Configurar salidas
    int output_pins[] = {
        PIN_LED_GREEN, PIN_LED_RED,
        PIN_DIR_A, PIN_DIR_B,
        PIN_PWM_LOW, PIN_PWM_HIGH
    };

    for(int i = 0; i < 6; i++){
        gpio_reset_pin(output_pins[i]);
        gpio_set_direction(output_pins[i], GPIO_MODE_OUTPUT);
    }

    // Segmentos del display
    for(int i = 0; i < 7; i++){
        gpio_reset_pin(segment_pins[i]);
        gpio_set_direction(segment_pins[i], GPIO_MODE_OUTPUT);
    }

    // Selección de dígitos
    for(int i = 0; i < 3; i++){
        gpio_reset_pin(digit_select_pins[i]);
        gpio_set_direction(digit_select_pins[i], GPIO_MODE_OUTPUT);
    }

    // Botones (pull-up interno)
    gpio_set_direction(PIN_BTN_RIGHT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BTN_RIGHT, GPIO_PULLUP_ONLY);

    gpio_set_direction(PIN_BTN_LEFT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BTN_LEFT, GPIO_PULLUP_ONLY);

    // ADC (potenciómetro)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(PIN_POTENTIOMETER, ADC_ATTEN_DB_12);

    // Timer display
    esp_timer_handle_t display_timer;
    esp_timer_create_args_t display_timer_args = {
        .callback = &display_multiplex_callback
    };
    esp_timer_create(&display_timer_args, &display_timer);
    esp_timer_start_periodic(display_timer, 2000);

    // Timer PWM
    esp_timer_handle_t pwm_timer;
    esp_timer_create_args_t pwm_timer_args = {
        .callback = &pwm_update_callback
    };
    esp_timer_create(&pwm_timer_args, &pwm_timer);

    // 50 µs → PWM
    esp_timer_start_periodic(pwm_timer, 50);

  
    while(1)
    {
        int adc_read = adc1_get_raw(PIN_POTENTIOMETER);
        pwm_duty = adc_read / 16;

        int button_right_pressed = !gpio_get_level(PIN_BTN_RIGHT);
        int button_left_pressed  = !gpio_get_level(PIN_BTN_LEFT);

        // Dirección del motor + LEDs
        if(button_right_pressed){
            gpio_set_level(PIN_DIR_A, 1);
            gpio_set_level(PIN_DIR_B, 0);

            gpio_set_level(PIN_LED_GREEN, 1);
            gpio_set_level(PIN_LED_RED, 0);
        }
        else if(button_left_pressed){
            gpio_set_level(PIN_DIR_A, 0);
            gpio_set_level(PIN_DIR_B, 1);

            gpio_set_level(PIN_LED_GREEN, 0);
            gpio_set_level(PIN_LED_RED, 1);
        }

        // Valor mostrado en display (%)
        display_value = abs(((pwm_duty * 100) / 255) - 100);

        printf("ADC:%d PWM:%d BTN_R:%d BTN_L:%d\n",
               adc_read, pwm_duty,
               button_right_pressed, button_left_pressed);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


