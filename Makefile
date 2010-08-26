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
CFLAGS += -DAPP_NAME_FULL='"$(APP_NAME) WIP"'
#CFLAGS += -DAPP_NAME_FULL='"$(APP_NAME) $(VERSION_MAJ).$(VERSION_MIN)"'
CFLAGS += -DVERSION_COPYRIGHTS='$(VERSION_COPYRIGHTS)'
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
PKGDIR = Oricutron_$(PLATFORM)_v$(VERSION_MAJ)$(VERSION_MIN)
DOCFILES = ReadMe.txt oricutron.cfg ChangeLog.txt

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
  ifeq ($(UNAME_S),MorphOS)
  PLATFORM ?= MorphOS
  endif
endif
# default
PLATFORM ?= os4
#$(info Target platform: $(PLATFORM))

####### PLATFORM SPECIFIC STUFF HERE #######

# Amiga OS4
ifeq ($(PLATFORM),os4)
CFLAGS += -mcrt=newlib -gstabs -I/SDK/Local/common/include/ -I/SDK/Local/common/include/SDL/ -I/SDK/Local/newlib/include/ -I/SDK/Local/newlib/include/SDL/ -D__OPENGL_AVAILABLE__
LFLAGS += -lm `sdl-config --libs` -lGL -mcrt=newlib -gstabs
FILEREQ_SRC = filereq_amiga.c
MSGBOX_SRC = msgbox_os4.c
endif

# MorphOS
ifeq ($(PLATFORM),MorphOS)
CFLAGS += `sdl-config --cflags` -D__OPENGL_AVAILABLE__
LFLAGS += `sdl-config --libs` -s
FILEREQ_SRC = filereq_amiga.c
endif

# Windows 32bit
ifeq ($(PLATFORM),win32)
CFLAGS += -Dmain=SDL_main -D__SPECIFY_SDL_DIR__ -D__OPENGL_AVAILABLE__
LFLAGS += -lm -mwindows -lmingw32 -lSDLmain -lSDL -lopengl32
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
CFLAGS += -D__OPENGL_AVAILABLE__ $(shell sdl-config --cflags)
LFLAGS += $(shell sdl-config --libs)
CFLAGS += -Wno-multichar
CFLAGS += -g
LFLAGS += -lbe -ltracker -lGL
TARGET = oricutron
INSTALLDIR = /boot/apps/Oricutron
FILEREQ_SRC = filereq_beos.cpp
MSGBOX_SRC = msgbox_beos.cpp
CLIPBOARD_SRC = clipboard_beos.cpp
EXTRAOBJS = clipboard.o
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
CFLAGS += -D__OPENGL_AVAILABLE__ $(shell sdl-config --cflags)
LFLAGS += $(shell sdl-config --libs)
LFLAGS += -lm -Wl,-framework,OpenGL
TARGET = oricutron
FILEREQ_SRC = filereq_osx.m
MSGBOX_SRC = msgbox_osx.m
endif

# Linux
ifeq ($(PLATFORM),linux)
CFLAGS += $(shell sdl-config --cflags)
LFLAGS += -lm $(shell sdl-config --libs)
TARGET = oricutron
INSTALLDIR = /usr/local
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

install: install-$(PLATFORM)

package: package-$(PLATFORM)

$(TARGET): main.o 6502.o machine.o ula.o gui.o font.o monitor.o via.o 8912.o 6551.o disk.o filereq.o msgbox.o avi.o render_sw.o render_gl.o render_null.o joystick.o $(EXTRAOBJS) $(RESOURCES)
	$(CXX) -o $(TARGET) main.o 6502.o machine.o ula.o gui.o font.o monitor.o via.o 8912.o 6551.o disk.o filereq.o msgbox.o avi.o render_sw.o render_gl.o render_null.o joystick.o $(EXTRAOBJS) $(LFLAGS)
ifeq ($(PLATFORMTYPE),beos)
	$(BEOS_XRES) -o $(TARGET) $(RSRC_BEOS)
	$(BEOS_SETVER) $(TARGET) \
                -app $(VERSION_MAJ) $(VERSION_MIN) 0 d 0 \
                -short "$(APP_NAME) $(VERSION_FULL)" \
                -long $(VERSION_COPYRIGHTS)
	$(BEOS_MIMESET) $(TARGET)
endif


main.o: main.c system.h 6502.h via.h 8912.h gui.h disk.h 6551.h machine.h monitor.h avi.h msgbox.h filereq.h main.h ula.h joystick.h
	$(CC) -c main.c -o main.o $(CFLAGS)

6502.o: 6502.c system.h 6502.h
	$(CC) -c 6502.c -o 6502.o $(CFLAGS)

machine.o: machine.c system.h 6502.h via.h 8912.h gui.h disk.h 6551.h machine.h monitor.h avi.h main.h joystick.h
	$(CC) -c machine.c -o machine.o $(CFLAGS)

ula.o: ula.c ula.h system.h 6502.h via.h 8912.h disk.h monitor.h 6551.h machine.h avi.h
	$(CC) -c ula.c -o ula.o $(CFLAGS)

render_sw.o: render_sw.c render_sw.h system.h 6502.h via.h 8912.h disk.h monitor.h 6551.h machine.h
	$(CC) -c render_sw.c -o render_sw.o $(CFLAGS)

render_gl.o: render_gl.c render_gl.h system.h 6502.h via.h 8912.h disk.h monitor.h 6551.h machine.h
	$(CC) -c render_gl.c -o render_gl.o $(CFLAGS)

render_null.o: render_null.c render_null.h system.h 6502.h via.h 8912.h disk.h monitor.h 6551.h machine.h
	$(CC) -c render_null.c -o render_null.o $(CFLAGS)

gui.o: gui.c system.h 6502.h via.h 8912.h gui.h disk.h 6551.h machine.h monitor.h filereq.h
	$(CC) -c gui.c -o gui.o $(CFLAGS)

monitor.o: monitor.c system.h 6502.h via.h 8912.h gui.h disk.h 6551.h machine.h monitor.h ula.h
	$(CC) -c monitor.c -o monitor.o $(CFLAGS)

via.o: via.c system.h 6502.h via.h 8912.h gui.h disk.h 6551.h machine.h
	$(CC) -c via.c -o via.o $(CFLAGS)

6551.o: 6551.c system.h 6502.h via.h 8912.h gui.h disk.h 6551.h machine.h
	$(CC) -c 6551.c -o 6551.o $(CFLAGS)

disk.o: disk.c system.h 6502.h via.h 8912.h gui.h disk.h 6551.h machine.h monitor.h msgbox.h
	$(CC) -c disk.c -o disk.o $(CFLAGS)

8912.o: 8912.c system.h 6502.h 8912.h via.h gui.h disk.h 6551.h machine.h avi.h
	$(CC) -c 8912.c -o 8912.o $(CFLAGS)

font.o: font.c
	$(CC) -c font.c -o font.o $(CFLAGS)

filereq.o: $(FILEREQ_SRC) filereq.h system.h 6502.h via.h 8912.h gui.h disk.h 6551.h machine.h
	$(CC) -c $(FILEREQ_SRC) -o filereq.o $(CFLAGS)

msgbox.o: $(MSGBOX_SRC) msgbox.h system.h 6502.h via.h 8912.h gui.h disk.h 6551.h machine.h
	$(CC) -c $(MSGBOX_SRC) -o msgbox.o $(CFLAGS)

avi.o: avi.c system.h avi.h
	$(CC) -c avi.c -o avi.o $(CFLAGS)

joystick.o: joystick.c system.h 6502.h via.h 8912.h gui.h disk.h monitor.h 6551.h machine.h joystick.h
	$(CC) -c joystick.c -o joystick.o $(CFLAGS)

clipboard.o: $(CLIPBOARD_SRC)  system.h 6502.h via.h 8912.h gui.h disk.h monitor.h 6551.h machine.h
	$(CC) -c $(CLIPBOARD_SRC) -o clipboard.o $(CFLAGS)

winicon.o: winicon.ico oricutron.rc
	windres -i oricutron.rc -o winicon.o

Oricutron.guide: ReadMe.txt
	rx ReadMe2Guide <ReadMe.txt >$@
	-GuideCheck $@ NoNodes

$(RSRC_BEOS): oricutron.rdef
	$(BEOS_RC) -o $@ $<

clean:
	rm -f $(TARGET) *.bak *.o $(RESOURCES) printer_out.txt
	rm -rf "$(PKGDIR)"


install-beos install-haiku:
	mkdir -p $(INSTALLDIR)
	copyattr -d $(TARGET) $(INSTALLDIR)
# TODO: use resources
	mkdir -p $(INSTALLDIR)/images
	copyattr -d images/* $(INSTALLDIR)/images

install-linux:
	install -m 755 $(TARGET) $(INSTALLDIR)/bin


package-MorphOS: Oricutron.guide
	lha -r u RAM:$(PKGDIR) // $(TARGET) $(TARGET).info

package-beos package-haiku:
	mkdir -p $(PKGDIR)/images
	copyattr -d $(TARGET) $(PKGDIR)
	copyattr -d images/* $(PKGDIR)/images
	install -m 644 $(DOCFILES) $(PKGDIR)
	zip -ry9 $(PKGDIR).zip $(PKGDIR)/

package-osx:
	mkdir -p $(PKGDIR)/Oriculator.app/Contents/MacOS
	mkdir -p $(PKGDIR)/Oriculator.app/Contents/Resources/images
	install -m 755 $(TARGET) $(PKGDIR)/Oriculator.app/Contents/MacOS/Oricutron
	echo 'APPL????' > $(PKGDIR)/Oriculator.app/Contents/PkgInfo
	sed "s/@@VERSION@@/$(VERSION_MAJ).$(VERSION_MIN)/g" Info.plist > $(PKGDIR)/Oriculator.app/Contents/Info.plist
	install -m 644 images/* $(PKGDIR)/Oriculator.app/Contents/Resources/images
	# XXX: SDL now opens files in bundles first, but not old versions
	ln -s Oriculator.app/Contents/Resources/images $(PKGDIR)/images
	#install -m 644 roms/* $(PKGDIR)/Oriculator.app/Contents/Resources/roms
	ln -s Oriculator.app/Contents/Resources/roms $(PKGDIR)/roms
	install -m 644 $(DOCFILES) $(PKGDIR)
	zip -ry9 $(PKGDIR).zip $(PKGDIR)/

# torpor: added to test commit status
