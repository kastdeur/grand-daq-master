# Makefile

DAQINC  = ../ainc
FTDLIB = /home/daq/libftd2xx0.4.16/static_lib
FTDINC = /home/daq/libftd2xx0.4.16

all: DUFake DUV1

clean:
	rm DUFake
	rm DUV1


DUV1:   Makefile dudaq.c dudaq.h buffer.c scope.c scope.h ../Adaq/ad_shm.h ../Adaq/ad_shm.c rs232.c 
	gcc dudaq.c buffer.c scope.c  ../Adaq/ad_shm.c -DSCOPE_V4 -DTRIG_INT \
	-I$(DAQINC)  -I../Adaq -O \
	-lm -ldl -lpthread -o DUV1


DUFake:  Makefile dudaq.c dudaq.h buffer.c scope.c scope.h ../Adaq/ad_shm.h ../Adaq/ad_shm.c 
	gcc dudaq.c buffer.c scope.c  ../Adaq/ad_shm.c -DSCOPE_V4 -DTRIG_INT -DFake \
	-I$(DAQINC)  -I../Adaq -O \
	-lm -ldl -lpthread -o DUFake

