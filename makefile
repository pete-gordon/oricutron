PLATFORM ?= os4
# PLATFORM = win32
# PLATFORM = beos
# PLATFORM = haiku
# PLATFORM = osx

####### DEFAULT SETTINGS HERE #######

CFLAGS = -Wall -O3
LFLAGS = 

CC = gcc
CXX = g++
AR = ar
RANLIB = ranlib
DEBUGLIB =
TARGET = oriculator

####### PLATFORM SPECIFIC STUFF HERE #######

# Amiga OS4
ifeq ($(PLATFORM),os4)
CFLAGS += -mcrt=newlib -gstabs -I/SDK/Local/common/include/ -I/SDK/Local/common/include/SDL/ -I/SDK/Local/newlib/include/ -I/SDK/Local/newlib/include/SDL/
LFLAGS += -lm `sdl-config --libs` -lm -mcrt=newlib -gstabs
BDBLFLAGS += -lunix
endif

# Windows 32bit
ifeq ($(PLATFORM),win32)
CFLAGS += -Dmain=SDL_main -D__SPECIFY_SDL_DIR__
LFLAGS += -lm -mwindows -lmingw32 -lSDLmain -lSDL
TARGET = oriculator.exe
endif

ifeq ($(PLATFORM),beos)
LFLAGS += -lSDL
TARGET = oriculator
endif

ifeq ($(PLATFORM),haiku)
LFLAGS += -lSDL
TARGET = oriculator
endif

ifeq ($(PLATFORM),osx)
CFLAGS += $(shell sdl-config --cflags)
LFLAGS += -lm $(shell sdl-config --libs)
TARGET = oriculator
endif

####### SHOULDN'T HAVE TO CHANGE THIS STUFF #######

all: $(TARGET)

run: $(TARGET)
	$(TARGET)

$(TARGET): main.o 6502.o machine.o gui.o font.o monitor.o via.o 8912.o disk.o
	$(CXX) -o $(TARGET) main.o 6502.o machine.o gui.o font.o monitor.o via.o 8912.o disk.o $(LFLAGS)

main.o: main.c 6502.h machine.h via.h 8912.h
	$(CC) -c main.c -o main.o $(CFLAGS)

6502.o: 6502.c 6502.h
	$(CC) -c 6502.c -o 6502.o $(CFLAGS)

machine.o: machine.c 6502.h via.h 8912.h disk.h monitor.h
	$(CC) -c machine.c -o machine.o $(CFLAGS)

gui.o: gui.c gui.h machine.h 6502.h via.h monitor.h 8912.h
	$(CC) -c gui.c -o gui.o $(CFLAGS)

monitor.o: monitor.c monitor.h 6502.h gui.h machine.h via.h 8912.h
	$(CC) -c monitor.c -o monitor.o $(CFLAGS)

via.o: via.c via.h 6502.h machine.h 8912.h
	$(CC) -c via.c -o via.o $(CFLAGS)

disk.o: disk.c 6502.h via.h 8912.h gui.h disk.h machine.h
	$(CC) -c disk.c -o disk.o $(CFLAGS)

8912.o: 8912.c 8912.h via.h machine.h
	$(CC) -c 8912.c -o 8912.o $(CFLAGS)

font.o: font.c
	$(CC) -c font.c -o font.o $(CFLAGS)

clean:
	rm -f $(TARGET) *.bak *.o


# torpor: added to test commit status
