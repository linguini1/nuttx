/****************************************************************************
 * arch/arm64/src/bcm2711/hardware/bcm2711_genet.h
 *
 * SPDX-License-Identifer: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements. See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership. The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License. You may obtain a copy of the License at
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

#ifndef __ARCH_ARM64_SRC_BCM2711_GENET_H
#define __ARCH_ARM64_SRC_BCM2711_GENET_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "bcm2711_memmap.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Ethernet controller register definitions, determined from the Linux kernel
 */

#define UMAC_MIB_START			0x400

#define UMAC_MDIO_CMD			0x614
#define  MDIO_START_BUSY		(1 << 29)
#define  MDIO_READ_FAIL			(1 << 28)
#define  MDIO_RD			(2 << 26)
#define  MDIO_WR			(1 << 26)
#define  MDIO_PMD_SHIFT			21
#define  MDIO_PMD_MASK			0x1F
#define  MDIO_REG_SHIFT			16
#define  MDIO_REG_MASK			0x1F

#define UMAC_RBUF_OVFL_CNT_V1		0x61C
#define RBUF_OVFL_CNT_V2		0x80
#define RBUF_OVFL_CNT_V3PLUS		0x94

#define UMAC_MPD_CTRL			0x620
#define  MPD_EN				(1 << 0)
#define  MPD_PW_EN			(1 << 27)
#define  MPD_MSEQ_LEN_SHIFT		16
#define  MPD_MSEQ_LEN_MASK		0xFF

#define UMAC_MPD_PW_MS			0x624
#define UMAC_MPD_PW_LS			0x628
#define UMAC_RBUF_ERR_CNT_V1		0x634
#define RBUF_ERR_CNT_V2			0x84
#define RBUF_ERR_CNT_V3PLUS		0x98
#define UMAC_MDF_ERR_CNT		0x638
#define UMAC_MDF_CTRL			0x650
#define UMAC_MDF_ADDR			0x654
#define UMAC_MIB_CTRL			0x580
#define  MIB_RESET_RX			(1 << 0)
#define  MIB_RESET_RUNT			(1 << 1)
#define  MIB_RESET_TX			(1 << 2)

#define RBUF_CTRL			0x00
#define  RBUF_64B_EN			(1 << 0)
#define  RBUF_ALIGN_2B			(1 << 1)
#define  RBUF_BAD_DIS			(1 << 2)

#define RBUF_STATUS			0x0C
#define  RBUF_STATUS_WOL		(1 << 0)
#define  RBUF_STATUS_MPD_INTR_ACTIVE	(1 << 1)
#define  RBUF_STATUS_ACPI_INTR_ACTIVE	(1 << 2)

#define RBUF_CHK_CTRL			0x14
#define  RBUF_RXCHK_EN			(1 << 0)
#define  RBUF_SKIP_FCS			(1 << 4)
#define  RBUF_L3_PARSE_DIS		(1 << 5)

#define RBUF_ENERGY_CTRL		0x9c
#define  RBUF_EEE_EN			(1 << 0)
#define  RBUF_PM_EN			(1 << 1)

#define RBUF_TBUF_SIZE_CTRL		0xb4

#define RBUF_HFB_CTRL_V1		0x38
#define  RBUF_HFB_FILTER_EN_SHIFT	16
#define  RBUF_HFB_FILTER_EN_MASK	0xffff0000
#define  RBUF_HFB_EN			(1 << 0)
#define  RBUF_HFB_256B			(1 << 1)
#define  RBUF_ACPI_EN			(1 << 2)

#define RBUF_HFB_LEN_V1			0x3C
#define  RBUF_FLTR_LEN_MASK		0xFF
#define  RBUF_FLTR_LEN_SHIFT		8

#define TBUF_CTRL			0x00
#define  TBUF_64B_EN			(1 << 0)
#define TBUF_BP_MC			0x0C
#define TBUF_ENERGY_CTRL		0x14
#define  TBUF_EEE_EN			(1 << 0)
#define  TBUF_PM_EN			(1 << 1)

#define TBUF_CTRL_V1			0x80
#define TBUF_BP_MC_V1			0xA0

#define HFB_CTRL			0x00
#define HFB_FLT_ENABLE_V3PLUS		0x04
#define HFB_FLT_LEN_V2			0x04
#define HFB_FLT_LEN_V3PLUS		0x1C

/* uniMac intrl2 registers */
#define INTRL2_CPU_STAT			0x00
#define INTRL2_CPU_SET			0x04
#define INTRL2_CPU_CLEAR		0x08
#define INTRL2_CPU_MASK_STATUS		0x0C
#define INTRL2_CPU_MASK_SET		0x10
#define INTRL2_CPU_MASK_CLEAR		0x14

/* INTRL2 instance 0 definitions */
#define UMAC_IRQ_SCB			(1 << 0)
#define UMAC_IRQ_EPHY			(1 << 1)
#define UMAC_IRQ_PHY_DET_R		(1 << 2)
#define UMAC_IRQ_PHY_DET_F		(1 << 3)
#define UMAC_IRQ_LINK_UP		(1 << 4)
#define UMAC_IRQ_LINK_DOWN		(1 << 5)
#define UMAC_IRQ_LINK_EVENT		(UMAC_IRQ_LINK_UP | UMAC_IRQ_LINK_DOWN)
#define UMAC_IRQ_UMAC			(1 << 6)
#define UMAC_IRQ_UMAC_TSV		(1 << 7)
#define UMAC_IRQ_TBUF_UNDERRUN		(1 << 8)
#define UMAC_IRQ_RBUF_OVERFLOW		(1 << 9)
#define UMAC_IRQ_HFB_SM			(1 << 10)
#define UMAC_IRQ_HFB_MM			(1 << 11)
#define UMAC_IRQ_MPD_R			(1 << 12)
#define UMAC_IRQ_WAKE_EVENT		(UMAC_IRQ_HFB_SM | UMAC_IRQ_HFB_MM | \
					 UMAC_IRQ_MPD_R)
#define UMAC_IRQ_RXDMA_MBDONE		(1 << 13)
#define UMAC_IRQ_RXDMA_PDONE		(1 << 14)
#define UMAC_IRQ_RXDMA_BDONE		(1 << 15)
#define UMAC_IRQ_RXDMA_DONE		UMAC_IRQ_RXDMA_MBDONE
#define UMAC_IRQ_TXDMA_MBDONE		(1 << 16)
#define UMAC_IRQ_TXDMA_PDONE		(1 << 17)
#define UMAC_IRQ_TXDMA_BDONE		(1 << 18)
#define UMAC_IRQ_TXDMA_DONE		UMAC_IRQ_TXDMA_MBDONE

/* Only valid for GENETv3+ */
#define UMAC_IRQ_MDIO_DONE		(1 << 23)
#define UMAC_IRQ_MDIO_ERROR		(1 << 24)
#define UMAC_IRQ_MDIO_EVENT		(UMAC_IRQ_MDIO_DONE | \
					 UMAC_IRQ_MDIO_ERROR)

/* INTRL2 instance 1 definitions */
#define UMAC_IRQ1_TX_INTR_MASK		0xFFFF
#define UMAC_IRQ1_RX_INTR_MASK		0xFFFF
#define UMAC_IRQ1_RX_INTR_SHIFT		16

/* Register block offsets */
#define GENET_SYS_OFF			0x0000
#define GENET_GR_BRIDGE_OFF		0x0040
#define GENET_EXT_OFF			0x0080
#define GENET_INTRL2_0_OFF		0x0200
#define GENET_INTRL2_1_OFF		0x0240
#define GENET_RBUF_OFF			0x0300
#define GENET_UMAC_OFF			0x0800

/* SYS block offsets and register definitions */
#define SYS_REV_CTRL			0x00
#define SYS_PORT_CTRL			0x04
#define  PORT_MODE_INT_EPHY		0
#define  PORT_MODE_INT_GPHY		1
#define  PORT_MODE_EXT_EPHY		2
#define  PORT_MODE_EXT_GPHY		3
#define  PORT_MODE_EXT_RVMII_25		(4 | BIT(4))
#define  PORT_MODE_EXT_RVMII_50		4
#define  LED_ACT_SOURCE_MAC		(1 << 9)

#define SYS_RBUF_FLUSH_CTRL		0x08
#define SYS_TBUF_FLUSH_CTRL		0x0C
#define RBUF_FLUSH_CTRL_V1		0x04

/* Ext block register offsets and definitions */
#define EXT_EXT_PWR_MGMT		0x00
#define  EXT_PWR_DOWN_BIAS		(1 << 0)
#define  EXT_PWR_DOWN_DLL		(1 << 1)
#define  EXT_PWR_DOWN_PHY		(1 << 2)
#define  EXT_PWR_DN_EN_LD		(1 << 3)
#define  EXT_ENERGY_DET			(1 << 4)
#define  EXT_IDDQ_FROM_PHY		(1 << 5)
#define  EXT_IDDQ_GLBL_PWR		(1 << 7)
#define  EXT_PHY_RESET			(1 << 8)
#define  EXT_ENERGY_DET_MASK		(1 << 12)
#define  EXT_PWR_DOWN_PHY_TX		(1 << 16)
#define  EXT_PWR_DOWN_PHY_RX		(1 << 17)
#define  EXT_PWR_DOWN_PHY_SD		(1 << 18)
#define  EXT_PWR_DOWN_PHY_RD		(1 << 19)
#define  EXT_PWR_DOWN_PHY_EN		(1 << 20)

#define EXT_RGMII_OOB_CTRL		0x0C
#define  RGMII_MODE_EN_V123		(1 << 0)
#define  RGMII_LINK			(1 << 4)
#define  OOB_DISABLE			(1 << 5)
#define  RGMII_MODE_EN			(1 << 6)
#define  ID_MODE_DIS			(1 << 16)

#define EXT_GPHY_CTRL			0x1C
#define  EXT_CFG_IDDQ_BIAS		(1 << 0)
#define  EXT_CFG_PWR_DOWN		(1 << 1)
#define  EXT_CK25_DIS			(1 << 4)
#define  EXT_CFG_IDDQ_GLOBAL_PWR	(1 << 3)
#define  EXT_GPHY_RESET			(1 << 5)

#endif /* __ARCH_ARM64_SRC_BCM2711_GENET_H */
