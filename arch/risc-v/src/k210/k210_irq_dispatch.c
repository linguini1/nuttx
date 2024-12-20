/****************************************************************************
 * arch/risc-v/src/k210/k210_irq_dispatch.c
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

#include <stdint.h>
#include <assert.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <sys/types.h>

#include "riscv_internal.h"
#include "group/group.h"

#include "k210_memorymap.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define RV_IRQ_MASK 59

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * riscv_dispatch_irq
 ****************************************************************************/

void *riscv_dispatch_irq(uintptr_t vector, uintptr_t *regs)
{
  int irq = (vector >> RV_IRQ_MASK) | (vector & 0xf);

  /* Firstly, check if the irq is machine external interrupt */

  if (RISCV_IRQ_MEXT == irq)
    {
      uint32_t val = getreg32(K210_PLIC_CLAIM);

      /* Add the value to nuttx irq which is offset to the mext */

      irq += val;
    }

  /* Acknowledge the interrupt */

  riscv_ack_irq(irq);

  /* MEXT means no interrupt */

  if (RISCV_IRQ_MEXT != irq)
    {
      /* Deliver the IRQ */

      regs = riscv_doirq(irq, regs);
    }

  if (RISCV_IRQ_MEXT <= irq)
    {
      /* Then write PLIC_CLAIM to clear pending in PLIC */

      putreg32(irq - RISCV_IRQ_MEXT, K210_PLIC_CLAIM);
    }

  return regs;
}
