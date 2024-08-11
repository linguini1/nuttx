/****************************************************************************
 * arch/arm64/src/bcm2711/bcm2711_i2c.c
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
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <nuttx/arch.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/mutex.h>

#include "arm64_arch.h"
#include "bcm2711_gpio.h"
#include "bcm2711_i2c.h"
#include "chip.h"
#include "hardware/bcm2711_bsc.h"

#if defined(CONFIG_BCM2711_I2C)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Default bus frequency at 400kbps. */

#define I2C_DEFAULT_FREQUENCY 400000

/* Core clock nominal frequency in Hz */

#define CORE_CLOCK_FREQUENCY 150000000

/* Get divisor for desired I2C bus frequency */

#define CLK_DIVISOR(freq) (CORE_CLOCK_FREQUENCY / (freq))

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* BCM2711 I2C device. */

struct bcm2711_i2cdev_s
{
  struct i2c_master_s dev; /* Generic I2C device */
  uint32_t base;           /* Base address of I2C interface */

  mutex_t lock;       /* Exclusive access */
  sem_t wait;         /* Wait for transfer completion */
  uint32_t frequency; /* I2C bus frequency */

  struct i2c_msg_s *msgs; /* Messages to send */

  int err;  /* Error status of transfers */
  int refs; /* Reference count */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void bcm2711_i2c_setfrequency(struct bcm2711_i2cdev_s *priv,
                                     uint32_t frequency);
static void bcm2711_i2c_disable(struct bcm2711_i2cdev_s *priv);
static void bcm2711_i2c_enable(struct bcm2711_i2cdev_s *priv);

static int bcm2711_i2c_transfer(struct i2c_master_s *dev,
                                struct i2c_msg_s *msgs, int count);
#ifdef CONFIG_I2C_RESET
static int bcm2711_i2c_reset(struct i2c_master_s *dev);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* I2C operations for BCM2711 I2C interfaces. */

struct i2c_ops_s bcm2711_i2c_ops = {
    .transfer = bcm2711_i2c_transfer,
#if defined(CONFIG_I2C_RESET)
    .reset = bcm2711_i2c_reset,
#endif // defined(CONFIG_I2C_RESET)
};

#ifdef CONFIG_BCM2711_I2C1

/* I2C1 interface. */

static struct bcm2711_i2cdev_s g_i2c1dev = {
    .base = BCM_BSC1,
    .lock = NXMUTEX_INITIALIZER,
    .wait = SEM_INITIALIZER(0),
    .refs = 0,
    .err = 0,
};

#endif // CONFIG_BCM2711_I2C1

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bcm2711_i2c_setfrequency
 *
 * Description:
 *   Set the frequency for the next transfer.
 *
 * Input Parameters:
 *     priv - The BCM2711 I2C interface to set the frequency of.
 *     frequency - The new frequency.
 *
 ****************************************************************************/

static void bcm2711_i2c_setfrequency(struct bcm2711_i2cdev_s *priv,
                                     uint32_t frequency)
{
  putreg32(CLK_DIVISOR(frequency), BCM_BSC_DIV(priv->base));
  priv->frequency = frequency;
}

/****************************************************************************
 * Name: bcm2711_i2c_disable
 *
 * Description:
 *   Disable the I2C interface.
 *
 * Input Parameters:
 *     priv - The BCM2711 I2C interface to disable.
 *
 ****************************************************************************/

static void bcm2711_i2c_disable(struct bcm2711_i2cdev_s *priv)
{
  /* Disable interrupts */

  modreg32(0, BCM_BSC_C_INTD, BCM_BSC_C(priv->base));
  modreg32(0, BCM_BSC_C_INTR, BCM_BSC_C(priv->base));
  modreg32(0, BCM_BSC_C_INTT, BCM_BSC_C(priv->base));

  /* Clear FIFO */

  modreg32(0, BCM_BSC_C_CLRFIFO, BCM_BSC_C(priv->base));

  /* Disable interface */

  modreg32(0, BCM_BSC_C_I2CEN, BCM_BSC_C(priv->base));
}

/****************************************************************************
 * Name: bcm2711_i2c_enable
 *
 * Description:
 *   Enable the I2C interface.
 *
 * Input Parameters:
 *     priv - The BCM2711 I2C interface to enable.
 *
 ****************************************************************************/

static void bcm2711_i2c_enable(struct bcm2711_i2cdev_s *priv)
{
  /* Enable interface */

  modreg32(BCM_BSC_C_I2CEN, BCM_BSC_C_I2CEN, BCM_BSC_C(priv->base));

  /* Clear FIFO */

  modreg32(0, BCM_BSC_C_CLRFIFO, BCM_BSC_C(priv->base));

  /* Enable interrupts */

  modreg32(BCM_BSC_C_INTD, BCM_BSC_C_INTD, BCM_BSC_C(priv->base));
  modreg32(BCM_BSC_C_INTR, BCM_BSC_C_INTR, BCM_BSC_C(priv->base));
  modreg32(BCM_BSC_C_INTT, BCM_BSC_C_INTT, BCM_BSC_C(priv->base));
}

/****************************************************************************
 * Name: bcm2711_i2c_transfer
 *
 * Description:
 *   Perform a sequence of I2C transfers.
 *
 * Input Parameters:
 *     dev - The I2C master interface to transfer on.
 *     msgs - The messages to transfer.
 *     count - The number of messages to transfer.
 ****************************************************************************/

static int bcm2711_i2c_transfer(struct i2c_master_s *dev,
                                struct i2c_msg_s *msgs, int count)
{
  // TODO
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bcm2711_i2cbus_initialize
 *
 * Description:
 *   Initialise an I2C device for the BCM2711.
 *
 * Input parameters:
 *     port - The bus number for the I2C interface.
 *
 ****************************************************************************/

struct i2c_master_s *bcm2711_i2cbus_initialize(int port)
{

  struct bcm2711_i2cdev_s *priv;

  /* Initialize selected port */

  switch (port)
    {
#if defined(CONFIG_BCM2711_I2C1)
    case 1:
      priv = &g_i2c1dev;
      priv->dev.ops = &bcm2711_i2c_ops;
      break;
#endif
    default:
      i2cerr("Port %d is unsupported.\n", port);
      return NULL;
    }

  /* Exclusive access */

  nxmutex_lock(&priv->lock);

  /* Test for previous initialization. If already initialized, nothing more
   * needs to be done.
   */

  if (1 < ++priv->refs)
    {
      nxmutex_unlock(&priv->lock);
      return &priv->dev;
    }

  /* Not yet initialized, little more work to do. */
  // TODO

  nxmutex_unlock(&priv->lock);
  return &priv->dev;
}

/****************************************************************************
 * Name: bcm2711_i2cbus_uninitialize
 *
 * Description:
 *   Uninitialize an I2C device on the BCM2711.
 *
 * Input parameters;
 *     dev - The device to uninitialize.
 *
 ****************************************************************************/

int bcm2711_i2cbus_uninitialize(struct i2c_master_s *dev)
{

  int ret = 0;
  struct bcm2711_i2cdev_s *priv = (struct bcm2711_i2cdev_s *)dev;

  if (priv->refs == 0)
    {
      return -1;
    }

  /* If there are still references to the device, just decrement references.
   */

  nxmutex_lock(&priv->lock);
  if (--priv->refs)
    {
      nxmutex_unlock(&priv->lock);
      return OK;
    }

  /* This was the last reference to the I2C device. */
  // TODO: final cleanup

  return ret;
}

#endif // defined(CONFIG_BCM2711_I2C)
