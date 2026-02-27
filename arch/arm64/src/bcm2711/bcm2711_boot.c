/****************************************************************************
 * arch/arm64/src/bcm2711/bcm2711_boot.c
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

#include "arm64_arch.h"
#include "arm64_internal.h"
#include "arm64_mmu.h"
#include "bcm2711_boot.h"
#include "bcm2711_serial.h"
#include "bcm2711_mailbox.h"

#include <arch/chip/chip.h>

#ifdef CONFIG_SMP
#include "arm64_smp.h"
#endif

#include <nuttx/cache.h>
#ifdef CONFIG_LEGACY_PAGING
#include <nuttx/page.h>
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct arm_mmu_region g_mmu_regions[] =
{
  MMU_REGION_FLAT_ENTRY("DEVICE_REGION", CONFIG_DEVICEIO_BASEADDR,
                        CONFIG_DEVICEIO_SIZE,
                        MT_DEVICE_NGNRNE | MT_RW | MT_SECURE),

  MMU_REGION_FLAT_ENTRY("DRAM0_S0", CONFIG_RAMBANK1_ADDR,
                        CONFIG_RAMBANK1_SIZE,
                        MT_NORMAL | MT_RW | MT_SECURE),

  /* TODO: verify this works on the 8GB variant */

#if defined(CONFIG_RPI4B_RAM_8GB)
  MMU_REGION_FLAT_ENTRY("DRAM0_S1",
                        CONFIG_RAMBANK2_ADDR, CONFIG_RAMBANK2_SIZE,
                        MT_NORMAL | MT_RW | MT_SECURE),
#endif /* defined(CONFIG_RPI4B_RAM_8GB) */
};

const struct arm_mmu_config g_mmu_config =
{
  .num_regions = nitems(g_mmu_regions),
  .mmu_regions = g_mmu_regions,
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

extern uint64_t _start[]; /* Kernel load address from linker script */

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_SMP

/****************************************************************************
 * Name: arm64_get_mpid
 *
 * Description:
 *   The function from cpu index to get cpu mpid which is reading
 *   from mpidr_el1 register. Different ARM64 Core will use different
 *   Affn define, the mpidr_el1 value is not CPU number, So we need
 *   to change CPU number to mpid and vice versa
 *
 ****************************************************************************/

uint64_t arm64_get_mpid(int cpu)
{
  return CORE_TO_MPID(cpu, 0);
}

/****************************************************************************
 * Name: arm64_get_cpuid
 *
 * Description:
 *   The function from mpid to get cpu id
 *
 ****************************************************************************/

int arm64_get_cpuid(uint64_t mpid)
{
  return MPID_TO_CORE(mpid);
}

#endif /* CONFIG_SMP */

/****************************************************************************
 * Name: arm64_el_init
 *
 * Description:
 *   The function called from arm64_head.S at very early stage for this
 *   platform. It's used to:
 *   - Handle special hardware initialization routines which need to
 *     run at high ELs
 *   - Initialize system software such as hypervisor or security firmware
 *     which needs to run at high ELs
 *
 ****************************************************************************/

void arm64_el_init(void)
{
  /* According to the bcm2711.dtsi file in the Linux source tree [1], the CPUs
   * on the BCM2711 use a spin-table enable method and poll the following
   * addresses:
   *
   * CPU0: 0x000000d8 (BCM_MBOX_CLR06)
   * CPU1: 0x000000e0 (BCM_MBOX_CLR08)
   * CPU2: 0x000000e8 (BCM_MBOX_CLR10)
   * CPU3: 0x000000f0 (BCM_MBOX_CLR12)
   *
   * Some kernel docs about booting [2] have a handy explanation of how this
   * works:
   *
   * "polling their cpu-release-addr location, which must be contained in the
   * reserved region ... when a read of the location pointed to by the
   * cpu-release-addr returns a non-zero value, the CPU must jump to this
   * value" [2]
   *
   * In our case, we want these CPUs to load the NuttX kernel defined by
   * `_start`. We don't need to worry about CPU0, that one always starts.
   * These are 64-bit words (hence skipping every second register).
   *
   * [1]
   * https://github.com/raspberrypi/linux/blob/rpi-6.12.y/arch/arm/boot/dts/broadcom/bcm2711.dtsi
   *
   * [2] https://www.kernel.org/doc/Documentation/arm64/booting.txt
   */

#ifdef CONFIG_SMP

  for (uint8_t cpu = 0; cpu < CONFIG_SMP_NCPUS; cpu++) {
      putreg64((uint64_t)_start, BCM_SPINTBL_CPU(cpu));
  }

  UP_DSB(); /* Sync writes */
  UP_SEV(); /* Notify all cores of change */

#endif /* CONFIG_SMP */
}

/****************************************************************************
 * Name: arm64_chip_boot
 *
 * Description:
 *   Complete boot operations started in arm64_head.S
 *
 ****************************************************************************/

void arm64_chip_boot(void)
{
  /* MAP IO and DRAM, enable MMU. */

  arm64_mmu_init(true);

#if defined(CONFIG_ARM64_PSCI)
  arm64_psci_init("smc");

#endif

  /* Perform board-specific device initialization. This would include
   * configuration of board specific resources such as GPIOs, LEDs, etc.
   */

  bcm2711_board_initialize();

#ifdef USE_EARLYSERIALINIT
  /* Perform early serial initialization if we are going to use the serial
   * driver.
   */

  arm64_earlyserialinit();
#endif
}
