APP = dudaq

# Add any other object files to this list below
APP_OBJS = dudaq.o buffer.o scope.o ad_shm.o

all: build

build: $(APP)

clean:
	rm -f *.o
	rm -f dudaq

$(APP): $(APP_OBJS)
	$(CC) $(LDFLAGS) -DSCOPE_V4  -o $@ $(APP_OBJS) $(LDLIBS)

