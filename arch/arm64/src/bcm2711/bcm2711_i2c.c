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
#include <nuttx/irq.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <nuttx/arch.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/mutex.h>

#include "arm64_arch.h"
#include "arm64_gic.h"
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

/* The FIFO buffer length of the I2C interfaces */

#define FIFO_DEPTH 16

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* BCM2711 I2C device. */

struct bcm2711_i2cdev_s
{
  struct i2c_master_s dev; /* Generic I2C device */
  uint32_t base;           /* Base address of I2C interface */
  uint8_t port;            /* Port number */

  mutex_t lock;       /* Exclusive access */
  sem_t wait;         /* Wait for transfer completion */
  uint32_t frequency; /* I2C bus frequency */

  struct i2c_msg_s *msgs; /* Messages to send */
  size_t reg_buff_offset; /* Offset into message buffer */
  uint8_t rw_size;        /* max(FIFO_DEPTH, remaining message size) */

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
static void bcm2711_i2c_drainrxfifo(struct bcm2711_i2cdev_s *priv);
static int bcm2711_i2c_send(struct bcm2711_i2cdev_s *priv);
static int bcm2711_i2c_receive(struct bcm2711_i2cdev_s *priv);

static int bcm2711_i2c_interrupt_handler(int irq, void *context, void *arg);

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
    .port = 1,
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
 * Name: bcm2711_i2c_drainrxfifo
 *
 * Description:
 *   Drain the RX FIFO into the receive message buffer of the I2C device.
 *
 * Input Parameters:
 *     priv - The BCM2711 I2C interface to receive on.
 *
 ****************************************************************************/

static void bcm2711_i2c_drainrxfifo(struct bcm2711_i2cdev_s *priv)
{
  struct i2c_msg_s *msg = priv->msgs;
  uint32_t status_addr = BCM_BSC_S(priv->base);
  uint32_t fifo_addr = BCM_BSC_FIFO(priv->base);
  uint32_t data;
  size_t i;

  DEBUGASSERT(msg != NULL);

  /* While the RX FIFO contains data to be received, drain it into the message
   * buffer. Do not read more than the `rw_size` to avoid overflowing the
   * message buffer.
   */

  for (i = 0; (i < priv->rw_size) && (getreg32(status_addr) & BCM_BSC_S_RXD);
       i++)
    {
      data = getreg32(fifo_addr);
      msg->buffer[priv->reg_buff_offset + i] = data & 0xff;
    }

  /* We have either reached the rw_size or the RX FIFO is out of data.
   * Update the buffer offset with the amount of data we have read.
   * NOTE: If the receive FIFO contains less data than `rw_size`, then we skip
   * bytes.
   */

  priv->reg_buff_offset += i;
}

/****************************************************************************
 * Name: bcm2711_i2c_receive
 *
 * Description:
 *   Receive I2C data.
 *
 * Input Parameters:
 *     dev - The I2C interface to receive on.
 ****************************************************************************/

static int bcm2711_i2c_receive(struct bcm2711_i2cdev_s *priv)
{
  struct i2c_msg_s *msg = priv->msgs;
  ssize_t msg_length;
  bool en;

  DEBUGASSERT(msg != NULL);

  /* Start buffer fresh for receiving full message. */

  priv->reg_buff_offset = 0;

  /* Continuously read until message has been completely read. */

  for (msg_length = msg->length; msg_length > 0; msg_length -= priv->rw_size)
    {
      /* Read maximum FIFO depth or the remaining message length. */

      if (msg_length <= FIFO_DEPTH)
        {
          priv->rw_size = msg_length;
          en = 1;
        }
      else
        {
          priv->rw_size = FIFO_DEPTH;
          en = 0;
        }

      // TODO
    }

  return 0;
}

/****************************************************************************
 * Name: bcm2711_i2c_send
 *
 * Description:
 *   Send I2C data.
 *
 * Input Parameters:
 *     dev - The I2C interface to send on.
 ****************************************************************************/

static int bcm2711_i2c_send(struct bcm2711_i2cdev_s *priv)
{
  struct i2c_msg_s *msg = priv->msgs;
  ssize_t i;
  uint32_t status_reg = BCM_BSC_S(priv->base);
  uint32_t fifo_reg = BCM_BSC_FIFO(priv->base);

  DEBUGASSERT(msg != NULL);

  /* Send the entire message */

  for (i = 0; i < msg->length - 1; i++)
    {

      /* While the TX FIFO cannot accept more data, wait. */

      while (!(getreg32(status_reg) & BCM_BSC_S_TXD))
        ;

      /* Once TX FIFO can accept more data, send a byte at a time. */

      putreg32(msg->buffer[i] & 0xff, fifo_reg);
    }

  /* Once message has been fully transmitted, determine whether or not to send
   * stop condition based on message configuration.
   */

  // TODO

  return 0;
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
 * Name: bcm2711_i2c_interrupt_handler
 *
 * Description:
 *   Handle I2C interrupts.
 *
 * Input Parameters:
 *     irq - The IRQ number
 *     context - The interrupt context
 *     arg - The argument passed to the interrupt handler
 ****************************************************************************/

static int bcm2711_i2c_interrupt_handler(int irq, void *context, void *arg)
{
  int ret = OK;
  uint32_t status;
  uint32_t status_addr;
  struct bcm2711_i2cdev_s *priv = (struct bcm2711_i2cdev_s *)arg;

  /* Get interrupt status for this device. */

  status_addr = BCM_BSC_S(priv->base);
  status = getreg32(status_addr);

  /* Decide what to do with the status */

  /* There was an ACK error */

  if (status & BCM_BSC_S_ERR)
    {
      modreg32(BCM_BSC_S_ERR, BCM_BSC_S_ERR,
               status_addr); /* Acknowledge err */
      priv->err = -EIO;
    }

  /* There was a clock stretch timeout */

  if (status & BCM_BSC_S_CLKT)
    {
      modreg32(BCM_BSC_S_CLKT, BCM_BSC_S_CLKT,
               status_addr); /* Acknowledge err */
      priv->err = -EIO;
    }

  /* RX FIFO is full */

  if (status & BCM_BSC_S_RXF)
    {
      /* This status bit is cleared after reading data from RX FIFO. */

      bcm2711_i2c_drainrxfifo(priv);
    }

  /* FIFO is empty */

  if (status & BCM_BSC_S_TXE)
    {
      // TODO
    }

  /* Transfer is done */

  if (status & BCM_BSC_S_DONE)
    {
      modreg32(BCM_BSC_S_DONE, BCM_BSC_S_DONE, status_addr);
      // TODO
    }

  return ret;
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
  int ret;
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

  // TODO: init
  bcm2711_i2c_disable(priv); /* Only enabled when used. */
  bcm2711_i2c_setfrequency(priv, I2C_DEFAULT_FREQUENCY);

  /* Attach interrupt handler */

  ret = irq_attach(BCM_IRQ_VC_I2C, bcm2711_i2c_interrupt_handler, priv);
  if (ret < 0)
    {
      i2cerr("Could not attach interrupt handler for port %d: %d\n", port,
             ret);
      return NULL;
    }

  /* Enable interrupt handler */

  arm64_gic_irq_set_priority(BCM_IRQ_VC_I2C, 0, IRQ_TYPE_EDGE);
  up_enable_irq(BCM_IRQ_VC_I2C);

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
