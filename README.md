# ldprog

Programming software for Lone Dynamics FPGA boards

## Installation (Raspbian)

```
apt-get install pigpio
make
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

## Supported Interfaces

  * Raspiberry Pi GPIO

### Raspberry Pi GPIO Default Wiring

| Signal | GPIO# |
| ------ | ----- |
| CSPI\_SS | 25 |
| CSPI\_SO | 9 |
| CSPI\_SI | 10 |
| CSPI\_SCK | 11 |
| CRESET | 23 |
| CDONE | 24 |

