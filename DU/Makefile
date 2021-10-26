# Makefile

DAQINC  = ../ainc
FTDLIB = /home/daq/libftd2xx0.4.16/static_lib
FTDINC = /home/daq/libftd2xx0.4.16

all: DUFake

clean:
	rm DUFake
	rm DUV1

test:   test.c scope.c scope.h
	arm-linux-gcc test.c scope.c \
	--sysroot $(ARM_HOME) \
	-lm -ldl -lrt -lpthread -o test

testcal: test.c scope.c scope.h
	arm-linux-gcc -DCALFIRST -DCALFAST test.c scope.c \
	--sysroot $(ARM_HOME) \
        -lm -ldl -lrt -lpthread -o testcal


DUV1:   Makefile DU.c DU.h buffer.c scope.c scope.h ../Adaq/ad_shm.h ../Adaq/ad_shm.c rs232.c 
	arm-linux-gcc LS.c buffer.c scope.c ../Adaq/ad_shm.c -DSCOPE_V4 \
	--sysroot $(ARM_HOME) \
	-I$(DAQINC)  -I../Adaq -O \
	-lm -ldl -lrt -lathered -o DUV1

DUV1T:  Makefile DU.c DU.h buffer.c scope.c scope.h ../Adaq/ad_shm.h ../Adaq/ad_shm.c 
	arm-linux-gcc DU.c buffer.c scope.c  ../Adaq/ad_shm.c -DSCOPE_V4 -DTRIG_INT \
	--sysroot $(ARM_HOME) \
	-I$(DAQINC)  -I../Adaq -O \
	-lm -ldl -lrt -lpthread -o DUV1T

DUFake:  Makefile DU.c DU.h buffer.c scope.c scope.h ../Adaq/ad_shm.h ../Adaq/ad_shm.c 
	gcc DU.c buffer.c scope.c  ../Adaq/ad_shm.c -DSCOPE_V4 -DTRIG_INT -DFake \
	-I$(DAQINC)  -I../Adaq -O \
	-lm -ldl -lpthread -o DUFake

