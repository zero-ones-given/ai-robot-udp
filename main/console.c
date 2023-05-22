#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_wifi.h"
#include "esp_vfs_fat.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "lwip/err.h"
#include "lwip/sys.h"

char *USER_PROMPT = " user > ";
char *ROBOT_PROMPT = "robot > ";

char get_user_input_character(char *prompt, bool is_silent) {
    printf(prompt);
    char user_input = 255;
    while (user_input == 255) {
        user_input = fgetc(stdin);
        vTaskDelay(10/portTICK_RATE_MS);
    }
    if (!is_silent) {
        printf("%c\r\n", user_input);
    }
    return user_input;
}

char* get_user_input_string(char *prompt) {
    char* line = NULL;
    while(line == NULL) {
        line = linenoise(prompt);
        vTaskDelay(10/portTICK_RATE_MS);
    }
    return line;
}

void show_help()
{
    printf("robot > no mercy for the helpless \n"
        "robot > just follow the instructions \n"
        "robot > press y to continue \n");
}

void print_sad_message()
{
    char sad_messages[] [64] = {
        "let's pretend it didn't happen",
        "here, another chance for you",
        "we can forget this", 
        "how difficult can it be?", 
        "humans make mistakes",
        "let's try again",
        "hopeless... go again",
    };

    int message_id = esp_random()%(sizeof(sad_messages)/(64*sizeof(char)));
    printf("%s%s\n", ROBOT_PROMPT, sad_messages[message_id]);
}

void show_ip(void* ip_address)
{
    /* For some reason this does not seem to work:
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(ESP_IF_WIFI_STA, &ip_info) == ESP_OK) {
        printf("IP address: " IPSTR "\n", IP2STR(&ip_info.ip));
    } else {
        printf("Could not get IP address.\n");
    }
    As a workaround the ip address is passed along to this task
    */
    printf("%sIP address: %s\n", ROBOT_PROMPT, (char*)ip_address);
}

void configure_wifi()
{
    char* ssid = get_user_input_string("WiFi SSID: ");
    printf("\n");
    char* password = get_user_input_string("WiFi password: ");

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    strcpy((char*)wifi_config.sta.ssid, ssid);
    strcpy((char*)wifi_config.sta.password, password);
    /*esp_err_t error = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    switch (error) {
        case ESP_OK:
            printf("\nThe configration has been saved.\n");
            break;
        case ESP_ERR_WIFI_NOT_INIT:
            printf("\nWiFi is not initialized by esp_wifi_init\n");
            break;
        case ESP_ERR_INVALID_ARG:
            printf("\ninvalid argument\n");
            break;
        case ESP_ERR_WIFI_IF:
            printf("\ninvalid interface\n");
            break;
        case ESP_ERR_WIFI_MODE:
            printf("\ninvalid mode\n");
            break;
        case ESP_ERR_WIFI_PASSWORD:
            printf("\ninvalid password\n");
            break;
        case ESP_ERR_WIFI_NVS:
            printf("\nWiFi internal NVS error\n");
            break;
        default:
            printf("\nOther error: %d\n", error);
            break;
    }
    if (error != ESP_OK) {
        printf("\nThe configration could not be saved.\n");
    }*/
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    linenoiseFree(ssid);
    linenoiseFree(password);

    get_user_input_character("Presse any key to reboot.", false);
    esp_restart();
}

void initialize_console()
{
    fflush(stdout);
    fsync(fileno(stdout));
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    const uart_config_t uart_config = {
            .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .source_clk = UART_SCLK_REF_TICK,
    };

    ESP_ERROR_CHECK( uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0) );
    ESP_ERROR_CHECK( uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config) );
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

    esp_console_config_t console_config = {
            .max_cmdline_args = 8,
            .max_cmdline_length = 256,
    };
    ESP_ERROR_CHECK( esp_console_init(&console_config) );

    linenoiseSetMultiLine(1);
    /*linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);
    linenoiseHistorySetMaxLen(100);*/
}

void console_task(void *ip_address)
{
    initialize_console();

    // wait for a second for the other tasks to stop outputting init messages
    vTaskDelay(1000/portTICK_RATE_MS);

    // ensure the user sees the welcome screen by waiting for key press
    get_user_input_character("Press any key to start terminal", true);

    int probe_status = linenoiseProbe();
    if (probe_status) {
        printf("\n"
               "Your terminal application does not support escape sequences.\n"
               "Line editing and history features are disabled.\n"
               "On Windows, try using Putty instead.\n");
        linenoiseSetDumbMode(1);
    }

    printf("\n"
           "robot > ***** MicroInvaderBot Terminal ***** \n"
           "robot > ==================================== \n"
           "robot > h: help \n"
           "robot > i: show IP address \n"
           "robot > w: configure Wi-Fi \n"
           "robot > b: configure Bluetooth \n");

    char selection;
    while(true)
    {
        selection = get_user_input_character(USER_PROMPT, false);

        switch(selection)
        {
            case 'h':
                show_help();
                break;
            case 'i':
                show_ip(ip_address);
                break;
            case 'w':
                configure_wifi();
                break;
            case 'b':
                printf("%sUnfortunately Bluetooth configuration has not been implemented yet.\n", ROBOT_PROMPT);
                break;
            case 'y':
                printf("%snice\n", ROBOT_PROMPT);
                break;
            default:
                print_sad_message();
        }
    }
}
