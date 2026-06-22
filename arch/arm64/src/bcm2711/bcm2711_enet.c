/****************************************************************************
 * arch/arm64/src/bcm2711/bcm2711_enet.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include "bcm2711_enet.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if !defined(CONFIG_SCHED_WORKQUEUE)
#  error "Work queue support is required."
#endif

#define ETHWORK LPWORK

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bcm2711_enet_s
{
  struct net_driver_s dev;

  /* TODO: */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int bcm2711_ifup(struct net_driver_s *dev);
static int bcm2711_ifdown(struct net_driver_s *dev);
static int bcm2711_txavail(struct net_driver_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* A single packet buffer is used */

static uint8_t g_enet_pktbuf[MAX_NETDEV_PKTSIZE + CONFIG_NET_GUARDSIZE];

static struct bcm2711_enet_s g_enet =
{
  .dev =
  {
    .d_buf = g_enet_pktbuf,
    .d_ifup = bcm2711_ifup,
    .d_ifdown = bcm2711_ifup,
    .d_txavail = bcm2711_txavail,
  },

  /* TODO */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/


static int bcm2711_ifup(struct net_driver_s *dev)
{
  /* TODO */

  return 0;
}

static int bcm2711_ifdown(struct net_driver_s *dev)
{
  /* TODO */

  return 0;
}

static int bcm2711_txavail(struct net_driver_s *dev)
{
  /* TODO */

  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Function: bcm2711_enet_initialize
 *
 * Description:
 *   Initialize the Ethernet driver.
 *
 * Returned Value:
 *   OK on success; Negated errno on failure.
 *
 * Assumptions:
 *   Called very early in the initialization sequence.
 *
 ****************************************************************************/

int bcm2711_enet_initialize(void)
{
  int ret;

  /* TODO: All the IRQ stuff n junk */

  /* Register the device with the OS so that socket IOCTLs can be performed */

  ret = netdev_register(&g_enet.dev, NET_LL_ETHERNET);
  if (ret < 0)
    {
      nerr("ERROR: netdev_register() failed: %d\n", ret);
    }

  return ret;
}

/****************************************************************************
 * Name: arm_netinitialize
 *
 * Description:
 *   Initialize the first network interface. If there is more than one
 *   interface in the chip, then board-specific logic will have to provide
 *   this function to determine which, if any, Ethernet controllers should
 *   be initialized.
 *
 ****************************************************************************/

#ifndef CONFIG_NETDEV_LATEINIT
void arm64_netinitialize(void)
{
  bcm2711_enet_initialize();
}
#endif
