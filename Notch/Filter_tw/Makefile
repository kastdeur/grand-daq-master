# Makefile      27-Feb-2012.

CC              = gcc
#CFLAGS          = -Wall -m32
CFLAGS          = -Wall
LINKER          = $(CC)
#CFLAGS          = -D_FILE_OFFSET_BITS=64 -m32

# Dependencies  ------------------------------------------

PRG1            = filter_settings
SRC1            = filter_settings.c
PRG2            = iirtest
SRC2            = iirtest.c

HDRS            = filter.h
SRCS            = filter.c
LIBS            = -lm
OBJS            =

# Make          ------------------------------------------

all:	        $(PRG1) $(PRG2)

$(PRG1):	$(HDRS) $(SRCS) $(SRC1)
	$(CC) $(CFLAGS) $(LIBS) $(SRCS) $(SRC1) -o $(PRG1)

$(PRG2):	$(HDRS) $(SRCS) $(SRC2)
	$(CC) $(CFLAGS) $(LIBS) $(SRCS) $(SRC2) -o $(PRG2)

clean:
	rm -f   $(PRG1) $(PRG2)

#
