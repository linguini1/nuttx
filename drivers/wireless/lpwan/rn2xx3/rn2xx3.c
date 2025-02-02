/****************************************************************************
 * drivers/wireless/lpwan/rn2xx3/rn2xx3.c
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
#include <debug.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#include <nuttx/fs/fs.h>
#include <nuttx/kmalloc.h>
#include <nuttx/mutex.h>
#include <nuttx/semaphore.h>
#include <nuttx/signal.h>
#include <nuttx/wqueue.h>

#include <nuttx/wireless/lpwan/rn2xx3.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Duration of maximum MAC layer pause in milliseconds */

#define MAC_PAUSE_DUR "4294967245"

/* TODO: implement the following commands
 * coding rate
 * iqi
 * crc
 * sync word
 * bitrate
 */

/****************************************************************************
 * Private Data Types
 ****************************************************************************/

/* Radio transceiver modules */

enum rn2xx3_type_e
{
  RN2903 = 0, /* RN2903 transceiver */
  RN2483 = 1, /* RN2483 transceiver */
};

struct rn2xx3_dev_s
{
  FAR struct file uart;     /* UART interface */
  bool receiving;           /* Currently receiving */
  mutex_t devlock;          /* Exclusive access */
  enum rn2xx3_type_e model; /* Transceiver model */
  enum rn2xx3_mod_e mod;    /* Modulation mode */
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
  int16_t crefs; /* Number of open references */
#endif
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
static int rn2xx3_open(FAR struct file *filep);
static int rn2xx3_close(FAR struct file *filep);
#endif
static ssize_t rn2xx3_write(FAR struct file *filep, FAR const char *buffer,
                            size_t buflen);
static ssize_t rn2xx3_read(FAR struct file *filep, FAR char *buffer,
                           size_t buflen);
static int rn2xx3_ioctl(FAR struct file *filep, int cmd, unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_rn2xx3fops =
{
#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS
  .open = rn2xx3_open,
  .close = rn2xx3_close,
#else
  .open = NULL,
  .close = NULL,
#endif
  .read = rn2xx3_read,
  .write = rn2xx3_write,
  .ioctl = rn2xx3_ioctl,
};

/* Modulation types */

static const char *MODULATIONS[] =
{
  "lora", /* RN2XX3_MOD_LORA */
  "fsk",  /* RN2XX3_MOD_FSK */
};

/* Max packet sizes for each modulation type. */

static const uint8_t MAX_PKTSIZE[] =
{
  255, /* RN2XX3_MOD_LORA */
  64,  /* RN2XX3_MOD_FSK */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: radio_flush
 *
 * Description:
 *   Flushes all characters in the receive buffer from the RN2xx3 radio.
 *   Returns the number of bytes read.
 *
 ****************************************************************************/

static int radio_flush(FAR struct rn2xx3_dev_s *priv)
{
  int err;
  int to_read;
  char buf[10];

  err = file_ioctl(&priv->uart, FIONREAD, &to_read);
  if (err < 0)
    {
      return err;
    }

  while (to_read > 0)
    {
      err = file_read(&priv->uart, buf, sizeof(buf));
      if (err < 0)
        {
          return err;
        }

      to_read -= err;
    }

  return 0;
}

/****************************************************************************
 * Name: read_line
 *
 * Description:
 *   Reads a whole line of response from the RN2xx3 radio. Returns the number
 *   of bytes read, excluding the terminating \r\n and null terminator.
 *
 ****************************************************************************/

static ssize_t read_line(FAR struct rn2xx3_dev_s *priv, FAR char *buf,
                         size_t nbytes)
{
  ssize_t length = 0;
  ssize_t i = 0;
  bool line_complete = false;

  for (; i <= nbytes; )
    {
      length = file_read(&priv->uart, &buf[i], nbytes - i);

      if (length < 0)
        {
          return length;
        }

      i += length;

      /* Check if the character we just read was '\n'. This only occurs when
       * the transceiver's response is complete. Length check ensures that
       * we're not checking uninitialized memory in the case that `length` is
       * 0.
       */

      if (length > 0 && buf[i - 1] == '\n')
        {
          line_complete = true;
          break;
        }
    }

  /* Insufficient buffer space to handle response */

  if (!line_complete)
    {
      return -ENOBUFS;
    }

  /* Overwrite preceding \r with null terminator */

  buf[i - 2] = '\0';
  return i - 2; /* Number of bytes read excluding \r\n */
}

/****************************************************************************
 * Name: mac_pause
 *
 * Description:
 *   Pauses the MAC layer. Required before every transmission/receive.
 *
 ****************************************************************************/

static int mac_pause(FAR struct rn2xx3_dev_s *priv)
{
  ssize_t length = 0;
  char response[30];

  /* Issue pause command */

  length =
      file_write(&priv->uart, "mac pause\r\n", sizeof("mac pause\r\n") - 1);
  if (length < 0)
    {
      return length;
    }

  /* Wait for response of watchdog timer timeout */

  length = read_line(priv, response, sizeof(response));
  if (length < 0)
    {
      return length;
    }

  /* Check for pause duration */

  if (strstr(response, MAC_PAUSE_DUR) == NULL)
    {
      return -EIO;
    }

  return 0;
};

/****************************************************************************
 * Name: get_command_err
 *
 * Description:
 *   Parses the command error response from the radio module and converts it
 *   into an error code. 0 for 'ok', -EINVAL for 'invalid_param' and -EBUSY
 *   for 'busy'. If an unknown error occurs, -EIO is returned.
 *
 ****************************************************************************/

static int get_command_err(FAR struct rn2xx3_dev_s *priv)
{
  char response[18];
  ssize_t length;

  length = read_line(priv, response, sizeof(response));
  if (length < 0)
    {
      return length;
    }

  if (strstr(response, "ok"))
    {
      /* Do nothing, this is good */

      return 0;
    }
  else if (strstr(response, "invalid_param"))
    {
      wlerr("RN2xx3 invalid_param\n");
      return -EINVAL;
    }
  else if (strstr(response, "busy"))
    {
      wlerr("RN2xx3 busy");
      return -EBUSY;
    }

  wlerr("Unknown error");
  return -EIO;
}

/****************************************************************************
 * Name: rn2903_txpwrlevel
 *
 * Description:
 *    Get the transmission power level that is greater than or equal to the
 *    requested transmission power in dBm. These levels are for the RN2903.
 *    The true dBm level that was set is returned in the `dbm` pointer.
 *
 ****************************************************************************/

static int8_t rn2903_txpwrlevel(FAR float *dbm)
{
  if (*dbm <= 3.0f)
    {
      *dbm = 3.0f;
      return 2;
    }
  else if (3.0f < *dbm && *dbm <= 4.0f)
    {
      *dbm = 4.0f;
      return 3;
    }
  else if (4.0f < *dbm && *dbm <= 5.0f)
    {
      *dbm = 5.0f;
      return 4;
    }
  else if (5.0f < *dbm && *dbm <= 6.0f)
    {
      *dbm = 6.0f;
      return 5;
    }
  else if (6.0f < *dbm && *dbm <= 7.0f)
    {
      *dbm = 7.0f;
      return 6;
    }
  else if (7.0f < *dbm && *dbm <= 8.0f)
    {
      *dbm = 8.0f;
      return 7;
    }
  else if (8.0f < *dbm && *dbm <= 9.0f)
    {
      *dbm = 9.0f;
      return 8;
    }
  else if (9.0f < *dbm && *dbm <= 10.0f)
    {
      *dbm = 10.0f;
      return 9;
    }
  else if (10.0f < *dbm && *dbm <= 11.0f)
    {
      *dbm = 11.0f;
      return 10;
    }
  else if (11.0f < *dbm && *dbm <= 12.0f)
    {
      *dbm = 12.0f;
      return 11;
    }
  else if (12.0f < *dbm && *dbm <= 13.0f)
    {
      *dbm = 13.0f;
      return 12;
    }
  else if (13.0f < *dbm && *dbm <= 14.7f)
    {
      *dbm = 14.7f;
      return 14;
    }
  else if (14.7f < *dbm && *dbm <= 15.5f)
    {
      *dbm = 15.5f;
      return 15;
    }
  else if (15.5f < *dbm && *dbm <= 16.3f)
    {
      *dbm = 16.3f;
      return 16;
    }
  else if (16.3f < *dbm && *dbm <= 17.0f)
    {
      *dbm = 17.0f;
      return 16;
    }

  *dbm = 18.5f;
  return 20;
}

/****************************************************************************
 * Name: rn2483_txpwrlevel
 *
 * Description:
 *    Get the transmission power level that is greater than or equal to the
 *    requested transmission power in dBm. These levels are for the RN2483.
 *    The true dBm level that was set is returned in the `dbm` pointer.
 *
 ****************************************************************************/

static int8_t rn2483_txpwrlevel(FAR float *dbm)
{
  if (*dbm <= -4.0f)
    {
      *dbm = -4.0f;
      return -3;
    }
  else if (-4.0f < *dbm && *dbm <= -2.9f)
    {
      *dbm = -2.9f;
      return -2;
    }
  else if (-2.9f < *dbm && *dbm <= -1.9f)
    {
      *dbm = -1.9f;
      return -1;
    }
  else if (-1.9f < *dbm && *dbm <= -1.7f)
    {
      *dbm = -1.7f;
      return 0;
    }
  else if (-1.7f < *dbm && *dbm <= -0.6f)
    {
      *dbm = -0.6f;
      return 1;
    }
  else if (-0.6f < *dbm && *dbm <= 0.4f)
    {
      *dbm = 0.4f;
      return 2;
    }
  else if (0.4f < *dbm && *dbm <= 1.4f)
    {
      *dbm = 1.4f;
      return 3;
    }
  else if (1.4f < *dbm && *dbm <= 2.5f)
    {
      *dbm = 2.5f;
      return 4;
    }
  else if (2.5f < *dbm && *dbm <= 3.6f)
    {
      *dbm = 3.6f;
      return 5;
    }
  else if (3.6f < *dbm && *dbm <= 4.7f)
    {
      *dbm = 4.7f;
      return 6;
    }
  else if (4.7f < *dbm && *dbm <= 5.8f)
    {
      *dbm = 5.8f;
      return 7;
    }
  else if (5.8f < *dbm && *dbm <= 6.9f)
    {
      *dbm = 6.9f;
      return 8;
    }
  else if (6.9f < *dbm && *dbm <= 8.1f)
    {
      *dbm = 8.1f;
      return 9;
    }
  else if (8.1f < *dbm && *dbm <= 9.3f)
    {
      *dbm = 9.3f;
      return 10;
    }
  else if (9.3f < *dbm && *dbm <= 10.4f)
    {
      *dbm = 10.4f;
      return 11;
    }
  else if (10.4f < *dbm && *dbm <= 11.6f)
    {
      *dbm = 11.6f;
      return 12;
    }
  else if (11.6f < *dbm && *dbm <= 12.5f)
    {
      *dbm = 12.5f;
      return 13;
    }
  else if (12.5f < *dbm && *dbm <= 13.5f)
    {
      *dbm = 13.5f;
      return 14;
    }

  *dbm = 14.1f;
  return 15;
}

/****************************************************************************
 * Name: rn2xx3_get_param
 *
 * Description:
 *   Send a command and read the response from the RN2XX3.
 *
 * Arguments:
 *   priv - Pointer to radio device struct
 *   cmd - Null terminated command string, including \r\n
 *   resp - Buffer to store radio response
 *   nbytes - Size of 'resp' buffer
 *
 * Returns: Number of bytes read in response.
 *
 ****************************************************************************/

static ssize_t rn2xx3_getparam(FAR struct rn2xx3_dev_s *priv, FAR char *cmd,
                               FAR char *resp, size_t nbytes)
{
  ssize_t length;

  length = file_write(&priv->uart, cmd, strlen(cmd));
  if (length < 0)
    {
      return length;
    }

  length = read_line(priv, resp, nbytes);
  return length;
}

/****************************************************************************
 * Name: rn2xx3_getmodel
 *
 * Description:
 *    Get the transceiver model type.
 *
 ****************************************************************************/

static int rn2xx3_getmodel(FAR struct rn2xx3_dev_s *priv,
                           FAR enum rn2xx3_type_e *model)
{
  ssize_t length;
  char response[40];

  length = radio_flush(priv);
  if (length < 0)
    {
      return length;
    }

  length =
      rn2xx3_getparam(priv, "sys get ver\r\n", response, sizeof(response));
  if (length < 0)
    {
      return length;
    }

  if (strstr(response, "RN2903") != NULL)
    {
      *model = RN2903;
    }
  else if (strstr(response, "RN2483") != NULL)
    {
      *model = RN2483;
    }
  else
    {
      /* Something went wrong, model number should have been returned */

      return -EIO;
    }

  /* Flush any leftover radio data */

  length = radio_flush(priv);
  if (length < 0)
    {
      return length;
    }

  return 0;
}

/****************************************************************************
 * Name: rn2xx3_getsnr
 *
 * Description:
 *    Get the signal to noise ratio of the last received packet.
 *
 ****************************************************************************/

static int rn2xx3_getsnr(FAR struct rn2xx3_dev_s *priv, FAR int8_t *snr)
{
  ssize_t length;
  char response[10];

  length =
      rn2xx3_getparam(priv, "radio get snr\r\n", response, sizeof(response));
  if (length < 0)
    {
      return length;
    }

  errno = 0;
  *snr = strtol(response, NULL, 10);
  return -errno; /* Set by strtol on failure */
}

/****************************************************************************
 * Name: rn2xx3_setfreq
 *
 * Description:
 *    Set the operating frequency of the RN2xx3 in Hz.
 *
 ****************************************************************************/

static int rn2xx3_setfreq(FAR struct rn2xx3_dev_s *priv, uint32_t freq)
{
  char freqstr[18];
  ssize_t length;

  length = file_write(&priv->uart, "radio set freq ",
                      sizeof("radio set freq ") - 1);
  if (length < 0)
    {
      return length;
    }

  /* Write actual parameter and end command */

  length = snprintf(freqstr, sizeof(freqstr), "%lu\r\n", freq);
  length = file_write(&priv->uart, freqstr, length);
  if (length < 0)
    {
      return length;
    }

  return get_command_err(priv);
}

/****************************************************************************
 * Name: rn2xx3_getfreq
 *
 * Description:
 *    Get the operating frequency of the RN2xx3 in Hz.
 *
 ****************************************************************************/

static int rn2xx3_getfreq(FAR struct rn2xx3_dev_s *priv, FAR uint32_t *freq)
{
  ssize_t length;
  char response[20];

  length = rn2xx3_getparam(priv, "radio get freq\r\n", response,
                           sizeof(response));
  if (length < 0)
    {
      return length;
    }

  errno = 0;
  *freq = strtoul(response, NULL, 10);
  return -errno;
}

/****************************************************************************
 * Name: rn2xx3_settxpwr
 *
 * Description:
 *    Set the transmit power of the RN2xx3.
 *
 ****************************************************************************/

static int rn2xx3_settxpwr(FAR struct rn2xx3_dev_s *priv, FAR float *pwr)
{
  char powerstr[16];
  ssize_t length;
  int8_t txpwr;

  if (priv->model == RN2903)
    {
      txpwr = rn2903_txpwrlevel(pwr);
    }
  else
    {
      txpwr = rn2483_txpwrlevel(pwr);
    }

  length = file_write(&priv->uart, "radio set pwr ",
                      sizeof("radio set pwr ") - 1);
  if (length < 0)
    {
      return length;
    }

  /* Write actual parameter and end command */

  length = snprintf(powerstr, sizeof(powerstr), "%d\r\n", txpwr);
  length = file_write(&priv->uart, powerstr, length);
  if (length < 0)
    {
      return length;
    }

  return get_command_err(priv);
}

/****************************************************************************
 * Name: rn2xx3_gettxpwr
 *
 * Description:
 *    Get the transmission power of the RN2xx3.
 *
 ****************************************************************************/

static int rn2xx3_gettxpwr(FAR struct rn2xx3_dev_s *priv, FAR int8_t *txpwr)
{
  ssize_t length;
  char response[20];

  length =
      rn2xx3_getparam(priv, "radio get pwr\r\n", response, sizeof(response));
  if (length < 0)
    {
      return length;
    }

  errno = 0;
  *txpwr = strtol(response, NULL, 10);
  return -errno;
}

/****************************************************************************
 * Name: rn2xx3_setbw
 *
 * Description:
 *    Set the bandwidth of the RN2xx3.
 *
 ****************************************************************************/

static int rn2xx3_setbw(FAR struct rn2xx3_dev_s *priv, uint32_t bw)
{
  char bwstr[16];
  ssize_t length;
  length =
      file_write(&priv->uart, "radio set bw ", sizeof("radio set bw ") - 1);
  if (length < 0)
    {
      return length;
    }

  /* Write actual parameter and end command */

  length = snprintf(bwstr, sizeof(bwstr), "%lu\r\n", bw);
  length = file_write(&priv->uart, bwstr, length);
  if (length < 0)
    {
      return length;
    }

  return get_command_err(priv);
}

/****************************************************************************
 * Name: rn2xx3_getbw
 *
 * Description:
 *    Get the bandwidth of the RN2xx3.
 *
 ****************************************************************************/

static int rn2xx3_getbw(FAR struct rn2xx3_dev_s *priv, FAR uint32_t *bw)
{
  int length;
  char response[10];

  length =
      rn2xx3_getparam(priv, "radio get bw\r\n", response, sizeof(response));
  if (length < 0)
    {
      return length;
    }

  errno = 0;
  *bw = strtoul(response, NULL, 10);
  return -errno;
}

/****************************************************************************
 * Name: rn2xx3_setsf
 *
 * Description:
 *    Set the spread factor of the RN2xx3.
 *
 ****************************************************************************/

static int rn2xx3_setsf(FAR struct rn2xx3_dev_s *priv, uint8_t sf)
{
  char sfstr[15];
  ssize_t length;

  length =
      file_write(&priv->uart, "radio set sf ", sizeof("radio set sf ") - 1);
  if (length < 0)
    {
      return length;
    }

  /* Write actual parameter and end command */

  length = snprintf(sfstr, sizeof(sfstr), "sf%u\r\n", sf);
  length = file_write(&priv->uart, sfstr, length);
  if (length < 0)
    {
      return length;
    }

  return get_command_err(priv);
}

/****************************************************************************
 * Name: rn2xx3_getsf
 *
 * Description:
 *    Get the spread factor of the RN2xx3.
 *
 ****************************************************************************/

static int rn2xx3_getsf(FAR struct rn2xx3_dev_s *priv, FAR uint8_t *sf)
{
  ssize_t length;
  char response[20];

  length =
      rn2xx3_getparam(priv, "radio get sf\r\n", response, sizeof(response));
  if (length < 0)
    {
      return length;
    }

  /* String should contain at least `sf` followed by one digit */

  if (length < 3)
    {
      return -EIO;
    }

  errno = 0;
  *sf = strtoul(&response[2], NULL, 10); /* Skip 'sf' */
  return -errno;
}

/****************************************************************************
 * Name: rn2xx3_setprlen
 *
 * Description:
 *    Set the preamble length of the RN2xx3.
 *
 ****************************************************************************/

static int rn2xx3_setprlen(FAR struct rn2xx3_dev_s *priv, uint16_t prlen)
{
  char prstr[16];
  ssize_t length;

  length = file_write(&priv->uart, "radio set prlen ",
                      sizeof("radio set prlen ") - 1);
  if (length < 0)
    {
      return length;
    }

  /* Write actual parameter and end command */

  length = snprintf(prstr, sizeof(prstr), "%u\r\n", prlen);
  length = file_write(&priv->uart, prstr, length);
  if (length < 0)
    {
      return length;
    }

  return get_command_err(priv);
}

/****************************************************************************
 * Name: rn2xx3_getprlen
 *
 * Description:
 *    Get the preamble length of the RN2xx3.
 *
 ****************************************************************************/

static int rn2xx3_getprlen(FAR struct rn2xx3_dev_s *priv,
                           FAR uint16_t *prlen)
{
  ssize_t length;
  char response[20];

  length = rn2xx3_getparam(priv, "radio get prlen\r\n", response,
                           sizeof(response));
  if (length < 0)
    {
      return length;
    }

  errno = 0;
  *prlen = strtoul(response, NULL, 10);
  return -errno;
}

/****************************************************************************
 * Name: rn2xx3_setmod
 *
 * Description:
 *    Set the modulation of the RN2xx3.
 *
 ****************************************************************************/

static int rn2xx3_setmod(FAR struct rn2xx3_dev_s *priv,
                         enum rn2xx3_mod_e mod)
{
  int err;
  char modstr[16];
  ssize_t length;

  DEBUGASSERT(mod == RN2XX3_MOD_LORA || mod == RN2XX3_MOD_FSK);

  length = file_write(&priv->uart, "radio set mod ",
                      sizeof("radio set mod ") - 1);
  if (length < 0)
    {
      return length;
    }

  /* Write actual parameter and end command */

  length = snprintf(modstr, sizeof(modstr), "%s\r\n", MODULATIONS[mod]);
  length = file_write(&priv->uart, modstr, length);
  if (length < 0)
    {
      return length;
    }

  err = get_command_err(priv);
  if (err < 0)
    {
      return err;
    }

  priv->mod = mod;
  return 0;
}

/****************************************************************************
 * Name: rn2xx3_getmod
 *
 * Description:
 *    Get the modulation type of the RN2xx3.
 *
 ****************************************************************************/

static int rn2xx3_getmod(FAR struct rn2xx3_dev_s *priv, FAR char *mod)
{
  ssize_t length;
  char response[12];

  length =
      rn2xx3_getparam(priv, "radio get mod\r\n", response, sizeof(response));
  if (length < 0)
    {
      return length;
    }

  /* Only 'lora' and 'fsk' are valid return values from the radio */

  if (strstr(response, "lora") == NULL && strstr(response, "fsk") == NULL)
    {
      return -EIO;
    }

  /* Copy over modulation */

  memcpy(mod, response, length);
  return 0;
}

/****************************************************************************
 * Name: rn2xx3_set_mode_recv
 *
 * Description:
 *    Put the RN2xx3 into indefinite receive mode.
 *
 ****************************************************************************/

static int rn2xx3_set_mode_recv(FAR struct rn2xx3_dev_s *priv)
{
  ssize_t ret;
  static const char receive_cmd[] = "radio rx 0\r\n";

  /* Pause the MAC layer */

  ret = mac_pause(priv);
  if (ret < 0)
    {
      return ret;
    }

  /* Put the radio into indefinite receive mode */

  ret = file_write(&priv->uart, receive_cmd, sizeof(receive_cmd) - 1);
  if (ret < 0)
    {
      return ret;
    }

  /* Check for okay receipt of command */

  ret = get_command_err(priv);
  if (ret == 0)
    {
      priv->receiving = true;
    }

  return ret;
}

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS

/****************************************************************************
 * Name: rn2xx3_open
 ****************************************************************************/

static int rn2xx3_open(FAR struct file *filep)
{
  FAR struct inode *inode = filep->f_inode;
  FAR struct rn2xx3_dev_s *priv = inode->i_private;
  int err;

  err = nxmutex_lock(&priv->devlock);
  if (err < 0)
    {
      return err;
    }

  /* Only one user at a time */

  if (priv->crefs > 0)
    {
      err = -EBUSY;
      wlerr("Too many fds");
      goto early_ret;
    }

  priv->crefs++;

early_ret:
  nxmutex_unlock(&priv->devlock);
  return err;
}

#endif

#ifndef CONFIG_DISABLE_PSEUDOFS_OPERATIONS

/****************************************************************************
 * Name: rn2xx3_close
 ****************************************************************************/

static int rn2xx3_close(FAR struct file *filep)
{
  FAR struct inode *inode = filep->f_inode;
  FAR struct rn2xx3_dev_s *priv = inode->i_private;
  int err;

  err = nxmutex_lock(&priv->devlock);
  if (err < 0)
    {
      return err;
    }

  priv->crefs--;
  nxmutex_unlock(&priv->devlock);
  return err;
}

#endif

/****************************************************************************
 * Name: rn2xx3_read
 *
 * Description:
 *   Puts the RN2xx3 into continuous receive mode and waits for data to be
 *   received before returning it.
 *
 ****************************************************************************/

static ssize_t rn2xx3_read(FAR struct file *filep, FAR char *buffer,
                           size_t buflen)
{
  FAR struct inode *inode = filep->f_inode;
  FAR struct rn2xx3_dev_s *priv = inode->i_private;
  ssize_t ret = 0;
  ssize_t received = 0;
  char response[30];
  char hex_byte[3] =
  {
    0, 0 /* Extra space for null terminator */
  };

  /* Exclusive access */

  ret = nxmutex_lock(&priv->devlock);
  if (ret < 0)
    {
      return ret;
    }

  /* If the file position is non-zero and we're not receiving, return 0 to
   * indicate the end of the last packet.
   */

  if (filep->f_pos > 0 && !priv->receiving)
    {
      filep->f_pos = 0;
      return nxmutex_unlock(&priv->devlock);
    }

  /* If we're still marked as receiving and the file position in non-zero, we
   * can skip initiating receive mode and just continue parsing the packet.
   */

  else if (filep->f_pos > 0 && priv->receiving)
    {
      goto parse_packet;
    }

  /* Otherwise, we're not receiving and haven't been receiving, so we
   * initiate a new receive mode.
   */

  /* Flush data */

  ret = radio_flush(priv);
  if (ret < 0)
    {
      wlerr("Could not flush RN2xx3\n");
      goto early_ret;
    }

  ret = rn2xx3_set_mode_recv(priv);
  if (ret < 0)
    {
      wlerr("Could not enter receive mode\n");
      goto early_ret;
    }

  /* Begin slowly parsing the response to check if we either received data or
   * had an error (timeout)
   * 9 is the size of 'radio_rx ' or 'radio_err'.
   */

  for (uint8_t i = 0; i < 10; )
    {
      ret = file_read(&priv->uart, &response[i], 10 - i);
      if (ret < 0)
        {
          goto early_ret;
        }

      i += ret;
    }

  response[10] = '\0';

  /* Check for either error or received message */

  if (strstr(response, "radio_err") != NULL)
    {
      ret = -EAGAIN; /* Timeout */
      wlerr("Timeout during receive\n");
      goto early_ret;
    }
  else if (strstr(response, "radio_rx ") == NULL)
    {
      /* Unknown error, we didn't expect this output */

      wlerr("Unknown error during receive\n");
      ret = -EIO;
      goto early_ret;
    }

  /* Here we've received some actual data, and that data is what's currently
   * pending to be read.
   * We read until the end of message is signalled with \r\n.
   */

parse_packet:

  hex_byte[2] = '\0'; /* Appease strtoul */

  while (received < buflen)
    {
      ret = file_read(&priv->uart, &hex_byte, 2);
      if (ret < 2)
        {
          goto early_ret;
        }

      if (hex_byte[0] == '\r' && hex_byte[1] == '\n')
        {
          break;
        }

      /* Convert ASCII hex byte into real bytes to store in the buffer */

      buffer[received] = strtoul(hex_byte, NULL, 16);
      received++;
    }

  /* We've received everything we can possibly receive, let the user know by
   * returning how much of the buffer we've filled.
   * If we've seen the end of packet indicator (\r\n), we'll mark the receive
   * as complete. However, if we only stopped to not exceed the buffer
   * length, we will leave our status as receiving to continue on the next
   * read call.
   */

  if (hex_byte[0] == '\r' && hex_byte[1] == '\n')
    {
      priv->receiving = false;
    }

  ret = received;
  filep->f_pos += received; /* Record our position in received packet */

early_ret:

  /* We can't continue receiving if we had an issue */

  if (ret < 0)
    {
      priv->receiving = false;
    }

  nxmutex_unlock(&priv->devlock);
  return ret;
}

/****************************************************************************
 * Name: rn2xx3_write
 *
 * Description:
 *   Transmits the data from the user provided buffer over radio as a
 *   packet.
 *
 ****************************************************************************/

static ssize_t rn2xx3_write(FAR struct file *filep, FAR const char *buffer,
                            size_t buflen)
{
  FAR struct inode *inode = filep->f_inode;
  FAR struct rn2xx3_dev_s *priv = inode->i_private;
  ssize_t length = 0;
  size_t i;
  char response[20];
  char hexbyte[3]; /* Two hex characters per byte, plus null terminator */

  /* Exclusive access */

  length = nxmutex_lock(&priv->devlock);
  if (length < 0)
    {
      return length;
    }

  /* Receives in progress are forfeited */

  priv->receiving = false;

  /* Flush data */

  length = radio_flush(priv);
  if (length < 0)
    {
      wlerr("Could not flush RN2xx3\n");
    }

  /* Pause the MAC layer */

  length = mac_pause(priv);
  if (length < 0)
    {
      wlerr("Could not pause MAC layer\n");
      goto early_ret;
    }

  /* Start the transmission with the initial 'radio tx', followed by all the
   * data in the user buffer converted into hexadecimal characters one by
   * one
   */

  length = file_write(&priv->uart, "radio tx ", sizeof("radio tx ") - 1);
  if (length < 0)
    {
      goto early_ret;
    }

  /* Print out every character up to the buffer size or the packet size limit
   */

  for (i = 0; i < buflen && i < MAX_PKTSIZE[priv->mod]; i++)
    {
      snprintf(hexbyte, sizeof(hexbyte), "%02X", buffer[i]);
      length = file_write(&priv->uart, hexbyte, 2);
      if (length < 0)
        {
          goto early_ret;
        }
    }

  /* Complete the packet */

  length = file_write(&priv->uart, "\r\n", sizeof("\r\n") - 1);
  if (length < 0)
    {
      goto early_ret;
    }

  /* Check for okay message */

  length = get_command_err(priv);
  if (length < 0)
    {
      goto early_ret;
    }

  /* Check for radio_tx_ok */

  length = read_line(priv, response, sizeof(response));
  if (length < 0)
    {
      goto early_ret;
    }

  if (strstr(response, "radio_tx_ok") == NULL)
    {
      length = -EIO;
      goto early_ret;
    }

  /* All transmissions were nominal, so we let the user know that `i` bytes
   * were sent
   */

  length = i;

early_ret:
  nxmutex_unlock(&priv->devlock);
  return length;
}

/****************************************************************************
 * Name: rn2xx3_ioctl
 *
 * Description:
 *   Execute commands on the RN2xx3.
 ****************************************************************************/

static int rn2xx3_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  int err;
  FAR struct inode *inode = filep->f_inode;
  FAR struct rn2xx3_dev_s *priv = inode->i_private;

  /* Exclusive access */

  err = nxmutex_lock(&priv->devlock);
  if (err < 0)
    {
      return err;
    }

  /* Receives in progress are forfeited */

  priv->receiving = false;

  /* Flush radio to clear any lingering messages */

  err = radio_flush(priv);
  if (err < 0)
    {
      wlerr("Couldn't flush radio: %d\n");
      goto early_ret;
    }

  switch (cmd)
    {
    case WLIOC_GETSNR:
      {
        FAR int8_t *snr = (FAR int8_t *)(arg);
        DEBUGASSERT(snr != NULL);
        err = rn2xx3_getsnr(priv, snr);
        break;
      }

    case WLIOC_SETRADIOFREQ:
      {
        err = rn2xx3_setfreq(priv, arg);
        break;
      }

    case WLIOC_GETRADIOFREQ:
      {
        FAR uint32_t *freq = (FAR uint32_t *)(arg);
        DEBUGASSERT(freq != NULL);
        err = rn2xx3_getfreq(priv, freq);
        break;
      }

    case WLIOC_SETTXPOWER:
      {
        FAR float *txpwr = (FAR float *)(arg);
        DEBUGASSERT(txpwr != NULL);
        err = rn2xx3_settxpwr(priv, txpwr);
        break;
      }

    case WLIOC_GETTXPOWER:
      {
        FAR int8_t *txpwr = (FAR int8_t *)(arg);
        DEBUGASSERT(txpwr != NULL);
        err = rn2xx3_gettxpwr(priv, txpwr);
        break;
      }

    case WLIOC_SETBANDWIDTH:
      {
        err = rn2xx3_setbw(priv, arg);
        break;
      }

    case WLIOC_GETBANDWIDTH:
      {
        FAR uint32_t *bw = (FAR uint32_t *)(arg);
        DEBUGASSERT(bw != NULL);
        err = rn2xx3_getbw(priv, bw);
        break;
      }

    case WLIOC_SETSPREAD:
      {
        err = rn2xx3_setsf(priv, arg);
        break;
      }

    case WLIOC_GETSPREAD:
      {
        FAR uint8_t *sf = (FAR uint8_t *)(arg);
        DEBUGASSERT(sf != NULL);
        err = rn2xx3_getsf(priv, sf);
        break;
      }

    case WLIOC_SETPRLEN:
      {
        err = rn2xx3_setprlen(priv, arg);
        break;
      }

    case WLIOC_GETPRLEN:
      {
        FAR uint16_t *prlen = (FAR uint16_t *)(arg);
        DEBUGASSERT(prlen != NULL);
        err = rn2xx3_getprlen(priv, prlen);
        break;
      }

    case WLIOC_SETMOD:
      {
        err = rn2xx3_setmod(priv, arg);
        break;
      }

    case WLIOC_GETMOD:
      {
        FAR char *mod = (FAR char *)(arg);
        DEBUGASSERT(mod != NULL);
        err = rn2xx3_getmod(priv, mod);
        break;
      }

    default:
      {
        err = -EINVAL;
        break;
      }
    }

early_ret:
  nxmutex_unlock(&priv->devlock);
  return err;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: rn2xx3_register
 *
 * Description:
 *   Register the RN2xx3 LoRa transceiver driver.
 *
 * Arguments:
 *    devpath - The device path to use for the driver
 *    uartpath - The path to the UART character driver connected to the
 *               transceiver
 *
 ****************************************************************************/

int rn2xx3_register(FAR const char *devpath, FAR const char *uartpath)
{
  FAR struct rn2xx3_dev_s *priv = NULL;
  int err = 0;
  int retries = 0;

  DEBUGASSERT(uartpath != NULL);
  DEBUGASSERT(devpath != NULL);

  /* Initialize device structure */

  priv = kmm_zalloc(sizeof(struct rn2xx3_dev_s));
  if (priv == NULL)
    {
      wlerr("Failed to allocate instance of RN2xx3 driver.");
      return -ENOMEM;
    }

  priv->receiving = false;     /* Not receiving yet */
  priv->mod = RN2XX3_MOD_LORA; /* Starts in LoRa */

  /* Initialize mutex */

  err = nxmutex_init(&priv->devlock);
  if (err < 0)
    {
      wlerr("Failed to initialize mutex for RN2xx3 device: %d\n", err);
      goto free_mem;
    }

  /* Open UART interface for use */

  err = file_open(&priv->uart, uartpath, O_RDWR | O_CLOEXEC);
  if (err < 0)
    {
      wlerr("Failed to open UART interface %s for RN2xx3 driver: %d\n",
            uartpath, err);
      goto destroy_mutex;
    }

  /* Check model type of the transceiver. Try a few times since the
   * transceiver has some boot time delay.
   */

  do
    {
      err = rn2xx3_getmodel(priv, &priv->model);
      retries++;
      usleep(10000); /* 10ms between tries */
    }
  while (retries < 3 && err < 0);

  if (err < 0)
    {
      wlerr("Could not detect model: %d\n", err);
      goto close_file;
    }

  wlinfo("Detected model %s\n", priv->model == RN2903 ? "RN2903" : "RN2483");

  /* Register driver */

  err = register_driver(devpath, &g_rn2xx3fops, 0666, priv);
  if (err < 0)
    {
      wlerr("Failed to register RN2xx3 driver: %d\n", err);
      goto close_file;
    }

  /* Cleanup items on error */

  if (err < 0)
    {
    close_file:
      file_close(&priv->uart);
    destroy_mutex:
      nxmutex_destroy(&priv->devlock);
    free_mem:
      kmm_free(priv);
    }

  return err;
}
