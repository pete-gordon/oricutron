#
# valid platforms in alphabetical order:
#
# PLATFORM = aitouchbook
# PLATFORM = aros
# PLATFORM = beos
# PLATFORM = gphwiz
# PLATFORM = haiku
# PLATFORM = lin-32
# PLATFORM = lin-64
# PLATFORM = linux
# PLATFORM = linux-nogl
# PLATFORM = mint
# PLATFORM = mint-cf
# PLATFORM = morphos
# PLATFORM = os4
# PLATFORM = osx
# PLATFORM = pandora
# PLATFORM = rpi
# PLATFORM = win32
# PLATFORM = win32-gcc
# PLATFORM = win64-gcc

# important build parameters:
# SDL_LIB - select SDL versions
#     SDL_LIB=sdl  - use v.1.x - default
#     SDL_LIB=sdl2 - use v.2.x



VERSION_MAJ = 1
VERSION_MIN = 2
VERSION_REV = 0
VERSION_FULL = $(VERSION_MAJ).$(VERSION_MIN).$(VERSION_REV)
APP_INFO =
APP_NAME = $(strip Oricutron $(APP_INFO))
APP_YEAR = 2019
COPYRIGHTS = (c)$(APP_YEAR) Peter Gordon (pete@gordon.plus)
VERSION_COPYRIGHTS = "$(APP_NAME) $(VERSION_FULL) $(COPYRIGHTS)"
#COPYRIGHTS = "$(APP_NAME) $(VERSION_FULL) ©$(APP_YEAR) Peter Gordon (pete@gordon.plus)"

####### DEFAULT SETTINGS HERE #######

SRC_DIR = .
VPATH = $(SRC_DIR) $(SRC_DIR)/plugins/ch376 $(SRC_DIR)/plugins/twilighte_board

### extract git/svn revision
GITREVISION = $(shell git rev-parse --short HEAD || svnversion -n $(SRC_DIR))

DEFINES =  -DAPP_NAME_FULL='"$(APP_NAME) WIP Rev: $(GITREVISION)"'
#DEFINES = -DAPP_NAME_FULL='"$(APP_NAME) $(VERSION_MAJ).$(VERSION_MIN)"'
#DEFINES += -DAPP_WVER='$(VERSION_MAJ),$(VERSION_MIN),$(VERSION_REV),0'
#DEFINES += -DAPP_COPYRIGHTS='"$(COPYRIGHTS)"'
DEFINES += -DAPP_YEAR='"$(APP_YEAR)"' -DVERSION_COPYRIGHTS='$(VERSION_COPYRIGHTS)'

ifneq ($(DEBUG),y)
CFLAGS = -Wall -O3
else
CFLAGS = -Wall -g -O0
endif
CFLAGS += $(DEFINES)
LFLAGS =

ifneq ($(NO_GETADDRINFO),)
CFLAGS += -DNO_GETADDRINFO=1
endif

#CFLAGS += -DDEBUG_CPU_TRACE=1000
#CFLAGS += -DDEBUG_CPU_TRACE=200000

ifneq ($(DEBUG_VSYNC),)
CFLAGS += -DDEBUG_VSYNC
endif

CC = gcc
CXX = g++
AR = ar
RANLIB = ranlib
STRIP = strip
DEBUGLIB =
TARGET_NAME = oricutron
TARGET = $(TARGET_NAME)
FILEREQ_OBJ = filereq_sdl.o
MSGBOX_OBJ = msgbox_sdl.o
EXTRAOBJS =
CUSTOMOBJS =
PKGDIR = build/Oricutron_$(PLATFORM)_v$(VERSION_MAJ)$(VERSION_MIN)
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
$(info Host OS         : $(HOSTOS))
$(info Target platform : $(PLATFORM))

####### PLATFORM SPECIFIC STUFF HERE #######

### set SDL_LIB to 'sdl' or 'sdl2' for SDL2 (default is sdl)
SDL_LIB ?= sdl
$(info Using SDL lib   : $(SDL_LIB))
$(info Using SDL prefix: $(SDL_PREFIX))

# Amiga OS4
ifeq ($(PLATFORM),os4)
CFLAGS += -mcrt=newlib -gstabs -I/SDK/Local/common/include/ -I/SDK/Local/common/include/SDL/ -I/SDK/Local/newlib/include/ -I/SDK/Local/newlib/include/SDL/ -D__OPENGL_AVAILABLE__ -DNO_GETADDRINFO=1
LFLAGS += -lm `$(SDL_LIB)-config --libs` -lGL -mcrt=newlib -gstabs
FILEREQ_OBJ = filereq_amiga.o
MSGBOX_OBJ = msgbox_os4.o
AMIGA_ICONS = os4icon
endif

# MorphOS
ifeq ($(PLATFORM),morphos)
CFLAGS += `$(SDL_LIB)-config --cflags` -D__OPENGL_AVAILABLE__ -DNO_GETADDRINFO=1
LFLAGS += `$(SDL_LIB)-config --libs` -s
FILEREQ_OBJ = filereq_amiga.o
MSGBOX_OBJ = msgbox_os2.o
AMIGA_ICONS = pngicon
EXTRAOBJS = oric_ch376_plugin.o ch376.o oric_twilighte_board_plugin.o
endif

# AROS
ifeq ($(PLATFORM),aros)
CFLAGS += `$(SDL_LIB)-config --cflags` -D__OPENGL_AVAILABLE__
LFLAGS += `$(SDL_LIB)-config --libs` -s
FILEREQ_OBJ = filereq_amiga.o
MSGBOX_OBJ = msgbox_os2.o
AMIGA_ICONS = pngicon
endif

# Windows 32bit
ifeq ($(PLATFORM),win32)
ifneq ($(HOSTOS),win32)
# in Debian: apt:mingw32
CROSS_COMPILE ?= i586-mingw32msvc-
else
ifeq ($(SDL_LIB),sdl)
CFLAGS += -DSDL_MAJOR_VERSION=1
else
CFLAGS += -DSDL_MAJOR_VERSION=2
endif
endif
CC := $(CROSS_COMPILE)$(CC)
CXX := $(CROSS_COMPILE)$(CXX)
AR :=  $(CROSS_COMPILE)$(AR)
RANLIB :=  $(CROSS_COMPILE)$(RANLIB)
WINDRES := $(CROSS_COMPILE)windres
STRIP :=  $(CROSS_COMPILE)$(STRIP)
ifneq ($(SDL_PREFIX),)
CFLAGS += -I$(SDL_PREFIX)/include
LFLAGS += -L$(SDL_PREFIX)/lib
endif
CFLAGS += -Dmain=SDL_main -D__SPECIFY_SDL_DIR__ -D__OPENGL_AVAILABLE__ -D__CBCOPY__ -D__CBPASTE__ -g
LFLAGS += -g -lm -mwindows -lmingw32 -lSDLmain -lSDL -lopengl32 -lws2_32 -static-libgcc
ifneq ($(PROFILING),)
CFLAGS += -pg
LFLAGS += -pg
endif
TARGET = oricutron.exe
FILEREQ_OBJ = filereq_win32.o
MSGBOX_OBJ = msgbox_win32.o
CUSTOMOBJS = gui_win.o winicon.o
endif

# Windows 32bit GCC
ifeq ($(PLATFORM),win32-gcc)
ifneq ($(HOSTOS),win32)
# in Debian: apt:mingw32
CROSS_PREFIX ?= i686-w64-mingw32
CROSS_COMPILE ?= $(CROSS_PREFIX)-
ifeq ($(SDL_LIB),sdl)
CFLAGS += -DSDL_MAJOR_VERSION=1
else
CFLAGS += -DSDL_MAJOR_VERSION=2
endif
endif
CC := $(CROSS_COMPILE)$(CC)
CXX := $(CROSS_COMPILE)$(CXX)
AR :=  $(CROSS_COMPILE)$(AR)
RANLIB :=  $(CROSS_COMPILE)$(RANLIB)
WINDRES := $(CROSS_COMPILE)windres
STRIP :=  $(CROSS_COMPILE)$(STRIP)
ifneq ($(SDL_PREFIX),)
ifneq (,$(SDL_CFLAGS))
CFLAGS += $(SDL_CFLAGS)
else
CFLAGS += -I$(SDL_PREFIX)/include
endif
ifneq (,$(SDL_LFLAGS))
LFLAGS += $(SDL_LFLAGS)
else
LFLAGS += -L$(SDL_PREFIX)/lib
endif
else
CFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(CROSS_PREFIX)/sys-root/mingw/lib/pkgconfig pkg-config $(SDL_LIB) --cflags)
LFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(CROSS_PREFIX)/sys-root/mingw/lib/pkgconfig pkg-config $(SDL_LIB) --libs)
endif
CFLAGS += -Dmain=SDL_main -D__SPECIFY_SDL_DIR__ -D__OPENGL_AVAILABLE__ -D__CBCOPY__ -D__CBPASTE__ -g
LFLAGS += -g -static-libgcc -static-libstdc++ -mwindows -lopengl32 -lws2_32
ifneq ($(PROFILING),)
CFLAGS += -pg
LFLAGS += -pg
endif
TARGET = $(TARGET_NAME).exe
TARGET_DEPS = /usr/$(CROSS_PREFIX)/sys-root/mingw/bin/SDL.dll
FILEREQ_OBJ = filereq_win32.o
MSGBOX_OBJ = msgbox_win32.o
CUSTOMOBJS = gui_win.o winicon.o
EXTRAOBJS = oric_ch376_plugin.o ch376.o oric_twilighte_board_plugin.o
endif

# Windows 64bit GCC
ifeq ($(PLATFORM),win64-gcc)
ifneq ($(HOSTOS),win32)
# in Debian: apt:mingw32
CROSS_PREFIX ?= x86_64-w64-mingw32
CROSS_COMPILE ?= $(CROSS_PREFIX)-
ifeq ($(SDL_LIB),sdl)
CFLAGS += -DSDL_MAJOR_VERSION=1
else
CFLAGS += -DSDL_MAJOR_VERSION=2
endif
endif
CC := $(CROSS_COMPILE)$(CC)
CXX := $(CROSS_COMPILE)$(CXX)
AR :=  $(CROSS_COMPILE)$(AR)
RANLIB :=  $(CROSS_COMPILE)$(RANLIB)
WINDRES := $(CROSS_COMPILE)windres
STRIP :=  $(CROSS_COMPILE)$(STRIP)
ifneq ($(SDL_PREFIX),)
ifneq (,$(SDL_CFLAGS))
CFLAGS += $(SDL_CFLAGS)
else
CFLAGS += -I$(SDL_PREFIX)/include
endif
ifneq (,$(SDL_LFLAGS))
LFLAGS += $(SDL_LFLAGS)
else
LFLAGS += -L$(SDL_PREFIX)/lib
endif
else
CFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(CROSS_PREFIX)/sys-root/mingw/lib/pkgconfig pkg-config $(SDL_LIB) --cflags)
LFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(CROSS_PREFIX)/sys-root/mingw/lib/pkgconfig pkg-config $(SDL_LIB) --libs)
endif
CFLAGS += -Dmain=SDL_main -D__SPECIFY_SDL_DIR__ -D__OPENGL_AVAILABLE__ -D__CBCOPY__ -D__CBPASTE__ -g
LFLAGS += -g -static-libgcc -static-libstdc++ -mwindows -lopengl32 -lws2_32
ifneq ($(PROFILING),)
CFLAGS += -pg
LFLAGS += -pg
endif
TARGET = $(TARGET_NAME).exe
TARGET_DEPS = /usr/$(CROSS_PREFIX)/sys-root/mingw/bin/SDL.dll
FILEREQ_OBJ = filereq_win32.o
MSGBOX_OBJ = msgbox_win32.o
CUSTOMOBJS = gui_win.o winicon.o
EXTRAOBJS = oric_ch376_plugin.o ch376.o oric_twilighte_board_plugin.o
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
CFLAGS += $(shell $(SDL_LIB)-config --cflags)
LFLAGS += $(shell $(SDL_LIB)-config --libs)
CFLAGS += -Wno-multichar
CFLAGS += -g -D__CBCOPY__ -D__CBPASTE__
LFLAGS += -lbe -ltracker -lGL
TARGET = oricutron
INSTALLDIR = /boot/apps/Oricutron
FILEREQ_OBJ =
MSGBOX_OBJ =
CUSTOMOBJS = gui_beos.o msgbox_beos.o filereq_beos.o
EXTRAOBJS = plugins/ch376/oric_ch376_plugin.o plugins/twilighte_board/oric_twilighte_board_plugin.o
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
ifneq (,$(CROSS_CFLAGS))
CFLAGS += -D__OPENGL_AVAILABLE__ -D__CBCOPY__ -D__CBPASTE__ $(CROSS_CFLAGS)
else
CFLAGS += -D__OPENGL_AVAILABLE__ -D__CBCOPY__ -D__CBPASTE__ $(shell $(SDL_LIB)-config --cflags)
endif
ifneq (,$(CROSS_LFLAGS))
LFLAGS += $(CROSS_LFLAGS)
else
LFLAGS += $(shell $(SDL_LIB)-config --libs)
endif
LFLAGS += -lm -Wl,-framework,OpenGL -framework CoreFoundation -framework AppKit
TARGET = $(TARGET_NAME)
FILEREQ_OBJ =
MSGBOX_OBJ =
CUSTOMOBJS = gui_osx.o filereq_osx.o msgbox_osx.o
EXTRAOBJS = oric_ch376_plugin.o ch376.o oric_twilighte_board_plugin.o
endif


# Special-case for Pandora Linux:
ifeq ($(PLATFORM),pandora)
TARGET = oricutron
INSTALLDIR = /usr/local
STRIP :=  $(CROSS_COMPILE)$(STRIP)
CFLAGS += -g $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config $(SDL_LIB) --cflags) -D__CBCOPY__ -D__CBPASTE__
LFLAGS += -lm -L/usr/$(BASELIBDIR) $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config $(SDL_LIB) --libs) -lX11
CUSTOMOBJS = gui_x11.o
FILEREQ_OBJ = filereq_sdl.o
MSGBOX_OBJ = msgbox_sdl.o
TARGET = oricutron
INSTALLDIR = /usr/local
endif
# Pandora


# Linux
ifeq ($(PLATFORM),linux)
ifeq (x86_64,$(shell uname -m))
BASELIBDIR := lib64
CFLAGS += -m64
LFLAGS += -m64
else
BASELIBDIR := lib
CFLAGS += -m32
LFLAGS += -m32
endif
STRIP :=  $(CROSS_COMPILE)$(STRIP)
ifeq ($(NOGTK),)
CFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config gtk+-3.0 --cflags)
LFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config gtk+-3.0 --libs)
FILEREQ_OBJ = filereq_gtk.o
MSGBOX_OBJ = msgbox_gtk.o
endif
CFLAGS += -g $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config $(SDL_LIB) --cflags) -D__OPENGL_AVAILABLE__ -DAUDIO_BUFLEN=1024 -D__CBCOPY__ -D__CBPASTE__
LFLAGS += -lm -L/usr/$(BASELIBDIR) $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config $(SDL_LIB) --libs) -lGL -lX11
CUSTOMOBJS = gui_x11.o
EXTRAOBJS = oric_ch376_plugin.o ch376.o oric_twilighte_board_plugin.o
TARGET = oricutron
INSTALLDIR = /usr/local
endif

# Linux-cross 32
ifeq ($(PLATFORM),lin-32)
BASELIBDIR := lib
CFLAGS += -m32
LFLAGS += -m32
STRIP :=  $(CROSS_COMPILE)$(STRIP)
ifeq ($(NOGTK),)
CFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config gtk+-3.0 --cflags)
LFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config gtk+-3.0 --libs)
FILEREQ_OBJ = filereq_gtk.o
MSGBOX_OBJ = msgbox_gtk.o
endif
CFLAGS += -g $(SDL_CFLAGS) -D__OPENGL_AVAILABLE__ -DAUDIO_BUFLEN=1024 -D__CBCOPY__ -D__CBPASTE__
LFLAGS += -lm -L/usr/$(BASELIBDIR) $(SDL_LFLAGS) -lGL -lX11
CUSTOMOBJS = gui_x11.o
EXTRAOBJS = oric_ch376_plugin.o ch376.o oric_twilighte_board_plugin.o
TARGET = $(TARGET_NAME)
INSTALLDIR = /usr/local
endif

# Linux-cross 64
ifeq ($(PLATFORM),lin-64)
BASELIBDIR := lib64
CFLAGS += -m64
LFLAGS += -m64
STRIP :=  $(CROSS_COMPILE)$(STRIP)
ifeq ($(NOGTK),)
CFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config gtk+-3.0 --cflags)
LFLAGS += $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config gtk+-3.0 --libs)
FILEREQ_OBJ = filereq_gtk.o
MSGBOX_OBJ = msgbox_gtk.o
endif
CFLAGS += -g $(SDL_CFLAGS) -D__OPENGL_AVAILABLE__ -DAUDIO_BUFLEN=1024 -D__CBCOPY__ -D__CBPASTE__
LFLAGS += -lm -L/usr/$(BASELIBDIR) $(SDL_LFLAGS) -lGL -lX11
CUSTOMOBJS = gui_x11.o
EXTRAOBJS = oric_ch376_plugin.o ch376.o oric_twilighte_board_plugin.o
TARGET = $(TARGET_NAME)
INSTALLDIR = /usr/local
endif


# Linux no-OpenGL
ifeq ($(PLATFORM),linux-nogl)
ifeq (x86_64,$(shell uname -m))
BASELIBDIR := lib64
CFLAGS += -m64
LFLAGS += -m64
else
BASELIBDIR := lib
CFLAGS += -m32
LFLAGS += -m32
endif
STRIP :=  $(CROSS_COMPILE)$(STRIP)
CFLAGS += -g $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config $(SDL_LIB) --cflags) $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config gtk+-3.0 --cflags) -D__CBCOPY__ -D__CBPASTE__
LFLAGS += -lm -L/usr/$(BASELIBDIR) $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config $(SDL_LIB) --libs) $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config gtk+-3.0 --libs) -lX11
CUSTOMOBJS = gui_x11.o
EXTRAOBJS = oric_ch376_plugin.o ch376.o oric_twilighte_board_plugin.o
FILEREQ_OBJ = filereq_gtk.o
MSGBOX_OBJ = msgbox_gtk.o
TARGET = oricutron
INSTALLDIR = /usr/local
endif

# Linux - Raspberry Pi
ifeq ($(PLATFORM),rpi)
BASELIBDIR := lib
STRIP :=  $(CROSS_COMPILE)$(STRIP)
CFLAGS += -g $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config $(SDL_LIB) --cflags)
LFLAGS += -lm -L/usr/$(BASELIBDIR) $(shell PKG_CONFIG_PATH=/usr/$(BASELIBDIR)/pkgconfig pkg-config $(SDL_LIB) --libs) #-lX11
CUSTOMOBJS = gui_x11.o
FILEREQ_OBJ = filereq_sdl.o
MSGBOX_OBJ = msgbox_sdl.o
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
CFLAGS += `$(WIZ_HOME)/bin/$(SDL_LIB)-config --cflags`
LFLAGS += -lm `$(WIZ_HOME)/bin/$(SDL_LIB)-config --libs`
TARGET = oricutron.gpe
endif

# AI touchbook OS
ifeq ($(PLATFORM),aitouchbook)
AITB_HOME = /usr/bin/
AITB_PREFIX = $(AITB_HOME)arm-angstrom-linux-gnueabi
CC = $(AITB_PREFIX)-gcc
CXX = $(AITB_PREFIX)-g++
AR =  $(AITB_PREFIX)-ar
CFLAGS += `$(AITB_HOME)/$(SDL_LIB)-config --cflags`
LFLAGS += -lm `$(AITB_HOME)/$(SDL_LIB)-config --libs`
RANLIB = $(AITB_PREFIX)-ranlib
TARGET = oricutron_AITB
endif

# Atari MINT
ifeq ($(PLATFORM),mint-cf)
SDK_HOME ?=/opt/netsurf/m5475-atari-mint
SDK_PREFIX := $(SDK_HOME)/cross/bin/m5475-atari-mint
TARGET = oricutcf.app
endif
ifeq ($(PLATFORM),mint)
SDK_HOME ?=/opt/netsurf/m68k-atari-mint
SDK_PREFIX ?= $(SDK_HOME)/cross/bin/m68k-atari-mint
TARGET = oricutrn.app
endif
ifeq ($(PLATFORM:%-cf=%),mint)
CC = $(SDK_PREFIX)-gcc
CXX = $(SDK_PREFIX)-gcc
AR =  $(SDK_PREFIX)-ar
RANLIB = $(SDK_PREFIX)-ranlib
CFLAGS += `$(SDK_HOME)/env/bin/$(SDL_LIB)-config --cflags`
LFLAGS += -Wl,--stack,256k -Wl,--msuper-memory -lm `$(SDK_HOME)/env/bin/$(SDL_LIB)-config --libs`
EXTRAOBJS = oric_ch376_plugin.o ch376.o oric_twilighte_board_plugin.o
endif

####### SHOULDN'T HAVE TO CHANGE THIS STUFF #######

OBJECTS = \
	system_sdl.o \
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
	6551_loopback.o \
	6551_modem.o \
	6551_com.o \
	disk.o \
	disk_pravetz.o \
	avi.o \
	render_sw.o \
	render_sw8.o \
	render_gl.o \
	render_null.o \
	joystick.o \
	snapshot.o \
	keyboard.o \
	$(FILEREQ_OBJ) \
	$(MSGBOX_OBJ) \
	$(EXTRAOBJS)

LINKOBJECTS := $(subst plugins/ch376/,,$(OBJECTS) $(CUSTOMOBJS))

all: $(TARGET)

run: $(TARGET)
	$(TARGET)

install: install-$(PLATFORM) $(TARGET)

package: package-$(PLATFORM) $(TARGET)

$(TARGET): $(LINKOBJECTS) $(RESOURCES)
	$(CXX) -o $(TARGET) $(LINKOBJECTS) $(LFLAGS)
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
	$(CC) -c $(CFLAGS) $< -o $(notdir $@)
	@$(CC) -MM $(CFLAGS) $< > $(notdir $*).d

%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $(notdir $@)
	@$(CC) -MM $(CFLAGS) $< > $(notdir $*).d

# Overide the default C rule for C++
%.o: %.cpp
	$(CXX) -c $(CFLAGS) $< -o $(notdir $@)
	@$(CXX) -MM $(CFLAGS) $< > $(notdir $*).d

# Overide the default C rule for ObjC
%.o: %.m
	$(CC) -c $(CFLAGS) $< -o $(notdir $@)
	@$(CC) -MM $(CFLAGS) $< > $(notdir $*).d

winicon.o: $(SRC_DIR)/winicon.ico $(SRC_DIR)/oricutron.rc
	$(WINDRES) $(DEFINES) -i $(SRC_DIR)/oricutron.rc -o winicon.o

%.guide: ReadMe.txt
# AROS needs path
	-rx ReadMe2Guide <$? >$(APP_NAME).guide $(APP_NAME).guide $(VERSION_FULL) || SYS:Rexxc/rx ReadMe2Guide <$? >$(APP_NAME).guide $(APP_NAME).guide $(VERSION_FULL)
	-GuideCheck $@

$(RSRC_BEOS): oricutron.rdef
	$(BEOS_RC) -o $@ $<

clean:
	rm -f $(TARGET) *.bak *.o *.d $(RESOURCES) printer_out.txt debug_log.txt
	rm -rf "$(PKGDIR)"

release: $(TARGET)
	$(STRIP) $(TARGET)

install-beos install-haiku:
	mkdir -p $(INSTALLDIR)
	copyattr -d $(TARGET) $(INSTALLDIR)
# TODO: use resources
	mkdir -p $(INSTALLDIR)/images
	copyattr -d images/* $(INSTALLDIR)/images

install-linux:
	install -m 755 $(TARGET) $(INSTALLDIR)/bin

%.info: %_$(AMIGA_ICONS).info
	copy CLONE $< $@

package-morphos package-aros package-os4: Oricutron.guide $(patsubst %_$(AMIGA_ICONS).info,%.info,$(wildcard *_$(AMIGA_ICONS).info))
	-@delete ram:Oricutron all >NIL:
	-@delete ram:$(PKGDIR).lha >NIL:
	makedir ram:Oricutron ram:Oricutron/disks ram:Oricutron/tapes ram:Oricutron/teledisks ram:Oricutron/pravdisks ram:Oricutron/roms ram:Oricutron/snapshots ram:Oricutron/images
	copy CLONE ENVARC:sys/def_drawer.info ram:Oricutron.info
	copy CLONE $(patsubst %_$(AMIGA_ICONS).info,%.info,$(wildcard *_$(AMIGA_ICONS).info)) ram:Oricutron
	copy CLONE images/#?.bmp ram:Oricutron/images/
	copy CLONE "disks/#?.(dsk|txt)" ram:Oricutron/disks/
	copy CLONE "tapes/#?.(tap|ort|txt)" ram:Oricutron/tapes/
	copy CLONE "roms/#?.(rom|sym|pch)" ram:Oricutron/roms/
	copy CLONE $(DOCFILES) Oricutron.guide ram:Oricutron
	copy CLONE $(TARGET) ram:Oricutron
	lha a -r -e ram:$(PKGDIR).lha ram:Oricutron ram:Oricutron.info
	delete $(patsubst %_$(AMIGA_ICONS).info,%.info,$(wildcard *_$(AMIGA_ICONS).info))
	-@delete ram:Oricutron ram:Oricutron.info all >NIL:

package-beos package-haiku:
	mkdir -p $(PKGDIR)/images
	copyattr -d $(TARGET) $(PKGDIR)
	copyattr -d images/* $(PKGDIR)/images
	install -m 644 $(DOCFILES) $(PKGDIR)
	zip -ry9 $(PKGDIR).zip $(PKGDIR)/

package-osx: $(TARGET)
	mkdir -p $(PKGDIR)/Oricutron.app/Contents/MacOS
	mkdir -p $(PKGDIR)/Oricutron.app/Contents/Resources/images
	mkdir -p $(PKGDIR)/Oricutron.app/Contents/Resources/roms
	install -m 755 $(TARGET) $(PKGDIR)/Oricutron.app/Contents/MacOS/Oricutron
	echo 'APPL????' > $(PKGDIR)/Oricutron.app/Contents/PkgInfo
	sed "s/@@VERSION@@/$(VERSION_MAJ).$(VERSION_MIN)/g" Info.plist > $(PKGDIR)/Oricutron.app/Contents/Info.plist
	install -m 644 XCode/Oricutron/Oricutron/winicon.icns $(PKGDIR)/Oricutron.app/Contents/Resources
	install -m 644 images/* $(PKGDIR)/Oricutron.app/Contents/Resources/images
	# XXX: SDL now opens files in bundles first, but not old versions
	ln -s Oricutron.app/Contents/Resources/images $(PKGDIR)/images
	install -m 644 roms/* $(PKGDIR)/Oricutron.app/Contents/Resources/roms
	ln -s Oricutron.app/Contents/Resources/roms $(PKGDIR)/roms
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

package-win-gcc: clean release
	mkdir -p $(PKGDIR)/images
	mkdir -p $(PKGDIR)/disks
	mkdir -p $(PKGDIR)/teledisks
	mkdir -p $(PKGDIR)/pravdisks
	mkdir -p $(PKGDIR)/tapes
	mkdir -p $(PKGDIR)/roms
	install -m 755 $(TARGET) $(TARGET_DEPS) $(PKGDIR)/
	install -m 644 $(SRC_DIR)/images/* $(PKGDIR)/images/
	#install -m 644 $(SRC_DIR)/disks/* $(PKGDIR)/disks/
	#install -m 644 $(SRC_DIR)/tapes/* $(PKGDIR)/tapes/
	install -m 644 $(SRC_DIR)/roms/* $(PKGDIR)/roms/
	install -m 644 $(addprefix $(SRC_DIR)/,$(DOCFILES)) $(PKGDIR)/
	# unix2dos is not always installed
	sed -i "s/$$/\r/g" $(PKGDIR)/ReadMe.txt
	zip -ry9 $(PKGDIR).zip $(PKGDIR)/

package-mint package-mint-cf:
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


.PHONY: mrproper
PLATFORMS := aitouchbook aros beos gphwiz haiku lin-32 lin-64 linux linux-nogl mint mint-cf morphos os4 osx pandora rpi win32 win32-gcc win64-gcc
mrproper:
	@for plat in $(PLATFORMS); do $(MAKE) -f $(SRC_DIR)/Makefile clean PLATFORM=$${plat}; done
