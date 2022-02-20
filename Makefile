build:
	gcc -Wall -DBACKEND_PIGPIO -o ldprog ldprog.c -lpigpio

clean:
	rm -f ldprog
