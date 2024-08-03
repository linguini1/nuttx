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

Add the downloaded toolchain ``gcc-arm-...-aarch64-none-elf/bin``
to the ``PATH`` Environment Variable.

Check the ARM64 Toolchain:

Board Peripheral Support
==================

NuttX for the Raspberry Pi 4 supports these on-board peripherals:

======================== =======
Peripheral               Support
======================== =======
I2C                      No 
UART                     No 
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

.. code:: console

   $ aarch64-none-elf-gcc -v

.. include:: README.txt
   :literal:
