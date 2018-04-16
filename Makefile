CC = g++

CURENT_PATH = $(shell pwd)

TARGET := test

INC = -I$(CURENT_PATH)/include
INC += -I/usr/include/gstreamer-1.0  
INC += -I/usr/include/glib-2.0
INC += -I/usr/lib/arm-linux-gnueabihf/glib-2.0/include
CFLAGS  = $(INC) -g
LDFLAGS = -lpthread -lm -ldl -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lgstapp-1.0
LDFLAGS+= `pkg-config --static --libs opencv`

#src
PROJECT_SRC := $(wildcard $(CURENT_PATH)/src/*.c)
OBJ := $(PROJECT_SRC:%.c=%.o)
all : $(TARGET)
	
$(TARGET):$(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

.PHONY: clean
clean:
	@echo "DEL OBJ files"
	@rm -f $(CURENT_PATH)/src/*.o $(TARGET)
	@echo "--------------------------------------------" 

