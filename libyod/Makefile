#target you can change test to what you want
TARGET := libyod.so

#compile and lib parameter
#CC      := mipsel-openwrt-linux-uclibc-gcc
CC      := gcc
LIBS    := -L. -ldl -lm -lmysqlclient -lpthread
LDFLAGS := 
DEFINES := -DHAVE_CONFIG_H
INCLUDE := -I. -I/usr/include/mysql5
CFLAGS  := -g -Wall -Os $(CFLAGS) $(INCLUDE)
SHARE    := -fPIC -shared

#i think you should do anything here

#source file
SOURCE := $(wildcard *.c)
OBJS   := $(patsubst %.c,%.o,$(SOURCE))

.PHONY  : all objs clean remove rebuild inst

all     : $(TARGET)

objs    : $(OBJS)

rebuild : remove all

test    :

clean   :
	rm -rfv *.o */*.o

install :
	

remove  : clean
	rm -rfv $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(DEFINES) $(SHARE) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)
