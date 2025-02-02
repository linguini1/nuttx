======
RN2XX3
======

This driver provides support for the RN2XX3 family of LoRa radio transceivers by
Microchip. This includes both the RN2903 and RN2483 modules.

.. warning::
   This driver only contains preliminary support for a few 'radio set' commands
   and raw radio transmit/receive. There is no support for the LoRaWAN stack
   yet.

Application Programming Interface
=================================

To register the device for use, you will need to enable the standard upper half
serial drivers (``CONFIG_STANDARD_SERIAL``), since the RN2XX3 driver requires
the path to the UART interface the module is connected to.

You will also need to ensure that the baud rate of the UART interface is set to
57600, which is the baud rate of the RN2XX3.

At registration time, the driver will automatically determine if the device is
the RN2903 or RN2483.

.. code-block:: c

   #include <nuttx/wireless/lpwan/rn2xx3.h>

   #ifdef CONFIG_LPWAN_RN2XX3

   /* Register the RN2XX3 device driver */

   ret = rn2xx3_register("/dev/rn2903", "/dev/ttyS1");
   if (ret < 0) {
     syslog(LOG_ERR, "Failed to register RN2XX3 device driver: %d\n", ret);
   }
   #endif

This driver uses the standard POSIX character device interface, implementing
``read()``, ``write()`` and ``ioctl()``.

To transmit, the ``write()`` function can be used. Bytes in the provided buffer
will be transmitted as a packet. This has the following behaviour:

* If the radio is in FSK modulation mode, packets will only contain up to 64
  bytes. A buffer of more than 64 bytes will only have 64 bytes transmitted.
* If the radio is in LoRa modulation mode, packets will only contain up to 255
  bytes.
* If the buffer contains less than the current packet size limit (64 or 255
  bytes), its contents will be transmitted as a single packet.

.. code-block:: c

   int radio = open("/dev/rn2903", O_RDWR);
   if (radio < 0)
     {
       fprintf(stderr, "Couldn't open radio: %d\n", errno);
       return -1;
     }

    char message[] = "Hello, world!";
    ssize_t b_sent = write(radio, message, sizeof(message));
    if (b_sent < 0)
      {
        fprintf(stderr, "Couldn't transmit: %d\n", errno);
        return -1;
      }

To receive, the ``read()`` function can be used. As much of the received packet
as possible will be stored in the user buffer. This has the following behaviour:

* If the buffer is too small to contain the full received packet, as much of the
  packet as possible will be stored in the buffer.
* When the packet is fully read, ``read()`` will return ``0``.
* If only part of the packet has been read and a call to ``write()`` or
  ``ioctl()`` is made, the remainder of the packet is discarded.

.. code-block:: c

   int radio = open("/dev/rn2903", O_RDWR);
   if (radio < 0)
     {
       fprintf(stderr, "Couldn't open radio: %d\n", errno);
       return -1;
     }

    char buffer[16];
    ssize_t b_read;

    do {
        b_read = read(radio, buffer, sizeof(buffer));
        if (b_read < 0)
          {
            fprintf(stderr, "Couldn't receive: %d\n", errno);
            return -1;
          }
        write(0, buffer, b_read); /* Print received bytes to stdout */
    } while (b_read != 0);

Finally, the ``ioctl()`` interface provides access to some underlying module
commands.

``WLIOC_GETSNR``
----------------

Gets the signal to noise ration of the last received packet. If no packets have
been received, it will default to -128. Argument is a pointer to an ``int8_t``.

.. code-block:: c

   int8_t snr;
   err = ioctl(radio, WLIOC_GETSNR, &snr);

``WLIOC_SETRADIOFREQ``
----------------------

Sets the operating frequency of the radio module. The argument is the desired
frequency in Hz (``uint32_t``).

.. code-block:: c

   err = ioctl(radio, WLIOC_SETRADIOFREQ, 902400000);

``WLIOC_GETRADIOFREQ``
----------------------

Gets the current operating frequency of the radio module in Hz. The argument is
a pointer to a ``uint32_t``.

.. code-block:: c

   uint32_t freq;
   err = ioctl(radio, WLIOC_GETRADIOFREQ, &freq);

``WLIOC_SETTXPOWER``
--------------------

Sets the transmission power of the radio. Argument is a pointer to a ``float``
containing the desired transmission power in dBm. After setting the transmission
power successfully, this pointer will contain the new transmission power. This
value may be different from the desired value, but will be the closest available
setting that is greater than or equal to the desired value.

.. code-block:: c

  float txpower = 12.0;
  err = ioctl(radio, WLIOC_SETTXPOWER, &txpower);
  printf("Actual TX power: %.2f dBm\n", txpower);

``WLIOC_GETTXPOWER``
--------------------

Gets the current transmission power level, as the integer level that the radio
uses. The argument is a pointer to an ``int8_t``.

.. code-block:: c

   int8_t txpwr;
   err = ioctl(radio, WLIOC_GETTXPOWER, &txpwr);

``WLIOC_SETBANDWIDTH``
----------------------

Sets the operating bandwidth of the radio module. The argument is the desired
bandwidth in kHz (``uint32_t``). The radio only supports exact values of 125,
250 and 500.

.. code-block:: c

   err = ioctl(radio, WLIOC_SETBANDWIDTH, 250);

``WLIOC_GETBANDWIDTH``
----------------------

Gets the current operating bandwidth of the radio module in kHz. The argument is
a pointer to a ``uint32_t``.

.. code-block:: c

   uint32_t bandwidth;
   err = ioctl(radio, WLIOC_GETBANDWIDTH, &bandwidth);

``WLIOC_SETSPREAD``
----------------------

Sets the operating spread factor of the radio module. The argument is a
``uint8_t`` containing the desired spread factor between 7 and 12 (inclusive).

.. code-block:: c

   err = ioctl(radio, WLIOC_SETSPREAD, 8);

``WLIOC_GETSPREAD``
----------------------

Gets the current operating spread factor of the radio module. The argument is a
pointer to a ``uint8_t``.

.. code-block:: c

   uint8_t spread;
   err = ioctl(radio, WLIOC_GETSPREAD, &spread);

``WLIOC_SETPRLEN``
----------------------

Sets the operating preamble length of the radio module. The argument is a
``uint16_t`` containing the desired preamble length.

.. code-block:: c

   err = ioctl(radio, WLIOC_SETPRLEN, 8);

``WLIOC_GETPRLEN``
----------------------

Gets the current operating preamble length of the radio module. The argument is
a pointer to a ``uint16_t``.

.. code-block:: c

  uint16_t prlen;
  err = ioctl(radio, WLIOC_GETPRLEN, &prlen);

``WLIOC_SETMOD``
----------------------

Sets the operating modulation of the radio module. The argument is one of the
values in ``enum rn2xx3_mod_e``.

.. code-block:: c

   err = ioctl(radio, WLIOC_SETMOD, RN2XX3_MOD_FSK);

``WLIOC_GETMOD``
----------------------

Gets the current operating modulation of the radio module. The argument is a
pointer to a string which can store at least 10 characters.

.. code-block:: c

   char modulation[10];
   err = ioctl(radio, WLIOC_GETMOD, modulation);
