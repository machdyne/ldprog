# ldprog

Programming software for Lone Dynamics FPGA boards

## Overview

ldprog can be used to program the configuration SRAM or flash on Lone Dynamics FPGA boards.

There are two ways to use ldprog:

  * Run ldprog on a Raspberry Pi and directly connect the target device to the Raspberry Pi GPIO pins
  * Run ldprog on a Linux computer and use a USB interface device (see supported devices below)

## Installation for use with Müsli or Raspberry Pi Pico (Ubuntu)

```
apt-get install libusb-1.0-0-dev
make musli
```

## Installation for use with Raspberry Pi GPIO (Raspbian)

```
apt-get install pigpio
make gpio
```

## Usage

Display help:

```
./ldprog -h
```

## Supported Target Devices

  * [Riegel Computer](https://machdyne.com/product/riegel-computer/)
  * [Bonbon Computer](https://machdyne.com/product/bonbon-computer/)
  * [Keks Game Console](https://machdyne.com/product/keks-game-console/)
  * [Brot FPGA Board](https://machdyne.com/product/brot-fpga-board/)
  * [Kröte FPGA Board](https://machdyne.com/product/krote-fpga-board/)
  * [Winzig Logic Module](https://machdyne.com/product/winzig-logic-module/)

## Supported Interface Devices

  * Raspberry Pi (GPIO)
  * Raspberry Pi Pico (USB)
  * [Werkzeug](https://machdyne.com/product/werkzeug-multi-tool/) (USB)
  * [Müsli](https://machdyne.com/product/musli-usb-pmod/) (USB)

Note: Raspberry Pi Pico and Werkzeug must be loaded with the [Müsli firmware](https://github.com/machdyne/musli).

### Raspberry Pi GPIO Default Wiring

| Signal | GPIO# | Target Pin |
| ------ | ----- | ---------- |
| CSPI\_SS | 25 | 1 |
| CSPI\_SCK | 11 | 2 |
| CSPI\_SI | 10 | 3 |
| CSPI\_SO | 9 | 4 |
| CRESET | 23 | 5 |
| CDONE | 24 | 6 |

#### Raspbery Pi 40-pin Header Pinout

  <img src="https://www.raspberrypi.com/documentation/computers/images/GPIO-Pinout-Diagram-2.png" width="50%">

### Müsli / Werkzeug / Raspberry Pi Pico Default Wiring

| Signal | GPIO# | Müsli Pin | Werkzeug Pin | Pico Pin | Target Pin[^1] |
| ------ | ----- | --------- | ------------ | ---------| ---------- |
| CSPI\_SS | 9 | 8 | 12 | 12 | 1 |
| CSPI\_SCK | 10 | 9 | 13 | 14 | 2 |
| CSPI\_SI | 11 | 10 | 14 | 15 | 3 |
| CSPI\_SO | 8 | 7 | 11 | 11 | 4 |
| CRESET | 3 | 4 | 4 | 5 | 5 |
| CDONE | 2 | 3 | 3 | 4 | 6 |

[^1]: For boards with a 6-pin ISP header.
