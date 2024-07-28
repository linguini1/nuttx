/****************************************************************************
 * arch/arm64/src/bcm2711/hardware/bcm2711_memmap.h
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

#ifndef __ARCH_ARM64_SRC_BCM2711_MM_H
#define __ARCH_ARM64_SRC_BCM2711_MM_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Base addresses for chip registers */

#define BCM_ARMT_BASEADDR 0x7e00b000  /* ARM timer */
#define BCM_AUX_BASEADDR 0x7e215000   /* Auxilliary */
#define BCM_GPCLK_BASEADDR 0x7e101000 /* General purpose clock */
#define BCM_GPIO_BASEADDR 0x7e200000  /* GPIO */
#define BCM_PCM_BASEADDR 0x7e203000   /* PCM */
#define BCM_SYST_BASEADDR 0x7e003000  /* System timer */

/* BSC interface base addresses */

#define BCM_BSC0 0x7e205000 /* BSC/I2C interface 0 */
#define BCM_BSC1 0x7e804000 /* BSC/I2C interface 1 */
#define BCM_BSC3 0x7e205600 /* BSC/I2C interface 2 */
#define BCM_BSC4 0x7e205800 /* BSC/I2C interface 3 */
#define BCM_BSC5 0x7e205a80 /* BSC/I2C interface 4 */
#define BCM_BSC6 0x7e205c00 /* BSC/I2C interface 5 */

/* SPI interface register base addresses */

#define BCM_SPI0_BASEADDR 0x7e204000 /* SPI interface 0 */
#define BCM_SPI3_BASEADDR 0x7e204600 /* SPI interface 3 */
#define BCM_SPI4_BASEADDR 0x7e204800 /* SPI interface 4 */
#define BCM_SPI5_BASEADDR 0x7e204a00 /* SPI interface 5 */
#define BCM_SPI6_BASEADDR 0x7e204c00 /* SPI interface 6 */

/* UART interface base addresses */

#define BCM_UART0_BASEADDR 0x7e201000 /* UART interface 0 */
#define BCM_UART2_BASEADDR 0x7e201400 /* UART interface 2 */
#define BCM_UART3_BASEADDR 0x7e201600 /* UART interface 3 */
#define BCM_UART4_BASEADDR 0x7e201800 /* UART interface 4 */
#define BCM_UART5_BASEADDR 0x7e201a00 /* UART interface 5 */

/* DMA channel base addresses */

#define BCM_DMA0_BASE 0x7e007000  /* DMA Channel 0 */
#define BCM_DMA15_BASE 0x7ee05000 /* DMA Channel 15 */

/* ARM_LOCAL base address */

#if defined(CONFIG_BCM2711_LOW_PERIPHERAL)
#define BCM_ARMLOCAL_BASEADDR 0xff800000
#else
#define BCM_ARMLOCAL_BASEADDR 0x4c0000000
#endif // defined(CONFIG_BCM2711_LOW_PERIPHERAL)

#endif // __ARCH_ARM64_SRC_BCM2711_MM_H
