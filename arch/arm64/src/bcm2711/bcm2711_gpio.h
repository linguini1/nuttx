/***************************************************************************
 * arch/arm64/src/bcm2711/bcm2711_gpio.h
 *
 * Author: Matteo Golin <matteo.golin@gmail.com>
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ***************************************************************************/

/***************************************************************************
 * Included Files
 ***************************************************************************/

#ifndef __ARCH_ARM64_SRC_BCM2711_BCM2711_GPIO_H
#define __ARCH_ARM64_SRC_BCM2711_BCM2711_GPIO_H

#include <nuttx/config.h>

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "arm64_arch.h"
#include "arm64_internal.h"
#include "hardware/bcm2711_gpio.h"

/***************************************************************************
 * Pre-processor Definitions
 ***************************************************************************/

/***************************************************************************
 * Public Functions
 ***************************************************************************/

/****************************************************************************
 * Name: bcm2711_gpio_set_pulls
 *
 * Description:
 *   Set the specified GPIO pin to have pull up, pull down or no resistor.
 *   With both `up` and `down` as false, the resistor will be set to none.
 *   It is not possible to set both pull-up and pull-down.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the resistors on.
 *   up - True to set pull-up resistor, false otherwise.
 *   down - True to set pull-down resistor, false otherwise.
 *
 ****************************************************************************/

void rp2040_gpio_set_pulls(uint32_t gpio, bool up, bool down);

/****************************************************************************
 * Name: bcm2711_gpio_set_func
 *
 * Description:
 *   Set the specified GPIO pin to use one of its alternative functions.
 *   This will override the input/output direction selection previously set
 *   for this pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the function of.
 *   func - The function to set the GPIO pin to (0-5). This overrides the
 *          pin's input/output direction with the function.
 *
 ****************************************************************************/

void bcm2711_gpio_set_func(uint32_t gpio, uint8_t func);

/****************************************************************************
 * Name: bcm2711_gpio_set_dir
 *
 * Description:
 *   Set the direction (input/output) of a specific GPIO pin.
 *   Calling this function will override any previous function selection for
 *   this pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the direction of.
 *   out  - True to set the pin as an output, false to be an input.
 *
 ****************************************************************************/

void bcm2711_gpio_set_dir(uint32_t gpio, bool out);

/****************************************************************************
 * Name: bcm2711_gpio_pin_set
 *
 * Description:
 *   Set the output of a GPIO output pin to high or low.
 *   Calling this function on a GPIO pin set as an input does nothing.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set high or low.
 *   high  - True to set the pin high, false to set the pin low.
 *
 ****************************************************************************/

void bcm2711_gpio_pin_set(uint32_t gpio, bool set);

/****************************************************************************
 * Name: bcm2711_gpio_pin_get
 *
 * Description:
 *   Get the current value of the GPIO.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set high or low.
 *
 * Return:
 *    True for high, false for low.
 *
 ****************************************************************************/

bool bcm2711_gpio_pin_get(uint32_t gpio);

/****************************************************************************
 * Name: bcm2711_gpio_event_get
 *
 * Description:
 *   Check if an event was detected for the given GPIO pin.
 *   The event bit will be set if an event has happened that matches the event
 *   detection configuration for the given pin (rising edge, falling edge,
 *   level).
 *
 * Input parameters:
 *   gpio - The GPIO pin number to check for an event.
 *
 * Return:
 *    True if an event was detected, false otherwise.
 *
 ****************************************************************************/

bool bcm2711_gpio_event_get(uint32_t gpio);

/****************************************************************************
 * Name: bcm2711_gpio_event_clear
 *
 * Description:
 *   Clear the event detect status for the given GPIO pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to clear the event status of.
 *
 ****************************************************************************/

void bcm2711_gpio_event_clear(uint32_t gpio);

#endif // __ARCH_ARM64_SRC_BCM2711_BCM2711_GPIO_H
