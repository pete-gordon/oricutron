# valid platforms:
# PLATFORM = os4
# PLATFORM = morphos
# PLATFORM = win32
# PLATFORM = win32-gcc
# PLATFORM = win64-gcc
# PLATFORM = beos
# PLATFORM = haiku
# PLATFORM = osx
# PLATFORM = linux
# PLATFORM = linux-opengl
# PLATFORM = gphwiz
# PLATFORM = aitouchbook
# PLATFORM = aros

VERSION_MAJ = 0
VERSION_MIN = 9
VERSION_REV = 0
VERSION_FULL = $(VERSION_MAJ).$(VERSION_MIN).$(VERSION_REV)
APP_NAME = Oricutron
APP_YEAR = 2013
COPYRIGHTS = (c)$(APP_YEAR) Peter Gordon (pete@petergordon.org.uk)
VERSION_COPYRIGHTS = "$(APP_NAME) $(VERSION_FULL) $(COPYRIGHTS)"
#COPYRIGHTS = "$(APP_NAME) $(VERSION_FULL) ©$(APP_YEAR) Peter Gordon (pete@petergordon.org.uk)"

####### DEFAULT SETTINGS HERE #######

VPATH ?= .

### extract svn revision
SVNREVISON := $(shell svn info $(VPATH)|grep 'Revision: '|sed -e 's:.* ::')

DEFINES =  -DAPP_NAME_FULL='"$(APP_NAME) WIP Rev: $(SVNREVISON)"'
#DEFINES = -DAPP_NAME_FULL='"$(APP_NAME) $(VERSION_MAJ).$(VERSION_MIN)"'
#DEFINES += -DAPP_WVER='$(VERSION_MAJ),$(VERSION_MIN),$(VERSION_REV),0'
#DEFINES += -DAPP_COPYRIGHTS='"$(COPYRIGHTS)"'
DEFINES += -DAPP_YEAR='"$(APP_YEAR)"' -DVERSION_COPYRIGHTS='$(VERSION_COPYRIGHTS)'

CFLAGS = -Wall -O3
CFLAGS += $(DEFINES)
LFLAGS =

#CFLAGS += -DDEBUG_CPU_TRACE=1000
#CFLAGS += -DDEBUG_CPU_TRACE=200000

CC = gcc
CXX = g++
AR = ar
RANLIB = ranlib
DEBUGLIB =
TARGET = oricutron
FILEREQ_OBJ = filereq_sdl.o
MSGBOX_OBJ = msgbox_sdl.o
EXTRAOBJS =
CUSTOMBJS =
PKGDIR = Oricutron_$(PLATFORM)_v$(VERSION_MAJ)$(VERSION_MIN)
DOCFILES = ReadMe.txt oricutron.cfg ChangeLog.txt

####### PLATFORM DETECTION HERE #######

UNAME_S = $(shell uname -s)
ifeq ($(UNAME_S),AROS)
HOSTOS = aros
PLATFORM ?= aros
endif
ifeq ($(UNAME_S),BeOS)
HOSTOS = beos
PLATFORM ?= beos
endif
ifeq ($(UNAME_S),Darwin)
HOSTOS = osx
PLATFORM ?= osx
endif
ifeq ($(UNAME_S),Haiku)
HOSTOS = haiku
PLATFORM ?= haiku
endif
ifeq ($(UNAME_S),Linux)
HOSTOS = linux
PLATFORM ?= linux
endif
ifeq ($(UNAME_S),MorphOS)
HOSTOS = morphos
PLATFORM ?= morphos
endif
ifeq ($(PLATFORM),)
UNAME_O = $(shell uname -o)
ifeq ($(UNAME_O),Msys)
HOSTOS = win32
PLATFORM ?= win32
endif
endif
# default
HOSTOS ?= os4
PLATFORM ?= os4
$(info Target platform: $(PLATFORM))

####### PLATFORM SPECIFIC STUFF HERE #######

# Amiga OS4
ifeq ($(PLATFORM),os4)
CFLAGS += -mcrt=newlib -gstabs -I/SDK/Local/common/include/ -I/SDK/Local/common/include/SDL/ -I/SDK/Local/newlib/include/ -I/SDK/Local/newlib/include/SDL/ -D__OPENGL_AVAILABLE__
LFLAGS += -lm `sdl-config --libs` -lGL -mcrt=newlib -gstabs
FILEREQ_OBJ = filereq_amiga.o
MSGBOX_OBJ = msgbox_os4.o
AMIGA_ICONS = os4icon
endif

# MorphOS
ifeq ($(PLATFORM),morphos)
CFLAGS += `sdl-config --cflags` -D__OPENGL_AVAILABLE__
LFLAGS += `sdl-config --libs` -s
FILEREQ_OBJ = filereq_amiga.o
AMIGA_ICONS = pngicon
endif

# AROS
ifeq ($(PLATFORM),aros)
CFLAGS += `sdl-config --cflags` -D__OPENGL_AVAILABLE__
LFLAGS += `sdl-config --libs` -s
FILEREQ_OBJ = filereq_amiga.o
AMIGA_ICONS = pngicon
endif

# Windows 32bit
ifeq ($(PLATFORM),win32)
ifneq ($(HOSTOS),win32)
# in Debian: apt:mingw32
CROSS_COMPILE ?= i586-mingw32msvc-
endif
CC := $(CROSS_COMPILE)$(CC)
CXX := $(CROSS_COMPILE)$(CXX)
AR :=  $(CROSS_COMPILE)$(AR)
RANLIB :=  $(CROSS_COMPILE)$(RANLIB)
WINDRES := $(CROSS_COMPILE)windres
ifneq ($(SDL_PREFIX),)
CFLAGS += -I$(SDL_PREFIX)/include
LFLAGS += -L$(SDL_PREFIX)/lib
endif
CFLAGS += -Dmain=SDL_main -D__SPECIFY_SDL_DIR__ -D__OPENGL_AVAILABLE__ -g
LFLAGS += -g -lm -mwindows -lmingw32 -lSDLmain -lSDL -lopengl32
ifneq ($(PROFILING),)
CFLAGS += -pg
LFLAGS += -pg
endif
TARGET = oricutron.exe
FILEREQ_OBJ = filereq_win32.o
MSGBOX_OBJ = msgbox_win32.o
CUSTOMOBJS = winicon.o
endif

# Windows 32bit GCC
ifeq ($(PLATFORM),win32-gcc)
ifneq ($(HOSTOS),win32)
# in Debian: apt:mingw32
CROSS_PREFIX ?= i686-w64-mingw32
CROSS_COMPILE ?= $(CROSS_PREFIX)-
endif
CC := $(CROSS_COMPILE)$(CC)
CXX := $(CROSS_COMPILE)$(CXX)
AR :=  $(CROSS_COMPILE)$(AR)
RANLIB :=  $(CROSS_COMPILE)$(RANLIB)
WINDRES := $(CROSS_COMPILE)windres
ifneq ($(SDL_PREFIX),)
CFLAGS += -I$(SDL_PREFIX)/include
LFLAGS += -L$(SDL_PREFIX)/lib
else
CFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(CROSS_PREFIX)/sys-root/mingw/lib/pkgconfig pkg-config sdl --cflags)
LFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(CROSS_PREFIX)/sys-root/mingw/lib/pkgconfig pkg-config sdl --libs)
endif
CFLAGS += -Dmain=SDL_main -D__SPECIFY_SDL_DIR__ -D__OPENGL_AVAILABLE__ -g
LFLAGS += -g -static-libgcc -static-libstdc++ -mwindows -lopengl32
ifneq ($(PROFILING),)
CFLAGS += -pg
LFLAGS += -pg
endif
TARGET = oricutron.exe
FILEREQ_OBJ = filereq_win32.o
MSGBOX_OBJ = msgbox_win32.o
CUSTOMOBJS = winicon.o
endif

# Windows 64bit GCC
ifeq ($(PLATFORM),win64-gcc)
ifneq ($(HOSTOS),win32)
# in Debian: apt:mingw32
CROSS_PREFIX ?= x86_64-w64-mingw32
CROSS_COMPILE ?= $(CROSS_PREFIX)-
endif
CC := $(CROSS_COMPILE)$(CC)
CXX := $(CROSS_COMPILE)$(CXX)
AR :=  $(CROSS_COMPILE)$(AR)
RANLIB :=  $(CROSS_COMPILE)$(RANLIB)
WINDRES := $(CROSS_COMPILE)windres
ifneq ($(SDL_PREFIX),)
CFLAGS += -I$(SDL_PREFIX)/include
LFLAGS += -L$(SDL_PREFIX)/lib
else
CFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(CROSS_PREFIX)/sys-root/mingw/lib/pkgconfig pkg-config sdl --cflags)
LFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(CROSS_PREFIX)/sys-root/mingw/lib/pkgconfig pkg-config sdl --libs)
endif
CFLAGS += -Dmain=SDL_main -D__SPECIFY_SDL_DIR__ -D__OPENGL_AVAILABLE__ -g
LFLAGS += -g -static-libgcc -static-libstdc++ -mwindows -lopengl32
ifneq ($(PROFILING),)
CFLAGS += -pg
LFLAGS += -pg
endif
TARGET = oricutron-64.exe
FILEREQ_OBJ = filereq_win32.o
MSGBOX_OBJ = msgbox_win32.o
CUSTOMOBJS = winicon.o
endif

# BeOS / Haiku
ifeq ($(PLATFORM),beos)
PLATFORMTYPE = beos
endif

ifeq ($(PLATFORM),haiku)
PLATFORMTYPE = beos
endif

ifeq ($(PLATFORMTYPE),beos)
#CFLAGS += -D__OPENGL_AVAILABLE__
CFLAGS += $(shell sdl-config --cflags)
LFLAGS += $(shell sdl-config --libs)
CFLAGS += -Wno-multichar
CFLAGS += -g
LFLAGS += -lbe -ltracker -lGL
TARGET = oricutron
INSTALLDIR = /boot/apps/Oricutron
FILEREQ_OBJ =
MSGBOX_OBJ = 
CUSTOMOBJS = gui_beos.o msgbox_beos.o filereq_beos.o
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
LFLAGS += $(shell sdl-config --libs) -s
LFLAGS += -lm -Wl,-framework,OpenGL
TARGET = oricutron
FILEREQ_OBJ =
MSGBOX_OBJ =
CUSTOMOBJS = gui_osx.o filereq_osx.o msgbox_osx.o
endif

# Linux
ifeq ($(PLATFORM),linux)
CFLAGS += -g $(shell sdl-config --cflags)
LFLAGS += -lm $(shell sdl-config --libs)
TARGET = oricutron
INSTALLDIR = /usr/local
endif

# Linux OpenGL
ifeq ($(PLATFORM),linux-opengl)
ifeq (x86_64,$(shell uname -m))
BASELIBDIR := lib64
CFLAGS += -m64
LFLAGS += -m64
else
BASELIBDIR := lib
CFLAGS += -m32
LFLAGS += -m32
endif
CFLAGS += -g $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config sdl --cflags) -D__OPENGL_AVAILABLE__
LFLAGS += -lm -L/usr/$(BASELIBDIR) $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config sdl --libs) -lGL
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

OBJECTS = \
	main.o \
	6502.o \
	machine.o \
	ula.o \
	gui.o \
	font.o \
	monitor.o \
	via.o \
	tape.o \
	8912.o \
	6551.o \
	disk.o \
	disk_pravetz.o \
	avi.o \
	render_sw.o \
	render_sw8.o \
	render_gl.o \
	render_null.o \
	joystick.o \
	snapshot.o \
	$(FILEREQ_OBJ) \
	$(MSGBOX_OBJ) \
	$(EXTRAOBJS)


all: $(TARGET)

run: $(TARGET)
	$(TARGET)

install: install-$(PLATFORM) $(TARGET)

package: package-$(PLATFORM) $(TARGET)

$(TARGET): $(OBJECTS) $(CUSTOMOBJS) $(RESOURCES)
	$(CXX) -o $(TARGET) $(OBJECTS) $(CUSTOMOBJS) $(LFLAGS)
ifeq ($(PLATFORMTYPE),beos)
	$(BEOS_XRES) -o $(TARGET) $(RSRC_BEOS)
	$(BEOS_SETVER) $(TARGET) \
                -app $(VERSION_MAJ) $(VERSION_MIN) 0 d 0 \
                -short "$(APP_NAME) $(VERSION_FULL)" \
                -long $(VERSION_COPYRIGHTS)
	$(BEOS_MIMESET) $(TARGET)
endif


-include $(OBJECTS:.o=.d)

# Rules based build for standard *.c to *.o compilation
$(OBJECTS): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@
	@$(CC) -MM $(CFLAGS) $< > $*.d

# Overide the default C rule for C++
%.o: %.cpp
	$(CXX) -c $(CFLAGS) $< -o $@
	@$(CXX) -MM $(CFLAGS) $< > $*.d

# Overide the default C rule for ObjC
%.o: %.m
	$(CC) -c $(CFLAGS) $< -o $@
	@$(CC) -MM $(CFLAGS) $< > $*.d

winicon.o: $(VPATH)/winicon.ico $(VPATH)/oricutron.rc
	$(WINDRES) $(DEFINES) -i $(VPATH)/oricutron.rc -o winicon.o

%.guide: ReadMe.txt
# AROS needs path
	-rx ReadMe2Guide <$? >$(APP_NAME).guide $(APP_NAME) $(VERSION_FULL) || SYS:Rexxc/rx ReadMe2Guide <$? >$(APP_NAME).guide $(APP_NAME) $(VERSION_FULL)
	-GuideCheck $@

$(RSRC_BEOS): oricutron.rdef
	$(BEOS_RC) -o $@ $<

clean:
	rm -f $(TARGET) *.bak *.o *.d $(RESOURCES) printer_out.txt debug_log.txt
	rm -rf "$(PKGDIR)"


install-beos install-haiku:
	mkdir -p $(INSTALLDIR)
	copyattr -d $(TARGET) $(INSTALLDIR)
# TODO: use resources
	mkdir -p $(INSTALLDIR)/images
	copyattr -d images/* $(INSTALLDIR)/images

install-linux:
	install -m 755 $(TARGET) $(INSTALLDIR)/bin

%.info: %_$(AMIGA_ICONS).info
	copy $< $@

package-morphos package-aros package-os4: Oricutron.guide $(patsubst %_$(AMIGA_ICONS).info,%.info,$(wildcard *_$(AMIGA_ICONS).info))
	-@delete ram:Oricutron all >NIL:
	-@delete ram:$(PKGDIR).lha >NIL:
	makedir ram:Oricutron ram:Oricutron/disks ram:Oricutron/tapes ram:Oricutron/teledisks ram:Oricutron/pravdisks ram:Oricutron/roms ram:Oricutron/snapshots ram:Oricutron/images
	copy ENVARC:sys/def_drawer.info ram:Oricutron.info
	copy $(patsubst %_$(AMIGA_ICONS).info,%.info,$(wildcard *_$(AMIGA_ICONS).info)) ram:Oricutron
	copy images/#?.bmp ram:Oricutron/images/
	copy "disks/#?.(dsk|txt)" ram:Oricutron/disks/
	copy "tapes/#?.(tap|ort|txt)" ram:Oricutron/tapes/
	copy "roms/#?.(rom|sym|pch)" ram:Oricutron/roms/
	copy $(DOCFILES) ram:Oricutron
	copy Oricutron.guide ram:Oricutron
	copy $(TARGET) ram:Oricutron
	lha a -r -e ram:$(PKGDIR).lha ram:Oricutron ram:Oricutron.info
	delete $(patsubst %_$(AMIGA_ICONS).info,%.info,$(wildcard *_$(AMIGA_ICONS).info))
	-@delete ram:Oricutron ram:Oricutron.info all >NIL:

package-beos package-haiku:
	mkdir -p $(PKGDIR)/images
	copyattr -d $(TARGET) $(PKGDIR)
	copyattr -d images/* $(PKGDIR)/images
	install -m 644 $(DOCFILES) $(PKGDIR)
	zip -ry9 $(PKGDIR).zip $(PKGDIR)/

package-osx:
	mkdir -p $(PKGDIR)/Oriculator.app/Contents/MacOS
	mkdir -p $(PKGDIR)/Oriculator.app/Contents/Resources/images
	mkdir -p $(PKGDIR)/Oriculator.app/Contents/Resources/roms
	install -m 755 $(TARGET) $(PKGDIR)/Oriculator.app/Contents/MacOS/Oricutron
	echo 'APPL????' > $(PKGDIR)/Oriculator.app/Contents/PkgInfo
	sed "s/@@VERSION@@/$(VERSION_MAJ).$(VERSION_MIN)/g" Info.plist > $(PKGDIR)/Oriculator.app/Contents/Info.plist
	install -m 644 images/* $(PKGDIR)/Oriculator.app/Contents/Resources/images
	# XXX: SDL now opens files in bundles first, but not old versions
	ln -s Oriculator.app/Contents/Resources/images $(PKGDIR)/images
	install -m 644 roms/* $(PKGDIR)/Oriculator.app/Contents/Resources/roms
	ln -s Oriculator.app/Contents/Resources/roms $(PKGDIR)/roms
	install -m 644 $(DOCFILES) $(PKGDIR)
	zip -ry9 $(PKGDIR).zip $(PKGDIR)/

package-win32:
	mkdir -p $(PKGDIR)/images
	mkdir -p $(PKGDIR)/disks
	mkdir -p $(PKGDIR)/teledisks
	mkdir -p $(PKGDIR)/pravdisks
	mkdir -p $(PKGDIR)/tapes
	mkdir -p $(PKGDIR)/roms
	install -m 755 $(TARGET) $(PKGDIR)
	install -m 644 images/* $(PKGDIR)/images
	#install -m 644 disks/* $(PKGDIR)/disks
	#install -m 644 tapes/* $(PKGDIR)/tapes
	install -m 644 roms/* $(PKGDIR)/roms
	install -m 644 $(DOCFILES) $(PKGDIR)
	# unix2dos is not always installed
	sed -i "s/$$/\r/g" $(PKGDIR)/ReadMe.txt
	zip -ry9 $(PKGDIR).zip $(PKGDIR)/

