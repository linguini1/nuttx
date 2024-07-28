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

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "arm64_arch.h"
#include "arm64_internal.h"
#include "hardware/bcm2711_aux.h"

/***************************************************************************
 * Pre-processor Definitions
 ***************************************************************************/

/* Mini UART settings. */

#ifndef CONFIG_MINIUART_BAUD
#define CONFIG_MINIUART_BAUD 115200
#endif

#ifndef CONFIG_MINIUART_BITS
#define CONFIG_MINIUART_BITS 8
#endif

#ifndef CONFIG_MINIUART_PARITY
#define CONFIG_MINIUART_PARITY 0
#endif

#ifndef CONFIG_MINIUART_2STOP
#define CONFIG_MINIUART_2STOP 0
#endif

#ifndef CONFIG_MINIUART_RXBUFSIZE
#define CONFIG_MINIUART_RXBUFSIZE 256
#endif

#ifndef CONFIG_MINIUART_TXBUFSIZE
#define CONFIG_MINIUART_TXBUFSIZE 256
#endif

#define CONSOLE_DEV g_miniuartport /* Mini UART is console */

/***************************************************************************
 * Private Function Prototypes
 ***************************************************************************/

static int bcm2711_uart_setup(struct uart_dev_s *dev);
void bcm2711_uart_shutdown(struct uart_dev_s *dev);
static bool bcm2711_uart_txready(struct uart_dev_s *dev);
static bool bcm2711_uart_txempty(struct uart_dev_s *dev);
static void bcm2711_uart_txint(struct uart_dev_s *dev, bool enable);
static void bcm2711_uart_rxint(struct uart_dev_s *dev, bool enable);
static bool bcm2711_uart_rxavailable(struct uart_dev_s *dev);
static int bcm2711_uart_receive(struct uart_dev_s *dev, unsigned int *status);
static void bcm2711_uart_send(struct uart_dev_s *dev, int c);
static int bcm2711_uart_ioctl(struct file *filep, int cmd, unsigned long arg);
static void bcm2711_uart_attach(struct uart_dev_s *dev);
static void bcm2711_uart_detach(struct uart_dev_s *dev);

/***************************************************************************
 * Private Data
 ***************************************************************************/

/* UART operations for serial driver */

static const struct uart_ops_s g_uart_ops = {
    .setup = bcm2711_uart_setup,
    .shutdown = bcm2711_uart_shutdown,
    .attach = bcm2711_uart_attach,
    .detach = bcm2711_uart_detach,
    .ioctl = bcm2711_uart_ioctl,
    .receive = bcm2711_uart_receive,
    .rxint = bcm2711_uart_rxint,
    .rxavailable = bcm2711_uart_rxavailable,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
    .rxflowcontrol = NULL,
#endif
    .send = bcm2711_uart_send,
    .txint = bcm2711_uart_txint,
    .txready = bcm2711_uart_txready,
    .txempty = bcm2711_uart_txempty,
};

/* Mini UART I/O Buffers (Console) */

static char g_miniuartrxbuffer[CONFIG_MINIUART_RXBUFSIZE];
static char g_miniuarttxbuffer[CONFIG_MINIUART_TXBUFSIZE];

static struct uart_dev_s g_miniuartport = {
    .recv =
        {
            .size = CONFIG_MINIUART_RXBUFSIZE,
            .buffer = g_miniuartrxbuffer,
        },

    .xmit =
        {
            .size = CONFIG_MINIUART_TXBUFSIZE,
            .buffer = g_miniuarttxbuffer,
        },

    .ops = &g_uart_ops,
    .priv = NULL, // TODO
};

/***************************************************************************
 * Private Functions
 ***************************************************************************/

/***************************************************************************
 * Name: bcm2711_uart_txint
 *
 * Description:
 *   Call to enable or disable TX interrupts
 *
 * Input Parameters:
 *   dev    - UART Device
 *   enable - True to enable TX interrupts; false to disable
 *
 * Returned Value:
 *   None
 *
 ***************************************************************************/

static void bcm2711_uart_txint(struct uart_dev_s *dev, bool enable)
{
  if (enable)
    {
      putreg32(getreg32(BCM_AUX_MU_IER_REG) | BCM_AUX_MU_IER_TXD,
               BCM_AUX_MU_IER_REG);
    }
  else
    {
      putreg32(getreg32(BCM_AUX_MU_IER_REG) & ~BCM_AUX_MU_IER_TXD,
               BCM_AUX_MU_IER_REG);
    }
}

/***************************************************************************
 * Name: bcm2711_uart_rxint
 *
 * Description:
 *   Call to enable or disable RX interrupts
 *
 * Input Parameters:
 *   dev    - UART Device
 *   enable - True to enable RX interrupts; false to disable
 *
 * Returned Value:
 *   None
 *
 ***************************************************************************/

static void bcm2711_uart_rxint(struct uart_dev_s *dev, bool enable)
{
  if (enable)
    {
      putreg32(getreg32(BCM_AUX_MU_IER_REG) | BCM_AUX_MU_IER_RXD,
               BCM_AUX_MU_IER_REG);
    }
  else
    {
      putreg32(getreg32(BCM_AUX_MU_IER_REG) & ~BCM_AUX_MU_IER_RXD,
               BCM_AUX_MU_IER_REG);
    }
}

/***************************************************************************
 * Name: bcm2711_uart_shutdown
 *
 * Description:
 *   Disable the UART Port.  This method is called when the serial
 *   port is closed.
 *
 * Input Parameters:
 *   dev - UART Device
 *
 * Returned Value:
 *   None
 *
 ***************************************************************************/

void bcm2711_uart_shutdown(struct uart_dev_s *dev)
{
  bcm2711_uart_rxint(dev, false);
  bcm2711_uart_txint(dev, false);
}

static int bcm2711_uart_setup(struct uart_dev_s *dev)
{

  /* Enable 8 bit */
  putreg32(getreg32(BCM_AUX_MU_LCR_REG) | BCM_AUX_MU_LCR_DATA8B,
           BCM_AUX_MU_LCR_REG);

  // TODO

  return 0;
}

/***************************************************************************
 * Name: bcm2711_uart_txready
 *
 * Description:
 *   Return true if the Transmit FIFO is not full
 *
 * Input Parameters:
 *   dev - UART Device
 *
 * Returned Value:
 *   True if the Transmit FIFO is not full; false otherwise
 *
 ***************************************************************************/

static bool bcm2711_uart_txready(struct uart_dev_s *dev)
{
  return getreg32(BCM_AUX_MU_STAT_REG) & BCM_AUX_MU_STAT_SPACEAVAIL;
}

/***************************************************************************
 * Name: bcm2711_uart_txempty
 *
 * Description:
 *   Return true if the Transmit FIFO is empty
 *
 * Input Parameters:
 *   dev - UART Device
 *
 * Returned Value:
 *   True if the Transmit FIFO is empty; false otherwise
 *
 ***************************************************************************/

static bool bcm2711_uart_txempty(struct uart_dev_s *dev)
{
  return getreg32(BCM_AUX_MU_STAT_REG) & BCM_AUX_MU_STAT_TXEMPTY;
}

static bool bcm2711_uart_rxavail(struct uart_dev_s *dev)
{
  return getreg32(BCM_AUX_MU_STAT_REG) & BCM_AUX_MU_STAT_SYMAVAIL;
}

/***************************************************************************
 * Name: bcm2711_uart_wait_send
 *
 * Description:
 *   Wait for Transmit FIFO until it is not full, then transmit the
 *   character over UART.
 *
 * Input Parameters:
 *   dev - UART Device
 *   c  - Character to be sent
 *
 * Returned Value:
 *   None
 *
 ***************************************************************************/

static void bcm2711_uart_wait_send(struct uart_dev_s *dev, char c)
{

  while (!bcm2711_uart_txready(dev))
    ;
  /* Write byte (do I need to mask to avoid writing to LS8 baud rate bits?) */
  bcm2711_uart_send(dev, c);
}

/***************************************************************************
 * Name: bcm2711_uart_send
 *
 * Description:
 *   This method will send one byte on the UART
 *
 * Input Parameters:
 *   dev - UART Device
 *   c  - Character to be sent
 *
 * Returned Value:
 *   None
 *
 ***************************************************************************/

static void bcm2711_uart_send(struct uart_dev_s *dev, int c)
{
  putreg32(c, BCM_AUX_MU_IO_REG);
}

/***************************************************************************
 * Name: bcm2711_uart_receive
 *
 * Description:
 *   Called (usually) from the interrupt level to receive one
 *   character from the UART.  Error bits associated with the
 *   receipt are provided in the return 'status'.
 *
 * Input Parameters:
 *   dev    - UART Device
 *   status - Return status, zero on success
 *
 * Returned Value:
 *   Received character
 *
 ***************************************************************************/

static int bcm2711_uart_receive(struct uart_dev_s *dev, unsigned int *status)
{
  // TODO proper status
  *status = 0;                               /* OK */
  return getreg32(BCM_AUX_MU_IO_REG) & 0xff; /* Read byte */
}

/***************************************************************************
 * Name: bcm2711_uart_ioctl
 *
 * Description:
 *   All ioctl calls will be routed through this method.
 *
 * Input Parameters:
 *   filep - File Struct
 *   cmd   - ioctl Command
 *   arg   - ioctl Argument
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value is returned on any failure.
 *
 ***************************************************************************/

static int bcm2711_uart_ioctl(struct file *filep, int cmd, unsigned long arg)
{
  return -ENOSYS; // TODO
}

/***************************************************************************
 * Name: bcm2711_uart_attach
 *
 * Description:
 *   Configure the UART to operation in interrupt driven mode.
 *   This method is called when the serial port is opened.
 *   Normally, this is just after the setup() method is called,
 *   however, the serial console may operate in
 *   a non-interrupt driven mode during the boot phase.
 *
 *   RX and TX interrupts are not enabled when by the attach method
 *   (unless the hardware supports multiple levels of interrupt
 *   enabling).  The RX and TX interrupts are not enabled until
 *   the txint() and rxint() methods are called.
 *
 * Input Parameters:
 *   dev - UART Device
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value is returned on any failure.
 *
 ***************************************************************************/

static int bcm2711_uart_attach(struct uart_dev_s *dev)
{
  return -ENOSYS; // TODO
}

/***************************************************************************
 * Name: bcm2711_uart_detach
 *
 * Description:
 *   Detach UART interrupts.  This method is called when the serial port is
 *   closed normally just before the shutdown method is called.  The
 *   exception is the serial console which is never shutdown.
 *
 * Input Parameters:
 *   dev - UART Device
 *
 * Returned Value:
 *   None
 *
 ***************************************************************************/

static void bcm2711_uart_detach(struct uart_dev_s *dev)
{
  return; // TODO
}

/***************************************************************************
 * Public Functions
 ***************************************************************************/

/***************************************************************************
 * Name: arm64_earlyserialinit
 *
 * Description:
 *   Performs the low level UART initialization early in
 *   debug so that the serial console will be available
 *   during bootup.  This must be called before arm64_serialinit.
 *
 * Returned Value:
 *   None
 *
 ***************************************************************************/

void arm64_earlyserialinit(void)
{
  _err("arm64_earlyserialinit not implemented.");
  // TODO
}

/***************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Provide priority, low-level access to support OS debug
 *   writes
 *
 * Input Parameters:
 *   ch - Character to be transmitted over UART
 *
 * Returned Value:
 *   Character that was transmitted
 *
 ***************************************************************************/

int up_putc(int ch)
{
#ifdef CONSOLE_DEV
  struct uart_dev_s *dev = &CONSOLE_DEV;

  /* Check for LF */

  if (ch == '\n')
    {
      /* Add CR */

      bcm2711_uart_send(dev, '\r');
    }

  bcm2711_uart_send(dev, ch);
#endif // CONSOLE_DEV
  return ch;
}

/***************************************************************************
 * Name: arm64_serialinit
 *
 * Description:
 *   Register serial console and serial ports. This assumes
 *   that bcm2711_earlyserialinit was called previously.
 *
 * Returned Value:
 *   None
 *
 ***************************************************************************/

void arm64_serialinit(void)
{
  int ret;
  ret = uart_register("/dev/console", &CONSOLE_DEV);
  if (ret < 0)
    {
      _err("Could not register /dev/console, ret=%d\n", ret);
    }
  _err("arm64_serialinit not implemented");
  // TODO
}
