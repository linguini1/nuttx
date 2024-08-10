/***************************************************************************
 * arch/arm64/src/bcm2711/bcm2711_gpio.c
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

#include <nuttx/config.h>

#include <nuttx/arch.h>

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "arm64_arch.h"
#include "arm64_internal.h"
#include "bcm2711_gpio.h"

/***************************************************************************
 * Pre-processor Definitions
 ***************************************************************************/

/***************************************************************************
 * Private Types
 ***************************************************************************/

/***************************************************************************
 * Private Data
 ***************************************************************************/

static const uint8_t g_fsel_map[] = {
    [0] = BCM_GPIO_FS_ALT0, [1] = BCM_GPIO_FS_ALT1, [2] = BCM_GPIO_FS_ALT2,
    [3] = BCM_GPIO_FS_ALT3, [4] = BCM_GPIO_FS_ALT4, [5] = BCM_GPIO_FS_ALT5,
};

/***************************************************************************
 * Private Function Prototypes
 ***************************************************************************/

static inline void bcm2711_gpio_help_set(uint32_t gpio, uint32_t reg1,
                                         uint32_t reg2, bool val);
static inline bool bcm2711_gpio_help_get(uint32_t gpio, uint32_t reg1,
                                         uint32_t reg2);

/***************************************************************************
 * Private Functions
 ***************************************************************************/

/****************************************************************************
 * Name: bcm2711_gpio_help_set
 *
 * Description:
 *   Helper function for setting a GPIO pin value on the BCM2711 GPIO
 *   register where one register is for pins 0-31 and the second is pins
 *   32-57.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the value of.
 *   reg1 - The GPIO register for pins 0-31.
 *   reg2 - The GPIO register for pins 0-33.
 *   val - The value to set (true for 1, false for 0).
 *
 ****************************************************************************/

static inline void bcm2711_gpio_help_set(uint32_t gpio, uint32_t reg1,
                                         uint32_t reg2, bool val)
{
  /* Choose appropriate bit and register */

  uint32_t bitmask;
  uint32_t reg;

  if (gpio <= 31)
    {
      bitmask = (1 << gpio);
      reg = reg1;
    }
  else
    {
      bitmask = (1 << (gpio - 32));
      reg = reg2;
    }

  /* Set or clear bit */

  if (val)
    {
      modreg32(bitmask, bitmask, reg);
    }
  else
    {
      modreg32(0, bitmask, reg);
    }
}

/****************************************************************************
 * Name: bcm2711_gpio_help_get
 *
 * Description:
 *   Helper function for reading a GPIO pin value on the BCM2711 GPIO
 *   register where one register is for pins 0-31 and the second is pins
 *   32-57.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to get the value of.
 *   reg1 - The GPIO register for pins 0-31.
 *   reg2 - The GPIO register for pins 0-33.
 *
 ****************************************************************************/

static inline bool bcm2711_gpio_help_get(uint32_t gpio, uint32_t reg1,
                                         uint32_t reg2)
{
  /* Choose appropriate bit and register */

  uint32_t bitmask;
  uint32_t reg;

  if (gpio <= 31)
    {
      bitmask = (1 << gpio);
      reg = reg1;
    }
  else
    {
      bitmask = (1 << (gpio - 32));
      reg = reg2;
    }

  return getreg32(reg) & bitmask;
}

/***************************************************************************
 * Public Functions
 ***************************************************************************/

/****************************************************************************
 * Name: bcm2711_gpio_set_pulls
 *
 * Description:
 *   Set the specified GPIO pin to have pull up, pull down or no
 *resistor. With both `up` and `down` as false, the resistor will be set
 *to none. It is not possible to set both pull-up and pull-down.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the resistors on.
 *   up - True to set pull-up resistor, false otherwise.
 *   down - True to set pull-down resistor, false otherwise.
 *
 ****************************************************************************/

void rp2040_gpio_set_pulls(uint32_t gpio, bool up, bool down)
{
  DEBUGASSERT(gpio <= BCM_GPIO_NUM);
  DEBUGASSERT(!(up && down)); /* Not valid to set pull-up and pull-down */

  /* Pick direction. */

  uint32_t direction = 0;
  if (up)
    {
      direction = BCM_GPIO_PULLUP;
    }
  else if (down)
    {
      direction = BCM_GPIO_PULLDOWN;
    }
  else
    {
      direction = BCM_GPIO_NORES;
    }

  /* Set GPIO pin resistor. */

  uint32_t value = 0;
  if (gpio <= 15)
    {
      value = (direction << (gpio * 2));
      modreg32(value, value, BCM_GPIO_PUP_PDN_CNTRL_REG0);
    }
  else if (gpio <= 31 && gpio > 15)
    {
      value = (direction << ((gpio - 16) * 2));
      modreg32(value, value, BCM_GPIO_PUP_PDN_CNTRL_REG1);
    }
  else if (gpio <= 47 && gpio > 31)
    {
      value = (direction << ((gpio - 32) * 2));
      modreg32(value, value, BCM_GPIO_PUP_PDN_CNTRL_REG2);
    }
  else if (gpio <= 57 && gpio > 47)
    {
      value = (direction << ((gpio - 48) * 2));
      modreg32(value, value, BCM_GPIO_PUP_PDN_CNTRL_REG3);
    }
}

/****************************************************************************
 * Name: bcm2711_gpio_set_func
 *
 * Description:
 *   Set the specified GPIO pin to use one of its alternative functions.
 *   This will override the input/output direction selection previously
 *set for this pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the function of.
 *   func - The function to set the GPIO pin to (0-5). This overrides
 *the pin's input/output direction with the function.
 *
 ****************************************************************************/

void bcm2711_gpio_set_func(uint32_t gpio, uint8_t func)
{
  DEBUGASSERT(gpio <= BCM_GPIO_NUM);
  DEBUGASSERT(0 <= func && func <= 5); /* Only 0-5 to select from. */

  uint32_t value = 0;
  if (gpio <= 9)
    {
      value = (g_fsel_map[func] << (gpio * 3));
      modreg32(value, value, BCM_GPIO_GPFSEL0);
    }
  else if (gpio <= 19 && gpio > 9)
    {
      value = (g_fsel_map[func] << ((gpio - 10) * 3));
      modreg32(value, value, BCM_GPIO_GPFSEL1);
    }
  else if (gpio <= 29 && gpio > 20)
    {
      value = (g_fsel_map[func] << ((gpio - 20) * 3));
      modreg32(value, value, BCM_GPIO_GPFSEL2);
    }
  else if (gpio <= 39 && gpio > 30)
    {
      value = (g_fsel_map[func] << ((gpio - 30) * 3));
      modreg32(value, value, BCM_GPIO_GPFSEL3);
    }
  else if (gpio <= 49 && gpio > 40)
    {
      value = (g_fsel_map[func] << ((gpio - 40) * 3));
      modreg32(value, value, BCM_GPIO_GPFSEL4);
    }
  else if (gpio <= 57 && gpio > 50)
    {
      value = (g_fsel_map[func] << ((gpio - 50) * 3));
      modreg32(value, value, BCM_GPIO_GPFSEL5);
    }
}

/****************************************************************************
 * Name: bcm2711_gpio_set_dir
 *
 * Description:
 *   Set the direction (input/output) of a specific GPIO pin.
 *   Calling this function will override any previous function selection
 *for this pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the direction of.
 *   out  - True to set the pin as an output, false to be an input.
 *
 ****************************************************************************/

void bcm2711_gpio_set_dir(uint32_t gpio, bool out)
{
  DEBUGASSERT(gpio <= BCM_GPIO_NUM);

  /* Select input or output. */

  uint32_t value = out ? BCM_GPIO_FS_OUT : BCM_GPIO_FS_IN;

  /* Modify appropriate register. */

  if (gpio <= 9)
    {
      modreg32(value, value, BCM_GPIO_GPFSEL0);
    }
  else if (gpio <= 19 && gpio > 9)
    {
      modreg32(value, value, BCM_GPIO_GPFSEL1);
    }
  else if (gpio <= 29 && gpio > 20)
    {
      modreg32(value, value, BCM_GPIO_GPFSEL2);
    }
  else if (gpio <= 39 && gpio > 30)
    {
      modreg32(value, value, BCM_GPIO_GPFSEL3);
    }
  else if (gpio <= 49 && gpio > 40)
    {
      modreg32(value, value, BCM_GPIO_GPFSEL4);
    }
  else if (gpio <= 57 && gpio > 50)
    {
      modreg32(value, value, BCM_GPIO_GPFSEL5);
    }
}

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

void bcm2711_gpio_pin_set(uint32_t gpio, bool set)
{
  DEBUGASSERT(gpio <= BCM_GPIO_NUM);

  if (set)
    {
      bcm2711_gpio_help_set(gpio, BCM_GPIO_GPSET0, BCM_GPIO_GPSET0, true);
    }
  else
    {
      bcm2711_gpio_help_set(gpio, BCM_GPIO_GPCLR0, BCM_GPIO_GPCLR0, true);
    }
}

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

bool bcm2711_gpio_pin_get(uint32_t gpio)
{
  DEBUGASSERT(gpio <= BCM_GPIO_NUM);

  if (gpio <= 31)
    {
      return getreg32(BCM_GPIO_GPLEV0) & (1 << gpio);
    }
  else
    {
      return getreg32(BCM_GPIO_GPLEV1) & (1 << (gpio - 32));
    }
}

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

bool bcm2711_gpio_event_get(uint32_t gpio)
{
  DEBUGASSERT(gpio <= BCM_GPIO_NUM);
  return bcm2711_gpio_help_get(gpio, BCM_GPIO_GPEDS0, BCM_GPIO_GPEDS1);
}

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

void bcm2711_gpio_event_clear(uint32_t gpio)
{
  DEBUGASSERT(gpio <= BCM_GPIO_NUM);
  bcm2711_gpio_help_set(gpio, BCM_GPIO_GPEDS0, BCM_GPIO_GPEDS1, false);
}

/****************************************************************************
 * Name: bcm2711_gpio_rising_edge
 *
 * Description:
 *   Set/clear rising edge event detection for the given GPIO pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the event detection of.
 *   set - True to set, false to clear.
 *
 ****************************************************************************/

void bcm2711_gpio_rising_edge(uint32_t gpio, bool set)
{
  DEBUGASSERT(gpio <= BCM_GPIO_NUM);
  bcm2711_gpio_help_set(gpio, BCM_GPIO_GPREN0, BCM_GPIO_GPREN1, set);
}

/****************************************************************************
 * Name: bcm2711_gpio_falling_edge
 *
 * Description:
 *   Set/clear falling edge event detection for the given GPIO pin.
 *
 * Input parameters:
 *   gpio - The GPIO pin number to set the event detection of.
 *   set - True to set, false to clear.
 *
 ****************************************************************************/

void bcm2711_gpio_falling_edge(uint32_t gpio, bool set)
{
  DEBUGASSERT(gpio <= BCM_GPIO_NUM);
  bcm2711_gpio_help_set(gpio, BCM_GPIO_GPFEN0, BCM_GPIO_GPFEN1, set);
}
