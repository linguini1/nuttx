/***************************************************************************
 * arch/arm64/src/bcm2711/bcm2711_serial.c
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
#include <nuttx/serial/serial.h>
#include <stdbool.h>
#include <stdint.h>

#include "arm64_arch.h"
#include "hardware/bcm2711_aux.h"

/***************************************************************************
 * Pre-processor Definitions
 ***************************************************************************/

/***************************************************************************
 * Private Function Prototypes
 ***************************************************************************/
static int bcm2711_uart_setup(struct uart_dev_s *dev);
static bool bcm2711_uart_txready(struct uart_dev_s *dev);
static bool bcm2711_uart_txempty(struct uart_dev_s *dev);
static bool bcm2711_uart_rxavail(struct uart_dev_s *dev);
static void bcm2711_uart_send(struct uart_dev_s *dev, char c);
static int bcm2711_uart_receive(struct uart_dev_s *dev, unsigned int *status);

/***************************************************************************
 * Private Functions
 ***************************************************************************/

static int bcm2711_uart_setup(struct uart_dev_s *dev)
{

  /* Enable 8 bit */
  putreg32(getreg32(BCM_AUX_MU_LCR_REG) | BCM_AUX_MU_LCR_DATA8B,
           BCM_AUX_MU_LCR_REG);

  // TODO

  return 0;
}

static bool bcm2711_uart_txready(struct uart_dev_s *dev)
{
  return getreg32(BCM_AUX_MU_STAT_REG) & BCM_AUX_MU_STAT_SPACEAVAIL;
}

static bool bcm2711_uart_txempty(struct uart_dev_s *dev)
{
  return getreg32(BCM_AUX_MU_STAT_REG) & BCM_AUX_MU_STAT_TXEMPTY;
}

static bool bcm2711_uart_rxavail(struct uart_dev_s *dev)
{
  return getreg32(BCM_AUX_MU_STAT_REG) & BCM_AUX_MU_STAT_SYMAVAIL;
}

static void bcm2711_uart_send(struct uart_dev_s *dev, char c)
{

  while (!bcm2711_uart_txready(dev))
    {
      /* Wait to send byte */
    }
  /* Write byte (do I need to mask to avoid writing to LS8 baud rate bits?) */
  putreg32(c, BCM_AUX_MU_IO_REG);
}

static int bcm2711_uart_receive(struct uart_dev_s *dev, unsigned int *status)
{
  while (!bcm2711_uart_rxavail(dev))
    {
      /* Wait for symbol */
    }
  *status = 0;                               /* OK */
  return getreg32(BCM_AUX_MU_IO_REG) & 0xff; /* Read byte */
}
