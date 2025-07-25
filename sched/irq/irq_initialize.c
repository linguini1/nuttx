/****************************************************************************
 * sched/irq/irq_initialize.c
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
#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/trace.h>

#include <assert.h>
#include "irq/irq.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* This is the number of entries in the interrupt vector table */

#ifdef CONFIG_ARCH_MINIMAL_VECTORTABLE
#  define TAB_SIZE CONFIG_ARCH_NUSER_INTERRUPTS
#else
#  define TAB_SIZE NR_IRQS
#endif

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* This is the interrupt vector table */

#ifdef CONFIG_ARCH_MINIMAL_VECTORTABLE
struct irq_info_s g_irqvector[CONFIG_ARCH_NUSER_INTERRUPTS];
#else
struct irq_info_s g_irqvector[NR_IRQS];
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_ARCH_PROTECT_ZERO_ADDRESS
static void zero_addr_handler(int type, FAR void *addr, size_t size,
                              FAR void *arg)
{
  PANIC();
}
#endif

/****************************************************************************
 * Name: irq_initialize
 *
 * Description:
 *   Configure the IRQ subsystem
 *
 ****************************************************************************/

void irq_initialize(void)
{
  int i;

  sched_trace_begin();

  /* Point all interrupt vectors to the unexpected interrupt */

  for (i = 0; i < TAB_SIZE; i++)
    {
      g_irqvector[i].handler = irq_unexpected_isr;
    }

#ifdef CONFIG_IRQCHAIN
  /* Initialize IRQ chain support */

  irqchain_initialize();
#endif

  up_irqinitialize();

#ifdef CONFIG_ARCH_PROTECT_ZERO_ADDRESS
  up_debugpoint_add(DEBUGPOINT_WATCHPOINT_RW, 0, 0,
                    zero_addr_handler, NULL);
#endif

  sched_trace_end();
}
