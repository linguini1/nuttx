/****************************************************************************
 * arch/arm/src/at32/at32_exti_gpio.c
 *
 * SPDX-License-Identifier: Apache-2.0
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
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/irq.h>
#include <nuttx/arch.h>

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <debug.h>

#include <arch/irq.h>

#include "arm_internal.h"
#include "chip.h"
#include "at32_gpio.h"
#include "at32_exti.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct gpio_callback_s
{
  xcpt_t callback;
  void  *arg;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Interrupt handlers attached to each EXTI */

static struct gpio_callback_s g_gpio_callbacks[16];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Interrupt Service Routines - Dispatchers
 ****************************************************************************/

static int at32_exti0_isr(int irq, void *context, void *arg)
{
  int ret = OK;

  /* Clear the pending interrupt */

  putreg32(0x0001, AT32_EXTI_PR);

  /* And dispatch the interrupt to the handler */

  if (g_gpio_callbacks[0].callback != NULL)
    {
      xcpt_t callback = g_gpio_callbacks[0].callback;
      void  *cbarg    = g_gpio_callbacks[0].arg;

      ret = callback(irq, context, cbarg);
    }

  return ret;
}

static int at32_exti1_isr(int irq, void *context, void *arg)
{
  int ret = OK;

  /* Clear the pending interrupt */

  putreg32(0x0002, AT32_EXTI_PR);

  /* And dispatch the interrupt to the handler */

  if (g_gpio_callbacks[1].callback != NULL)
    {
      xcpt_t callback = g_gpio_callbacks[1].callback;
      void  *cbarg    = g_gpio_callbacks[1].arg;

      ret = callback(irq, context, cbarg);
    }

  return ret;
}

static int at32_exti2_isr(int irq, void *context, void *arg)
{
  int ret = OK;

  /* Clear the pending interrupt */

  putreg32(0x0004, AT32_EXTI_PR);

  /* And dispatch the interrupt to the handler */

  if (g_gpio_callbacks[2].callback != NULL)
    {
      xcpt_t callback = g_gpio_callbacks[2].callback;
      void  *cbarg    = g_gpio_callbacks[2].arg;

      ret = callback(irq, context, cbarg);
    }

  return ret;
}

static int at32_exti3_isr(int irq, void *context, void  * arg)
{
  int ret = OK;

  /* Clear the pending interrupt */

  putreg32(0x0008, AT32_EXTI_PR);

  /* And dispatch the interrupt to the handler */

  if (g_gpio_callbacks[3].callback != NULL)
    {
      xcpt_t callback = g_gpio_callbacks[3].callback;
      void  *cbarg    = g_gpio_callbacks[3].arg;

      ret = callback(irq, context, cbarg);
    }

  return ret;
}

static int at32_exti4_isr(int irq, void *context, void *arg)
{
  int ret = OK;

  /* Clear the pending interrupt */

  putreg32(0x0010, AT32_EXTI_PR);

  /* And dispatch the interrupt to the handler */

  if (g_gpio_callbacks[4].callback != NULL)
    {
      xcpt_t callback = g_gpio_callbacks[4].callback;
      void  *cbarg    = g_gpio_callbacks[4].arg;

      ret = callback(irq, context, cbarg);
    }

  return ret;
}

static int at32_exti_multiisr(int irq, void *context, void *arg,
                               int first, int last)
{
  uint32_t pr;
  int pin;
  int ret = OK;

  /* Examine the state of each pin in the group */

  pr = getreg32(AT32_EXTI_PR);

  /* And dispatch the interrupt to the handler */

  for (pin = first; pin <= last; pin++)
    {
      /* Is an interrupt pending on this pin? */

      uint32_t mask = (1 << pin);
      if ((pr & mask) != 0)
        {
          /* Clear the pending interrupt */

          putreg32(mask, AT32_EXTI_PR);

          /* And dispatch the interrupt to the handler */

          if (g_gpio_callbacks[pin].callback != NULL)
            {
              xcpt_t callback = g_gpio_callbacks[pin].callback;
              void  *cbarg    = g_gpio_callbacks[pin].arg;
              int tmp;

              tmp = callback(irq, context, cbarg);
              if (tmp < 0)
                {
                  ret = tmp;
                }
            }
        }
    }

  return ret;
}

static int at32_exti95_isr(int irq, void *context, void *arg)
{
  return at32_exti_multiisr(irq, context, arg, 5, 9);
}

static int at32_exti1510_isr(int irq, void *context, void *arg)
{
  return at32_exti_multiisr(irq, context, arg, 10, 15);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: at32_gpiosetevent
 *
 * Description:
 *   Sets/clears GPIO based event and interrupt triggers.
 *
 * Input Parameters:
 *  - pinset:      GPIO pin configuration
 *  - risingedge:  Enables interrupt on rising edges
 *  - fallingedge: Enables interrupt on falling edges
 *  - event:       Generate event when set
 *  - func:        When non-NULL, generate interrupt
 *  - arg:         Argument passed to the interrupt callback
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure indicating the
 *   nature of the failure.
 *
 ****************************************************************************/

int at32_gpiosetevent(uint32_t pinset, bool risingedge, bool fallingedge,
                       bool event, xcpt_t func, void *arg)
{
  struct gpio_callback_s *shared_cbs;
  uint32_t pin = pinset & GPIO_PIN_MASK;
  uint32_t exti = AT32_EXTI_BIT(pin);
  int      irq;
  xcpt_t   handler;
  int      nshared;
  int      i;

  /* Select the interrupt handler for this EXTI pin */

  if (pin < 5)
    {
      irq        = pin + AT32_IRQ_EXTI0;
      nshared    = 1;
      shared_cbs = &g_gpio_callbacks[pin];
      switch (pin)
        {
          case 0:
            handler = at32_exti0_isr;
            break;

          case 1:
            handler = at32_exti1_isr;
            break;

          case 2:
            handler = at32_exti2_isr;
            break;

          case 3:
            handler = at32_exti3_isr;
            break;

          default:
            handler = at32_exti4_isr;
            break;
        }
    }
  else if (pin < 10)
    {
      irq        = AT32_IRQ_EXTI95;
      handler    = at32_exti95_isr;
      shared_cbs = &g_gpio_callbacks[5];
      nshared    = 5;
    }
  else
    {
      irq        = AT32_IRQ_EXTI1510;
      handler    = at32_exti1510_isr;
      shared_cbs = &g_gpio_callbacks[10];
      nshared    = 6;
    }

  /* Get the previous GPIO IRQ handler; Save the new IRQ handler. */

  g_gpio_callbacks[pin].callback = func;
  g_gpio_callbacks[pin].arg      = arg;

  /* Install external interrupt handlers */

  if (func)
    {
      irq_attach(irq, handler, NULL);
      up_enable_irq(irq);
    }
  else
    {
      /* Only disable IRQ if shared handler does not have any active
       * callbacks.
       */

      for (i = 0; i < nshared; i++)
        {
          if (shared_cbs[i].callback != NULL)
            {
              break;
            }
        }

      if (i == nshared)
        {
          up_disable_irq(irq);
        }
    }

  /* Configure GPIO, enable EXTI line enabled if event or interrupt is
   * enabled.
   */

  if (event || func)
    {
      pinset |= GPIO_EXTI;
    }

  at32_configgpio(pinset);

  /* Configure rising/falling edges */

  modifyreg32(AT32_EXTI_RTSR,
              risingedge ? 0 : exti,
              risingedge ? exti : 0);
  modifyreg32(AT32_EXTI_FTSR,
              fallingedge ? 0 : exti,
              fallingedge ? exti : 0);

  /* Enable Events and Interrupts */

  modifyreg32(AT32_EXTI_EMR,
              event ? 0 : exti,
              event ? exti : 0);
  modifyreg32(AT32_EXTI_IMR,
              func ? 0 : exti,
              func ? exti : 0);

  return OK;
}

/****************************************************************************
 * Name: at32_gpioswirq
 *
 * Description:
 *   Trigger an extended interrupt (EXTI) via software on the EXTI line
 *   corresponding to a GPIO pin.
 *
 * Input Parameters:
 *  pinset - GPIO pin to trigger an EXTI on.
 *
 ****************************************************************************/

void at32_gpioswirq(uint32_t pinset)
{
  putreg32(1u << (pinset & GPIO_PIN_MASK), AT32_EXTI_SWIER);
}
