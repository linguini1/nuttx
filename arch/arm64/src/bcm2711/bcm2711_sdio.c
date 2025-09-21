/****************************************************************************
 * arch/arm64/src/bcm2711/bcm2711_sdio.c
 *
 * Author: Matteo Golin <matteo.golin@gmail.com>
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

#include <assert.h>
#include <debug.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <nuttx/arch.h>
#include <nuttx/clock.h>
#include <nuttx/mmcsd.h>
#include <nuttx/sdio.h>
#include <nuttx/semaphore.h>
#include <nuttx/wdog.h>
#include <nuttx/wqueue.h>

#include <arch/board/board.h>
#include <nuttx/irq.h>

#include "arm64_arch.h"
#include "arm64_gic.h"
#include "bcm2711_gpio.h"
#include "bcm2711_mailbox.h"
#include "bcm2711_sdio.h"
#include "chip.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* EMMC clock rates
 * NOTE: taken from LLD again
 * TODO: How to choose between rates? Kconfig? Does driver interface exist?
 */

#define EMMC_RATE_ID (400000)       /* Clock rate to get SD ID */
#define EMMC_RATE_NORMAL (25000000) /* Normal clock rate */
#define EMMC_RATE_HIGH (50000000)   /* High speed */
#define EMMC_RATE_100 (100000000)   /* Double high speed */
#define EMMC_RATE_208 (208000000)   /* Ultra high speed */

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bcm2711_sdio_dev_s
{
  struct sdio_dev_s dev; /* SDIO device for upper-half */

  const uint32_t base; /* Peripheral base address */
  uint32_t baseclk;    /* Base clock rate */
  const int slotno;    /* The slot number */
  int err;             /* The error reported from the IRQ handler */
  const uint8_t clkid; /* EMMC clock ID for mailbox to enable */
  sem_t wait;          /* Wait semaphore */

  /* Callback support */

#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
  sdio_eventset_t cbevents; /* Callback events */
  void *cbarg;              /* Callback argument */
  worker_t cb;              /* Callback that was registered */
  struct work_s cbwork;     /* Callback work queue */
#endif

  uint32_t *buffer; /* Transfer buffer */
  size_t remaining; /* Transfer length */
  bool wrxfr;       /* Write transfer (false = read) */
  bool cmdneedrst;  /* Command circuitry needs reset */
  bool dataneedrst; /* Data circuitry needs reset */

  enum sdio_clock_e cur_rate; /* Current clock rate */
  sdio_statset_t status;      /* Device status */
  sdio_eventset_t curevents;  /* Events that happened */
  sdio_eventset_t waitevents; /* Events to wait for */
  bool inited;                /* Whether the device has been initialized */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifdef CONFIG_SDIO_MUXBUS
static int bcm2711_lock(FAR struct sdio_dev_s *dev, bool lock);
#endif
static void bcm2711_reset(FAR struct sdio_dev_s *dev);
static sdio_capset_t bcm2711_capabilities(FAR struct sdio_dev_s *dev);
static sdio_statset_t bcm2711_status(FAR struct sdio_dev_s *dev);
static void bcm2711_widebus(FAR struct sdio_dev_s *dev, bool enable);
static void bcm2711_clock(FAR struct sdio_dev_s *dev,
                          enum sdio_clock_e rate);
static int bcm2711_attach(FAR struct sdio_dev_s *dev);

static int bcm2711_sendcmd(FAR struct sdio_dev_s *dev, uint32_t cmd,
                           uint32_t arg);

#ifdef CONFIG_SDIO_BLOCKSETUP
static void bcm2711_blocksetup(FAR struct sdio_dev_s *dev,
                               unsigned int blocklen, unsigned int nblocks);
#endif

static int bcm2711_recvsetup(FAR struct sdio_dev_s *dev, FAR uint8_t *buffer,
                             size_t nbytes);
static int bcm2711_sendsetup(FAR struct sdio_dev_s *dev,
                             FAR const uint8_t *buffer, size_t nbytes);
static int bcm2711_cancel(FAR struct sdio_dev_s *dev);
static int bcm2711_waitresponse(FAR struct sdio_dev_s *dev, uint32_t cmd);
static int bcm2711_recvshort(FAR struct sdio_dev_s *dev, uint32_t cmd,
                             FAR uint32_t *rshort);
static int bcm2711_recvshortcrc(FAR struct sdio_dev_s *dev, uint32_t cmd,
                                FAR uint32_t *rshort);
static int bcm2711_recvnotimpl(FAR struct sdio_dev_s *dev, uint32_t cmd,
                               FAR uint32_t *rnotimpl);
static int bcm2711_recvlong(FAR struct sdio_dev_s *dev, uint32_t cmd,
                            FAR uint32_t rlong[4]);

static void bcm2711_waitenable(FAR struct sdio_dev_s *dev,
                               sdio_eventset_t eventset, uint32_t timeout);
static sdio_eventset_t bcm2711_eventwait(FAR struct sdio_dev_s *dev);
static void bcm2711_callbackenable(FAR struct sdio_dev_s *dev,
                                   sdio_eventset_t eventset);

#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
static int bcm2711_registercallback(FAR struct sdio_dev_s *dev,
                                    worker_t callback, FAR void *arg);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool g_emmc_irqinit = false;

/* TODO: revisit when EMMC1 supported */

#ifdef CONFIG_BCM2711_EMMC1
#error "EMMC1 is not yet supported/tested."
#endif

#ifdef CONFIG_BCM2711_EMMC2
static struct bcm2711_sdio_dev_s g_emmc2 =
{
  .dev =
  {
#ifdef CONFIG_SDIO_MUXBUS
    .lock = bcm2711_lock,
#endif

    .reset = bcm2711_reset,
    .capabilities = bcm2711_capabilities,
    .status = bcm2711_status,
    .widebus = bcm2711_widebus,
    .clock = bcm2711_clock,
    .attach = bcm2711_attach,

    .sendcmd = bcm2711_sendcmd,

#ifdef CONFIG_SDIO_BLOCKSETUP
    .blocksetup = bcm2711_blocksetup,
#endif

    .recvsetup = bcm2711_recvsetup,
    .sendsetup = bcm2711_sendsetup,
    .cancel = bcm2711_cancel,
    .waitresponse = bcm2711_waitresponse,
    .recv_r1 = bcm2711_recvshortcrc,
    .recv_r2 = bcm2711_recvlong,
    .recv_r3 = bcm2711_recvshort,
    .recv_r4 = bcm2711_recvnotimpl,
    .recv_r5 = bcm2711_recvnotimpl,
    .recv_r6 = bcm2711_recvshortcrc,
    .recv_r7 = bcm2711_recvshort,

    .waitenable = bcm2711_waitenable,
    .eventwait = bcm2711_eventwait,
    .callbackenable = bcm2711_callbackenable,
#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
    .registercallback = bcm2711_registercallback,
#endif

    /* TODO: DMA implementation */

    .gotextcsd = NULL,
  },
  .base = BCM_EMMC2_BASEADDR,
  .wait = SEM_INITIALIZER(0),
  .slotno = 2,
  .clkid = MBOX_CLK_EMMC2,
  .err = 0,
  .status = 0,
  .waitevents = 0,
  .curevents = 0,
  .cur_rate = CLOCK_SDIO_DISABLED,
  .inited = false,

#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)
  .cb = NULL,
  .cbarg = NULL,
  .cbevents = 0,
#endif
};
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: get_clock_divider
 *
 * Description:
 *   Calculates clock divider for target clock rate with some known base
 *   clock.
 *   NOTE: This was taken from rockytriton/LLD, a bare-metal RPi EMMC driver.
 *
 * Input Parameters:
 *   base   - The base block rate to be divided
 *   target - The target clock rate
 *
 * Returned Value:
 *   The calculated clock divider that will get as close as possible to the
 *   target rate.
 *
 ****************************************************************************/

static uint32_t get_clock_divider(uint32_t base_clock, uint32_t target_rate)
{
  uint32_t target_div = 1;
  uint32_t freq_sel;
  uint32_t upper;
  uint32_t ret;

  if (target_rate <= base_clock)
    {
      target_div = base_clock / target_rate;

      if (base_clock % target_rate)
        {
          target_div = 0;
        }
    }

  int div = -1;
  for (int fb = 31; fb >= 0; fb--)
    {
      uint32_t bt = (1 << fb);

      if (target_div & bt)
        {
          div = fb;
          target_div &= ~(bt);

          if (target_div)
            {
              div++;
            }

          break;
        }
    }

  /* Out of bounds, clip division to as much as possible */

  if (div == -1 || div >= 32)
    {
      div = 31;
    }

  if (div != 0)
    {
      div = (1 << (div - 1));
    }

  if (div >= 0x400)
    {
      div = 0x3ff;
    }

  freq_sel = div & 0xff;
  upper = (div >> 8) & 0x3;
  ret = (freq_sel << 8) | (upper << 6) | (0 << 5);

  return ret;
}

/****************************************************************************
 * Name: bcm2711_clk_waitstable
 *
 * Description:
 *   Busy-waits until the EMMC controller clock has become stable.
 *   TODO: can we do better than a busy wait?
 *
 * Input Parameters:
 *   priv - The device whose EMMC controller clock we are waiting for
 *
 ****************************************************************************/

static void bcm2711_clk_waitstable(struct bcm2711_sdio_dev_s *priv)
{
  while (!(getreg32(BCM_SDIO_CONTROL1(priv->base)) &
           BCM_SDIO_CONTROL1_CLK_STABLE))
    ;
}

/****************************************************************************
 * Name: bcm2711_fill_txfifo
 *
 * Description:
 *   Fill the TX FIFO with as much data as possible from the transfer buffer.
 *   NOTE: Assumes data for transfer is 32b aligned, assumes write transfer.
 *
 * Input Parameters:
 *   priv - The device the transfer is happening on.
 *
 ****************************************************************************/

void bcm2711_fill_txfifo(struct bcm2711_sdio_dev_s *priv)
{
  while (priv->remaining > 0 && (getreg32(BCM_SDIO_INTERRUPT(priv->base)) &
                                 BCM_SDIO_INTERRUPT_WRITE_RDY))
    {
      putreg32(*priv->buffer, BCM_SDIO_DATA(priv->base));
      priv->buffer++;
      priv->remaining -= 4;
    }
}

/****************************************************************************
 * Name: bcm2711_drain_rxfifo
 *
 * Description:
 *   Drain the RX FIFO into the transfer buffer.
 *   NOTE: Assumes data for transfer is 32b aligned, assumes read transfer.
 *
 * Input Parameters:
 *   priv - The device the transfer is happening on.
 *
 ****************************************************************************/

void bcm2711_drain_rxfifo(struct bcm2711_sdio_dev_s *priv)
{
  while (priv->remaining > 0 && (getreg32(BCM_SDIO_INTERRUPT(priv->base)) &
                                 BCM_SDIO_INTERRUPT_READ_RDY))
    {
      *priv->buffer = getreg32(BCM_SDIO_DATA(priv->base));
      priv->buffer++;
      priv->remaining -= 4;
    }
}

/****************************************************************************
 * Name: bcm2711_resetcmd
 *
 * Description:
 *   Reset the command handling circuit. Necessary when command errors
 *   happen.
 *
 * Input Parameters:
 *   priv - The SDIO interface to reset the command circuit of.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void bcm2711_resetcmd(struct bcm2711_sdio_dev_s *priv)
{
  putreg32(BCM_SDIO_CONTROL1_SRST_CMD, BCM_SDIO_CONTROL1(priv->base));

  /* Now we busy wait until the controller reports that it's done resetting
   * by setting the reset flag to 0.
   * TODO: can we do better than a busy wait?
   */

  while ((getreg32(BCM_SDIO_CONTROL1(priv->base)) &
          BCM_SDIO_CONTROL1_SRST_CMD) != 0)
    ;

  mcinfo("Reset %d command circuit.", priv->slotno);
  priv->cmdneedrst = false;
}

/****************************************************************************
 * Name: bcm2711_resetdata
 *
 * Description:
 *   Reset the data handling circuit. Necessary when data errors happen.
 *
 * Input Parameters:
 *   priv - The SDIO interface to reset the data circuit of.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void bcm2711_resetdata(struct bcm2711_sdio_dev_s *priv)
{
  mcinfo("Resetting %d data circuit...", priv->slotno);
  putreg32(BCM_SDIO_CONTROL1_SRST_DATA, BCM_SDIO_CONTROL1(priv->base));

  /* Now we busy wait until the controller reports that it's done resetting
   * by setting the reset flag to 0.
   * TODO: can we do better than a busy wait?
   */

  while ((getreg32(BCM_SDIO_CONTROL1(priv->base)) &
          BCM_SDIO_CONTROL1_SRST_DATA) != 0)
    ;

  mcinfo("Reset %d data circuit.", priv->slotno);
  priv->dataneedrst = false;
}

/****************************************************************************
 * Name: bcm2711_lock
 *
 * Description:
 *   Locks the bus.Function calls low-level multiplexed bus routines to
 *   resolve bus requests and acknowledge issues.
 *
 * Input Parameters:
 *   dev    - An instance of the SDIO device interface
 *   lock   - TRUE to lock, FALSE to unlock.
 *
 * Returned Value:
 *   OK on success; a negated errno on failure
 *
 ****************************************************************************/

#ifdef CONFIG_SDIO_MUXBUS
static int bcm2711_lock(FAR struct sdio_dev_s *dev, bool lock)
{
  return OK;
}
#endif

/****************************************************************************
 * Name: bcm2711_reset
 *
 * Description:
 *   Reset the SDIO controller.  Undo all setup and initialization.
 *
 * Input Parameters:
 *   dev    - An instance of the SDIO device interface
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void bcm2711_reset(FAR struct sdio_dev_s *dev)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;

  /* Reset is performed by resetting host controller. */

  putreg32(BCM_SDIO_CONTROL1_SRST_HC, BCM_SDIO_CONTROL1(priv->base));

  /* Now we busy wait until the controller reports that it's done resetting
   * by setting all the reset flags to 0.
   * TODO: can we do better than a busy wait?
   */

  while ((getreg32(BCM_SDIO_CONTROL1(priv->base)) &
          (BCM_SDIO_CONTROL1_SRST_HC | BCM_SDIO_CONTROL1_SRST_DATA |
           BCM_SDIO_CONTROL1_SRST_CMD)) != 0)
    ;

  /* Enable VDD1 bus power for SD card */

  modreg32(0x0f << 8, 0x0f << 8, BCM_SDIO_CONTROL0(priv->base));

  /* TODO: reset wdog and etc. */

  nxsem_reset(&priv->wait, 0);
  priv->cmdneedrst = false;
  priv->dataneedrst = false;
  mcinfo("Reset slot %d", priv->slotno);
}

/****************************************************************************
 * Name: bcm2711_capabilities
 *
 * Description:
 *   Get capabilities (and limitations) of the SDIO driver (optional)
 *
 * Input Parameters:
 *   dev   - Device-specific state data
 *
 * Returned Value:
 *   Returns a bitset of status values (see SDIO_CAPS_* defines)
 *
 ****************************************************************************/

static sdio_capset_t bcm2711_capabilities(FAR struct sdio_dev_s *dev)
{
  (void)(dev);
  return SDIO_CAPS_4BIT | SDIO_CAPS_8BIT | SDIO_CAPS_MMC_HS_MODE;
}

/****************************************************************************
 * Name: bcm2711_status
 *
 * Description:
 *   Get SDIO status.
 *
 * Input Parameters:
 *   dev   - Device-specific state data
 *
 * Returned Value:
 *   Returns a bitset of status values (see SDIO_STATUS_* defines)
 *
 ****************************************************************************/

static sdio_statset_t bcm2711_status(FAR struct sdio_dev_s *dev)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  sdio_statset_t stat = 0;

#ifdef CONFIG_MMCSD_HAVE_CARDDETECT
#endif

  /* TODO: for now, unsure how to detect card so always lie that there is a
   * card in the slot and have the MMCSD upper-half perform the probe.
   */

  stat |= SDIO_STATUS_PRESENT;

  /* This info statement shows the slot status, which might tell us about
   * what's present or not.
   */

  mcinfo("Slot %d status: %04x", priv->slotno,
         getreg32(BCM_SDIO_SLOTISR_VER(priv->base)) &
             BCM_SDIO_SLOTISR_VER_SLOT_STATUS);
  return stat;
}

/****************************************************************************
 * Name: bcm2711_widebus
 *
 * Description:
 *   Called after change in Bus width has been selected (via ACMD6).  Most
 *   controllers will need to perform some special operations to work
 *   correctly in the new bus mode.
 *
 * Input Parameters:
 *   dev  - An instance of the SDIO device interface
 *   wide - true: wide bus (4-bit) bus mode enabled
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void bcm2711_widebus(FAR struct sdio_dev_s *dev, bool enable)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  uint32_t regval = getreg32(BCM_SDIO_CONTROL0(priv->base));

  if (enable)
    {
      /* Use 4b data bus by enabling 4b, disabling 8b */

      regval |= BCM_SDIO_CONTROL0_HCTL_DWIDTH;
      regval &= ~BCM_SDIO_CONTROL0_HCTL_8BIT;
    }
  else
    {
      /* Use 1-bit data bus by disabling 4b and 8b widths */

      regval &=
          ~(BCM_SDIO_CONTROL0_HCTL_DWIDTH | BCM_SDIO_CONTROL0_HCTL_8BIT);
    }

  /* TODO: How can this be configured for 8-bit? Does the SDIO driver support
   * this?
   */

  putreg32(regval, BCM_SDIO_CONTROL0(priv->base));
  mcinfo("%d widebus: %d", priv->slotno, enable);
}

/****************************************************************************
 * Name: bcm2711_clock
 *
 * Description:
 *   Enable/disable SDIO clocking
 *
 * Input Parameters:
 *   dev  - An instance of the SDIO device interface
 *   rate - Specifies the clocking to use (see enum sdio_clock_e)
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void bcm2711_clock(FAR struct sdio_dev_s *dev, enum sdio_clock_e rate)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  uint32_t divider;
  bool enable;

  switch (rate)
    {
    case CLOCK_SDIO_DISABLED:
      enable = false;
      divider = 0;
      mcinfo("EMMC%d clock disabled.", priv->slotno);
      break;

    case CLOCK_IDMODE:
      mcinfo("EMMC%d clock in ID mode.", priv->slotno);
      enable = true;

      /* Calculate the appropriate clock divider */

      divider = get_clock_divider(priv->baseclk, EMMC_RATE_ID);
      break;

      /* TODO: let user select transfer speed instead of using normal rate */

    case CLOCK_MMC_TRANSFER:
      enable = true;
      divider = get_clock_divider(priv->baseclk, EMMC_RATE_NORMAL);
      mcinfo("EMMC%d clock in MMC transfer mode.", priv->slotno);
      break;

    case CLOCK_SD_TRANSFER_1BIT:
      enable = true;
      divider = get_clock_divider(priv->baseclk, EMMC_RATE_NORMAL);
      mcinfo("EMMC%d clock in 1b SD transfer mode.", priv->slotno);
      break;

    case CLOCK_SD_TRANSFER_4BIT:
      enable = true;
      divider = get_clock_divider(priv->baseclk, EMMC_RATE_NORMAL);
      mcinfo("EMMC%d clock in 4b SD transfer mode.", priv->slotno);
      break;

    default:
      DEBUGASSERT(false && "Should never reach here.");
      return;
    }

  /* First, disable the clock before making changes. */

  modreg32(0, BCM_SDIO_CONTROL1_CLK_EN | BCM_SDIO_CONTROL1_CLK_INTLEN,
           BCM_SDIO_CONTROL1(priv->base));

  /* Set the appropriate clock divider */

  modreg32(divider,
           BCM_SDIO_CONTROL1_CLK_FREQ8 | BCM_SDIO_CONTROL1_CLK_FREQ_MS2,
           BCM_SDIO_CONTROL1(priv->base));

  /* Set timeout */

  modreg32(0xb << BCM_SDIO_CONTROL1_DATA_TOUNIT_SHIFT,
           BCM_SDIO_CONTROL1_DATA_TOUNIT, BCM_SDIO_CONTROL1(priv->base));

  /* Enable the clock and wait for it to stabilize. */

  if (enable)
    {
      modreg32(~0, BCM_SDIO_CONTROL1_CLK_EN | BCM_SDIO_CONTROL1_CLK_INTLEN,
               BCM_SDIO_CONTROL1(priv->base));
      bcm2711_clk_waitstable(priv);
    }

  /* If we got all the way here, the clock rate was set successfully (as far
   * as we know), so we can update the current clock rate to save computation
   * later.
   */

  priv->cur_rate = rate;
}

/****************************************************************************
 * Name: bcm2711_emmc_handler_internal
 *
 * Description:
 *   Interrupt handling logic that works for all Arasan interfaces.
 *
 * Input Parameters:
 *   priv - The SDIO controller interface
 *
 ****************************************************************************/

static int bcm2711_emmc_handler_internal(struct bcm2711_sdio_dev_s *priv)
{
  uint32_t flags;
  uint32_t acknowledged = 0;

  /* Get interrupt flags */

  flags = getreg32(BCM_SDIO_INTERRUPT(priv->base));
  mcinfo("Slot %d int flags %08x", priv->slotno, flags);

  /* Set internal error and events to 0, then check if an error has occurred
   * using the master error flag. If one has, acknowledge all errors and set
   * events accordingly.
   */

  if (flags & BCM_SDIO_INTERRUPT_ERR)
    {
      priv->curevents |= SDIOWAIT_ERROR;

      /* I/O errors */

      if (flags & (BCM_SDIO_INTERRUPT_ERR | BCM_SDIO_INTERRUPT_CCRC_ERR |
                   BCM_SDIO_INTERRUPT_CEND_ERR |
                   BCM_SDIO_INTERRUPT_CBAD_ERR |
                   BCM_SDIO_INTERRUPT_DCRC_ERR |
                   BCM_SDIO_INTERRUPT_DEND_ERR |
                   BCM_SDIO_INTERRUPT_ACMD_ERR))
        {
          acknowledged |=
              (BCM_SDIO_INTERRUPT_ERR | BCM_SDIO_INTERRUPT_CCRC_ERR |
               BCM_SDIO_INTERRUPT_CEND_ERR | BCM_SDIO_INTERRUPT_CBAD_ERR |
               BCM_SDIO_INTERRUPT_DCRC_ERR | BCM_SDIO_INTERRUPT_DEND_ERR |
               BCM_SDIO_INTERRUPT_ACMD_ERR);
          priv->err = -EIO;
        }

      /* Timeout errors */

      if (flags & (BCM_SDIO_INTERRUPT_CTO_ERR | BCM_SDIO_INTERRUPT_DTO_ERR))
        {
          priv->cmdneedrst = flags & BCM_SDIO_INTERRUPT_CTO_ERR;
          priv->dataneedrst = flags & BCM_SDIO_INTERRUPT_DTO_ERR;

          acknowledged |=
              (BCM_SDIO_INTERRUPT_CTO_ERR | BCM_SDIO_INTERRUPT_DTO_ERR);
          priv->curevents |= SDIOWAIT_TIMEOUT;
          priv->err = -ETIMEDOUT;
        }
    }

  if (flags & BCM_SDIO_INTERRUPT_CARD)
    {
      acknowledged |= BCM_SDIO_INTERRUPT_CARD;
    }

  /* Data transfer interrupts */

  if (flags & BCM_SDIO_INTERRUPT_WRITE_RDY)
    {
      DEBUGASSERT(priv->wrxfr);
      bcm2711_fill_txfifo(priv);
      acknowledged |= BCM_SDIO_INTERRUPT_WRITE_RDY;
    }

  if (flags & BCM_SDIO_INTERRUPT_READ_RDY)
    {
      DEBUGASSERT(!priv->wrxfr);
      bcm2711_drain_rxfifo(priv);
      acknowledged |= BCM_SDIO_INTERRUPT_READ_RDY;
    }

  if (flags & BCM_SDIO_INTERRUPT_CMD_DONE)
    {
      priv->curevents |= SDIOWAIT_CMDDONE | SDIOWAIT_RESPONSEDONE;
      acknowledged |= BCM_SDIO_INTERRUPT_CMD_DONE;
    }

  if (flags & BCM_SDIO_INTERRUPT_DATA_DONE)
    {
      priv->curevents |= SDIOWAIT_TRANSFERDONE;
      acknowledged |= BCM_SDIO_INTERRUPT_DATA_DONE;
    }

  /* Acknowledge handled interrupts */

  putreg32(acknowledged, BCM_SDIO_INTERRUPT(priv->base));

  /* Post events if we're waiting for any of them */

  if (priv->curevents & priv->waitevents)
    {
      nxsem_post(&priv->wait);
    }

  return OK;
}

/****************************************************************************
 * Name: bcm2711_emmc_handler
 *
 * Description:
 *   Interrupt handler for EMMC interrupt
 *
 ****************************************************************************/

static int bcm2711_emmc_handler(int irq, void *context, void *arg)
{
  (void)(irq);
  (void)(context);
  (void)(arg);
  int err;

  /* Handle interrupts for all interfaces. The `handler_internal` function
   * checks for interrupts itself.
   */

  err = bcm2711_emmc_handler_internal(&g_emmc2);
  if (err)
    {
      return err;
    }

  /* TODO: Re-visit when EMMC1 implemented */

  return OK;
}

/****************************************************************************
 * Name: bcm2711_attach
 *
 * Description:
 *   Attach and prepare interrupts
 *
 * Input Parameters:
 *   dev    - An instance of the SDIO device interface
 *
 * Returned Value:
 *   OK on success; A negated errno on failure.
 *
 ****************************************************************************/

static int bcm2711_attach(FAR struct sdio_dev_s *dev)
{
  int err;
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;

  /* No need to register interrupt handler twice if already registered */

  if (g_emmc_irqinit)
    {
      mcinfo("EMMC interrupt handler already attached.");
      return 0;
    }

  err = irq_attach(BCM_IRQ_VC_EMMC, bcm2711_emmc_handler, NULL);
  if (err < 0)
    {
      mcerr("Couldn't attach EMMC interrupt handler: %d", err);
      return err;
    }

  mcinfo("EMMC interrupt handler attached for slot %d.", priv->slotno);

  /* Enable and unmask all interrupts. The interrupt mask is never used to
   * disable interrupts; interrupts will only be enabled/disabled via the
   * IRPT_EN register.
   */

  putreg32(BCM_SDIO_IRPT_MASK_ALL, BCM_SDIO_IRPT_MASK(priv->base));
  putreg32(BCM_SDIO_IRPT_EN_ALL, BCM_SDIO_IRPT_EN(priv->base));

  /* Enable the interrupt handler */

  arm64_gic_irq_set_priority(BCM_IRQ_VC_EMMC, 0, IRQ_TYPE_LEVEL);
  up_enable_irq(BCM_IRQ_VC_EMMC);
  g_emmc_irqinit = true;
  mcinfo("EMMC IRQ enabled.");
  return 0;
}

/****************************************************************************
 * Name: bcm2711_sendcmd
 *
 * Description:
 *   Send the SDIO command
 *
 * Input Parameters:
 *   dev  - An instance of the SDIO device interface
 *   cmd  - The command to send.  See 32-bit command definitions above.
 *   arg  - 32-bit argument required with some commands
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static int bcm2711_sendcmd(FAR struct sdio_dev_s *dev, uint32_t cmd,
                           uint32_t arg)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  uint32_t cmdtm_val = 0;

  mcinfo("CMD %u: cmd=%08x, arg=%08x", cmd & MMCSD_CMDIDX_MASK, cmd, arg);

  /* We're starting a fresh command (new transaction), so errors and events
   * start fresh.
   */

  priv->err = 0;
  priv->curevents = 0;

  /* Reset command circuit if necessary */

  if (priv->cmdneedrst)
    {
      mcwarn("Slot %d command circuit needs reset.", priv->slotno);
      bcm2711_resetcmd(priv);
      bcm2711_clock(dev, priv->cur_rate);
    }

  /* Reset command circuit if necessary */

  if (priv->dataneedrst)
    {
      mcwarn("Slot %d data circuit needs reset.", priv->slotno);
      bcm2711_resetdata(priv);
    }

  /* Put argument in ARG1 /unless/ the argument is to be sent with ACMD23.
   * Arguments must be put in the register before the command is sent.
   */

  if (cmd == SD_ACMD23)
    {
      putreg32(arg, BCM_SDIO_ARG2(priv->base));
    }
  else
    {
      putreg32(arg, BCM_SDIO_ARG1(priv->base));
    }

  /* Set the response type */

  switch (cmd & MMCSD_RESPONSE_MASK)
    {
    case MMCSD_NO_RESPONSE:
      cmdtm_val |= BCM_SDIO_CMDTM_CMD_RSPNS_TYPE_NONE;
      break;
    case MMCSD_R1B_RESPONSE:
      cmdtm_val |= BCM_SDIO_CMDTM_CMD_RSPNS_TYPE_48B;
      break;
    case MMCSD_R1_RESPONSE:
    case MMCSD_R3_RESPONSE:
    case MMCSD_R4_RESPONSE:
    case MMCSD_R5_RESPONSE:
    case MMCSD_R6_RESPONSE:
    case MMCSD_R7_RESPONSE:
      cmdtm_val |= BCM_SDIO_CMDTM_CMD_RSPNS_TYPE_48;
      break;
    case MMCSD_R2_RESPONSE:
      cmdtm_val |= BCM_SDIO_CMDTM_CMD_RSPNS_TYPE_136;

      /* TODO: Check CRC on these, should it be checked elsewhere? */

      cmdtm_val |= BCM_SDIO_CMDTM_CMD_CRCCHK_EN;
      break;
    }

  /* Check if command involves data transfer */

  if (cmd & MMCSD_DATAXFR_MASK)
    {
      cmdtm_val |= BCM_SDIO_CMDTM_CMD_ISDATA;
    }

  /* Set flag according to data transfer direction (0 write, 1 read) */

  if (cmd & MMCSD_RDDATAXFR)
    {
      cmdtm_val |= BCM_SDIO_CMDTM_TM_DAT_DIR;
    }

  /* Set multi-block transfer flag */

  if (cmd & MMCSD_MULTIBLOCK)
    {
      cmdtm_val |= BCM_SDIO_CMDTM_TM_MULTI_BLOCK;
    }

  /* Set index of command */

  cmdtm_val |= ((cmd & MMCSD_CMDIDX_MASK) >> MMCSD_CMDIDX_SHIFT)
               << BCM_SDIO_CMDTM_CMD_INDEX_SHIFT;

  /* Populate command register with correct values */

  putreg32(cmdtm_val, BCM_SDIO_CMDTM(priv->base));
  return 0;
}

/****************************************************************************
 * Name: bcm2711_blocksetup
 *
 * Description:
 *   Configure block size and the number of blocks for next transfer
 *
 * Input Parameters:
 *   dev       - An instance of the SDIO device interface
 *   blocklen  - The selected block size.
 *   nblocklen - The number of blocks to transfer
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

#ifdef CONFIG_SDIO_BLOCKSETUP
static void bcm2711_blocksetup(FAR struct sdio_dev_s *dev,
                               unsigned int blocklen, unsigned int nblocks)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  uint32_t regval = 0;

  DEBUGASSERT(nblocks <= 0xffff); /* Maximum transfer count */

  regval |= (blocklen & BCM_SDIO_BLKSIZECNT_BLKSIZE_MASK);
  regval |= ((nblocks & BCM_SDIO_BLKSIZECNT_BLKCNT_MASK)
             << BCM_SDIO_BLKSIZECNT_BLKCNT_SHIFTLEN);
  putreg32(regval, BCM_SDIO_BLKSIZECNT(priv->base));
  mcinfo("Slot %" PRId32 " block size %" PRIu32 ", count %" PRIu32 "\n",
         priv->slotno, blocklen, nblocks);
}
#endif

/****************************************************************************
 * Name: bcm2711_recvsetup
 *
 * Description:
 *   Setup hardware in preparation for data transfer from the card in non-DMA
 *   (interrupt driven mode).  This method will do whatever controller setup
 *   is necessary.  This would be called for SD memory just BEFORE sending
 *   CMD13 (SEND_STATUS), CMD17 (READ_SINGLE_BLOCK), CMD18
 *   (READ_MULTIPLE_BLOCKS), ACMD51 (SEND_SCR), etc.  Normally,
 *   SDIO_WAITEVENT will be called to receive the indication that the
 *   transfer is complete.
 *
 * Input Parameters:
 *   dev    - An instance of the SDIO device interface
 *   buffer - Address of the buffer in which to receive the data
 *   nbytes - The number of bytes in the transfer
 *
 * Returned Value:
 *   Number of bytes sent on success; a negated errno on failure
 *
 ****************************************************************************/

static int bcm2711_recvsetup(FAR struct sdio_dev_s *dev, FAR uint8_t *buffer,
                             size_t nbytes)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  DEBUGASSERT(buffer != NULL);
  DEBUGASSERT(((uintptr_t)buffer & 0x3) == 0); /* 32b aligned */

  mcinfo("Slot %d recv to buf=%p", priv->slotno, buffer);
  priv->buffer = (uint32_t *)buffer;
  priv->remaining = nbytes;
  priv->wrxfr = false;

  /* Enable read-ready interrupts */

  modreg32(BCM_SDIO_IRPT_EN_READ_RDY, BCM_SDIO_IRPT_EN_READ_RDY,
           BCM_SDIO_IRPT_EN(priv->base));
  return 0;
}

/****************************************************************************
 * Name: bcm2711_sendsetup
 *
 * Description:
 *   Setup hardware in preparation for data transfer from the card.  This
 *   method will do whatever controller setup is necessary.  This would be
 *   called for SD memory just AFTER sending CMD24 (WRITE_BLOCK), CMD25
 *   (WRITE_MULTIPLE_BLOCK), ... and before SDIO_SENDDATA is called.
 *
 * Input Parameters:
 *   dev    - An instance of the SDIO device interface
 *   buffer - Address of the buffer containing the data to send
 *   nbytes - The number of bytes in the transfer
 *
 * Returned Value:
 *   Number of bytes sent on success; a negated errno on failure
 *
 ****************************************************************************/

static int bcm2711_sendsetup(FAR struct sdio_dev_s *dev,
                             FAR const uint8_t *buffer, size_t nbytes)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  DEBUGASSERT(buffer != NULL);
  DEBUGASSERT(((uintptr_t)buffer & 0x3) == 0); /* 32b aligned */

  mcinfo("Slot %d send from buf=%p", priv->slotno, buffer);
  priv->buffer = (uint32_t *)buffer;
  priv->remaining = nbytes;
  priv->wrxfr = true;

  /* Enable write-ready interrupts */

  modreg32(BCM_SDIO_IRPT_EN_WRITE_RDY, BCM_SDIO_IRPT_EN_WRITE_RDY,
           BCM_SDIO_IRPT_EN(priv->base));
  return 0;
}

/****************************************************************************
 * Name: bcm2711_cancel
 *
 * Description:
 *   Cancel the data transfer setup of SDIO_RECVSETUP, SDIO_SENDSETUP,
 *   SDIO_DMARECVSETUP or SDIO_DMASENDSETUP.  This must be called to cancel
 *   the data transfer setup if, for some reason, you cannot perform the
 *   transfer.
 *
 * Input Parameters:
 *   dev  - An instance of the SDIO device interface
 *
 * Returned Value:
 *   OK is success; a negated errno on failure
 *
 ****************************************************************************/

static int bcm2711_cancel(FAR struct sdio_dev_s *dev)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  mcinfo("Slot %d cancelled.", priv->slotno);

  /* Disable all interrupts */

  modreg32(0, BCM_SDIO_IRPT_EN_ALL, BCM_SDIO_IRPT_EN(priv->base));

  /* Clear all interrupt flags */

  modreg32(0, BCM_SDIO_INTERRUPT_ALL, BCM_SDIO_INTERRUPT(priv->base));

  /* TODO cancel watchdog? */

  /* Clear transfer progress */

  priv->buffer = NULL;
  priv->remaining = 0;
  return 0;
}

/****************************************************************************
 * Name: bcm2711_waitresponse
 *
 * Description:
 *   Poll-wait for the response to the last command to be ready.  This
 *   function should be called even after sending commands that have no
 *   response (such as CMD0) to make sure that the hardware is ready to
 *   receive the next command.
 *
 * Input Parameters:
 *   dev  - An instance of the SDIO device interface
 *   cmd  - The command that was sent. See 32-bit command definitions above.
 *
 * Returned Value:
 *   OK is success; a negated errno on failure
 *
 ****************************************************************************/

static int bcm2711_waitresponse(FAR struct sdio_dev_s *dev, uint32_t cmd)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;

  mcinfo("Waiting for response to cmd=%04x", cmd);

  /* Enable command done interrupts to check for finished command */

  modreg32(BCM_SDIO_IRPT_EN_CMD_DONE, BCM_SDIO_IRPT_EN_CMD_DONE,
           BCM_SDIO_IRPT_EN(priv->base));

  /* Poll for command done, since this function requires 'poll-wait' */

  /* TODO: make this able to time out */

  do
    {
      /* Check for card error */

      if (priv->err)
        {
          mcerr("Got error: %d", priv->err);
          return priv->err;
        }
    }
  while ((!(priv->curevents & SDIOWAIT_CMDDONE)));

  return 0;
}

/****************************************************************************
 * Name: bcm2711_recvshortcrc
 *
 * Description:
 *
 * Input Parameters:
 *   dev - An instance of the SD card device interface
 *   cmd - The command that was sent
 *   rshort - The buffer to recv into
 *
 * Returned Value:
 *   Number of bytes sent on success; a negated errno on failure.  Here a
 *   failure means only a failure to obtain the requested response (due to
 *   transport problem -- timeout, CRC, etc.). The implementation only
 *   assures that the response is returned intact and does not check errors
 *   within the response itself.
 *
 ****************************************************************************/

static int bcm2711_recvshortcrc(FAR struct sdio_dev_s *dev, uint32_t cmd,
                                FAR uint32_t *rshort)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  DEBUGASSERT(rshort != NULL);
  mcinfo("Slot %d cmd=%04x", priv->slotno, cmd);

  /* Should only be called for R1, R1B, R6 responses */

  if ((cmd & MMCSD_RESPONSE_MASK) != MMCSD_R1_RESPONSE &&
      (cmd & MMCSD_RESPONSE_MASK) != MMCSD_R1B_RESPONSE &&
      (cmd & MMCSD_RESPONSE_MASK) != MMCSD_R6_RESPONSE)
    {
      mcerr("Expected R1, R1B or R6 response to cmd=%04x", cmd);
      return -EINVAL;
    }

  /* Check for error */

  if (priv->err)
    {
      mcerr("Error: %d", priv->err);
      return priv->err;
    }

  /* Get value */

  *rshort = getreg32(BCM_SDIO_RESP0(priv->base));
  return 0;
}

/****************************************************************************
 * Name: bcm2711_recvshort
 *
 * Description:
 *
 * Input Parameters:
 *   dev - An instance of the SD card device interface
 *   cmd - The command that was sent
 *   rshort - The buffer to recv into
 *
 * Returned Value:
 *   Number of bytes sent on success; a negated errno on failure.  Here a
 *   failure means only a failure to obtain the requested response (due to
 *   transport problem -- timeout, CRC, etc.). The implementation only
 *   assures that the response is returned intact and does not check errors
 *   within the response itself.
 *
 ****************************************************************************/

static int bcm2711_recvshort(FAR struct sdio_dev_s *dev, uint32_t cmd,
                             FAR uint32_t *rshort)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  DEBUGASSERT(rshort != NULL);
  mcinfo("Slot %d cmd=%04x", priv->slotno, cmd);

  /* This should only be used for R7 and R3 */

  if ((cmd & MMCSD_RESPONSE_MASK) != MMCSD_R3_RESPONSE &&
      (cmd & MMCSD_RESPONSE_MASK) != MMCSD_R7_RESPONSE)
    {
      mcerr("Expected R3 or R7 in response to cmd=%04x\n", cmd);
      return -EINVAL;
    }

  /* Check for errors */

  if (priv->err)
    {
      mcerr("Error: %d", priv->err);
      return priv->err;
    }

  /* Read short value */

  *rshort = getreg32(BCM_SDIO_RESP0(priv->base));
  return 0;
}

/****************************************************************************
 * Name: bcm2711_recvlong
 *
 * Description:
 *
 * Input Parameters:
 *   dev - An instance of the SD card device interface
 *   cmd - The command that was sent
 *   rlong - The buffer to receive into
 *
 * Returned Value:
 *   Number of bytes sent on success; a negated errno on failure. Here a
 *   failure means only a failure to obtain the requested response (due to
 *   transport problem -- timeout, CRC, etc.). The implementation only
 *   assures that the response is returned intact and does not check errors
 *   within the response itself.
 *
 ****************************************************************************/

static int bcm2711_recvlong(FAR struct sdio_dev_s *dev, uint32_t cmd,
                            FAR uint32_t rlong[4])
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  uint32_t rsp3;
  uint32_t rsp2;
  uint32_t rsp1;
  uint32_t rsp0;

  DEBUGASSERT(rlong != NULL);
  mcinfo("Slot %d cmd=%04x", priv->slotno, cmd);

  /* R2 should be the only response type for this function */

  if ((cmd & MMCSD_RESPONSE_MASK) != MMCSD_R2_RESPONSE)
    {
      mcerr("Expected R2 in response to cmd=%04x\n", cmd);
      return -EINVAL;
    }

  /* Some kind of transfer error happened */

  if (priv->err)
    {
      mcerr("Error: %d", priv->err);
      return priv->err;
    }

  /* Get long response */

  /* This is necessary because the SDMMC controller removes the trailing CRC
   * from the command. This results in two leading 0s and missing trailing
   * byte, which causes the upper-half to fail since it expects the CRC.
   *
   * Ex: 400e00325b590000e9e57f800a4040f1 becomes
   * 00400e00325b590000e9e57f800a4040
   */

  rsp3 = getreg32(BCM_SDIO_RESP3(priv->base));
  rsp2 = getreg32(BCM_SDIO_RESP2(priv->base));
  rsp1 = getreg32(BCM_SDIO_RESP1(priv->base));
  rsp0 = getreg32(BCM_SDIO_RESP0(priv->base));

  rlong[0] = rsp3 << 8 | rsp2 >> 24;
  rlong[1] = rsp2 << 8 | rsp1 >> 24;
  rlong[2] = rsp1 << 8 | rsp0 >> 24;
  rlong[3] = rsp0 << 8;

  return 0;
}

/****************************************************************************
 * Name: bcm2711_recvnotimpl
 *
 * Description: Handler for unimplemented receive functionality
 *
 * Input Parameters:
 *   dev - An instance of the SD card device interface
 *   cmd - The command that was sent
 *   rnotimpl - Unused
 *
 * Returned Value: -ENOSYS
 *
 ****************************************************************************/

static int bcm2711_recvnotimpl(FAR struct sdio_dev_s *dev, uint32_t cmd,
                               FAR uint32_t *rnotimpl)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  (void)(cmd);
  (void)(rnotimpl);

  /* TODO: Look at LPC43xx impl and see if anything else is necessary */

  mcerr("Called unimplemented receive for slot %d\n", priv->slotno);
  return -ENOSYS;
}

/****************************************************************************
 * Name: bcm2711_waitenable
 *
 * Description:
 *   Enable/disable of a set of SDIO wait events.  This is part of the
 *   the SDIO_WAITEVENT sequence.  The set of to-be-waited-for events is
 *   configured before calling either calling SDIO_DMARECVSETUP,
 *   SDIO_DMASENDSETUP, or SDIO_WAITEVENT.  This is the recommended
 *   ordering:
 *
 *     SDIO_WAITENABLE:    Discard any pending interrupts, enable event(s)
 *                         of interest
 *     SDIO_DMARECVSETUP/
 *     SDIO_DMASENDSETUP:  Setup the logic that will trigger the event the
 *                         event(s) of interest
 *     SDIO_WAITEVENT:     Wait for the event of interest (which might
 *                         already have occurred)
 *
 *   This sequence should eliminate race conditions between the command/
 *   transfer setup and the subsequent events.
 *
 *   The enabled events persist until either (1) SDIO_WAITENABLE is called
 *   again specifying a different set of wait events, or (2) SDIO_EVENTWAIT
 *   returns.
 *
 * Input Parameters:
 *   dev      - An instance of the SDIO device interface
 *   eventset - A bitset of events to enable or disable (see SDIOWAIT_*
 *              definitions). 0=disable; 1=enable.
 *   timeout  - Maximum time in milliseconds to wait.  Zero means immediate
 *              timeout with no wait.  The timeout value is ignored if
 *              SDIOWAIT_TIMEOUT is not included in the waited-for eventset.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void bcm2711_waitenable(FAR struct sdio_dev_s *dev,
                               sdio_eventset_t eventset, uint32_t timeout)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  uint32_t ints_en = 0;
  mcinfo("Slot %d waitenable for events %04x", priv->slotno, eventset);

  /* Disable event-related interrupts and clear pending interrupts. */

  putreg32(BCM_SDIO_INTERRUPT_ALL,
           BCM_SDIO_INTERRUPT(priv->base));  /* Clear pending */

  priv->waitevents = eventset; /* Record wait events */

  /* Enable interrupts for the new event set */

  if (eventset & SDIOWAIT_CMDDONE)
    {
      ints_en |= BCM_SDIO_IRPT_EN_CMD_DONE;
    }

  if (eventset & SDIOWAIT_RESPONSEDONE)
    {
      /* TODO: Based on LLD code, it looks like CMDDONE is the response
       * interrupt
       */

      ints_en |= BCM_SDIO_IRPT_EN_CMD_DONE;
    }

  if (eventset & SDIOWAIT_TRANSFERDONE)
    {
      ints_en |= BCM_SDIO_IRPT_EN_DATA_DONE;
    }

  if (eventset & SDIOWAIT_TIMEOUT)
    {
      ints_en |= (BCM_SDIO_IRPT_EN_DTO_ERR | BCM_SDIO_IRPT_EN_CTO_ERR);
    }

  if (eventset & SDIOWAIT_ERROR)
    {
      ints_en |= (BCM_SDIO_IRPT_EN_CCRC_ERR | BCM_SDIO_IRPT_EN_CEND_ERR |
                  BCM_SDIO_IRPT_EN_CBAD_ERR | BCM_SDIO_IRPT_EN_DCRC_ERR |
                  BCM_SDIO_IRPT_EN_DEND_ERR | BCM_SDIO_IRPT_EN_ACMD_ERR);
    }

  /* Enable new interrupts. Allow everything through the interrupt mask. */

  modreg32(BCM_SDIO_IRPT_EN_ALL, ints_en,
           BCM_SDIO_IRPT_EN(priv->base));
  putreg32(BCM_SDIO_IRPT_MASK_ALL, BCM_SDIO_IRPT_MASK(priv->base));
  mcinfo("Enabled new interrupts: %08x",
         getreg32(BCM_SDIO_IRPT_EN(priv->base)) & BCM_SDIO_IRPT_EN_ALL);
}

/****************************************************************************
 * Name: bcm2711_eventwait
 *
 * Description:
 *   Wait for one of the enabled events to occur (or a timeout).  Note that
 *   all events enabled by SDIO_WAITEVENTS are disabled when SDIO_EVENTWAIT
 *   returns.  SDIO_WAITEVENTS must be called again before SDIO_EVENTWAIT
 *   can be used again.
 *
 * Input Parameters:
 *   dev - An instance of the SDIO device interface
 *
 * Returned Value:
 *   Event set containing the event(s) that ended the wait.  Should always
 *   be non-zero.  All events are disabled after the wait concludes.
 *
 ****************************************************************************/

static sdio_eventset_t bcm2711_eventwait(FAR struct sdio_dev_s *dev)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  int err;
  sdio_eventset_t wakeupevents;

  DEBUGASSERT(priv->waitevents); /* Can't wait for no events */
  mcinfo("Slot %d waiting for events: %04x", priv->slotno, priv->waitevents);

  /* Wait on semaphore repeatedly until some event we were waiting for wakes
   * us up.
   * TODO: needs to be able to timeout
   */

  while (!(priv->curevents & priv->waitevents))
    {
      err = nxsem_wait_uninterruptible(&priv->wait);
      if (err < 0)
        {
          return SDIOWAIT_ERROR;
        }
    }

  /* TODO: LPC43 implementation disables all interrupts here, is that
   * necessary
   */

  wakeupevents = priv->curevents & priv->waitevents;
  mcinfo("Got events: %04x", wakeupevents);
  priv->curevents = 0; /* Acknowledge events */
  return wakeupevents;
}

/****************************************************************************
 * Name: bcm2711_callback
 *
 * Description:
 *  Perform the registered callback when events apply.
 *
 * Input Parameters:
 *   priv      - The device to perform the callback for
 *
 ****************************************************************************/

static void bcm2711_callback(struct bcm2711_sdio_dev_s *priv)
{
  /* All done here if no callback is registered, or callback events are
   * disabled
   */

  if (priv->cb == NULL || priv->cbevents == 0)
    {
      return;
    }

  if (priv->status & SDIO_STATUS_PRESENT)
    {
      /* If card is present and we don't care about insertion events, do
       * nothing.
       */

      if (!(priv->cbevents & SDIOMEDIA_INSERTED))
        {
          return;
        }
    }
  else
    {
      /* If card is not present and we don't care about ejection events, do
       * nothing.
       */

      if (!(priv->cbevents & SDIOMEDIA_EJECTED))
        {
          return;
        }
    }

  /* Some event that we care about happened, so call the callback and then
   * immediately disable it. Don't directly call the callback in an interrupt
   * context.
   */

  if (up_interrupt_context())
    {
      work_queue(HPWORK, &priv->cbwork, priv->cb, priv->cbarg, 0);
    }
  else
    {
      priv->cb(priv->cbarg);
    }

  priv->cbevents = 0; /* Disabled for future */
}

/****************************************************************************
 * Name: bcm2711_callbackenable
 *
 * Description:
 *   Enable/disable of a set of SDIO callback events.  This is part of the
 *   the SDIO callback sequence.  The set of events is configured to enabled
 *   callbacks to the function provided in SDIO_REGISTERCALLBACK.
 *
 *   Events are automatically disabled once the callback is performed and no
 *   further callback events will occur until they are again enabled by
 *   calling this method.
 *
 * Input Parameters:
 *   dev      - An instance of the SDIO device interface
 *   eventset - A bitset of events to enable or disable (see SDIOMEDIA_*
 *              definitions). 0=disable; 1=enable.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void bcm2711_callbackenable(FAR struct sdio_dev_s *dev,
                                   sdio_eventset_t eventset)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  mcinfo("Enabling callback on slot %d for events %02x\n", priv->slotno,
         eventset);

  priv->cbevents = eventset;
  bcm2711_callback(priv); /* Immediately check events */
}

#if defined(CONFIG_SCHED_WORKQUEUE) && defined(CONFIG_SCHED_HPWORK)

/****************************************************************************
 * Name: bcm2711_registercallback
 *
 * Description:
 *   Register a callback that that will be invoked on any media status
 *   change. Callbacks should not be made from interrupt handlers, rather
 *   interrupt level events should be handled by calling back on the work
 *   thread.
 *
 *   When this method is called, all callbacks should be disabled until they
 *   are enabled via a call to SDIO_CALLBACKENABLE.
 *
 *   NOTE: High-priority work queue support is required.
 *
 * Input Parameters:
 *   dev -      Device-specific state data
 *   callback - The function to call on the media change
 *   arg -      A caller provided value to return with the callback
 *
 * Returned Value:
 *   0 on success; negated errno on failure.
 *
 ****************************************************************************/

static int bcm2711_registercallback(FAR struct sdio_dev_s *dev,
                                    worker_t callback, FAR void *arg)
{
  struct bcm2711_sdio_dev_s *priv = (struct bcm2711_sdio_dev_s *)dev;
  mcinfo("Callback %p registered for %d", callback, priv->slotno);

  /* Register this callback and disable it */

  priv->cb = callback;
  priv->cbarg = arg;
  priv->cbevents = 0;

  return 0;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bcm2711_sdio_initialize
 *
 * Description:
 *   Initialize the BCM2711 SDIO peripheral for normal operation.
 *
 * Input Parameters:
 *   slotno - 1 for EMMC1, 2 for EMMC2
 *
 * Returned Value:
 *   A reference to an SDIO interface structure.
 *   NULL is returned on failures.
 *
 ****************************************************************************/

struct sdio_dev_s *bcm2711_sdio_initialize(int slotno)
{
  int err;
  struct bcm2711_sdio_dev_s *priv = NULL;

  /* NOTE:
   * According to https://ultibo.org/wiki/Unit_BCM2711:
   * - EMMC0 is an Arasan controller
   *   - No card detect pin
   *   - No write protect pin
   *   - Routed to 22/27 (ALT3) or 48-53 (ALT3)
   *   - GPIO pins 34-49 (ALT3) provide SDIO control to Wifi
   * - EMMC1 is non-SDHCI compliant device
   *   - Can be routed to 22-27 (ALT0) or 48-53 (ALT0), but only 22-27 can be
   *   used.
   * - EMMC2 is an SDHCI compliant device, doesn't appear on GPIO pins,
   *   connected to SD card slot
   *   - BCM2838_GPPINMUX register routes EMMC0 to SD-card slot, making EMMC2
   *   usable.
   */

  switch (slotno)
    {
    case 1:

      /* TODO: revisit when EMMC1 supported */

      mcerr("EMMC1 currently unsupported/untested.");
      return NULL;
    case 2:
      priv = &g_emmc2;
      break;
    default:
      mcerr("No SDIO slot number '%d'", slotno);
      return NULL;
    }

  /* If the device was already initialized, return it */

  if (priv->inited)
    {
      return &priv->dev;
    }

  /* Configure GPIO pins for the interface */

  switch (priv->slotno)
    {
    case 1:

      /* TODO: revisit when EMMC1 supported */

      break;
    case 2:
      bcm2711_gpio_set_func(34, BCM_GPIO_INPUT);
      bcm2711_gpio_set_func(35, BCM_GPIO_INPUT);
      bcm2711_gpio_set_func(36, BCM_GPIO_INPUT);
      bcm2711_gpio_set_func(37, BCM_GPIO_INPUT);
      bcm2711_gpio_set_func(38, BCM_GPIO_INPUT);
      bcm2711_gpio_set_func(39, BCM_GPIO_INPUT);

      bcm2711_gpio_set_func(48, BCM_GPIO_FUNC3);
      bcm2711_gpio_set_func(49, BCM_GPIO_FUNC3);
      bcm2711_gpio_set_func(50, BCM_GPIO_FUNC3);
      bcm2711_gpio_set_func(51, BCM_GPIO_FUNC3);
      bcm2711_gpio_set_func(52, BCM_GPIO_FUNC3);
      break;
    }

  /* Reset device */

  bcm2711_reset(&priv->dev);

  /* Enable correct clock.
   * TODO: clocks seem to be enabled by default. Currently `setclken`
   * returns 0x80000008 response code so I'm ignoring this out for now.
   */

  err = bcm2711_mbox_setclken(priv->clkid, true);
  if (err != 0 && err != -EAGAIN)
    {
      mcerr("Couldn't enable EMMC%d clock: %d", priv->slotno, err);
      return NULL;
    }

  /* Determine the base clock rate.
   * NOTE: This driver assumes that it is in complete control of the EMMC
   * base clocks, and that they will not change without its knowledge.
   *
   * TODO: This call also returns 0x80000008 response code, but the rate
   * value returned is reasonable (100MHz). For now, I am ignoring the
   * 0x80000008 response code, not sure why that happens though.
   */

  err = bcm2711_mbox_getclkrate(priv->clkid, &priv->baseclk, false);
  if (err != -EAGAIN && err != 0)
    {
      mcerr("Couldn't determine base clock rate for EMMC%d: %d\n",
            priv->slotno, err);
      return NULL;
    }

  mcinfo("EMMC%d base clock: %uHz\n", priv->slotno, priv->baseclk);

  /* Special case: EMMC2 accesses SD card, so ensure it is powered and
   * power is stable.
   */

  if (priv->slotno == 2)
    {
      err = bcm2711_mbox_setpwr(MBOX_PDOM_SDCARD, true, true);
      if (err)
        {
          mcerr("Couldn't power SD card: %d\n", err);
          return NULL;
        }
    }

  priv->inited = true;
  return &priv->dev;
}
