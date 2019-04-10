/*
 * Copyright (C) 2016-2018 Unwired Devices LLC <info@unwds.com>

 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software
 * is furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * @defgroup
 * @ingroup
 * @brief
 * @{
 * @file	umdk-pwm.c
 * @brief       umdk-pwm module implementation
 * @author      Mikhail Perkov
 * @author		Eugene Ponomarev
 */

#ifdef __cplusplus
extern "C" {
#endif

/* define is autogenerated, do not change */
#undef _UMDK_MID_
#define _UMDK_MID_ UNWDS_PWM_MODULE_ID

/* define is autogenerated, do not change */
#undef _UMDK_NAME_
#define _UMDK_NAME_ "pwm"

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "board.h"
#include "periph/gpio.h"

#include "umdk-ids.h"
#include "unwds-common.h"
#include "include/umdk-pwm.h"

#define ENABLE_DEBUG    (1)
#include "debug.h"

static uwnds_cb_t *callback;

static void set_pwm_value(gpio_t pin, uint32_t freq, uint8_t duty, uint16_t pulses) {
    for (uint32_t pwm_dev = 0; pwm_dev < PWM_NUMOF; pwm_dev++) {
        for (uint32_t pwm_chan = 0; pwm_chan < pwm_channels(pwm_dev); pwm_chan++) {
            if (pwm_config[pwm_dev].chan[pwm_chan].pin == pin) {
                
                printf("[umdk-" _UMDK_NAME_ "] PWM %lu:%lu", pwm_dev, pwm_chan);
                
                if (freq > 0) {
                    /* initialize PWM */
                    pwm_init(pwm_dev, PWM_LEFT, freq, 100);
                    /* set PWM duty cycle */
                    pwm_set(pwm_dev, pwm_chan, duty);
                    
                    /* start PWM */
                    if (pulses == 0) {
                        pwm_start(pwm_dev, pwm_chan);
                    } else {
                        pwm_pulses(pwm_dev, pwm_chan, pulses);
                    }
                    
                    puts(" started");
                    
                    DEBUG("F %lu Hz, D %d %%, N %d", freq, duty, pulses);
                } else {
                    pwm_stop(pwm_dev, pwm_chan);
                    
                    puts(" stopped");
                }
                
                return;
            }
        }
    }
}

static void reply_fail(module_data_t *reply) {
    if (reply) {
        reply->length = 2;
        reply->data[0] = _UMDK_MID_;
        reply->data[1] = UMDK_PWM_FAIL;
    }
}

static void reply_ok(module_data_t *reply) {
    if (reply) {
        reply->length = 2;
        reply->data[0] = _UMDK_MID_;
        reply->data[1] = UMDK_PWM_COMMAND;
    }
}

static int umdk_pwm_shell_cmd(int argc, char **argv) {
    if (argc < 5) {
        puts (_UMDK_NAME_ " set <pin> <frequency> <duty> <pulses>");
        return 0;
    }
    
    char *cmd = argv[1];
	
    if (strcmp(cmd, "set") == 0) {
        uint8_t pin = atoi(argv[2]);
        uint16_t freq = atoi(argv[3]);
        convert_to_be_sam((void *)&freq, sizeof(freq));
        
        uint8_t duty = atoi(argv[4]);
        uint16_t pulses = atoi(argv[5]);
        convert_to_be_sam((void *)&pulses, sizeof(pulses));
        
        module_data_t cmd;
        
        cmd.length = 7;
        cmd.data[0] = UMDK_PWM_COMMAND;
        cmd.data[1] = pin;
        cmd.data[2] = freq & 0xFF;
        cmd.data[3] = freq >> 8;
        cmd.data[4] = duty;
        cmd.data[2] = pulses & 0xFF;
        cmd.data[3] = pulses >> 8;
        
        umdk_pwm_cmd(&cmd, NULL);
    }
    
    return 1;
}

bool umdk_pwm_cmd(module_data_t *cmd, module_data_t *reply) {
    umdk_pwm_cmd_t c = cmd->data[0];
    
    if (cmd->length < 7) {
        puts("[umdk-" _UMDK_NAME_ "] Incorrect command");
        reply_fail(reply);
        return true;
    }

    switch (c) {
        case UMDK_PWM_COMMAND: {
            uint8_t pwm_pin = cmd->data[1];
            bool pwm_pin_correct = false;
            
            for (uint32_t k = 0; k < PWM_NUMOF; k++) {
                for (uint32_t m = 0; m < pwm_channels(k); m++) {
                    if (pwm_config[k].chan[m].pin == unwds_gpio_pin(pwm_pin)) {
                        pwm_pin_correct = true;
                        break;
                    }
                }
                if (pwm_pin_correct) {
                    break;
                }
            }
            
            if (!pwm_pin_correct) {
                printf("[umdk-" _UMDK_NAME_ "] PWM not available on pin %u\n", pwm_pin);
                reply_fail(reply);
                return true;
            }
            
            
            uint16_t pwm_freq = cmd->data[2] | cmd->data[3] << 8;
            convert_from_be_sam((void *)&pwm_freq, sizeof(pwm_freq));
            
            uint8_t pwm_duty = cmd->data[4];
            
            uint16_t pwm_pulses = cmd->data[5] | cmd->data[6] << 8;
            convert_from_be_sam((void *)&pwm_pulses, sizeof(pwm_pulses));
            
            /* setup and start PWM channel */
			set_pwm_value(unwds_gpio_pin(pwm_pin), pwm_freq, pwm_duty, pwm_pulses);

			reply_ok(reply);
			return true;
        }
		default:
        break;
    }

    return false;
}

void umdk_pwm_init(uwnds_cb_t *event_callback) {
    callback = event_callback;
    
    unwds_add_shell_command(_UMDK_NAME_, "type '" _UMDK_NAME_ "' for commands list", umdk_pwm_shell_cmd);
    
    puts("[umdk-" _UMDK_NAME_ "] PWM is ready");
}


#ifdef __cplusplus
}
#endif
