/*
 * Copyright (c) 2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "uvisor-lib/uvisor-lib.h"
#include "mbed.h"
#include "rtos.h"
#include "main-hw.h"
#include "secure_number.h"
#include <stdint.h>
#include <assert.h>

#warning "Warning: uVisor is superseded by the Secure Partition Manager (SPM) defined in the ARM Platform Security Architecture (PSA). \
          uVisor is deprecated as of Mbed OS 5.10, and being replaced by a native PSA-compliant implementation of SPM."

/* Create ACLs for main box. */
MAIN_ACL(g_main_acl);
/* Enable uVisor. */
UVISOR_SET_MODE_ACL(UVISOR_ENABLED, g_main_acl);
UVISOR_SET_PAGE_HEAP(1 * 1024, 1);

/* Targets with an ARMv7-M MPU needs this space adjustment to prevent a runtime
 * memory overflow error. The code below has been output directly by uVisor. */
#if defined(TARGET_EFM32GG_STK3700) || defined(TARGET_DISCO_F429ZI)
uint8_t __attribute__((section(".keep.uvisor.bss.boxes"), aligned(32))) __boxes_overhead[8064];
#endif

DigitalOut led_red(LED1);
DigitalOut led_green(LED2);
DigitalOut led_blue(LED3);

Serial shared_pc(USBTX, USBRX, SHARED_SERIAL_BAUD);

static uint32_t get_a_number()
{
    static uint32_t number = 425;
    return (number -= 400UL);
}

static void main_async_runner(void)
{
    while (1) {
        uvisor_rpc_result_t result;
        const uint32_t number = get_a_number();
        result = secure_number_set_number(number);

        // ...Do stuff asynchronously here...

        /* Wait for a non-error result synchronously. */
        while (1) {
            int status;
            /* TODO typesafe return codes */
            uint32_t ret;
            status = rpc_fncall_wait(result, UVISOR_WAIT_FOREVER, &ret);
            shared_pc.printf("public  : Attempt to write  0x%08X (%s)\r\n",
                             (unsigned int) number, (ret == 0) ? "granted" : "denied");
            if (!status) {
                break;
            }
        }

        Thread::wait(7000);
    }
}

static void main_sync_runner(void)
{
    while (1) {
        /* Synchronous access to the number. */
        const uint32_t number = secure_number_get_number();
        shared_pc.printf("public  : Attempt to read : 0x%08X (granted)\r\n", (unsigned int) number);

        Thread::wait(7000);
    }
}

int main(void)
{
    shared_pc.printf("\r\n***** uVisor secure number store example *****\r\n");
    led_red = LED_OFF;
    led_blue = LED_OFF;
    led_green = LED_OFF;

    /* Startup a few RPC runners. */
    /* Note: The stack must be at least 1kB since threads will use printf. */
    Thread sync(osPriorityNormal, 1024, NULL);
    sync.start(main_sync_runner);
    Thread async(osPriorityNormal, 1024, NULL);
    async.start(main_async_runner);

    size_t count = 0;
    while (1) {
        /* Spin forever. */
        ++count;
    }

    return 0;
}
