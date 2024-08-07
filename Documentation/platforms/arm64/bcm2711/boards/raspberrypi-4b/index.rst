==========
Raspberry Pi 4B
==========

The `Raspberry Pi 4B <https://www.raspberrypi.com/products/raspberry-pi-4-model-b/specifications/>`_ is an ARM64
hobbyist board created by Raspberry Pi.

Features
=========

- Broadcom BCM2711 @1.8GHz
- 1, 2, 4 and 8GB LPDDR4-3200 SDRAM models
- 2.4GHz and 5.0GHz IEEE 802.11ac wireless
- Bluetooth 5.0
- Gigabit Ethernet
- 2 USB 3.0 ports
- 2 USB 2.0 ports
- 2 micro-HDMI ports (4kp60)
- 2-lane MIPI DSI display port
- 2-lane MIPI CSI camera port
- 4-pole stereo audio and composite video port
- Micro SD slot

ARM64 Toolchain
==========

Before building NuttX for PinePhone, download the ARM64 Toolchain for
**AArch64 Bare-Metal Target** ``aarch64-none-elf`` from
`Arm GNU Toolchain Downloads <https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads>`_.
(Skip the section for Beta Releases)

Add the downloaded toolchain ``gcc-arm-...-aarch64-none-elf/bin`` to the ``PATH`` Environment Variable.

Check the ARM64 Toolchain:

.. code:: console

    $ aarch64-none-elf-gcc -v

Building
========

To build NuttX for the Raspberry Pi 4B, :doc:`install the prerequisites </quickstart/install>` and :doc:`clone the git
repositories </quickstart/install>` for ``nuttx`` and ``apps``.

Configure the NuttX project to use the Raspberry Pi 4B and build it (this example uses the ``nsh`` configuration).

.. code:: console

    $ cd nutxx
    $ tools/configure.sh raspberrypi-4b:nsh
    $ make

Booting
========

In order to boot NuttX on the Raspberry Pi 4B, you will need to have a formatted micro SD card. The SD card should
contain a FAT32 partition that is marked as bootable and which contains the generated ``nuttx.img`` and ``config.txt``
files from the build process. In addition to those files, you will also need the following files from the Raspberry Pi
repository for loading the image:

- `bcm2711-rpi-4-b.dtb <https://github.com/raspberrypi/firmware/blob/stable/boot/bcm2711-rpi-4-b.dtb>`_
- `fixup4.dat <https://github.com/raspberrypi/firmware/blob/stable/boot/fixup4.dat>`_
- `fixup4cd.dat <https://github.com/raspberrypi/firmware/blob/stable/boot/fixup4cd.dat>`_
- `fixup4db.dat <https://github.com/raspberrypi/firmware/blob/stable/boot/fixup4db.dat>`_
- `fixup4x.dat <https://github.com/raspberrypi/firmware/blob/stable/boot/fixup4x.dat>`_
- `start4.elf <https://github.com/raspberrypi/firmware/blob/stable/boot/start4.elf>`_
- `start4cd.elf <https://github.com/raspberrypi/firmware/blob/stable/boot/start4cd.elf>`_
- `start4db.elf <https://github.com/raspberrypi/firmware/blob/stable/boot/start4db.elf>`_
- `start4x.elf <https://github.com/raspberrypi/firmware/blob/stable/boot/start4x.elf>`_

TODO: include an example for formatting SD card.

Once all the files are copied, you can then eject the SD card and insert it onto your Raspberry Pi. The default console
is the Mini UART, which requires a `USB to TTL serial converter cable <https://www.adafruit.com/product/954>`_ to read.
You should connect the ground to one of the Pi's ground pins, and then connect the RX to GPIO 14 and TX to GPIO 15. **Do
not connect the red power cable**.

Once the converter is connected and plugged into your host computer, you can open up a serial terminal of your choice. I
use Minicom. Then, power your Raspberry Pi 4 with a USB-C cable and wait for the Pi to boot and the NSH prompt to appear
onscreen:

.. code:: console

    NuttShell (NSH) NuttX-12.6.0-RC0
    nsh> uname -a
    NuttX 12.6.0-RC0 c4f3a42131-dirty Aug  6 2024 21:17:01 arm64 raspberrypi-4b
    nsh> 

Board Peripheral Support
==================

NuttX for the Raspberry Pi 4 supports these on-board peripherals:

======================== =======
Peripheral               Support
======================== =======
I2C                      No 
UART                     Mini UART yes, PL011 no
GPIO                     No
PWM                      No
SPI                      No
PCM                      No
AV port                  No
HDMI                     No
WiFi                     No
Ethernet                 No
USB 3.0                  No
USB 2.0                  No
Bluetooth                No
======================== =======
