/*
 * @Author: HoGC
 * @Date: 2022-07-25 21:29:17
 * @Last Modified time: 2022-07-25 21:29:17
 */
#include <stdio.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "driver/uart.h"
#include "esp_vfs_dev.h"
#include "linenoise/linenoise.h"

#include "berry.h"

static char *TAG = "main";

#define CONSOLE_UART_CHANNEL    UART_NUM_0

static bbool is_multline(bvm *vm)
{
    if(be_top(vm) >= 1){
        const char *msg = be_tostring(vm, -1);
        size_t len = strlen(msg);
        if (len > 5) { /* multi-line text if the error message is 'EOS' at the end */
            return !strcmp(msg + len - 5, "'EOS'");
        }
    }
    return bfalse;
}

static int try_return(bvm *vm, const char *line)
{
    int res, idx;
    line = be_pushfstring(vm, "return (%s)", line);
    idx = be_absindex(vm, -1); /* get the source text absolute index */
    res = be_loadbuffer(vm, "stdin", line, strlen(line)); /* compile line */
    be_remove(vm, idx); /* remove source string */
    return res;
}

static int call_script(bvm *vm)
{
    int res = be_pcall(vm, 0); /* call the main function */
    switch (res) { 
    case BE_OK: /* execution succeed */
        if (!be_isnil(vm, -1)) { /* print return value when it's not nil */
            be_dumpvalue(vm, -1);
        }
        be_pop(vm, 1); /* pop the result value */
        break;
    case BE_EXCEPTION: /* vm run error */
        be_dumpexcept(vm);
        be_pop(vm, 1); /* pop the function value */
        break;
    default: /* BE_EXIT or BE_MALLOC_FAIL */
        return res;
    }
    return 0;
}

int repl_line(bvm *vm, char *line)
{
    int res;
    if(is_multline(vm)) {   
        be_pop(vm, 2); /* pop exception values */
        be_pushfstring(vm, "\n%s", line);
        be_strconcat(vm, -2);
        be_pop(vm, 1); /* pop new line */
    }else{
        res = try_return(vm, line);
        if (be_getexcept(vm, res) == BE_SYNTAX_ERROR) {
            be_pop(vm, 2); /* pop exception values */
            be_pushstring(vm, line);
        }else{
            goto RUN;
        }
    }
    const char *src = be_tostring(vm, -1); /* get source code */
    int idx = be_absindex(vm, -1); /* get the source text absolute index */
    /* compile source line */
    res = be_loadbuffer(vm, "stdin", src, strlen(src));
    if (!res || !is_multline(vm)) {
        be_remove(vm, idx); /* remove source code */
        goto RUN;
    }

    return res;

RUN:         
    if (res == BE_MALLOC_FAIL){
        return res;
    }
    if(res){
        be_dumpexcept(vm);
    }else{
        res = call_script(vm);
        if (res) {
            return res == BE_EXIT ? be_toindex(vm, -1) : res;
        }
    }
    return res;
}

void repl_task(void *arg){

    // Initialize VFS & UART so we can use std::cout/cin
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Install UART driver for interrupt-driven reads and writes */
    ESP_ERROR_CHECK( uart_driver_install( (uart_port_t)CONSOLE_UART_CHANNEL, 256, 0, 0, NULL, 0) );
    
    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONSOLE_UART_CHANNEL);
    esp_vfs_dev_uart_port_set_rx_line_endings(CONSOLE_UART_CHANNEL, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_port_set_tx_line_endings(CONSOLE_UART_CHANNEL, ESP_LINE_ENDINGS_CRLF);

    int probe_status = linenoiseProbe();
    if (probe_status) {
        /* zero indicates success */
        linenoiseSetDumbMode(1);
        printf("\r\n"
               "Your terminal application does not support escape sequences.\n\n"
               "Line editing and history features are disabled.\n\n"
               "On Windows, try using Putty instead.\r\n");
               
    }else{
        printf("\033[2J\033[1H");
    }


    char *line = NULL;
    bvm *vm = be_vm_new();
    while (is_multline(vm)?(line = linenoise("berry>> ")) != NULL:(line = linenoise("berry>  ")) != NULL){
        if (line[0] != '\0' && line[0] != '/') {
            repl_line(vm, line);
            linenoiseHistoryAdd(line);
            linenoiseFree(line);
        }
    }
    be_vm_delete(vm);

    vTaskDelete(NULL);
}

void app_main(void)
{
	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 8,
		.format_if_mount_failed =true
	};

	esp_err_t ret =esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",esp_err_to_name(ret));
		}
	}

    xTaskCreate(repl_task, "repl_task", 4096, NULL, 2, NULL);
}
