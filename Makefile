musli:
	gcc -Wall -DBACKEND_LIBUSB -o ldprog ldprog.c -lusb-1.0

gpio:
	gcc -Wall -DBACKEND_PIGPIO -o ldprog ldprog.c -lpigpio -pthread
