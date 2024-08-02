/***************************************************************************
 * arch/arm64/src/bcm2711/bcm2711_lowputc.c
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

#include "arm64_arch.h"
#include "hardware/bcm2711_aux.h"
#include "hardware/bcm2711_gpio.h"
#include <nuttx/config.h>

/***************************************************************************
 * Pre-processor definitions
 ***************************************************************************/

#define SYSTEM_CLOCK_FREQUENCY 500000000 // TODO: check this
#define AUX_MU_BAUD(baud) ((SYSTEM_CLOCK_FREQUENCY / (baud * 8)) - 1)

/***************************************************************************
 * Public functions
 ***************************************************************************/

#ifdef CONFIG_ARCH_EARLY_PRINT

void arm64_lowputc(char ch);
/****************************************************************************
 * Name: arm64_earlyprintinit
 *
 * Description:
 *   Configure Mini UART for non-interrupt driven operation
 *
 ****************************************************************************/

void arm64_earlyprintinit(char ch)
{
  // TODO: re-visit and make cleaner
  /* Enable Mini UART */
  putreg32(1, BCM_AUX_ENABLES);

  putreg32(0, BCM_AUX_MU_IER_REG);
  putreg32(0, BCM_AUX_MU_CNTL_REG);
  putreg32(3, BCM_AUX_MU_LCR_REG);
  putreg32(0, BCM_AUX_MU_MCR_REG);
  putreg32(0, BCM_AUX_MU_IER_REG);
  putreg32(0xc6, BCM_AUX_MU_IIR_REG);
  putreg32(AUX_MU_BAUD(115200), BCM_AUX_MU_BAUD_REG);

  // Use GPIO 14 and 15 as UART1
  putreg32((BCM_GPIO_FS_ALT5 << 12) | (BCM_GPIO_FS_ALT5 << 15),
           BCM_GPIO_GPFSEL1);
  putreg32(0, BCM_GPIO_PUP_PDN_CNTRL_REG1);

  // Enable UART again
  putreg32(3, BCM_AUX_MU_CNTL_REG);

  static char str[] = "Hello from NuttX!\n";
  for (uint8_t i = 0; i < sizeof(str); i++)
    {
      arm64_lowputc(str[i]);
    }

  char str2[] = "Second hello\n";
  for (uint8_t i = 0; i < sizeof(str2); i++)
    {
      arm64_lowputc(str2[i]);
    }
}

/****************************************************************************
 * Name: arm64_lowputc
 *
 * Description:
 *   Output a byte with as few system dependencies as possible.
 *
 ****************************************************************************/

void arm64_lowputc(char ch)
{
  // TODO: re-visit and make cleaner

  // Wait until space for one byte is free
  while (!(getreg32(BCM_AUX_MU_LSR_REG) & BCM_AUX_MU_LSR_TXEMPTY))
    ;

  // Add carriage return
  if (ch == '\n')
    {
      putreg32('\r', BCM_AUX_MU_IO_REG);

      // Wait again
      while (!(getreg32(BCM_AUX_MU_LSR_REG) & BCM_AUX_MU_LSR_TXEMPTY))
        ;
    }

  // Send byte
  putreg32(ch, BCM_AUX_MU_IO_REG);
}

#endif // CONFIG_ARCH_EARLY_PRINT
