musli_libusb:
	gcc -Wall -DBACKEND_LIBUSB -o ldprog ldprog.c -lusb-1.0

musli_hidapi:
	gcc -Wall -DBACKEND_HIDAPI -o ldprog ldprog.c hidapi.c -ludev

musli_gpio:
	gcc -Wall -DBACKEND_PIGPIO -o ldprog ldprog.c -lpigpio
