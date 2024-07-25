/****************************************************************************
 * arch/arm64/src/bcm2711/hardware/bcm2711_irq.h
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

#ifndef __ARCH_ARM64_SRC_BCM2711_IRQ_H
#define __ARCH_ARM64_SRC_BCM2711_IRQ_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* ARM_LOCAL interrupts */

/* ARMC interrupts */

#define BCM_IRQ_ARMC_TIMER 0      /* Timer */
#define BCM_IRQ_ARMC_MAILBOX 1    /* Mailbox */
#define BCM_IRQ_ARMC_DOORBELL0 2  /* Doorbell 0 */
#define BCM_IRQ_ARMC_DOORBELL1 3  /* Doorbell 1 */
#define BCM_IRQ_ARMC_VPU0HALT 4   /* VPU 0 halted */
#define BCM_IRQ_ARMC_VPU1HALT 5   /* VPU 1 halted */
#define BCM_IRQ_ARMC_ARMADDRERR 6 /* ARM address error */
#define BCM_IRQ_ARMC_ARMAXIERR 7  /* ARM AXI error */
#define BCM_IRQ_ARMC_SWI0 8       /* Software interrupt 0 */
#define BCM_IRQ_ARMC_SWI1 9       /* Software interrupt 1 */
#define BCM_IRQ_ARMC_SWI2 10      /* Software interrupt 2 */
#define BCM_IRQ_ARMC_SWI3 11      /* Software interrupt 3 */
#define BCM_IRQ_ARMC_SWI4 12      /* Software interrupt 4 */
#define BCM_IRQ_ARMC_SWI5 13      /* Software interrupt 5 */
#define BCM_IRQ_ARMC_SWI6 14      /* Software interrupt 6 */
#define BCM_IRQ_ARMC_SWI7 15      /* Software interrupt 7 */

/* VideoCore interrupts */

#define BCM_IRQ_VC_TIMER0 0
#define BCM_IRQ_VC_TIMER1 1
#define BCM_IRQ_VC_TIMER2 2
#define BCM_IRQ_VC_TIMER3 3
#define BCM_IRQ_VC_H2640 4
#define BCM_IRQ_VC_H2641 5
#define BCM_IRQ_VC_H2642 6
#define BCM_IRQ_VC_JPEG 7
#define BCM_IRQ_VC_ISP 8
#define BCM_IRQ_VC_USB 9
#define BCM_IRQ_VC_V3D 10
#define BCM_IRQ_VC_TRANSPOSE 11
#define BCM_IRQ_VC_MCSYNC0 12
#define BCM_IRQ_VC_MCSYNC1 13
#define BCM_IRQ_VC_MCSYNC2 14
#define BCM_IRQ_VC_MCSYNC3 15
#define BCM_IRQ_VC_DMA0 16
#define BCM_IRQ_VC_DMA1 17
#define BCM_IRQ_VC_DMA2 18
#define BCM_IRQ_VC_DMA3 19
#define BCM_IRQ_VC_DMA4 20
#define BCM_IRQ_VC_DMA5 21
#define BCM_IRQ_VC_DMA6 22
#define BCM_IRQ_VC_DMA7N8 23
#define BCM_IRQ_VC_DMA9N10 24
#define BCM_IRQ_VC_DMA11 25
#define BCM_IRQ_VC_DMA12 26
#define BCM_IRQ_VC_DMA13 27
#define BCM_IRQ_VC_DMA14 28
#define BCM_IRQ_VC_AUX 29
#define BCM_IRQ_VC_ARM 30
#define BCM_IRQ_VC_DMA15 31
#define BCM_IRQ_VC_HDMICEC 32
#define BCM_IRQ_VC_HVS 33
#define BCM_IRQ_VC_RPIVID 34
#define BCM_IRQ_VC_SDC 35
#define BCM_IRQ_VC_DSI0 36
#define BCM_IRQ_VC_PIXVLV2 37
#define BCM_IRQ_VC_CAM0 38
#define BCM_IRQ_VC_CAM1 39
#define BCM_IRQ_VC_HDMI0 40
#define BCM_IRQ_VC_HDMI1 41
#define BCM_IRQ_VC_PIXVLV3 42
#define BCM_IRQ_VC_SPIBSCSLV 43
#define BCM_IRQ_VC_DSI1 44
#define BCM_IRQ_VC_PXLVLV0 45
#define BCM_IRQ_VC_PXLVLV1N4 46
#define BCM_IRQ_VC_CPR 47
#define BCM_IRQ_VC_SMI 48
#define BCM_IRQ_VC_GPIO0 49
#define BCM_IRQ_VC_GPIO1 50
#define BCM_IRQ_VC_GPIO2 51
#define BCM_IRQ_VC_GPIO3 52
#define BCM_IRQ_VC_I2C 53
#define BCM_IRQ_VC_SPI 54
#define BCM_IRQ_VC_PCMI2S 55
#define BCM_IRQ_VC_SDHOST 56
#define BCM_IRQ_VC_PL011UART 57
#define BCM_IRQ_VC_ETHPCIE 58
#define BCM_IRQ_VC_VEC 59
#define BCM_IRQ_VC_CPG 60
#define BCM_IRQ_VC_RNG 61
#define BCM_IRQ_VC_EMMC 62
#define BCM_IRQ_VC_ETHPCIESEC 63

/* TODO: what about PACTL_CS address section 6.2.4? */

/* ETH_PCIe interrupts */

#define BCM_IRQ_ETH_AVS 9
#define BCM_IRQ_ETH_PCIE0_INTA 15
#define BCM_IRQ_ETH_PCIE0_INTB 16
#define BCM_IRQ_ETH_PCIE0_INTC 17
#define BCM_IRQ_ETH_PCIE0_INTD 18
#define BCM_IRQ_ETH_PCIE0_MSI 20
#define BCM_IRQ_ETH_GENET0_A 29
#define BCM_IRQ_ETH_GENET0_B 30
#define BCM_IRQ_ETH_USB0_XHCI0 48

#define BCM_IRQ_ARMLOCAL_IRQ_SOURCEN_MAILBOX_CORE2_IRQ (1 << 5)
#define BCM_IRQ_ARMLOCAL_IRQ_SOURCEN_MAILBOX_CORE3_IRQ (1 << 4)
#define BCM_IRQ_ARMLOCAL_IRQ_SOURCEN_CNT_V_IRQ (1 << 3)

#endif // __ARCH_ARM64_SRC_BCM2711_IRQ_H
