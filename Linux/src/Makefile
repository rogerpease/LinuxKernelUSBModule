obj-m += USBModuleCharDriver.o


KERNEL_DIR=/lib/modules/$(shell uname -r)/build/

# Set these to the Priv and DER keys to sign modules. 
PRIVFILE=""
DERFILE=""


#  If we were cross-compiling we would add: 	ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \  to each of these
all:
	make -C $(KERNEL_DIR) M=$(shell pwd) modules 
ifeq ($(PRIVFILE),"")
	exit 0 
else
	/usr/src/linux-headers-$(shell uname -r)/scripts/sign-file sha256 \
	    $PRIVFILE \
	    $DERFILE \
	    ./USBModuleCharDriver.ko 
endif 

clean:
	make -C $(KERNEL_DIR) M=$(shell pwd) clean 



