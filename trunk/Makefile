# valid platforms:
# PLATFORM = os4
# PLATFORM = MorphOS
# PLATFORM = win32
# PLATFORM = beos
# PLATFORM = haiku
# PLATFORM = osx
# PLATFORM = linux
# PLATFORM = gphwiz
# PLATFORM = aitouchbook

VERSION_MAJ = 0
VERSION_MIN = 6
VERSION_REV = 0
VERSION_FULL = $(VERSION_MAJ).$(VERSION_MIN).$(VERSION_REV)
APP_NAME = Oricutron
VERSION_COPYRIGHTS = "$(APP_NAME) $(VERSION_FULL) (c)2010 Peter Gordon (pete@petergordon.org.uk)"
#COPYRIGHTS = "$(APP_NAME) $(VERSION_FULL) ©2010 Peter Gordon (pete@petergordon.org.uk)"

####### DEFAULT SETTINGS HERE #######

CFLAGS = -Wall -O3
CFLAGS += -DVERSION_FULL='"$(VERSION_MAJ).$(VERSION_MIN)"' -DVERSION_COPYRIGHTS='$(VERSION_COPYRIGHTS)'
LFLAGS = 

CC = gcc
CXX = g++
AR = ar
RANLIB = ranlib
DEBUGLIB =
TARGET = oricutron
FILEREQ_SRC = filereq_sdl.c
MSGBOX_SRC = msgbox_sdl.c
EXTRAOBJS =

####### PLATFORM DETECTION HERE #######

ifeq ($(OSTYPE),beos)
PLATFORM ?= beos
else
UNAME_S = $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
PLATFORM ?= osx
endif
ifeq ($(UNAME_S),Haiku)
PLATFORM ?= haiku
endif
ifeq ($(UNAME_S),Linux)
PLATFORM ?= linux
endif
endif
# default
PLATFORM ?= os4
$(info Target platform: $(PLATFORM))

####### PLATFORM SPECIFIC STUFF HERE #######

# Amiga OS4
ifeq ($(PLATFORM),os4)
CFLAGS += -mcrt=newlib -gstabs -I/SDK/Local/common/include/ -I/SDK/Local/common/include/SDL/ -I/SDK/Local/newlib/include/ -I/SDK/Local/newlib/include/SDL/
LFLAGS += -lm `sdl-config --libs` -lm -mcrt=newlib -gstabs
BDBLFLAGS += -lunix
FILEREQ_SRC = filereq_amiga.c
MSGBOX_SRC = msgbox_os4.c
endif

# MorphOS
ifeq ($(PLATFORM),MorphOS)
CFLAGS += `sdl-config --cflags`
LFLAGS += `sdl-config --libs` -s
FILEREQ_SRC = filereq_amiga.c
endif

# Windows 32bit
ifeq ($(PLATFORM),win32)
CFLAGS += -Dmain=SDL_main -D__SPECIFY_SDL_DIR__
LFLAGS += -lm -mwindows -lmingw32 -lSDLmain -lSDL
TARGET = oricutron.exe
FILEREQ_SRC = filereq_win32.c
MSGBOX_SRC = msgbox_win32.c
EXTRAOBJS = winicon.o
endif

# BeOS / Haiku
ifeq ($(PLATFORM),beos)
PLATFORMTYPE = beos
endif

ifeq ($(PLATFORM),haiku)
PLATFORMTYPE = beos
endif

ifeq ($(PLATFORMTYPE),beos)
CFLAGS += $(shell sdl-config --cflags)
LFLAGS += $(shell sdl-config --libs)
CFLAGS += -Wno-multichar
CFLAGS += -g
LFLAGS += -lbe -ltracker
TARGET = oricutron
FILEREQ_SRC = filereq_beos.cpp
MSGBOX_SRC = msgbox_beos.cpp
BEOS_BERES := beres
BEOS_RC := rc
BEOS_XRES := xres
BEOS_SETVER := setversion
BEOS_MIMESET := mimeset
RSRC_BEOS := oricutron.rsrc
RESOURCES := $(RSRC_BEOS)
endif

# Mac OS X
ifeq ($(PLATFORM),osx)
CFLAGS += $(shell sdl-config --cflags)
LFLAGS += -lm $(shell sdl-config --libs)
TARGET = oricutron
FILEREQ_SRC = filereq_osx.m
MSGBOX_SRC = msgbox_osx.m
endif

# Linux
ifeq ($(PLATFORM),linux)
CFLAGS += $(shell sdl-config --cflags)
LFLAGS += -lm $(shell sdl-config --libs)
TARGET = oricutron
endif

# Linux-gph-wiz
ifeq ($(PLATFORM),gphwiz)
WIZ_HOME = /opt/openwiz/toolchain/arm-openwiz-linux-gnu
WIZ_PREFIX = $(WIZ_HOME)/bin/arm-openwiz-linux-gnu
CC = $(WIZ_PREFIX)-gcc
CXX = $(WIZ_PREFIX)-g++
AR =  $(WIZ_PREFIX)-ar
RANLIB = $(WIZ_PREFIX)-ranlib
CFLAGS += `$(WIZ_HOME)/bin/sdl-config --cflags`
LFLAGS += -lm `$(WIZ_HOME)/bin/sdl-config --libs`
TARGET = oricutron.gpe
endif

# AI touchbook OS
ifeq ($(PLATFORM),aitouchbook)
AITB_HOME = /usr/bin/
AITB_PREFIX = $(AITB_HOME)arm-angstrom-linux-gnueabi
CC = $(AITB_PREFIX)-gcc
CXX = $(AITB_PREFIX)-g++
AR =  $(AITB_PREFIX)-ar
CFLAGS += `$(AITB_HOME)/sdl-config --cflags`
LFLAGS += -lm `$(AITB_HOME)/sdl-config --libs`
RANLIB = $(AITB_PREFIX)-ranlib
TARGET = oricutron_AITB
endif

 
####### SHOULDN'T HAVE TO CHANGE THIS STUFF #######

all: $(TARGET)

run: $(TARGET)
	$(TARGET)

$(TARGET): main.o 6502.o machine.o gui.o font.o monitor.o via.o 8912.o disk.o filereq.o msgbox.o avi.o $(EXTRAOBJS) $(RESOURCES)
	$(CXX) -o $(TARGET) main.o 6502.o machine.o gui.o font.o monitor.o via.o 8912.o disk.o filereq.o msgbox.o avi.o $(EXTRAOBJS) $(LFLAGS)
ifeq ($(PLATFORMTYPE),beos)
	$(BEOS_XRES) -o $(TARGET) $(RSRC_BEOS)
	$(BEOS_SETVER) $(TARGET) \
                -app $(VERSION_MAJ) $(VERSION_MIN) 0 d 0 \
                -short "$(APP_NAME) $(VERSION_FULL)" \
                -long $(VERSION_COPYRIGHTS)
	$(BEOS_MIMESET) $(TARGET)
endif


main.o: main.c system.h 6502.h via.h 8912.h gui.h disk.h machine.h monitor.h avi.h msgbox.h filereq.h
	$(CC) -c main.c -o main.o $(CFLAGS)

6502.o: 6502.c system.h 6502.h
	$(CC) -c 6502.c -o 6502.o $(CFLAGS)

machine.o: machine.c system.h 6502.h via.h 8912.h gui.h disk.h machine.h monitor.h avi.h
	$(CC) -c machine.c -o machine.o $(CFLAGS)

gui.o: gui.c system.h 6502.h via.h 8912.h gui.h disk.h machine.h monitor.h filereq.h
	$(CC) -c gui.c -o gui.o $(CFLAGS)

monitor.o: monitor.c system.h 6502.h via.h 8912.h gui.h disk.h machine.h monitor.h
	$(CC) -c monitor.c -o monitor.o $(CFLAGS)

via.o: via.c system.h 6502.h via.h 8912.h gui.h disk.h machine.h
	$(CC) -c via.c -o via.o $(CFLAGS)

disk.o: disk.c system.h 6502.h via.h 8912.h gui.h disk.h machine.h monitor.h msgbox.h
	$(CC) -c disk.c -o disk.o $(CFLAGS)

8912.o: 8912.c system.h 6502.h 8912.h via.h gui.h disk.h machine.h avi.h
	$(CC) -c 8912.c -o 8912.o $(CFLAGS)

font.o: font.c
	$(CC) -c font.c -o font.o $(CFLAGS)

filereq.o: $(FILEREQ_SRC) filereq.h system.h 6502.h via.h 8912.h gui.h disk.h machine.h
	$(CC) -c $(FILEREQ_SRC) -o filereq.o $(CFLAGS)

msgbox.o: $(MSGBOX_SRC) msgbox.h system.h 6502.h via.h 8912.h gui.h disk.h machine.h
	$(CC) -c $(MSGBOX_SRC) -o msgbox.o $(CFLAGS)

avi.o: avi.c system.h avi.h
	$(CC) -c avi.c -o avi.o $(CFLAGS)

winicon.o: winicon.ico oricutron.rc
	windres -i oricutron.rc -o winicon.o

$(RSRC_BEOS): oricutron.rdef
	$(BEOS_RC) -o $@ $<

clean:
	rm -f $(TARGET) *.bak *.o $(RESOURCES)


# torpor: added to test commit status
