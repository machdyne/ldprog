/*
 * Lone Dynamics Device Programmer
 * Copyright (c) 2021 Lone Dynamics Corporation. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#ifdef BACKEND_PIGPIO

 #include <pigpio.h>
 #define GPIO_WRITE gpioWrite
 #define GPIO_READ gpioRead
 #define GPIO_SET_MODE gpioSetMode

 #define CSPI_SS	25
 #define CSPI_SO	9
 #define CSPI_SI	10
 #define CSPI_SCK	11
 #define CRESET	23
 #define CDONE		24

#elif BACKEND_LIBUSB

 #include <libusb-1.0/libusb.h>
 #define GPIO_WRITE musliWrite
 #define GPIO_READ musliRead
 #define GPIO_SET_MODE musliSetMode
 #define PI_INPUT 0
 #define PI_OUTPUT 1
 #define USB_MFG_ID 0x2e8a
 #define USB_DEV_ID 0x1025
 #define CSPI_SS	9
 #define CSPI_SO	8
 #define CSPI_SI	11
 #define CSPI_SCK	10
 #define CDONE		2
 #define CRESET	3
 #define MUSLI_BLK_SIZE 60
 #define MUSLI_CMD_READY 0x00
 #define MUSLI_CMD_INIT 0x01
 #define MUSLI_CMD_GPIO_SET_DIR 0x10
 #define MUSLI_CMD_GPIO_DISABLE_PULLS 0x11
 #define MUSLI_CMD_GPIO_PULL_UP 0x12
 #define MUSLI_CMD_GPIO_PULL_DOWN 0x13
 #define MUSLI_CMD_GPIO_GET 0x20
 #define MUSLI_CMD_GPIO_PUT 0x21
 #define MUSLI_CMD_SPI_READ 0x80
 #define MUSLI_CMD_SPI_WRITE 0x81
 #define MUSLI_CMD_CFG_PIO_SPI 0x8f
 #define MUSLI_CMD_RESET 0xf0
 void musliInit(uint8_t mode);
 void musliCmd(uint8_t cmd, uint8_t arg1, uint8_t arg2, uint8_t arg3);
 void musliSetMode(uint8_t pin, uint8_t dir);
 void musliWrite(uint8_t pin, uint8_t bit);
 uint8_t musliRead(uint8_t pin);
 struct libusb_device_handle *usb_dh = NULL;

#endif

// --
// CONFIGURATION:
// --
#define RST_DELAY 250000
#define QSPI_MODE false

#define DELAY() usleep(1000);

void fpga_reset(void);
void spi_release(void);
void spi_cmd(uint8_t cmd);
void spi_addr(uint32_t addr);
void spi_write(void *buf, uint32_t len);
void spi_read(void *buf, uint32_t len);
uint8_t spi_read_byte(void);
uint8_t flash_status(void);
void flash_wait(void);
void flash_write_enable(void);

// --

void show_usage(char **argv);

void show_usage(char **argv) {
   printf("usage: %s [-hsfgvmdr] [-a <bus> <addr>] <image.bin> [hex_offset] [hex_size]\n" \
      " -h\tdisplay help\n" \
      " -s\twrite <image.bin> to FPGA SRAM (default)\n" \
      " -f\twrite <image.bin> to flash starting at [hex_offset]\n" \
      " -d\tdump flash to file\n" \
      " -e\tbulk erase entire flash\n" \
      " -t\ttest fpga\n" \
      " -c\tsend musli command (args: <hex_cmd> [hex_arg1] [hex_arg2] [hex_arg3])\n" \
      " -g\tread or write gpio (args: <gpio#> [0/1])\n" \
      " -r\treset fpga\n" \
      " -m\tmanual reset mode\n" \
      " -a\tusb bus and address are specified as first argument\n" \
      " -b\tbonbon mode\n" \
      " -k\tkeks mode\n" \
      " -i\teis mode\n" \
      " -w\twerkzeug mode (only for flashing MMODs via Werkzeugs PMOD)\n" \
		"\nWARNING: writing to flash erases 4K blocks starting at offset\n",
      argv[0]);
}

#define MEM_TYPE_NONE 0
#define MEM_TYPE_TEST 1
#define MEM_TYPE_SRAM 2
#define MEM_TYPE_FLASH 3
#define MODE_NONE 0
#define MODE_READ 1
#define MODE_VERIFY 2
#define MODE_WRITE 3
#define MODE_ERASE 4
#define MODE_CMD 100
#define MODE_GPIO 200
#define OPTION_RESET 1
#define OPTION_MANUAL_RESET 2
#define OPTION_ADDR 4
#define OPTION_BONBON 8
#define OPTION_WERKZEUG 16
#define OPTION_KEKS 32
#define OPTION_EIS 64

int debug = 0;
int spi_swap = 0;

uint8_t cspi_ss = CSPI_SS;
uint8_t cspi_si = CSPI_SI;
uint8_t cspi_so = CSPI_SO;
uint8_t cspi_sck = CSPI_SCK;
uint8_t cdone = CDONE;
uint8_t creset = CRESET;

int main(int argc, char *argv[]) {

   int opt;
   int mem_type;
	int mode = MODE_NONE;
	int options = 0;

	uint32_t flash_offset = 0;
	uint32_t flash_size = 0;
	int gpionum;
	int gpioval = -1;

   while ((opt = getopt(argc, argv, "hsfrdvmetagbcDwki")) != -1) {
      switch (opt) {
         case 'h': show_usage(argv); return(0); break;
         case 's': mem_type = MEM_TYPE_SRAM; mode = MODE_WRITE; break;
         case 'f': mem_type = MEM_TYPE_FLASH; mode = MODE_WRITE; break;
         case 'd': mem_type = MEM_TYPE_FLASH; mode = MODE_READ; break;
         case 'v': mem_type = MEM_TYPE_FLASH; mode = MODE_VERIFY; break;
         case 'e': mem_type = MEM_TYPE_FLASH; mode = MODE_ERASE; break;
         case 't': mem_type = MEM_TYPE_TEST; break;
         case 'g': mode = MODE_GPIO; break;
         case 'c': mode = MODE_CMD; break;
         case 'a': options |= OPTION_ADDR; break;
         case 'r': options |= OPTION_RESET; break;
         case 'm': options |= OPTION_MANUAL_RESET; break;
         case 'b': options |= OPTION_BONBON; break;
         case 'k': options |= OPTION_KEKS; break;
         case 'i': options |= OPTION_EIS; break;
         case 'w': options |= OPTION_WERKZEUG; break;
         case 'D': debug = 1; break;
      }
   }

	int usb_bus = -1;
	int usb_addr = -1;

	int musli_cmd = 0;
	int musli_arg1 = 0;
	int musli_arg2 = 0;
	int musli_arg3 = 0;

	if ((options & OPTION_ADDR) == OPTION_ADDR) {
		
		usb_bus = (uint32_t)strtol(argv[optind], NULL, 10);
		usb_addr = (uint32_t)strtol(argv[optind + 1], NULL, 10);

		optind += 2;

	}

	if ((options & OPTION_BONBON) == OPTION_BONBON) {
		cspi_ss = 29;
		cspi_so = 28;
		cspi_si = 27;
		cspi_sck = 26;
		cdone = 18;
		creset = 19;
	}

	if ((options & OPTION_KEKS) == OPTION_KEKS) {
		cspi_ss = 25;
		cspi_so = 24;
		cspi_si = 27;
		cspi_sck = 26;
		cdone = 22;
		creset = 23;
	}

	if ((options & OPTION_EIS) == OPTION_EIS) {
		cspi_ss = 22;
		cspi_so = 24;
		cspi_si = 27;
		cspi_sck = 26;
		cdone = 3;
		creset = 2;
	}

	if (((options & OPTION_WERKZEUG) == OPTION_WERKZEUG) && mem_type == MEM_TYPE_FLASH) {
		cspi_ss = 19;	// PMOD_A1 / MMOD PIN 1 (SS)
		cspi_so = 17;	// PMOD_A2 / MMOD PIN 2 (MISO)
		cspi_si = 15;	// PMOD_A3 / MMOD PIN 3 (MOSI)
		cspi_sck = 13;	// PMOD_A4 /  MMOD PIN 4 (SCK)
	}

   if ((mode == MODE_READ || mode == MODE_WRITE) && optind >= argc) {
      show_usage(argv);
      return(1);
   }

   if (mode == MODE_GPIO && optind >= argc) {
      show_usage(argv);
      return(1);
   }

   if (mode == MODE_CMD) {

		musli_cmd = (uint32_t)strtol(argv[optind], NULL, 16);
		if (optind + 1 < argc) 
			musli_arg1 = (uint32_t)strtol(argv[optind + 1], NULL, 16);
		if (optind + 2 < argc) 
			musli_arg2 = (uint32_t)strtol(argv[optind + 2], NULL, 16);
		if (optind + 3 < argc) 
			musli_arg3 = (uint32_t)strtol(argv[optind + 3], NULL, 16);

   } else if (mode == MODE_GPIO) {

		gpionum = (uint32_t)strtol(argv[optind], NULL, 10);
		if (optind + 1 < argc) 
			gpioval = (uint32_t)strtol(argv[optind + 1], NULL, 10);

	} else {

		if (optind + 1 < argc) {
			flash_offset = (uint32_t)strtol(argv[optind + 1], NULL, 16);
		}

		if (optind + 2 < argc) {
			flash_size = (uint32_t)strtol(argv[optind + 2], NULL, 16);
		}

	}

#ifdef BACKEND_PIGPIO
	if (gpioInitialise() < 0) {
		fprintf(stderr, "gpio init error\n");
		exit(1);
	}
#elif BACKEND_LIBUSB
	if (libusb_init(NULL) < 0) {
		fprintf(stderr, "usb init error\n");
		exit(1);
	}

	libusb_device **list = NULL;
	ssize_t count = 0;

	count = libusb_get_device_list(NULL, &list);

	printf("devices found: \n");

	for (size_t idx = 0; idx < count; ++idx) {

		libusb_device *dev = list[idx];
		struct libusb_device_descriptor desc = {0};

		int rc = libusb_get_device_descriptor(dev, &desc);
		if (rc != 0) continue;

		int bus = libusb_get_bus_number(dev);
		int addr = libusb_get_device_address(dev);

		if (desc.idVendor == USB_MFG_ID && desc.idProduct == USB_DEV_ID) {

			printf(" vendor %04x id %04x serial %i bus %i addr %i\n",
				desc.idVendor, desc.idProduct, desc.iSerialNumber, bus, addr);

			if ((usb_bus == -1 && usb_addr == -1) ||
					(usb_bus == bus && usb_addr == addr)) {

				if (usb_dh == NULL) {
					printf("using bus %i addr %i\n", bus, addr);
					libusb_open(dev, &usb_dh);
				}

			}
		}

	}

	if (count == 0) printf("none.\n");

	libusb_free_device_list(list, count);

	if (!usb_dh) {
		fprintf(stderr, "usb device error\n");
		exit(1);
	}

	if (mode == MODE_CMD) {

		if (debug)
			printf("send cmd [%.2x %.2x %.2x %.2x]\n", musli_cmd,
				musli_arg1, musli_arg2, musli_arg3);

		musliCmd(musli_cmd, musli_arg1, musli_arg2, musli_arg3);
		exit(0);

	} else if (mode == MODE_GPIO) {

		if (gpioval != -1) {
			GPIO_SET_MODE(gpionum, PI_OUTPUT);
      	GPIO_WRITE(gpionum, gpioval);
			printf("write gpio #%i val: 0x%.2X\n", gpionum, gpioval);
		} else {
			GPIO_SET_MODE(gpionum, PI_INPUT);
			gpioval = GPIO_READ(gpionum);
			printf("read gpio #%i val: 0x%.2X\n", gpionum, gpioval);
		}

		exit(0);

	}

	if (mem_type == MEM_TYPE_FLASH) {
		musliInit(0);
		musliInit(2);
		// args: sck, mosi, mosi
		musliCmd(MUSLI_CMD_CFG_PIO_SPI, cspi_sck, cspi_so, cspi_si);
	} else {
		musliInit(0);
	}

#endif

	GPIO_SET_MODE(cspi_ss, PI_OUTPUT);
	GPIO_SET_MODE(creset, PI_OUTPUT);
	GPIO_SET_MODE(cdone, PI_INPUT);

	if (mem_type == MEM_TYPE_TEST) {
      GPIO_WRITE(creset, 0);
		printf("test mode; holding in reset\n");
		while(1) {
			printf("cdone: %i\n", GPIO_READ(cdone));
			usleep(500000);
		};
	}

	char *buf;
	uint32_t len;

	FILE *fp;

	if (mode == MODE_WRITE || mode == MODE_VERIFY) {

		fp = fopen(argv[optind], "r");

		if (fp == NULL) {
			fprintf(stderr, "unable to open file: %s\n", argv[optind]);
			exit(1);
		}

		fseek(fp, 0L, SEEK_END);
		len = ftell(fp);
		rewind(fp);

		printf("file size: %lu\n", (unsigned long)len);

		buf = (char *)malloc(len);

		fread(buf, 1, len, fp);
		fclose(fp);

	}

	if (mem_type == MEM_TYPE_SRAM) {

		printf("writing to sram ...\n");

		spi_swap = 1;

		GPIO_SET_MODE(cspi_si, PI_OUTPUT);
		GPIO_SET_MODE(cspi_so, PI_INPUT);

		if ((options & OPTION_MANUAL_RESET) == OPTION_MANUAL_RESET)
			printf("press reset button now\n");

		// reset fpga into SPI slave configuration mode
		GPIO_WRITE(cspi_ss, 0);
		GPIO_WRITE(creset, 0);
		usleep(RST_DELAY);
		printf("cdone: %i\n", GPIO_READ(cdone));

		if ((options & OPTION_MANUAL_RESET) == OPTION_MANUAL_RESET) {
			usleep(2000000);
			printf("release reset button now\n");
			usleep(2000000);
		}

		GPIO_WRITE(creset, 1);
		usleep(RST_DELAY);

		printf("cdone: %i\n", GPIO_READ(cdone));

		GPIO_WRITE(cspi_sck, 1);
		GPIO_WRITE(cspi_ss, 1);
		spi_write(NULL, 1);
		GPIO_WRITE(cspi_ss, 0);

		spi_write(buf, len);

		GPIO_WRITE(cspi_ss, 1);
		spi_write(NULL, 14);

		usleep(100000);
		printf("cdone: %i\n", GPIO_READ(cdone));

	} else if (mem_type == MEM_TYPE_FLASH && mode == MODE_WRITE) {

		spi_swap = 0;

#ifdef BACKEND_PIGPIO
		GPIO_SET_MODE(cspi_si, PI_INPUT);
		GPIO_SET_MODE(cspi_so, PI_OUTPUT);
#endif

		char fbuf[256];
		char vbuf[256];
		int i = 0;
		int flen = len;

		// hold fpga in reset mode
		GPIO_WRITE(creset, 0);
		DELAY();

#ifdef BACKEND_PIGPIO
		GPIO_WRITE(cspi_sck, 0);
#endif

		GPIO_WRITE(cspi_ss, 1);
		DELAY();

		printf(" flash status: 0x%.2x\n", flash_status());

		// exit power down mode
		printf("exiting power down mode\n");
		GPIO_WRITE(cspi_ss, 0);
		DELAY();
		spi_cmd(0xab);
		DELAY();
		GPIO_WRITE(cspi_ss, 1);
		usleep(5000);
		printf(" flash status: 0x%.2x\n", flash_status());

		flash_write_enable();
		printf(" flash status: 0x%.2x\n", flash_status());

		// global block unlock
		printf("global block unlock ...\n");
		GPIO_WRITE(cspi_ss, 0);
		DELAY();
		spi_cmd(0x98);
		DELAY();
		GPIO_WRITE(cspi_ss, 1);
		usleep(5000);
		printf(" flash status: 0x%.2x\n", flash_status());

		int blk_size = 4096;
		int blks = (len / blk_size);

		if (!blks) blks = 1;

		// bulk erase
		printf("erasing flash from %.6x to %.6x ...\n",
			flash_offset, flash_offset + blks*blk_size);

		for (int blk = 0; blk <= blks + 1; blk++) {

			printf(" erasing 4K flash at %.6x ...\n",
				flash_offset + (blk * blk_size));
			flash_write_enable();

			GPIO_WRITE(cspi_ss, 0);
			DELAY();
			spi_cmd(0x20);
			spi_addr(flash_offset + (blk * blk_size));
			DELAY();
			GPIO_WRITE(cspi_ss, 1);

			flash_wait();

		}

		printf("writing %i bytes @ %.6X ...\n", len, flash_offset);

		while (i < len) {

			int maxtries = 16;

			if (len - i >= 256) flen = 256; else flen = len - i;

			memcpy(fbuf, buf + i, flen);

			tryagain:

			printf(" writing %i bytes @ %.6x ... ", flen, flash_offset + i);

			flash_write_enable();

			// program
			GPIO_WRITE(cspi_ss, 0);
			spi_cmd(0x02);
			spi_addr(flash_offset + i);
			spi_write(fbuf, flen);
			GPIO_WRITE(cspi_ss, 1);

			// read back
			GPIO_WRITE(cspi_ss, 0);
			spi_cmd(0x03);
			spi_addr(flash_offset + i);
			spi_read(vbuf, flen);
			GPIO_WRITE(cspi_ss, 1);

			if (!memcmp(fbuf, vbuf, flen)) {
				printf("ok\n");
			} else {
				printf("failed; retrying\n");
				--maxtries;
				if (maxtries) {
					goto tryagain;
				} else {
					printf("failed to write; aborting\n");
					exit(1);
				}
			}

			i += flen;

			flash_wait();

		}
		printf("done writing.\n");

		printf(" flash status: 0x%.2x\n", flash_status());

	} else if (mem_type == MEM_TYPE_FLASH && mode == MODE_READ) {

		spi_swap = 0;

#ifdef BACKEND_PIGPIO
		GPIO_SET_MODE(cspi_si, PI_INPUT);
		GPIO_SET_MODE(cspi_so, PI_OUTPUT);
#endif

		printf("reading flash to %s ...\n", argv[optind]);

		char fbuf[256];
		fp = fopen(argv[optind], "w");

		// hold fpga in reset mode
		GPIO_WRITE(creset, 0);
		GPIO_WRITE(cspi_ss, 1);

#ifdef BACKEND_PIGPIO
		GPIO_WRITE(cspi_sck, 1);
#endif

		// exit power down mode
		printf("exiting power down mode\n");
		GPIO_WRITE(cspi_ss, 0);
		DELAY();
		spi_cmd(0xab);
		DELAY();
		GPIO_WRITE(cspi_ss, 1);
		usleep(5000);
		printf(" flash status: 0x%.2x\n", flash_status());

		// read JEDEC ID
		printf("flash id: ");
		GPIO_WRITE(cspi_ss, 0);
		spi_cmd(0x9f);
		for (int i = 0; i < 5; i++)
			printf("%.2x ", spi_read_byte());
		GPIO_WRITE(cspi_ss, 1);
		printf("\n");

		printf("reading %i bytes @ addr 0x%x\n", flash_size, flash_offset);

		for (int i = 0; i < flash_size / 256; i++) {

			printf("reading from 0x%.6x\n", flash_offset + (i * 256));

			// read data from flash
			GPIO_WRITE(cspi_ss, 0);
			spi_cmd(0x03);
			spi_addr(flash_offset + (i * 256));
			spi_read(fbuf, 256);
			GPIO_WRITE(cspi_ss, 1);

			fwrite(fbuf, 256, 1, fp);

		}

		fclose(fp);

	} else if (mem_type == MEM_TYPE_FLASH && mode == MODE_VERIFY) {

		spi_swap = 0;
		char fbuf[256];
		int i = 0;
		int flen;
		int mismatches = 0;

#ifdef BACKEND_PIGPIO
		GPIO_SET_MODE(cspi_si, PI_INPUT);
		GPIO_SET_MODE(cspi_so, PI_OUTPUT);
#endif

		printf("verifying flash ...\n");

		// hold fpga in reset mode
		GPIO_WRITE(creset, 0);
		GPIO_WRITE(cspi_ss, 1);

#ifdef BACKEND_PIGPIO
		GPIO_WRITE(cspi_sck, 1);
#endif

		// exit power down mode
		printf("exiting power down mode\n");
		GPIO_WRITE(cspi_ss, 0);
		DELAY();
		spi_cmd(0xab);
		DELAY();
		GPIO_WRITE(cspi_ss, 1);
		usleep(5000);
		printf(" flash status: 0x%.2x\n", flash_status());

		// read JEDEC ID
		printf("flash id: ");
		GPIO_WRITE(cspi_ss, 0);
		spi_cmd(0x9f);
		for (int i = 0; i < 5; i++)
			printf("%.2x ", spi_read_byte());
		GPIO_WRITE(cspi_ss, 1);
		printf("\n");

		printf("verifying %i bytes @ addr 0x%x\n", len, flash_offset);

		while (i < len) {

			if (len - i >= 256) flen = 256; else flen = len - i;

			printf(" reading %i bytes from 0x%.6x\n", flen, flash_offset + i);

			// read data from flash
			GPIO_WRITE(cspi_ss, 0);
			spi_cmd(0x03);
			spi_addr(flash_offset + i);
			spi_read(fbuf, flen);
			GPIO_WRITE(cspi_ss, 1);


			if (memcmp(fbuf, buf + i, flen)) {
				printf(" *** mismatch @ 0x%.6x\n", i);
				printf("   FILE: ");
				for (int x = 0; x < 256; x++) printf("%02x ", (unsigned char)buf[x+i]);
				printf("\n  FLASH: ");
				for (int x = 0; x < 256; x++) printf("%02x ", (unsigned char)fbuf[x]);
				printf("\n\n");
				mismatches++;
			}

			i += flen;

		}

		printf("block mismatches: %i\n", mismatches);

	} else if (mem_type == MEM_TYPE_FLASH && mode == MODE_ERASE) {

		spi_swap = 0;

		GPIO_SET_MODE(cspi_si, PI_INPUT);
		GPIO_SET_MODE(cspi_so, PI_OUTPUT);

		// hold fpga in reset mode
		GPIO_WRITE(creset, 0);
		DELAY();

#ifdef BACKEND_PIGPIO
		GPIO_WRITE(cspi_sck, 0);
#endif
		GPIO_WRITE(cspi_ss, 1);
		DELAY();

		printf(" flash status: 0x%.2x\n", flash_status());

		// exit power down mode
		printf("exiting power down mode\n");
		GPIO_WRITE(cspi_ss, 0);
		DELAY();
		spi_cmd(0xab);
		DELAY();
		GPIO_WRITE(cspi_ss, 1);
		usleep(5000);
		printf(" flash status: 0x%.2x\n", flash_status());

		flash_write_enable();
		printf(" flash status: 0x%.2x\n", flash_status());

		// global block unlock
		printf("global block unlock ...\n");
		GPIO_WRITE(cspi_ss, 0);
		DELAY();
		spi_cmd(0x98);
		DELAY();
		GPIO_WRITE(cspi_ss, 1);
		usleep(5000);
		printf(" flash status: 0x%.2x\n", flash_status());

		// bulk erase
		printf(" erasing flash ...\n");
		flash_write_enable();

		GPIO_WRITE(cspi_ss, 0);
		DELAY();
		spi_cmd(0xc7);
		DELAY();
		GPIO_WRITE(cspi_ss, 1);

		flash_wait();

		printf("done erasing.\n");

	}

	if ((options & OPTION_RESET) == OPTION_RESET) fpga_reset();

	if (mode == MODE_WRITE)
		free(buf);

	if ( ((options & OPTION_BONBON) == OPTION_BONBON) ||
			((options & OPTION_KEKS) == OPTION_KEKS) ) {
		musliInit(3);
	} else {
		spi_release();
	}

#ifdef BACKEND_LIBUSB
	libusb_exit(NULL);
#endif

	return 0;

}

void fpga_reset(void) {

	printf("resetting FPGA\n");

	// release the SPI pins
	spi_release();
	GPIO_SET_MODE(creset, PI_OUTPUT);
	GPIO_SET_MODE(cdone, PI_INPUT);

	printf("cdone: %i\n", GPIO_READ(cdone));

	// put fpga into master SPI mode and attempt self-configuration
	GPIO_WRITE(creset, 0);
	usleep(100000);
	GPIO_WRITE(creset, 1);
	usleep(100000);

	printf("cdone: %i\n", GPIO_READ(cdone));
	usleep(100000);
	printf("cdone: %i\n", GPIO_READ(cdone));

};

void spi_release(void) {
	musliInit(1);
	GPIO_SET_MODE(cspi_sck, PI_INPUT);
	GPIO_SET_MODE(cspi_so, PI_INPUT);
	GPIO_SET_MODE(cspi_si, PI_INPUT);
	GPIO_SET_MODE(cspi_ss, PI_INPUT);
}

void spi_cmd(uint8_t cmd) {

#ifdef BACKEND_LIBUSB
  	int actual;
	uint8_t lbuf[64];
	bzero(lbuf, 64);
	lbuf[0] = MUSLI_CMD_SPI_WRITE;
	lbuf[1] = 1;
	lbuf[4] = cmd;
	if (debug)
  		printf(" spi_cmd [%.2x]\n", cmd);
  	libusb_bulk_transfer(usb_dh, (1 | LIBUSB_ENDPOINT_OUT), lbuf, 64,
		&actual, 0);
#else
	uint8_t data_bit;
	for (int i = 7; i >= 0; i--) {
		GPIO_WRITE(cspi_sck, 0);
		data_bit = (cmd >> i) & 0x01;
		if (spi_swap)
			GPIO_WRITE(cspi_si, data_bit);
		else
			GPIO_WRITE(cspi_so, data_bit);
		GPIO_WRITE(cspi_sck, 1);
	}
#endif

}

void spi_addr(uint32_t addr) {

#ifdef BACKEND_LIBUSB
  	int actual;
	uint8_t lbuf[64];
	bzero(lbuf, 64);
	lbuf[0] = MUSLI_CMD_SPI_WRITE;
	lbuf[1] = 3;
	lbuf[4] = addr >> 16;
	lbuf[5] = addr >> 8;
	lbuf[6] = addr;
	if (debug)
		printf(" spi_addr [%.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x]\n",
			lbuf[0], lbuf[1], lbuf[2], lbuf[3],
			lbuf[4], lbuf[5], lbuf[6], lbuf[7]);
  	libusb_bulk_transfer(usb_dh, (1 | LIBUSB_ENDPOINT_OUT), lbuf, 64,
		&actual, 0);
#else
	uint8_t data_bit;
	for (int i = 23; i >= 0; i--) {
		GPIO_WRITE(cspi_sck, 0);
		data_bit = (addr >> i) & 0x01;
		if (spi_swap)
			GPIO_WRITE(cspi_si, data_bit);
		else
			GPIO_WRITE(cspi_so, data_bit);
		GPIO_WRITE(cspi_sck, 1);
	}
#endif

}

void spi_write(void *buf, uint32_t len) {

#ifdef BACKEND_LIBUSB
  	int actual;
	uint8_t lbuf[64];

	uint32_t rlen = len;
	uint32_t offset = 0;

	for (int blk = 0; blk < len / MUSLI_BLK_SIZE; blk++) {

		bzero(lbuf, 64);
		lbuf[0] = MUSLI_CMD_SPI_WRITE;
		lbuf[1] = MUSLI_BLK_SIZE;
		if (buf != NULL)
			memcpy(lbuf + 4, buf + offset, MUSLI_BLK_SIZE);
  		libusb_bulk_transfer(usb_dh, (1 | LIBUSB_ENDPOINT_OUT), lbuf, 64,
			&actual, 0);

		if (debug) printf("spi_write: [%i/%i]\n", offset, len);

		rlen -= MUSLI_BLK_SIZE;
		offset += MUSLI_BLK_SIZE;

	}

	if (rlen) {
		if (debug) printf("spi_write: [%i/%i]\n", offset, len);
		bzero(lbuf, 64);
		lbuf[0] = MUSLI_CMD_SPI_WRITE;
		lbuf[1] = rlen;
		if (buf != NULL)
			memcpy(lbuf + 4, buf + offset, rlen);
  		libusb_bulk_transfer(usb_dh, (1 | LIBUSB_ENDPOINT_OUT), lbuf, 64,
			&actual, 0);
	}

#else

	uint8_t data_bit;
	uint8_t data_byte;

	for (int p = 0; p < len; p++) {

		printf(" spi writing byte %i / %i\n", p, len);

		if (buf != NULL)
			data_byte = *(unsigned char *)(buf + p);

		for (int i = 7; i >= 0; i--) {
			GPIO_WRITE(cspi_sck, 0);
			data_bit = (data_byte >> i) & 0x01;
			if (spi_swap)
				GPIO_WRITE(cspi_si, data_bit);
			else
				GPIO_WRITE(cspi_so, data_bit);
			GPIO_WRITE(cspi_sck, 1);
		}

	}

#endif

}

void spi_read(void *buf, uint32_t len) {

#ifdef BACKEND_LIBUSB
  	int actual;
	uint8_t lbuf[64];

	uint32_t rlen = len;
	uint32_t offset = 0;

	for (int blk = 0; blk < len / 64; blk++) {

		bzero(lbuf, 64);
		musliCmd(MUSLI_CMD_SPI_READ, 64, 0, 0);
  		libusb_bulk_transfer(usb_dh, (2 | LIBUSB_ENDPOINT_IN), lbuf, 64,
			&actual, 0);

		if (buf != NULL)
			memcpy(buf + offset, lbuf, 64);

		if (debug)
	  		printf("spi_read: %i/%d [%i/%i]\n", actual, 64, offset, len);

		rlen -= 64;
		offset += 64;

	}

	if (rlen) {
		bzero(lbuf, 64);
		musliCmd(MUSLI_CMD_SPI_READ, 64, 0, 0);
  		libusb_bulk_transfer(usb_dh, (2 | LIBUSB_ENDPOINT_IN), lbuf, 64,
			&actual, 0);

		if (buf != NULL)
			memcpy(buf + offset, lbuf, rlen);
	}
#else
	uint8_t data_byte;
	for (int p = 0; p < len; p++) {

		data_byte = spi_read_byte();
		*(unsigned char *)(buf + p) = data_byte;

	}
#endif

}

uint8_t spi_read_byte(void) {

#ifdef BACKEND_LIBUSB
   int actual;
	uint8_t buf[64];
	musliCmd(MUSLI_CMD_SPI_READ, 1, 0, 0);
   libusb_bulk_transfer(usb_dh, (2 | LIBUSB_ENDPOINT_IN), buf, 64,
      &actual, 0);
	return buf[0];
#else
	uint8_t data_bit;
	uint8_t data_byte = 0x00;
	for (int i = 7; i >= 0; i--) {
		GPIO_WRITE(cspi_sck, 0);
		GPIO_WRITE(cspi_sck, 1);
		if (spi_swap)
			data_bit = GPIO_READ(cspi_so);
		else
			data_bit = GPIO_READ(cspi_si);
		data_byte |= data_bit << i;
	}

	return data_byte;
#endif

}

uint8_t flash_status(void) {

	uint8_t status;

	GPIO_WRITE(cspi_ss, 0);
	spi_cmd(0x05);
	status = spi_read_byte();
	GPIO_WRITE(cspi_ss, 1);

	return(status);

}

void flash_wait(void) {
	int status;
	while (((status = flash_status()) & 0x01) == 0x01) {
		usleep(100);
	}
}

void flash_write_enable(void) {
	GPIO_WRITE(cspi_ss, 0);
	DELAY();
	spi_cmd(0x06);
	DELAY();
	GPIO_WRITE(cspi_ss, 1);
	usleep(5000);
}

// ---

#ifdef BACKEND_LIBUSB

void musliInit(uint8_t mode) {
	musliCmd(MUSLI_CMD_INIT, mode, 0, 0);
}

void musliCmd(uint8_t cmd, uint8_t arg1, uint8_t arg2, uint8_t arg3) {
   int actual;
	uint8_t buf[64];
	bzero(buf, 64);
	buf[0] = cmd;
	buf[1] = arg1;
	buf[2] = arg2;
	buf[3] = arg3;
	if (debug)
		printf("send cmd [%.2x %.2x %.2x %.2x]\n", cmd, arg1, arg2, arg3);
   libusb_bulk_transfer(usb_dh, (1 | LIBUSB_ENDPOINT_OUT), buf, 64,
      &actual, 0);
}

// bit banging interface

void musliSetMode(uint8_t pin, uint8_t dir) {
	musliCmd(MUSLI_CMD_GPIO_SET_DIR, pin, dir, 0);
}

void musliWrite(uint8_t pin, uint8_t val) {
	musliCmd(MUSLI_CMD_GPIO_PUT, pin, val, 0);
}

uint8_t musliRead(uint8_t pin) {
   int actual;
	uint8_t buf[64];
	musliCmd(MUSLI_CMD_GPIO_GET, pin, 0, 0);
   libusb_bulk_transfer(usb_dh, (2 | LIBUSB_ENDPOINT_IN), buf, 64,
      &actual, 0);
	return buf[0];
}

#endif
