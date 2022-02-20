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
#endif

// --
// CONFIGURATION:
// --

#define CSPI_SS	25
#define CSPI_SO	9
#define CSPI_SI	10
#define CSPI_SCK	11
#define CRESET		23
#define CDONE		24

#define RST_DELAY 250000
#define QSPI_MODE false

#define DELAY()

void fpga_reset(void);
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
   printf("usage: %s [-hsfvdr] <image.bin> [hex_offset] [hex_size]\n" \
      " -h\tdisplay help\n" \
      " -s\twrite <image.bin> to FPGA SRAM (default)\n" \
      " -f\twrite <image.bin> to flash starting at [hex_offset]\n" \
      " -v\tverify flash against <image.bin>\n" \
      " -d\tdump flash to file\n" \
      " -t\ttest fpga\n" \
      " -r\treset fpga\n" \
		"\nWARNING: writing to flash erases 4K blocks starting at offset\n",
      argv[0]);
}

#define MEM_TYPE_NONE 0
#define MEM_TYPE_TEST 1
#define MEM_TYPE_SRAM 2
#define MEM_TYPE_FLASH 3
#define MODE_READ 1
#define MODE_WRITE 2
#define ACTION_RESET 1

int spi_swap = 0;

int main(int argc, char *argv[]) {

   int opt;
   int mem_type;
	int mode;
	int actions = 0;

	uint32_t flash_offset = 0;
	uint32_t flash_size = 0;

   while ((opt = getopt(argc, argv, "hsfrdt")) != -1) {
      switch (opt) {
         case 'h': show_usage(argv); return(0); break;
         case 's': mem_type = MEM_TYPE_SRAM; mode = MODE_WRITE; break;
         case 'f': mem_type = MEM_TYPE_FLASH; mode = MODE_WRITE; break;
         case 'd': mem_type = MEM_TYPE_FLASH; mode = MODE_READ; break;
         case 't': mem_type = MEM_TYPE_TEST; break;
         case 'r': actions |= ACTION_RESET; break;
      }
   }

   if (optind >= argc && !actions) {
      show_usage(argv);
      return(1);
   }

   if (optind + 1 < argc) {
   	flash_offset = (uint32_t)strtol(argv[optind + 1], NULL, 16);
	}

   if (optind + 2 < argc) {
   	flash_size = (uint32_t)strtol(argv[optind + 2], NULL, 16);
	}

	if (gpioInitialise() < 0) return 1;

	GPIO_SET_MODE(CSPI_SS, PI_OUTPUT);
	GPIO_SET_MODE(CRESET, PI_OUTPUT);
	GPIO_SET_MODE(CDONE, PI_INPUT);

	if (mem_type == MEM_TYPE_TEST) {
      GPIO_WRITE(CRESET, 0);
		printf("test mode; holding in reset\n");
		while(1) {
			printf("cdone: %i\n", GPIO_READ(CDONE));
			usleep(500000);
		};
	}

	char *buf;
	uint32_t len;

	FILE *fp;

	if (mode == MODE_WRITE) {

		fp = fopen(argv[optind], "r");

		fseek(fp, 0L, SEEK_END);
		len = ftell(fp);
		rewind(fp);

		printf("file size: %lu\n", (unsigned long)len);

		buf = (char *)malloc(len);

		fread(buf, 1, len, fp);
		fclose(fp);

	}

	if (mem_type == MEM_TYPE_SRAM) {

		spi_swap = 1;

		GPIO_SET_MODE(CSPI_SI, PI_OUTPUT);
		GPIO_SET_MODE(CSPI_SO, PI_INPUT);

		// reset fpga into SPI slave configuration mode
		GPIO_WRITE(CSPI_SS, 0);
		GPIO_WRITE(CRESET, 0);
		usleep(RST_DELAY);
		GPIO_WRITE(CRESET, 1);
		usleep(RST_DELAY);

		printf("cdone: %i\n", GPIO_READ(CDONE));

		GPIO_WRITE(CSPI_SCK, 1);
		GPIO_WRITE(CSPI_SS, 1);
		spi_write(NULL, 1);
		GPIO_WRITE(CSPI_SS, 0);

		spi_write(buf, len);

		GPIO_WRITE(CSPI_SS, 1);
		spi_write(NULL, 14);

		usleep(100000);
		printf("cdone: %i\n", GPIO_READ(CDONE));

		// release the SPI pins
		GPIO_SET_MODE(CSPI_SCK, PI_INPUT);
		GPIO_SET_MODE(CSPI_SO, PI_INPUT);
		GPIO_SET_MODE(CSPI_SI, PI_INPUT);
		GPIO_SET_MODE(CSPI_SS, PI_INPUT);

	} else if (mem_type == MEM_TYPE_FLASH && mode == MODE_WRITE) {

		spi_swap = 0;

		GPIO_SET_MODE(CSPI_SI, PI_INPUT);
		GPIO_SET_MODE(CSPI_SO, PI_OUTPUT);

		char fbuf[256];
		int i = 0;
		int flen = len;

		// hold fpga in reset mode
		GPIO_WRITE(CRESET, 0);
		DELAY();

		GPIO_WRITE(CSPI_SCK, 0);
		GPIO_WRITE(CSPI_SS, 1);
		DELAY();

		printf(" flash status: 0x%.2x\n", flash_status());

		// exit power down mode
		printf("exiting powder down mode\n");
		GPIO_WRITE(CSPI_SS, 0);
		DELAY();
		spi_cmd(0xab);
		DELAY();
		GPIO_WRITE(CSPI_SS, 1);
		usleep(5000);
		printf(" flash status: 0x%.2x\n", flash_status());

		flash_write_enable();
		printf(" flash status: 0x%.2x\n", flash_status());

		// global block unlock
		printf("global block unlock ...\n");
		GPIO_WRITE(CSPI_SS, 0);
		DELAY();
		spi_cmd(0x98);
		DELAY();
		GPIO_WRITE(CSPI_SS, 1);
		usleep(5000);
		printf(" flash status: 0x%.2x\n", flash_status());

		int blk_size = 4096;
		int blks = len / blk_size;

		// bulk erase
		printf("erasing flash from %.6x to %.6x ...\n", 0, len);

		for (int blk = 0; blk <= blks; blk++) {

			printf(" erasing 4K flash at %.6x ...\n",
				flash_offset + (blk * blk_size));
			flash_write_enable();

			GPIO_WRITE(CSPI_SS, 0);
			DELAY();
			spi_cmd(0x20);
			spi_addr(flash_offset + (blk * blk_size));
			DELAY();
			GPIO_WRITE(CSPI_SS, 1);

			flash_wait();

		}

		while (i < len) {

			if (len - i >= 256) flen = 256; else flen = len - i;

			memcpy(fbuf, buf + i, flen);

			printf("writing %i bytes @ %.6X ...\n", flen, flash_offset + i);

			flash_write_enable();

			// program
			GPIO_WRITE(CSPI_SS, 0);
			spi_cmd(0x02);
			spi_addr(flash_offset + i);
			spi_write(fbuf, flen);
			GPIO_WRITE(CSPI_SS, 1);

			i += flen;

			flash_wait();

		}
		printf("done writing.\n");

		printf(" flash status: 0x%.2x\n", flash_status());

	} else if (mem_type == MEM_TYPE_FLASH && mode == MODE_READ) {

		spi_swap = 0;

		GPIO_SET_MODE(CSPI_SI, PI_INPUT);
		GPIO_SET_MODE(CSPI_SO, PI_OUTPUT);

		printf("reading flash to %s ...\n", argv[optind]);

		char fbuf[256];
		fp = fopen(argv[optind], "w");

		// hold fpga in reset mode
		GPIO_WRITE(CRESET, 0);
		GPIO_WRITE(CSPI_SS, 1);
		GPIO_WRITE(CSPI_SCK, 1);

		// exit power down mode
		printf("exiting powder down mode\n");
		GPIO_WRITE(CSPI_SS, 0);
		DELAY();
		spi_cmd(0xab);
		DELAY();
		GPIO_WRITE(CSPI_SS, 1);
		usleep(5000);
		printf(" flash status: 0x%.2x\n", flash_status());

		// read JEDEC ID
		printf("flash id: ");
		GPIO_WRITE(CSPI_SS, 0);
		spi_cmd(0x9f);
		for (int i = 0; i < 5; i++)
			printf("%.2x ", spi_read_byte());
		GPIO_WRITE(CSPI_SS, 1);
		printf("\n");

		printf("reading 0x%x bytes @ addr 0x%x\n", flash_size, flash_offset);

		for (int i = 0; i < flash_size / 256; i++) {

			printf("reading from %.6x\n", flash_offset + (i * 256));

			// read data from flash
			GPIO_WRITE(CSPI_SS, 0);
			spi_cmd(0x03);
			spi_addr(flash_offset + (i * 256));
			spi_read(fbuf, 256);
			GPIO_WRITE(CSPI_SS, 1);

			fwrite(fbuf, 256, 1, fp);

		}

		fclose(fp);

	}

	if ((actions & ACTION_RESET) == ACTION_RESET) fpga_reset();

	if (mode == MODE_WRITE)
		free(buf);

	return 0;

}

void fpga_reset(void) {

	printf("resetting FPGA\n");

	// release the SPI pins
	GPIO_SET_MODE(CSPI_SCK, PI_INPUT);
	GPIO_SET_MODE(CSPI_SO, PI_INPUT);
	GPIO_SET_MODE(CSPI_SI, PI_INPUT);
	GPIO_SET_MODE(CSPI_SS, PI_INPUT);
	GPIO_SET_MODE(CRESET, PI_OUTPUT);
	GPIO_SET_MODE(CDONE, PI_INPUT);

	// put fpga into master SPI mode and attempt self-configuration
	GPIO_WRITE(CRESET, 0);
	usleep(1000000);
	GPIO_WRITE(CRESET, 1);
	usleep(1000000);

	printf("cdone: %i\n", GPIO_READ(CDONE));
	usleep(1000000);
	printf("cdone: %i\n", GPIO_READ(CDONE));
	usleep(1000000);
	printf("cdone: %i\n", GPIO_READ(CDONE));
	usleep(1000000);
	printf("cdone: %i\n", GPIO_READ(CDONE));

};

void spi_cmd(uint8_t cmd) {

	uint8_t data_bit;

	for (int i = 7; i >= 0; i--) {
		GPIO_WRITE(CSPI_SCK, 0);
		data_bit = (cmd >> i) & 0x01;
		if (spi_swap)
			GPIO_WRITE(CSPI_SI, data_bit);
		else
			GPIO_WRITE(CSPI_SO, data_bit);
		GPIO_WRITE(CSPI_SCK, 1);
	}

}

void spi_addr(uint32_t addr) {

	uint8_t data_bit;

	for (int i = 23; i >= 0; i--) {
		GPIO_WRITE(CSPI_SCK, 0);
		data_bit = (addr >> i) & 0x01;
		if (spi_swap)
			GPIO_WRITE(CSPI_SI, data_bit);
		else
			GPIO_WRITE(CSPI_SO, data_bit);
		GPIO_WRITE(CSPI_SCK, 1);
	}
}

void spi_write(void *buf, uint32_t len) {

	uint8_t data_bit;
	uint8_t data_byte;

	for (int p = 0; p < len; p++) {

		if (buf != NULL)
			data_byte = *(unsigned char *)(buf + p);

		for (int i = 7; i >= 0; i--) {
			GPIO_WRITE(CSPI_SCK, 0);
			data_bit = (data_byte >> i) & 0x01;
			if (spi_swap)
				GPIO_WRITE(CSPI_SI, data_bit);
			else
				GPIO_WRITE(CSPI_SO, data_bit);
			GPIO_WRITE(CSPI_SCK, 1);
		}

	}

}

void spi_read(void *buf, uint32_t len) {

	uint8_t data_byte;

	for (int p = 0; p < len; p++) {

		data_byte = spi_read_byte();
		*(unsigned char *)(buf + p) = data_byte;

	}

}


uint8_t spi_read_byte(void) {

	uint8_t data_bit;
	uint8_t data_byte = 0x00;

	for (int i = 7; i >= 0; i--) {
		GPIO_WRITE(CSPI_SCK, 0);
		GPIO_WRITE(CSPI_SCK, 1);
		if (spi_swap)
			data_bit = GPIO_READ(CSPI_SO);
		else
			data_bit = GPIO_READ(CSPI_SI);
		data_byte |= data_bit << i;
	}

	return data_byte;

}

uint8_t flash_status(void) {

	uint8_t status;

	GPIO_WRITE(CSPI_SS, 0);
	spi_cmd(0x05);
	status = spi_read_byte();
	GPIO_WRITE(CSPI_SS, 1);

	return(status);

}

void flash_wait(void) {
	int status;
	while (((status = flash_status()) & 0x01) == 0x01) {
		usleep(100);
	}
}

void flash_write_enable(void) {
	GPIO_WRITE(CSPI_SS, 0);
	DELAY();
	spi_cmd(0x06);
	DELAY();
	GPIO_WRITE(CSPI_SS, 1);
	usleep(5000);
}
