#include "motor_control.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/mcpwm.h" 

#include "math.h"
#include "stdio.h"

float motor_value_left = 0;
float motor_value_right = 0;

void motor_control_set_values(float motor_left, float motor_right)
{
    motor_value_left = motor_left;
    motor_value_right = motor_right;
}

void gpio_init()
{
    printf("Motor control GPIO init\n");
    gpio_config_t io_conf;
    
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = MOTOR_DIR_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    
    gpio_config(&io_conf);
}

void pwm_gpio_config()
{
    printf("MCPWM GPIO init\n");
    mcpwm_pin_config_t pin_config = {
        .mcpwm0a_out_num = RIGHT_MOTOR_PWM,
        .mcpwm0b_out_num = LEFT_MOTOR_PWM,
    };
    
    mcpwm_set_pin(MCPWM_UNIT_0, &pin_config);
}

void mcpwm_config()
{
    printf("MCPWM init\n");
    mcpwm_config_t pwm_config;
    
    // low frequencies work better for driving the motor at low speeds
    pwm_config.frequency = 20;     // frequency = 20Hz
    pwm_config.cmpr_a = 0.0;       // initial duty cycle 0
    pwm_config.cmpr_b = 0.0;       // initial duty cycle 0
    pwm_config.counter_mode = MCPWM_UP_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;
    
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config); 
}

void motor_control_setup()
{
    gpio_init();
    pwm_gpio_config();
    mcpwm_config();
}

void set_motor_directions()
{
    gpio_set_level(RIGHT_MOTOR_DIR_FORWARD, motor_value_right >= 0);
    gpio_set_level(RIGHT_MOTOR_DIR_REVERSE, motor_value_right < 0);

    gpio_set_level(LEFT_MOTOR_DIR_FORWARD, motor_value_left >= 0);
    gpio_set_level(LEFT_MOTOR_DIR_REVERSE, motor_value_left < 0);
}

void set_motor_pwm(int motor, float pwm)
{
    float absolute_pwm = fabs(pwm);
    // TODO: protect motor_x_pwm variables with mutexes
    if (absolute_pwm > 100)
    {
        printf("ERROR: Motor duty cycle cannot exceed 100 \n"); // TODO: make this a proper error log
        absolute_pwm = 0;
    }
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, motor, absolute_pwm);
}

void motor_control_task(void *args)
{
    while (1)
    {
        set_motor_directions();
        set_motor_pwm(MCPWM_OPR_A, motor_value_right);
        set_motor_pwm(MCPWM_OPR_B, motor_value_left);
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
