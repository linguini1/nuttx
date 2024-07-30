/****************************************************************************
 * arch/arm64/include/bcm2711/chip.h
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

#ifndef __ARCH_ARM64_INCLUDE_BCM2711_CHIP_H
#define __ARCH_ARM64_INCLUDE_BCM2711_CHIP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Number of bytes in x kilobytes/megabytes/gigabytes */

#define KB(x) ((x) << 10)
#define MB(x) (KB(x) << 10)
#define GB(x) (MB(UINT64_C(x)) << 10)

/* TODO: config option for low peripheral mode */

/* TODO: config option for GIC400 interrupt controller or legacy one */

/* GIC-400 base address.
 * The GIC-400 uses GICv2 architecture.
 */

#if defined(CONFIG_BCM2711_LOW_PERIPHERAL)

/* Low peripheral GIC address */

#define BCM_GIC400_BASEADDR 0xff840000

#else

/* Used for both 35-bit addressing and legacy mode. */

#define BCM_GIC400_BASEADDR 0x4c0040000

#endif // defined(CONFIG_BCM2711_LOW_PERIPHERAL)

#define BCM_GIC400_DISTOFFSET 0x00001000  /* Distributor */
#define BCM_GIC400_RDISTOFFSET 0x00002000 /* CPU Interfaces */
#define CONFIG_GICD_BASE (BCM_GIC400_BASEADDR + BCM_GIC400_DISTOFFSET)
#define CONFIG_GICR_BASE (BCM_GIC400_BASEADDR + BCM_GIC400_RDISTOFFSET)
#define CONFIG_GICR_OFFSET BCM_GIC400_RDISTOFFSET

/* BCM2711 memory map: RAM and Device I/O */

// TODO: verify

#define CONFIG_RAMBANK1_ADDR      0x40000000
#define CONFIG_RAMBANK1_SIZE      MB(128)

#define CONFIG_DEVICEIO_BASEADDR  0x00000000
#define CONFIG_DEVICEIO_SIZE      MB(512)

#define MPID_TO_CLUSTER_ID(mpid) ((mpid) & ~0xff)

/****************************************************************************
 * Assembly Macros
 ****************************************************************************/

#ifdef __ASSEMBLY__

.macro  get_cpu_id xreg0
  mrs    \xreg0, mpidr_el1
  ubfx   \xreg0, \xreg0, #0, #8
.endm

#endif /* __ASSEMBLY__ */

#endif /* __ARCH_ARM64_INCLUDE_BCM2711_CHIP_H */
