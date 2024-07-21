/****************************************************************************
 * arch/arm64/src/bcm2711/bcm2711_aux.h
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

#ifndef __ARCH_ARM_SRC_BCM2711_BCM2711_AUX_H
#define __ARCH_ARM_SRC_BCM2711_BCM2711_AUX_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* BCM2711 auxiliary register offsets. **************************************/

#define BCM_AUX_BASE 0x7e215000

/* BCM2711 mini UART register offsets. */

#define BCM_AUX_IRQ_OFFSET 0x00         /* Auxiliary interrupt status */
#define BCM_AUX_ENABLES_OFFSET 0x04     /* Auxiliary enables */
#define BCM_AUX_MU_IO_REG_OFFSET 0x40   /* Mini UART I/O Data */
#define BCM_AUX_MU_IER_REG_OFFSET 0x44  /* Mini UART interrupt enable */
#define BCM_AUX_MU_IIR_REG_OFFSET 0x48  /* Mini UART interrupt identify */
#define BCM_AUX_MU_LCR_REG_OFFSET 0x4c  /* Mini UART line control */
#define BCM_AUX_MU_MCR_REG_OFFSET 0x50  /* Mini UART modem control */
#define BCM_AUX_MU_LSR_REG_OFFSET 0x54  /* Mini UART Line Status */
#define BCM_AUX_MU_MSR_REG_OFFSET 0x58  /* Mini UART Modem Status */
#define BCM_AUX_MU_SCRATCH_OFFSET 0x5c  /* Mini UART Scratch */
#define BCM_AUX_MU_CNTL_REG_OFFSET 0x60 /* Mini UART Extra Control */
#define BCM_AUX_MU_STAT_REG_OFFSET 0x64 /* Mini UART Extra Status */
#define BCM_AUX_MU_BAUD_REG_OFFSET 0x68 /* Mini UART Baudrate */

/* BCM2711 SPI register offsets. */

#define BCM_AUX_SPI1_CNTL0_REG_OFFSET 0x80   /* SPI 1 Control register 0 */
#define BCM_AUX_SPI1_CNTL1_REG_OFFSET 0x84   /* SPI 1 Control register 1 */
#define BCM_AUX_SPI1_STAT_REG_OFFSET 0x88    /* SPI 1 Status */
#define BCM_AUX_SPI1_PEEK_REG_OFFSET 0x8c    /* SPI 1 Peek */
#define BCM_AUX_SPI1_IO_REGa_OFFSET 0xa0     /* SPI 1 Data */
#define BCM_AUX_SPI1_IO_REGb_OFFSET 0xa4     /* SPI 1 Data */
#define BCM_AUX_SPI1_IO_REGc_OFFSET 0xa8     /* SPI 1 Data */
#define BCM_AUX_SPI1_IO_REGd_OFFSET 0xac     /* SPI 1 Data */
#define BCM_AUX_SPI1_TXHOLD_REGa_OFFSET 0xb0 /* SPI 1 Extended Data */
#define BCM_AUX_SPI1_TXHOLD_REGb_OFFSET 0xb4 /* SPI 1 Extended Data */
#define BCM_AUX_SPI1_TXHOLD_REGc_OFFSET 0xb8 /* SPI 1 Extended Data */
#define BCM_AUX_SPI1_TXHOLD_REGd_OFFSET 0xbc /* SPI 1 Extended Data */
#define BCM_AUX_SPI2_CNTL0_REG_OFFSET 0xc0   /* SPI 2 Control register */
#define BCM_AUX_SPI2_CNTL1_REG_OFFSET 0xc4   /* SPI 2 Control register 1 */
#define BCM_AUX_SPI2_STAT_REG_OFFSET 0xc8    /* SPI 2 Status */
#define BCM_AUX_SPI2_PEEK_REG_OFFSET 0xcc    /* SPI 2 Peek */
#define BCM_AUX_SPI2_IO_REGa_OFFSET 0xe0     /* SPI 2 Data */
#define BCM_AUX_SPI2_IO_REGb_OFFSET 0xe4     /* SPI 2 Data */
#define BCM_AUX_SPI2_IO_REGc_OFFSET 0xe8     /* SPI 2 Data */
#define BCM_AUX_SPI2_IO_REGd_OFFSET 0xec     /* SPI 2 Data */
#define BCM_AUX_SPI2_TXHOLD_REGa_OFFSET 0xf0 /* SPI 2 Extended Data */
#define BCM_AUX_SPI2_TXHOLD_REGb_OFFSET 0xf4 /* SPI 2 Extended Data */
#define BCM_AUX_SPI2_TXHOLD_REGc_OFFSET 0xf8 /* SPI 2 Extended Data */
#define BCM_AUX_SPI2_TXHOLD_REGd_OFFSET 0xfc /* SPI 2 Extended Data */

/* BCM2711 auxiliary registers. *********************************************/

#define BCM_AUX_REG(offset) (BCM_AUX_BASE + (offset))

/* BCM2711 mini UART registers. */

#define BCM_AUX_IRQ         BCM_AUX_REG(BCM_AUX_IRQ_OFFSET)
#define BCM_AUX_ENABLES     BCM_AUX_REG(BCM_AUX_ENABLES_OFFSET)
#define BCM_AUX_MU_IO_REG   BCM_AUX_REG(BCM_AUX_MU_IO_REG_OFFSET)
#define BCM_AUX_MU_IER_REG  BCM_AUX_REG(BCM_AUX_MU_IER_REG_OFFSET)
#define BCM_AUX_MU_IIR_REG  BCM_AUX_REG(BCM_AUX_MU_IIR_REG_OFFSET)
#define BCM_AUX_MU_LCR_REG  BCM_AUX_REG(BCM_AUX_MU_LCR_REG_OFFSET)
#define BCM_AUX_MU_MCR_REG  BCM_AUX_REG(BCM_AUX_MU_MCR_REG_OFFSET)
#define BCM_AUX_MU_LSR_REG  BCM_AUX_REG(BCM_AUX_MU_LSR_REG_OFFSET)
#define BCM_AUX_MU_MSR_REG  BCM_AUX_REG(BCM_AUX_MU_MSR_REG_OFFSET)
#define BCM_AUX_MU_SCRATCH  BCM_AUX_REG(BCM_AUX_MU_SCRATCH_OFFSET)
#define BCM_AUX_MU_CNTL_REG BCM_AUX_REG(BCM_AUX_MU_CNTL_REG_OFFSET)
#define BCM_AUX_MU_STAT_REG BCM_AUX_REG(BCM_AUX_MU_STAT_REG_OFFSET)
#define BCM_AUX_MU_BAUD_REG BCM_AUX_REG(BCM_AUX_MU_BAUD_REG_OFFSET)

/* BCM2711 SPI registers. */

#define BCM_AUX_SPI1_CNTL0_REG   BCM_AUX_REG(BCM_AUX_SPI1_CNTL0_REG_OFFSET)
#define BCM_AUX_SPI1_CNTL1_REG   BCM_AUX_REG(BCM_AUX_SPI1_CNTL1_REG_OFFSET)
#define BCM_AUX_SPI1_STAT_REG    BCM_AUX_REG(BCM_AUX_SPI1_STAT_REG_OFFSET)
#define BCM_AUX_SPI1_PEEK_REG    BCM_AUX_REG(BCM_AUX_SPI1_PEEK_REG_OFFSET)
#define BCM_AUX_SPI1_IO_REGa     BCM_AUX_REG(BCM_AUX_SPI1_IO_REGa_OFFSET)
#define BCM_AUX_SPI1_IO_REGb     BCM_AUX_REG(BCM_AUX_SPI1_IO_REGb_OFFSET)
#define BCM_AUX_SPI1_IO_REGc     BCM_AUX_REG(BCM_AUX_SPI1_IO_REGc_OFFSET)
#define BCM_AUX_SPI1_IO_REGd     BCM_AUX_REG(BCM_AUX_SPI1_IO_REGd_OFFSET)
#define BCM_AUX_SPI1_TXHOLD_REGa BCM_AUX_REG(BCM_AUX_SPI1_TXHOLD_REGa_OFFSET)
#define BCM_AUX_SPI1_TXHOLD_REGb BCM_AUX_REG(BCM_AUX_SPI1_TXHOLD_REGb_OFFSET)
#define BCM_AUX_SPI1_TXHOLD_REGc BCM_AUX_REG(BCM_AUX_SPI1_TXHOLD_REGc_OFFSET)
#define BCM_AUX_SPI1_TXHOLD_REGd BCM_AUX_REG(BCM_AUX_SPI1_TXHOLD_REGd_OFFSET)
#define BCM_AUX_SPI2_CNTL0_REG   BCM_AUX_REG(BCM_AUX_SPI2_CNTL0_REG_OFFSET)
#define BCM_AUX_SPI2_CNTL1_REG   BCM_AUX_REG(BCM_AUX_SPI2_CNTL1_REG_OFFSET)
#define BCM_AUX_SPI2_STAT_REG    BCM_AUX_REG(BCM_AUX_SPI2_STAT_REG_OFFSET)
#define BCM_AUX_SPI2_PEEK_REG    BCM_AUX_REG(BCM_AUX_SPI2_PEEK_REG_OFFSET)
#define BCM_AUX_SPI2_IO_REGa     BCM_AUX_REG(BCM_AUX_SPI2_IO_REGa_OFFSET)
#define BCM_AUX_SPI2_IO_REGb     BCM_AUX_REG(BCM_AUX_SPI2_IO_REGb_OFFSET)
#define BCM_AUX_SPI2_IO_REGc     BCM_AUX_REG(BCM_AUX_SPI2_IO_REGc_OFFSET)
#define BCM_AUX_SPI2_IO_REGd     BCM_AUX_REG(BCM_AUX_SPI2_IO_REGd_OFFSET)
#define BCM_AUX_SPI2_TXHOLD_REGa BCM_AUX_REG(BCM_AUX_SPI2_TXHOLD_REGa_OFFSET)
#define BCM_AUX_SPI2_TXHOLD_REGb BCM_AUX_REG(BCM_AUX_SPI2_TXHOLD_REGb_OFFSET)
#define BCM_AUX_SPI2_TXHOLD_REGc BCM_AUX_REG(BCM_AUX_SPI2_TXHOLD_REGc_OFFSET)
#define BCM_AUX_SPI2_TXHOLD_REGd BCM_AUX_REG(BCM_AUX_SPI2_TXHOLD_REGd_OFFSET)

/* BCM2711 auxiliary register bit definitions. */

#define BCM_AUX_IRQ_MU (1 << 0)   /* Mini UART interrupt pending */
#define BCM_AUX_IRQ_SPI1 (1 << 1) /* SPI1 interrupt pending */
#define BCM_AUX_IRQ_SPI2 (1 << 2) /* SPI2 interrupt pending */

#define BCM_AUX_ENABLE_MU (1 << 0)   /* Mini UART enable */
#define BCM_AUX_ENABLE_SPI1 (1 << 1) /* SPI1 enable */
#define BCM_AUX_ENABLE_SPI2 (1 << 2) /* SPI2 enable */

#define BCM_AUX_MU_IO_BAUDRATE (0xff) /* LS 8 bits of baudrate register */
#define BCM_AUX_MU_IO_TXD (0xff)      /* If DLAB=0, write-only */
#define BCM_AUX_MU_IO_RXD (0xff)      /* If DLAB=0, read-only */

#define BCM_AUX_MU_IER_BAUDRATE (0xff) /* MS 8 bits of baudrate register */
#define BCM_AUX_MU_IER_RXD (1 << 1)    /* Enable receive interrupt */
#define BCM_AUX_MU_IER_TXD (1 << 0)    /* Enable transmit interrupt */

#define BCM_AUX_MU_IIR_PENDING (1 << 0) /* Clear when interrupt pending */
#define BCM_AUX_MU_IIR_MASK (0x03 << 1) /* Mask interrupt ID bits */
#define BCM_AUX_MU_IIR_TXEMPTY (1 << 1) /* TX holding register empty (RO) */
#define BCM_AUX_MU_IIR_RXBYTE (2 << 1)  /* RX holding valid byte (RO) */
#define BCM_AUX_MU_IIR_NONE (0 << 1)    /* No interrupts (RO) */
#define BCM_AUX_MU_IIR_RXCLEAR (1 << 1) /* Clear RX FIFO (WO) */
#define BCM_AUX_MU_IIR_TXCLEAR (1 << 2) /* Clear TX FIFO (WO) */

#define BCM_AUX_MU_LCR_DLAB (1 << 7)   /* Gives access to baudrate */
#define BCM_AUX_MU_LCR_BREAK (1 << 6)  /* Pull TX line low */
#define BCM_AUX_MU_LCR_DATA8B (1 << 0) /* 7-bit if clear, 8-bit if set */

#define BCM_AUX_MU_MCR_RTS (1 << 1) /* RTS high = 0, RTS low = 1 */

#define BCM_AUX_MU_LSR_TXIDLE (1 << 6)    /* Transmitter is idle */
#define BCM_AUX_MU_LSR_TXEMPTY (1 << 5)   /* Transmitter FIFO has space */
#define BCM_AUX_MU_LSR_RXOVERRUN (1 << 1) /* Receiver overrun */
#define BCM_AUX_MU_LSR_DREADY (1 << 0)    /* RX data is ready */

#define BCM_AUX_MU_MSR_CTS (1 << 4) /* CTS low = 1, CTS high = 0 */

#define BCM_AUX_MU_SCRATCHMASK (0xff) /* Byte of extra storage */

#define BCM_AUX_MU_CNTL_CTSLVL (1 << 7)       /* CTS flow assert low = 1 */
#define BCM_AUX_MU_CNTL_RTSLVL (1 << 6)       /* RTS flow assert low = 1 */
#define BCM_AUX_MU_CNTL_RTSFLOWMSK (0x3 << 4) /* RTS auto flow level mask */
#define BCM_AUX_MU_CNTL_FLOWLVL4 (3 << 4)     /* De-assert FIFO 4 empty */
#define BCM_AUX_MU_CNTL_FLOWLVL3 (0 << 4)     /* De-assert FIFO 3 empty */
#define BCM_AUX_MU_CNTL_FLOWLVL2 (1 << 4)     /* De-assert FIFO 2 empty */
#define BCM_AUX_MU_CNTL_FLOWLVL1 (2 << 4)     /* De-assert FIFO 1 empty */
#define BCM_AUX_MU_CNTL_TXAUTOFLOW (1 << 3)   /* Enable TX auto flow */
#define BCM_AUX_MU_CNTL_RXAUTOFLOW (1 << 2)   /* Enable RX auto flow */
#define BCM_AUX_MU_CNTL_TXENABLE (1 << 1)     /* Enable transmitter */
#define BCM_AUX_MU_CNTL_RXENABLE (1 << 0)     /* Enable receiver */

#endif // __ARCH_ARM_SRC_BCM2711_BCM2711_AUX_H
