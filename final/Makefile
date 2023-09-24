
obj-m := adxl_i2c_driver.o

adxl_i2c_driver.ko: adxl_i2c_driver.c
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C /home/msi/LabPracticeFiles/BB_KernelC/linux/ M=`pwd` modules
	
clean:
	make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -C  /home/msi/LabPracticeFiles/BB_KernelC/linux/ M=`pwd` clean clean

deploy: adxl_i2c_driver.ko
	scp adxl_i2c_driver.ko debian@192.168.7.2:~

.PHONY: deploy clean

