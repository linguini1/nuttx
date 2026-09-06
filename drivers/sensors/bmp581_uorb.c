/****************************************************************************
 * drivers/sensors/bmp581_uorb.c
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

#include <nuttx/debug.h>
#include <nuttx/nuttx.h>

#include <nuttx/i2c/i2c_master.h>
#include <nuttx/sched.h>
#include <nuttx/sensors/bmp581.h>
#include <nuttx/sensors/sensor.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Register address definitions */

#define REG_CHIP_ID (0x01)
#define REG_REV_ID (0x02)
#define REG_CHIP_STATUS (0x11)
#define REG_DRIVE_CONFIG (0x13)
#define REG_INT_CONFIG (0x14)
#define REG_INT_SOURCE (0x15)
#define REG_FIFO_CONFIG (0x16)
#define REG_FIFO_COUNT (0x17)
#define REG_FIFO_SEL (0x18)
#define REG_TEMP_DATA_XLSB (0x1d)
#define REG_TEMP_DATA_LSB (0x1e)
#define REG_TEMP_DATA_MSB (0x1f)
#define REG_PRESS_DATA_XLSB (0x20)
#define REG_PRESS_DATA_LSB (0x21)
#define REG_PRESS_DATA_MSB (0x22)
#define REG_INT_STATUS (0x27)
#define REG_STATUS (0x28)
#define REG_FIFO_DATA (0x29)
#define REG_NVM_ADDR (0x2b)
#define REG_NVM_LSB (0x2c)
#define REG_NVM_MSB (0x2d)
#define REG_DSP_CONFIG (0x30)
#define REG_DSP_IIR (0x31)
#define REG_OOR_THR_P_LSB (0x32)
#define REG_OOR_THR_P_MSB (0x33)
#define REG_OOR_RANGE (0x34)
#define REG_OOR_CONFIG (0x35)
#define REG_OSR_CONFIG (0x36)
#define REG_ODR_CONFIG (0x37)
#define REG_OSR_EFF (0x38)
#define REG_CMD (0x7e)

/* Register masks and values */

#define REG_ODR_CONFIG_PWR_MODE (0x3)
#define REG_ODR_CONFIG_PWR_MODE_STBY (0x0)  /* Standby */
#define REG_ODR_CONFIG_PWR_MODE_NORM (0x1)  /* Normal */
#define REG_ODR_CONFIG_PWR_MODE_FORCE (0x2) /* Forced */
#define REG_ODR_CONFIG_PWR_MODE_NSTP (0x3)  /* Nonstop */

#define REG_ODR_CONFIG_DEEP_DIS (1 << 7) /* Turn off deep standby */

/* Commands */

/* Reset command, overwrites user configurations. NOTE: ACK is not
 * transmitted back after this command over I2C.
 */

#define CMD_RESET (0xb6)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct bmp581_dev_s
{
  struct sensor_lowerhalf_s lower; /* Lower-half driver */
  FAR struct i2c_master_s *i2c;    /* I2C interface */
  uint8_t addr;                    /* I2C address */
  mutex_t lock;                    /* Lock */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int bmp581_control(FAR struct sensor_lowerhalf_s *lower,
                          FAR struct file *filep, int cmd, unsigned long arg);
static int bmp581_activate(FAR struct sensor_lowerhalf_s *lower,
                           FAR struct file *filep, bool enable);
static int bmp581_set_interval(FAR struct sensor_lowerhalf_s *lower,
                               FAR struct file *filep,
                               FAR uint32_t *period_us);
static int bmp581_selftest(FAR struct sensor_lowerhalf_s *lower,
                           FAR struct file *filep, unsigned long arg);
static int bmp581_get_info(FAR struct sensor_lowerhalf_s *lower,
                           FAR struct file *filep,
                           FAR struct sensor_device_info_s *info);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct sensor_ops_s g_sensor_ops = {
    .fetch = NULL,
    .activate = bmp581_activate,
    .control = bmp581_control,
    .set_interval = bmp581_set_interval,
    .selftest = bmp581_selftest,
    .set_calibvalue = NULL,
    .calibrate = NULL,
    .get_info = bmp581_get_info,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: bmp581_read_bytes
 *
 * Description:
 *   Read bytes from the BMP581.
 *
 * Input Parameters:
 *   priv - The BMP581 instance to read from
 *   regaddr - The address of the register to start reading from
 *   buf - The buffer to store the read into
 *   nbytes - The number of bytes to read
 *
 * Returned Value:
 *   0 on success, negated errno value on failure.
 *
 ****************************************************************************/

static int bmp581_read_bytes(FAR struct bmp581_dev_s *priv, uint8_t regaddr,
                             void *buf, size_t nbytes)
{
  struct i2c_msg_s cmd[2];

  cmd[0].frequency = CONFIG_BMP581_I2C_FREQUENCY;
  cmd[0].addr = priv->addr;
  cmd[0].flags = 0;
  cmd[0].buffer = &regaddr;
  cmd[0].length = sizeof(regaddr);

  cmd[1].frequency = CONFIG_BMP581_I2C_FREQUENCY;
  cmd[1].addr = priv->addr;
  cmd[1].flags = I2C_M_READ;
  cmd[1].buffer = buf;
  cmd[1].length = nbytes;

  return I2C_TRANSFER(priv->i2c, cmd, 2);
}

/****************************************************************************
 * Name: bmp581_write_bytes
 *
 * Description:
 *   Write bytes to the BMP581
 *
 * Input Parameters:
 *   priv - The BMP581 instance to write to
 *   regaddr - The register address to start the write at
 *   buf - The bytes to write
 *   nbytes - The number of bytes to write
 *
 * Returned Value:
 *   0 on success, negated errno value on failure.
 *
 ****************************************************************************/

static int bmp581_write_bytes(FAR struct bmp581_dev_s *priv, uint8_t regaddr,
                              void *buf, size_t nbytes)
{
  struct i2c_msg_s cmd[2];

  cmd[0].frequency = CONFIG_BMP581_I2C_FREQUENCY;
  cmd[0].addr = priv->addr;
  cmd[0].flags = 0;
  cmd[0].buffer = &regaddr;
  cmd[0].length = sizeof(regaddr);

  cmd[1].frequency = CONFIG_BMP581_I2C_FREQUENCY;
  cmd[1].addr = priv->addr;
  cmd[1].flags = I2C_M_NOSTART;
  cmd[1].buffer = buf;
  cmd[1].length = nbytes;

  return I2C_TRANSFER(priv->i2c, cmd, 2);
}

/****************************************************************************
 * Name: bmp581_reset
 *
 * Description:
 *   Resets the BMP581 device.
 *
 * Input Parameters:
 *   priv - The BMP581 device to reset.
 *
 * Returned Value:
 *   0 on success, negated errno value on failure.
 *
 ****************************************************************************/

static int bmp581_reset(FAR struct bmp581_dev_s *priv)
{
  uint8_t reset = CMD_RESET;
  int err;

  err = bmp581_write_bytes(priv, REG_CMD, &reset, sizeof(reset));
  nxsched_usleep(2000); /* Wait the reset delay time of 2ms */

  if (err == -EIO)
    {
      err = 0;
      sninfo("Reset command timed out as expected.");
    }

  return err;
}

/**************************************************************************
 * Name: bmp581_control
 *
 * With this method, the user can set some special config for the sensor,
 * such as changing the custom mode, setting the custom resolution, reset,
 * etc, which are all parsed and implemented by lower half driver.
 *
 * Input Parameters:
 *   lower      - The instance of lower half sensor driver.
 *   filep      - The pointer of file, represents each user using sensor.
 *   cmd        - The special cmd for sensor.
 *   arg        - The parameters associated with cmd.
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *   -ENOTTY    - The cmd don't support.
 *
 **************************************************************************/

static int bmp581_control(FAR struct sensor_lowerhalf_s *lower,
                          FAR struct file *filep, int cmd, unsigned long arg)
{
  FAR struct bmp581_dev_s *priv =
      container_of(lower, FAR struct bmp581_dev_s, lower);
  int err = 0;

  switch (cmd)
    {
    case SNIOC_WHO_AM_I:
      if ((uint8_t *)arg == NULL)
        {
          err = -EINVAL;
          break;
        }

      err = bmp581_read_bytes(priv, REG_CHIP_ID, (uint8_t *)arg, 1);
      break;

    default:
      err = -ENOTTY;
    }

  return err;
}

/**************************************************************************
 * Name: bmp581_activate
 *
 * Description:
 *   Enable or disable the BMP581. When enabling the BMP581, it will work in
 *   the current mode (if not set, it will use the default mode). When
 *   disabling the BMP581, it will disable sense path and stop conversion.
 *
 * Input Parameters:
 *   lower  - The instance of lower half sensor driver
 *   filep  - The pointer of file, represents each user using the sensor.
 *   enable - true(enable) and false(disable)
 *
 * Returned Value:
 *   Zero (OK) or positive on success; a negated errno value on failure.
 *
 **************************************************************************/

static int bmp581_activate(FAR struct sensor_lowerhalf_s *lower,
                           FAR struct file *filep, bool enable)
{
  FAR struct bmp581_dev_s *priv =
      container_of(lower, FAR struct bmp581_dev_s, lower);
  int err;
  uint8_t regval;

  nxmutex_lock(&priv->lock); /* Exclusive access */

  /* Get the current ODR configuration */

  err = bmp581_read_bytes(priv, REG_ODR_CONFIG, &regval, sizeof(regval));
  if (err < 0)
    {
      snerr("Couldn't get ODR configuration: %d", err);
      nxmutex_unlock(&priv->lock);
      return err;
    }

  /* Clear the bits for the current power mode */

  regval &= ~REG_ODR_CONFIG_PWR_MODE;

  if (enable)
    {
      /* Go into normal mode */

      regval |= REG_ODR_CONFIG_PWR_MODE_NORM | REG_ODR_CONFIG_DEEP_DIS;
    }
  else
    {
      /* Go into standby mode (not deep standby) */

      regval |= REG_ODR_CONFIG_PWR_MODE_STBY | REG_ODR_CONFIG_DEEP_DIS;
    }

  /* Write power setting change */

  err = bmp581_write_bytes(priv, REG_ODR_CONFIG, &regval, sizeof(regval));
  if (err < 0)
    {
      snerr("Couldn't set ODR configuration: %d", err);
    }

  nxmutex_unlock(&priv->lock);
  return err;
}

/**************************************************************************
 * Name: bmp581_set_interval
 *
 * Description:
 *   Set the sensor output data period in microseconds for a given sensor.
 *   If *period_us > max_delay it will be truncated to max_delay and if
 *   *period_us < min_delay it will be replaced by min_delay.
 *
 *   The lower-half can update update *period_us to reflect the actual
 *   period in case the value is rounded up to nearest supported value.
 *
 *   Before changing the interval, you need to push the prepared data to
 *   ensure that they are not lost.
 *
 * Input Parameters:
 *   lower     - The instance of lower half sensor driver.
 *   filep     - The pointer of file, represents each user using sensor.
 *   period_us - the time between samples, in us, it may be overwrite by
 *               lower half driver.
 *
 * Returned Value:
 *   Zero (OK) or positive on success; a negated errno value on failure.
 *
 **************************************************************************/

static int bmp581_set_interval(FAR struct sensor_lowerhalf_s *lower,
                               FAR struct file *filep,
                               FAR uint32_t *period_us)
{
  /* TODO */

  return 0;
}
/**************************************************************************
 * Name: bmp581_selftest
 *
 * Perform a self-test of the BPM581. TODO: crack check?
 *
 * Input Parameters:
 *   lower      - The instance of lower half sensor driver.
 *   filep      - The pointer of file, represents each user using sensor.
 *   arg        - The parameters associated with selftest.
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *
 **************************************************************************/

static int bmp581_selftest(FAR struct sensor_lowerhalf_s *lower,
                           FAR struct file *filep, unsigned long arg)
{
  /* TODO */

  return 0;
}

/**************************************************************************
 * Name: bmp581_get_info
 *
 * With this method, the user can obtain information about the current
 * device. The name and vendor information cannot exceed
 * SENSOR_INFO_NAME_SIZE.
 *
 * Input Parameters:
 *   lower   - The instance of lower half sensor driver.
 *   filep   - The pointer of file, represents each user using sensor.
 *   info    - Device information structure pointer.
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *
 **************************************************************************/

static int bmp581_get_info(FAR struct sensor_lowerhalf_s *lower,
                           FAR struct file *filep,
                           FAR struct sensor_device_info_s *info)
{
  FAR struct bmp581_dev_s *priv =
      container_of(lower, FAR struct bmp581_dev_s, lower);
  UNUSED(priv); /* TODO: remove */

  memset(info, 0, sizeof(*info));
  info->version = 0;   /* TODO: ASIC revision ID? */
  info->power = 0.08f; /* 80 uA */
  info->max_range = 1250;
  info->resolution = 0;    /* TODO: based on configured resolution */
  info->min_delay = 0;     /* TODO: based on configured ODR */
  info->max_delay = 80400; /* 80.4 ms at highest OSR */
  info->fifo_reserved_event_count = 16; /* 16 for both pressure and temp */
  info->fifo_max_event_count = 16;

  memcpy(info->name, "BMP581", sizeof("BMP581"));
  memcpy(info->vendor, "Bosch", sizeof("Bosch"));
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

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

int bmp581_register(FAR struct i2c_master_s *i2c, uint8_t addr, uint8_t devno)
{
  FAR struct bmp581_dev_s *priv;
  int err = 0;

  DEBUGASSERT(i2c != NULL);
  DEBUGASSERT(addr == BMP581_I2CADDR_46 || addr == BMP581_I2CADDR_47);

  priv = kmm_zalloc(sizeof(struct bmp581_dev_s));
  if (priv == NULL)
    {
      snerr("Failed to allocate BMP581 driver.");
      return -ENOMEM;
    }

  priv->i2c = i2c;
  priv->addr = addr;

  /* Create mutex */

  err = nxmutex_init(&priv->lock);
  if (err < 0)
    {
      snerr("Failed ot initialize mutex: %d", err);
      goto free_mem;
    }

  /* Set up lower-half driver */

  priv->lower.type = SENSOR_TYPE_BAROMETER;
  priv->lower.ops = &g_sensor_ops;
  priv->lower.nbuffer = CONFIG_SENSORS_BMP581_ORB_BUFSIZE;

  err = sensor_register(&priv->lower, devno);
  if (err < 0)
    {
      snerr("Failed to register BMP581 lower-half: %d", err);
      goto del_mutex;
    }

  /* Reset the BMP581 */

  err = bmp581_reset(priv);
  if (err < 0)
    {
      snerr("Failed to reset BMP581: %d", err);
    }

  /* If we are here, there have been no errors in the registration. Return a
   * success.
   */

  uint8_t standby_config =
      REG_ODR_CONFIG_PWR_MODE_STBY | (0x14 << 2) | REG_ODR_CONFIG_DEEP_DIS;
  err = bmp581_write_bytes(priv, REG_ODR_CONFIG, &standby_config,
                           sizeof(standby_config));
  if (err < 0)
    {
      snerr("Couldn't activate BMP581: %d", err);
    }
  else
    {
      uint8_t chip_id;
      err = bmp581_read_bytes(priv, REG_CHIP_ID, &chip_id, sizeof(chip_id));
      if (err < 0)
        {
          snerr("Couldn't read chip ID: %d", err);
        }
      else
        {
          sninfo("BMP581 Chip ID: %02x", chip_id);
        }
    }

  sninfo("BMP581 succesfully registered.");
  return err;

del_mutex:
  nxmutex_destroy(&priv->lock);
free_mem:
  kmm_free(priv);
  return err;
}
