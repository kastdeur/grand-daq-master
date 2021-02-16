CC=gcc
CFLAGS=-Wall

all: filter_settings iirtest

filter_settings: filter_settings.c filter.c filter.h
	$(CC) $(CFLAGS) -o filter_settings filter_settings.c filter.c

iirtest: iirtest.c filter.c filter.h
	$(CC) $(CFLAGS) -o iirtest iirtest.c filter.c

clean:
	rm -f iirtest filter_settings
