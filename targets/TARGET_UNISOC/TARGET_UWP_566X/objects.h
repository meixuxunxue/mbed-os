/* mbed Microcontroller Library
 * Copyright (c) 2006-2018 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MBED_OBJECTS_H
#define MBED_OBJECTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uwp_type.h"
#include "cmsis.h"
#include "PinNames.h"
#include "PeripheralNames.h"
#include "gpio_objects.h"

struct gpio_irq_s {
	int      flags;
	u32_t    port_base;
	u32_t    pin_enabled;
};

struct timer_s{
    uint32_t base;
    TIMER_MODE_T mode;
    TIMER_WID_MODE_T wid_mode;
    uint32_t clk_freq;
    uint32_t load_count;
    uint32_t load_count_h;
    uint32_t irq_op;
};

#ifdef __cplusplus
}
#endif

#endif
