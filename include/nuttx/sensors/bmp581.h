/****************************************************************************
 * include/nuttx/sensors/bmp581.h
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

#ifndef __INCLUDE_NUTTX_SENSORS_BMP581_H
#define __INCLUDE_NUTTX_SENSORS_BMP581_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <nuttx/i2c/i2c_master.h>
#include <nuttx/sensors/ioctl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define BMP581_I2CADDR_46 (0x46)
#define BMP581_I2CADDR_47 (0x47)

/****************************************************************************
 * Public Types
 ****************************************************************************/

struct i2c_master_s; /* Forward definition */

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: bmp581_register
 *
 * Description:
 *   Register the BMP581 uORB sensor driver. Publishes sensor_baro topics.
 *
 * Input Parameters:
 *   i2c - The I2C bus master on which to communicate with the sensor
 *   addr - The address of the sensor
 *   devno - The device number for the registered driver and topics (i.e.
 *           sensor_baro<n>)
 *
 * Returned Value:
 *   Zero on success, negated errno value on failure.
 *
 ****************************************************************************/

int bmp581_register(FAR struct i2c_master_s *i2c, uint8_t addr, uint8_t devno);

#undef EXTERN
#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_NUTTX_SENSORS_BMP581_H */
