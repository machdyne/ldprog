# ldprog

Programming software for Lone Dynamics FPGA boards

## Installation for use with Müsli (Ubuntu)

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
  * [Kröte FPGA Board](https://machdyne.com/product/krote-fpga-board/)
  * [Brot FPGA Board](https://machdyne.com/product/brot-fpga-board/)
  * [Krume FPGA SOM](https://machdyne.com/product/krume-fpga-som/)

## Supported Interface Devices

  * Raspberry Pi
  * Raspberry Pi Pico
  * [Müsli](https://machdyne.com/product/musli-usb-pmod/)

### Raspberry Pi GPIO Default Wiring

| Signal | GPIO# | Target Pin |
| ------ | ----- | ---------- |
| CSPI\_SS | 25 | 1 |
| CSPI\_SO | 9 | 4 |
| CSPI\_SI | 10 | 3 |
| CSPI\_SCK | 11 | 2 |
| CRESET | 23 | 5 |
| CDONE | 24 | 6 |

#### Raspbery Pi 40-pin Header Pinout

!(https://www.raspberrypi.com/documentation/computers/images/GPIO-Pinout-Diagram-2.png)

### Müsli Default Wiring

| Signal | GPIO# | Müsli Pin | Target Pin |
| ------ | ----- | --------- | ---------- |
| CSPI\_SS | 9 | 8 | 1 |
| CSPI\_SO | 8 | 7 | 4 |
| CSPI\_SI | 11 | 10 | 3 |
| CSPI\_SCK | 10 | 9 | 2 |
| CRESET | 3 | 4 | 5 |
| CDONE | 2 | 3 | 6 |

