/****************************************************************************
 * boards/arm64/bcm2711/raspberrypi-4b/src/rpi4b_bringup.c
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

#include "rpi4b.h"
#include <nuttx/config.h>
#include <sys/types.h>
#include <syslog.h>

#if defined(CONFIG_BCM2711_I2C)
#include "bcm2711_i2cdev.h"
#endif // defined(CONFIG_BCM2711_I2C)

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: rpi4b_bringup
 *
 * Description:
 *   Bring up board features
 *
 ****************************************************************************/

int rpi4b_bringup(void)
{

  int ret = OK;

  /* Initialize GPIO driver. */

#if defined(CONFIG_DEV_GPIO)
  ret = bcm2711_dev_gpio_init();
  if (ret < 0)
    {
      syslog(LOG_ERR, "Failed to initialize GPIO driver: %d\n.", ret);
    }
#endif // defined(CONFIG_DEV_GPIO)

    /* Initialize I2C interfaces. */

#if defined(CONFIG_BCM2711_I2C)

#if defined(CONFIG_BCM2711_I2C0)
  ret = bcm2711_i2cdev_initialize(0);
  if (ret < 0)
    {
      syslog(LOG_ERR, "Failed to initialize I2C0: %d\n", ret);
    }
#endif // defined(CONFIG_BCM2711_I2C0)

#if defined(CONFIG_BCM2711_I2C1)
  ret = bcm2711_i2cdev_initialize(1);
  if (ret < 0)
    {
      syslog(LOG_ERR, "Failed to initialize I2C1: %d\n", ret);
    }
#endif // defined(CONFIG_BCM2711_I2C1)

    // TODO: initializer calls for remaining I2C devices

#endif // defined(CONFIG_BCM2711_I2C)

  return ret;
}
