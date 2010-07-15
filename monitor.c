/*
**  Oricutron
**  Copyright (C) 2009-2010 Peter Gordon
**
**  This program is free software; you can redistribute it and/or
**  modify it under the terms of the GNU General Public License
**  as published by the Free Software Foundation, version 2
**  of the License.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
**  Monitor/Debugger
*/

#define MAX_CONS_INPUT 127        // Max line length in console
#define MAX_ASM_INPUT 36
#define CONS_WIDTH 46             // Actual console width for input

#define EMUL_EMULREGS_H     // MorphOS should not define REG_PC
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"
#include "ula.h"

#define LOG_DEBUG 1

#if LOG_DEBUG
#ifdef __amigaos4__ 
static char *debug_logname = "RAM:debug.log";
#else
static char *debug_logname = "debug_log.txt";
#endif
FILE *debug_logfile = NULL;
#endif

struct disinf
{
  char *name;
  int amode;
};

struct asminf
{
  char *name;
  short imp, imm, zp, zpx, zpy, abs, abx, aby, zix, ziy, rel, ind;
};


extern struct textzone *tz[];
extern char vsptmp[];

static char distmp[128];
static char ibuf[128], lastcmd;
static char history[10][128];
static int ilen, iloff=0, cursx, histu=0, histp=-1;
static unsigned short mon_addr, mon_asmmode = SDL_FALSE;
static SDL_bool kshifted = SDL_FALSE;
static int helpcount=0;

static struct symboltable defaultsyms;
struct symboltable usersyms;

char mon_bpmsg[80];

static SDL_bool mw_split = SDL_FALSE;
static int mw_which = 0;
static unsigned short mw_addr[2];
static int mw_mode=0, mw_koffs=0;
static char mw_ibuf[8];
static unsigned char mwatch_old[65536];
static SDL_bool mwatch_oldvalid = SDL_FALSE;
static struct m6502 cpu_old;
static int vidraster_old, frames_old;
static SDL_bool cpu_oldvalid = SDL_FALSE;
static struct ay8912 ay_old;
static SDL_bool ay_oldvalid = SDL_FALSE;
static struct via via_old;
static SDL_bool via_oldvalid = SDL_FALSE;
static struct via via2_old;
static SDL_bool via2_oldvalid = SDL_FALSE;


//                                                             12345678901       12345678 
static struct msym defsym_tele[]  = { { 0x0300, 0,            "VIA_IORB"      , "VIA_IORB"   , "VIA_IORB" },
                                      { 0x0301, 0,            "VIA_IORA"      , "VIA_IORA"   , "VIA_IORA" },
                                      { 0x0302, 0,            "VIA_DDRB"      , "VIA_DDRB"   , "VIA_DDRB" },
                                      { 0x0303, 0,            "VIA_DDRA"      , "VIA_DDRA"   , "VIA_DDRA" },
                                      { 0x0304, 0,            "VIA_T1C_L"     , "VIA_T1C\x16", "VIA_T1C_L" },
                                      { 0x0305, 0,            "VIA_T1C_H"     , "VIA_T1C\x16", "VIA_T1C_H" },
                                      { 0x0306, 0,            "VIA_T1L_L"     , "VIA_T1L\x16", "VIA_T1L_L" },
                                      { 0x0307, 0,            "VIA_T1L_H"     , "VIA_T1L\x16", "VIA_T1L_H" },
                                      { 0x0308, 0,            "VIA_T2C_L"     , "VIA_T2C\x16", "VIA_T2C_L" },
                                      { 0x0309, 0,            "VIA_T2C_H"     , "VIA_T2C\x16", "VIA_T2C_H" },
                                      { 0x030A, 0,            "VIA_SR"        , "VIA_SR"     , "VIA_SR" },
                                      { 0x030B, 0,            "VIA_ACR"       , "VIA_ACR"    , "VIA_ACR" },
                                      { 0x030C, 0,            "VIA_PCR"       , "VIA_PCR"    , "VIA_PCR" },
                                      { 0x030D, 0,            "VIA_IFR"       , "VIA_IFR"    , "VIA_IFR" },
                                      { 0x030E, 0,            "VIA_IER"       , "VIA_IER"    , "VIA_IER" },
                                      { 0x030F, 0,            "VIA_IORA2"     , "VIA_IORA2"  , "VIA_IORA2" },
                                      { 0x0310, 0,            "WD17_COMMA\x16", "WD17_CO\x16", "WD17_COMMAND" },
                                      { 0x0311, 0,            "WD17_TRACK\x16", "WD17_TR\x16", "WD17_TRACK" },
                                      { 0x0312, 0,            "WD17_SECTO\x16", "WD17_SE\x16", "WD17_SECTOR" },
                                      { 0x0313, 0,            "WD17_DATA"     , "WD17_DA\x16", "WD17_DATA" },
                                      { 0x0314, 0,            "MDSC_STATUS"   , "MDSC_ST\x16", "MDSC_STATUS" },
                                      { 0x0318, 0,            "MDSC_DRQ"      , "MDSC_DR\x16", "MDSC_DRQ" },
                                      { 0x031c, 0,            "ACIA_DATA"     , "ACIA_DA\x16", "ACIA_DATA" },
                                      { 0x031d, 0,            "ACIA_STATUS"   , "ACIA_ST\x16", "ACIA_STATUS" },
                                      { 0x031e, 0,            "ACIA_COMMAND"  , "ACIA_CO\x16", "ACIA_COMMAND" },
                                      { 0x031f, 0,            "ACIA_CONTROL"  , "ACIA_CO\x16", "ACIA_CONTROL" },
                                      { 0x0320, 0,            "VIA2_IORB"     , "VIA2_IORB"  , "VIA2_IORB" },
                                      { 0x0321, 0,            "VIA2_IORA"     , "VIA2_IORA"  , "VIA2_IORA" },
                                      { 0x0322, 0,            "VIA2_DDRB"     , "VIA2_DDRB"  , "VIA2_DDRB" },
                                      { 0x0323, 0,            "VIA2_DDRA"     , "VIA2_DDRA"  , "VIA2_DDRA" },
                                      { 0x0324, 0,            "VIA2_T1C_L"    , "VIA2_T1\x16", "VIA2_T1C_L" },
                                      { 0x0325, 0,            "VIA2_T1C_H"    , "VIA2_T1\x16", "VIA2_T1C_H" },
                                      { 0x0326, 0,            "VIA2_T1L_L"    , "VIA2_T1\x16", "VIA2_T1L_L" },
                                      { 0x0327, 0,            "VIA2_T1L_H"    , "VIA2_T1\x16", "VIA2_T1L_H" },
                                      { 0x0328, 0,            "VIA2_T2C_L"    , "VIA2_T2\x16", "VIA2_T2C_L" },
                                      { 0x0329, 0,            "VIA2_T2C_H"    , "VIA2_T2\x16", "VIA2_T2C_H" },
                                      { 0x032A, 0,            "VIA2_SR"       , "VIA2_SR"    , "VIA2_SR" },
                                      { 0x032B, 0,            "VIA2_ACR"      , "VIA2_ACR"   , "VIA2_ACR" },
                                      { 0x032C, 0,            "VIA2_PCR"      , "VIA2_PCR"   , "VIA2_PCR" },
                                      { 0x032D, 0,            "VIA2_IFR"      , "VIA2_IFR"   , "VIA2_IFR" },
                                      { 0x032E, 0,            "VIA2_IER"      , "VIA2_IER"   , "VIA2_IER" },
                                      { 0x032F, 0,            "VIA2_IORA2"    , "VIA2_IO\x16", "VIA2_IORA2" },
                                      { 0xc000, 0,            "BankedArea"    , "BankedA\x16", "BankedArea" } };

//                                                             12345678901       12345678 
static struct msym defsym_atmos[] = { { 0x0300, 0,            "VIA_IORB"      , "VIA_IORB"   , "VIA_IORB" },
                                      { 0x0301, 0,            "VIA_IORA"      , "VIA_IORA"   , "VIA_IORA" },
                                      { 0x0302, 0,            "VIA_DDRB"      , "VIA_DDRB"   , "VIA_DDRB" },
                                      { 0x0303, 0,            "VIA_DDRA"      , "VIA_DDRA"   , "VIA_DDRA" },
                                      { 0x0304, 0,            "VIA_T1C_L"     , "VIA_T1C\x16", "VIA_T1C_L" },
                                      { 0x0305, 0,            "VIA_T1C_H"     , "VIA_T1C\x16", "VIA_T1C_H" },
                                      { 0x0306, 0,            "VIA_T1L_L"     , "VIA_T1L\x16", "VIA_T1L_L" },
                                      { 0x0307, 0,            "VIA_T1L_H"     , "VIA_T1L\x16", "VIA_T1L_H" },
                                      { 0x0308, 0,            "VIA_T2C_L"     , "VIA_T2C\x16", "VIA_T2C_L" },
                                      { 0x0309, 0,            "VIA_T2C_H"     , "VIA_T2C\x16", "VIA_T2C_H" },
                                      { 0x030A, 0,            "VIA_SR"        , "VIA_SR"     , "VIA_SR" },
                                      { 0x030B, 0,            "VIA_ACR"       , "VIA_ACR"    , "VIA_ACR" },
                                      { 0x030C, 0,            "VIA_PCR"       , "VIA_PCR"    , "VIA_PCR" },
                                      { 0x030D, 0,            "VIA_IFR"       , "VIA_IFR"    , "VIA_IFR" },
                                      { 0x030E, 0,            "VIA_IER"       , "VIA_IER"    , "VIA_IER" },
                                      { 0x030F, 0,            "VIA_IORA2"     , "VIA_IORA2"  , "VIA_IORA2" },
                                      { 0xc000, SYMF_ROMDIS0, "ROMStart"      , "ROMStart"   , "ROMStart" },
                                      { 0xc000, SYMF_ROMDIS1, "Overlay"       , "Overlay"    , "Overlay" } };

enum
{
  MSHOW_VIA=0,
  MSHOW_VIA2,
  MSHOW_AY,
  MSHOW_DISK,
  MSHOW_LAST
};

enum
{
  CSHOW_CONSOLE=0,
  CSHOW_DEBUG,
  CSHOW_MWATCH,
  CSHOW_LAST
};

static int mshow, cshow;

enum
{
  REG_A=0,
  REG_X,
  REG_Y,
  REG_PC,
  REG_SP,
  REG_VIA_PCR,
  REG_VIA_ACR,
  REG_VIA_SR,
  REG_VIA_IFR,
  REG_VIA_IER,
  REG_VIA_IRA,
  REG_VIA_ORA,
  REG_VIA_DDRA,
  REG_VIA_IRB,
  REG_VIA_ORB,
  REG_VIA_DDRB,
  REG_VIA_T1L,
  REG_VIA_T1C,
  REG_VIA_T2L,
  REG_VIA_T2C,
  REG_VIA_CA1,
  REG_VIA_CA2,
  REG_VIA_CB1,
  REG_VIA_CB2
};

enum
{
  AM_IMP=0,
  AM_IMM,
  AM_ZP,
  AM_ZPX,
  AM_ZPY,
  AM_ABS,
  AM_ABX,
  AM_ABY,
  AM_ZIX,
  AM_ZIY,
  AM_REL,
  AM_IND
};

#define AMF_IMP (1<<AM_IMP)
#define AMF_IMM (1<<AM_IMM)
#define AMF_ZP  (1<<AM_ZP)
#define AMF_ZPX (1<<AM_ZPX)
#define AMF_ZPY (1<<AM_ZPY)
#define AMF_ABS (1<<AM_ABS)
#define AMF_ABX (1<<AM_ABX)
#define AMF_ABY (1<<AM_ABY)
#define AMF_ZIX (1<<AM_ZIX)
#define AMF_ZIY (1<<AM_ZIY)
#define AMF_REL (1<<AM_REL)
#define AMF_IND (1<<AM_IND)

//                                          imp   imm    zp   zpx   zpy   abs   abx   aby   zix   ziy   rel   ind
static struct asminf asmtab[] = { { "BRK", 0x00,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "ORA",   -1, 0x09, 0x05, 0x15,   -1, 0x0d, 0x1d, 0x19, 0x01, 0x11,   -1,   -1 },
                                  { "ASL", 0x0a,   -1, 0x06, 0x16,   -1, 0x0e, 0x1e,   -1,   -1,   -1,   -1,   -1 },
                                  { "PHP", 0x08,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BPL",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x10,   -1 },
                                  { "CLC", 0x18,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "JSR",   -1,   -1,   -1,   -1,   -1, 0x20,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "AND",   -1,   -1,   -1, 0x35,   -1,   -1, 0x3d, 0x39, 0x21, 0x31,   -1,   -1 },
                                  { "BIT",   -1,   -1, 0x24,   -1,   -1, 0x2c,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "AND",   -1, 0x29, 0x25,   -1,   -1, 0x2d,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "ROL", 0x2a,   -1, 0x26, 0x36,   -1, 0x2e, 0x3e,   -1,   -1,   -1,   -1,   -1 },
                                  { "PLP", 0x28,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BMI",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x30,   -1 },
                                  { "SEC", 0x38,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "RTI", 0x40,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "EOR",   -1, 0x49, 0x45, 0x55,   -1, 0x4d, 0x5d, 0x59, 0x41, 0x51,   -1,   -1 },
                                  { "LSR", 0x4a,   -1, 0x46, 0x56,   -1, 0x4e, 0x5e,   -1,   -1,   -1,   -1,   -1 },
                                  { "PHA", 0x48,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "JMP",   -1,   -1,   -1,   -1,   -1, 0x4c,   -1,   -1,   -1,   -1,   -1, 0x6c },
                                  { "BVC",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x50,   -1 },
                                  { "CLI", 0x58,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "RTS", 0x60,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "ADC",   -1, 0x69, 0x65, 0x75,   -1, 0x6d, 0x7d, 0x79, 0x61, 0x71,   -1,   -1 },
                                  { "ROR", 0x6a,   -1, 0x66, 0x76,   -1, 0x6e, 0x7e,   -1,   -1,   -1,   -1,   -1 },
                                  { "PLA", 0x68,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BVS",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x70,   -1 },
                                  { "SEI", 0x78,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "STY",   -1,   -1, 0x84, 0x94,   -1, 0x8c,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "STA",   -1,   -1, 0x85, 0x95,   -1, 0x8d, 0x9d, 0x99, 0x81, 0x91,   -1,   -1 },
                                  { "STX",   -1,   -1, 0x86, 0x96,   -1, 0x8e,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "DEY", 0x88,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "TXA", 0x8a,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BCC",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0x90,   -1 },
                                  { "TYA", 0x98,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "TXS", 0x9a,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "LDY",   -1, 0xa0, 0xa4, 0xb4,   -1, 0xac, 0xbc,   -1,   -1,   -1,   -1,   -1 },
                                  { "LDA",   -1, 0xa9, 0xa5, 0xb5,   -1, 0xad, 0xbd, 0xb9, 0xa1, 0xb1,   -1,   -1 },
                                  { "LDX",   -1, 0xa2, 0xa6,   -1, 0xb6, 0xae,   -1, 0xbe,   -1,   -1,   -1,   -1 },
                                  { "TAY", 0xa8,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "TAX", 0xaa,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BCS",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0xb0,   -1 },
                                  { "CLV", 0xb8,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "TSX", 0xba,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "CPY",   -1, 0xc0, 0xc4,   -1,   -1, 0xcc,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "CMP",   -1, 0xc9, 0xc5, 0xd5,   -1, 0xcd, 0xdd, 0xd9, 0xc1, 0xd1,   -1,   -1 },
                                  { "DEC",   -1,   -1, 0xc6, 0xd6,   -1, 0xce, 0xde,   -1, 0xc1,   -1,   -1,   -1 },
                                  { "INY", 0xc8,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "DEX", 0xca,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BNE",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0xd0,   -1 },
                                  { "CLD", 0xd8,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "CPX",   -1, 0xe0, 0xe4,   -1,   -1, 0xec,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "SBC",   -1, 0xe9, 0xe5, 0xf5,   -1, 0xed, 0xfd, 0xf9, 0xe1, 0xf1,   -1,   -1 },
                                  { "INC",   -1,   -1, 0xe6, 0xf6,   -1, 0xee, 0xfe,   -1,   -1,   -1,   -1,   -1 },
                                  { "INX", 0xe8,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "NOP", 0xea,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { "BEQ",   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1, 0xf0,   -1 },
                                  { "SED", 0xf8,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1 },
                                  { NULL, } };

static struct disinf distab[] = { { "BRK", AM_IMM },  // 00
                                  { "ORA", AM_ZIX },  // 01
                                  { "???", AM_IMP },  // 02
                                  { "???", AM_IMP },  // 03
                                  { "???", AM_IMP },  // 04
                                  { "ORA", AM_ZP  },  // 05
                                  { "ASL", AM_ZP  },  // 06
                                  { "???", AM_IMP },  // 07
                                  { "PHP", AM_IMP },  // 08
                                  { "ORA", AM_IMM },  // 09
                                  { "ASL", AM_IMP },  // 0A
                                  { "???", AM_IMP },  // 0B
                                  { "???", AM_IMP },  // 0C
                                  { "ORA", AM_ABS },  // 0D
                                  { "ASL", AM_ABS },  // 0E
                                  { "???", AM_IMP },  // 0F
                                  { "BPL", AM_REL },  // 10
                                  { "ORA", AM_ZIY },  // 11
                                  { "???", AM_IMP },  // 12
                                  { "???", AM_IMP },  // 13
                                  { "???", AM_IMP },  // 14
                                  { "ORA", AM_ZPX },  // 15
                                  { "ASL", AM_ZPX },  // 16
                                  { "???", AM_IMP },  // 17
                                  { "CLC", AM_IMP },  // 18
                                  { "ORA", AM_ABY },  // 19
                                  { "???", AM_IMP },  // 1A
                                  { "???", AM_IMP },  // 1B
                                  { "???", AM_IMP },  // 1C
                                  { "ORA", AM_ABX },  // 1D
                                  { "ASL", AM_ABX },  // 1E
                                  { "???", AM_IMP },  // 1F
                                  { "JSR", AM_ABS },  // 20
                                  { "AND", AM_ZIX },  // 21
                                  { "???", AM_IMP },  // 22
                                  { "???", AM_IMP },  // 23
                                  { "BIT", AM_ZP  },  // 24
                                  { "AND", AM_ZP  },  // 25
                                  { "ROL", AM_ZP  },  // 26
                                  { "???", AM_IMP },  // 27
                                  { "PLP", AM_IMP },  // 28
                                  { "AND", AM_IMM },  // 29
                                  { "ROL", AM_IMP },  // 2A
                                  { "???", AM_IMP },  // 2B
                                  { "BIT", AM_ABS },  // 2C
                                  { "AND", AM_ABS },  // 2D
                                  { "ROL", AM_ABS },  // 2E
                                  { "???", AM_IMP },  // 2F
                                  { "BMI", AM_REL },  // 30
                                  { "AND", AM_ZIY },  // 31
                                  { "???", AM_IMP },  // 32
                                  { "???", AM_IMP },  // 33
                                  { "???", AM_IMP },  // 34
                                  { "AND", AM_ZPX },  // 35
                                  { "ROL", AM_ZPX },  // 36
                                  { "???", AM_IMP },  // 37
                                  { "SEC", AM_IMP },  // 38
                                  { "AND", AM_ABY },  // 39
                                  { "???", AM_IMP },  // 3A
                                  { "???", AM_IMP },  // 3B
                                  { "???", AM_IMP },  // 3C
                                  { "AND", AM_ABX },  // 3D
                                  { "ROL", AM_ABX },  // 3E
                                  { "???", AM_IMP },  // 3F
                                  { "RTI", AM_IMP },  // 40
                                  { "EOR", AM_ZIX },  // 41
                                  { "???", AM_IMP },  // 42
                                  { "???", AM_IMP },  // 43
                                  { "???", AM_IMP },  // 44
                                  { "EOR", AM_ZP  },  // 45
                                  { "LSR", AM_ZP  },  // 46
                                  { "???", AM_IMP },  // 47
                                  { "PHA", AM_IMP },  // 48
                                  { "EOR", AM_IMM },  // 49
                                  { "LSR", AM_IMP },  // 4A
                                  { "???", AM_IMP },  // 4B
                                  { "JMP", AM_ABS },  // 4C
                                  { "EOR", AM_ABS },  // 4D
                                  { "LSR", AM_ABS },  // 4E
                                  { "???", AM_IMP },  // 4F
                                  { "BVC", AM_REL },  // 50
                                  { "EOR", AM_ZIY },  // 51
                                  { "???", AM_IMP },  // 52
                                  { "???", AM_IMP },  // 53
                                  { "???", AM_IMP },  // 54
                                  { "EOR", AM_ZPX },  // 55
                                  { "LSR", AM_ZPX },  // 56
                                  { "???", AM_IMP },  // 57
                                  { "CLI", AM_IMP },  // 58
                                  { "EOR", AM_ABY },  // 59
                                  { "???", AM_IMP },  // 5A
                                  { "???", AM_IMP },  // 5B
                                  { "???", AM_IMP },  // 5C
                                  { "EOR", AM_ABX },  // 5D
                                  { "LSR", AM_ABX },  // 5E
                                  { "???", AM_IMP },  // 5F
                                  { "RTS", AM_IMP },  // 60
                                  { "ADC", AM_ZIX },  // 61
                                  { "???", AM_IMP },  // 62
                                  { "???", AM_IMP },  // 63
                                  { "???", AM_IMP },  // 64
                                  { "ADC", AM_ZP  },  // 65
                                  { "ROR", AM_ZP  },  // 66
                                  { "???", AM_IMP },  // 67
                                  { "PLA", AM_IMP },  // 68
                                  { "ADC", AM_IMM },  // 69
                                  { "ROR", AM_IMP },  // 6A
                                  { "???", AM_IMP },  // 6B
                                  { "JMP", AM_IND },  // 6C
                                  { "ADC", AM_ABS },  // 6D
                                  { "ROR", AM_ABS },  // 6E
                                  { "???", AM_IMP },  // 6F
                                  { "BVS", AM_REL },  // 70
                                  { "ADC", AM_ZIY },  // 71
                                  { "???", AM_IMP },  // 72
                                  { "???", AM_IMP },  // 73
                                  { "???", AM_IMP },  // 74
                                  { "ADC", AM_ZPX },  // 75
                                  { "ROR", AM_ZPX },  // 76
                                  { "???", AM_IMP },  // 77
                                  { "SEI", AM_IMP },  // 78
                                  { "ADC", AM_ABY },  // 79
                                  { "???", AM_IMP },  // 7A
                                  { "???", AM_IMP },  // 7B
                                  { "???", AM_IMP },  // 7C
                                  { "ADC", AM_ABX },  // 7D
                                  { "ROR", AM_ABX },  // 7E
                                  { "???", AM_IMP },  // 7F
                                  { "???", AM_IMP },  // 80
                                  { "STA", AM_ZIX },  // 81
                                  { "???", AM_IMP },  // 82
                                  { "???", AM_IMP },  // 83
                                  { "STY", AM_ZP  },  // 84
                                  { "STA", AM_ZP  },  // 85
                                  { "STX", AM_ZP  },  // 86
                                  { "???", AM_IMP },  // 87
                                  { "DEY", AM_IMP },  // 88
                                  { "???", AM_IMP },  // 89
                                  { "TXA", AM_IMP },  // 8A
                                  { "???", AM_IMP },  // 8B
                                  { "STY", AM_ABS },  // 8C
                                  { "STA", AM_ABS },  // 8D
                                  { "STX", AM_ABS },  // 8E
                                  { "???", AM_IMP },  // 8F
                                  { "BCC", AM_REL },  // 90
                                  { "STA", AM_ZIY },  // 91
                                  { "???", AM_IMP },  // 92
                                  { "???", AM_IMP },  // 93
                                  { "STY", AM_ZPX },  // 94
                                  { "STA", AM_ZPX },  // 95
                                  { "STX", AM_ZPY },  // 96
                                  { "???", AM_IMP },  // 97
                                  { "TYA", AM_IMP },  // 98
                                  { "STA", AM_ABY },  // 99
                                  { "TXS", AM_IMP },  // 9A
                                  { "???", AM_IMP },  // 9B
                                  { "???", AM_IMP },  // 9C
                                  { "STA", AM_ABX },  // 9D
                                  { "???", AM_IMP },  // 9E
                                  { "???", AM_IMP },  // 9F
                                  { "LDY", AM_IMM },  // A0
                                  { "LDA", AM_ZIX },  // A1
                                  { "LDX", AM_IMM },  // A2
                                  { "???", AM_IMP },  // A3
                                  { "LDY", AM_ZP  },  // A4
                                  { "LDA", AM_ZP  },  // A5
                                  { "LDX", AM_ZP  },  // A6
                                  { "???", AM_IMP },  // A7
                                  { "TAY", AM_IMP },  // A8
                                  { "LDA", AM_IMM },  // A9
                                  { "TAX", AM_IMP },  // AA
                                  { "???", AM_IMP },  // AB
                                  { "LDY", AM_ABS },  // AC
                                  { "LDA", AM_ABS },  // AD
                                  { "LDX", AM_ABS },  // AE
                                  { "???", AM_IMP },  // AF
                                  { "BCS", AM_REL },  // B0
                                  { "LDA", AM_ZIY },  // B1
                                  { "???", AM_IMP },  // B2
                                  { "???", AM_IMP },  // B3
                                  { "LDY", AM_ZPX },  // B4
                                  { "LDA", AM_ZPX },  // B5
                                  { "LDX", AM_ZPY },  // B6
                                  { "???", AM_IMP },  // B7
                                  { "CLV", AM_IMP },  // B8
                                  { "LDA", AM_ABY },  // B9
                                  { "TSX", AM_IMP },  // BA
                                  { "???", AM_IMP },  // BB
                                  { "LDY", AM_ABX },  // BC
                                  { "LDA", AM_ABX },  // BD
                                  { "LDX", AM_ABY },  // BE
                                  { "???", AM_IMP },  // BF
                                  { "CPY", AM_IMM },  // C0
                                  { "CMP", AM_ZIX },  // C1
                                  { "???", AM_IMP },  // C2
                                  { "???", AM_IMP },  // C3
                                  { "CPY", AM_ZP  },  // C4
                                  { "CMP", AM_ZP  },  // C5
                                  { "DEC", AM_ZP  },  // C6
                                  { "???", AM_IMP },  // C7
                                  { "INY", AM_IMP },  // C8
                                  { "CMP", AM_IMM },  // C9
                                  { "DEX", AM_IMP },  // CA
                                  { "???", AM_IMP },  // CB
                                  { "CPY", AM_ABS },  // CC
                                  { "CMP", AM_ABS },  // CD
                                  { "DEC", AM_ABS },  // CE
                                  { "???", AM_IMP },  // CF
                                  { "BNE", AM_REL },  // D0
                                  { "CMP", AM_ZIY },  // D1
                                  { "???", AM_IMP },  // D2
                                  { "???", AM_IMP },  // D3
                                  { "???", AM_IMP },  // D4
                                  { "CMP", AM_ZPX },  // D5
                                  { "DEC", AM_ZPX },  // D6
                                  { "???", AM_IMP },  // D7
                                  { "CLD", AM_IMP },  // D8
                                  { "CMP", AM_ABY },  // D9
                                  { "???", AM_IMP },  // DA
                                  { "???", AM_IMP },  // DB
                                  { "???", AM_IMP },  // DC
                                  { "CMP", AM_ABX },  // DD
                                  { "DEC", AM_ABX },  // DE
                                  { "???", AM_IMP },  // DF
                                  { "CPX", AM_IMM },  // E0
                                  { "SBC", AM_ZIX },  // E1
                                  { "???", AM_IMP },  // E2
                                  { "???", AM_IMP },  // E3
                                  { "CPX", AM_ZP  },  // E4
                                  { "SBC", AM_ZP  },  // E5
                                  { "INC", AM_ZP  },  // E6
                                  { "???", AM_IMP },  // E7
                                  { "INX", AM_IMP },  // E8
                                  { "SBC", AM_IMM },  // E9
                                  { "NOP", AM_IMP },  // EA
                                  { "???", AM_IMP },  // EB
                                  { "CPX", AM_ABS },  // EC
                                  { "SBC", AM_ABS },  // ED
                                  { "INC", AM_ABS },  // EE
                                  { "???", AM_IMP },  // EF
                                  { "BEQ", AM_REL },  // F0
                                  { "SBC", AM_ZIY },  // F1
                                  { "???", AM_IMP },  // F2
                                  { "???", AM_IMP },  // F3
                                  { "???", AM_IMP },  // F4
                                  { "SBC", AM_ZPX },  // F5
                                  { "INC", AM_ZPX },  // F6
                                  { "???", AM_IMP },  // F7
                                  { "SED", AM_IMP },  // F8
                                  { "SBC", AM_ABY },  // F9
                                  { "???", AM_IMP },  // FA
                                  { "???", AM_IMP },  // FB
                                  { "???", AM_IMP },  // FC
                                  { "SBC", AM_ABX },  // FD
                                  { "INC", AM_ABX },  // FE
                                  { "???", AM_IMP } };// FF

SDL_bool isws( char c )
{
  if( ( c == 9 ) || ( c == 32 ) ) return SDL_TRUE;
  return SDL_FALSE;
}

SDL_bool isbin( char c )
{
  if( ( c >= '0' ) && ( c <= '1' ) ) return SDL_TRUE;
  return SDL_FALSE;
}

SDL_bool isnum( char c )
{
  if( ( c >= '0' ) && ( c <= '9' ) ) return SDL_TRUE;
  return SDL_FALSE;
}

SDL_bool ishex( char c )
{
  if( isnum( c ) ) return SDL_TRUE;
  if( ( c >= 'a' ) && ( c <= 'f' ) ) return SDL_TRUE;
  if( ( c >= 'A' ) && ( c <= 'F' ) ) return SDL_TRUE;
  return SDL_FALSE;
}

SDL_bool isalph( char c )
{
  if( ( c >= 'a' ) && ( c <= 'z' ) ) return SDL_TRUE;
  if( ( c >= 'A' ) && ( c <= 'Z' ) ) return SDL_TRUE;
  return SDL_FALSE;
}

SDL_bool issymstart( char c )
{
  if( isalph( c ) ) return SDL_TRUE;
  if( c == '_' ) return SDL_TRUE;
  return SDL_FALSE;
}

SDL_bool issymchar( char c )
{
  if( issymstart( c ) ) return SDL_TRUE;
  if( isnum( c ) ) return SDL_TRUE;
  return SDL_FALSE;
}

int hexit( char c )
{
  if( isnum( c ) ) return c-'0';
  if( ( c >= 'a' ) && ( c <= 'f' ) ) return c-('a'-10);
  if( ( c >= 'A' ) && ( c <= 'F' ) ) return c-('A'-10);
  return -1;
}

void dbg_scroll( void )
{
  int x, y, s, d;
  struct textzone *ptz = tz[TZ_DEBUG];

  s = ptz->w*2+1;
  d = ptz->w+1;
  for( y=0; y<18; y++ )
  {
    for( x=0; x<48; x++ )
    {
      ptz->tx[d] = ptz->tx[s];
      ptz->fc[d] = ptz->fc[s];
      ptz->bc[d++] = ptz->bc[s++];
    }
    d += 2;
    s += 2;
  }

  for( x=0; x<48; x++ )
  {
    ptz->tx[d] = ' ';
    ptz->fc[d] = 2;
    ptz->bc[d++] = 3;
  }
}

void dbg_printf( char *fmt, ... )
{
  va_list ap;

  dbg_scroll();

  va_start( ap, fmt );
  if( vsnprintf( vsptmp, VSPTMPSIZE, fmt, ap ) != -1 )
  {
    vsptmp[VSPTMPSIZE-1] = 0;
#if LOG_DEBUG
    if( debug_logfile )
    {
      fprintf( debug_logfile, "%s\n", vsptmp );
      fflush( debug_logfile );
    }
#endif
    vsptmp[48] = 0;
    tzstrpos( tz[TZ_DEBUG], 1, 19, vsptmp );
  }
  va_end( ap );
}

// Don't mess with registers that change because you read them!
unsigned char mon_read( struct machine *oric, unsigned short addr )
{
  // microdisc registers could screw things up
  if( oric->drivetype == DRV_MICRODISC )
  {
    switch( addr )
    {
      case 0x310:
        return oric->wddisk.r_status;
      
      case 0x311:
        return oric->wddisk.r_track;

      case 0x312:
        return oric->wddisk.r_sector;

      case 0x313:
        return oric->wddisk.r_data;
      
      case 0x314:
        return oric->md.intrq|0x7f;

      case 0x318:
        return oric->md.drq|0x7f;    
    }
  }

  if( ( addr & 0xff00 ) == 0x0300 )
    return via_mon_read( &oric->via, addr );

  return oric->cpu.read( &oric->cpu, addr );
}

struct msym *mon_tab_find_sym_by_addr( struct symboltable *stab, struct machine *oric, unsigned short addr )
{
  int i, j, k,romdis;

  romdis = oric->romdis;
  if( ( oric->drivetype == DRV_JASMIN ) && ( oric->jasmin.olay ) )
    romdis = 1;

  for( i=0; i<stab->numsyms; i++ )
  {
    if( oric->type == MACH_TELESTRAT )
    {
      if( stab->syms[i].addr >= 0xc000 )
      {
        for( j=0, k=SYMF_TELEBANK0; j<8; j++, k<<=1 )
        {
          if( ((stab->syms[i].flags&k)==0) && ( oric->tele_currbank == j ) )
            break;
        }

        if( j != 8 ) continue;
      }
    } else {
      if( stab->syms[i].flags&SYMF_MICRODISC )
      {
        if( ( oric->drivetype != DRV_MICRODISC ) || ( !oric->md.diskrom ) )
          continue;
      }

      if( (stab->syms[i].flags&SYMF_JASMIN) && ( oric->drivetype != DRV_JASMIN ) )
        continue;
    
      if( romdis )
      {
        if( (stab->syms[i].flags&SYMF_ROMDIS0) != 0 )
          continue;
      } else {
        if( (stab->syms[i].flags&SYMF_ROMDIS1) != 0 )
          continue;
      }
    }

    if( addr == stab->syms[i].addr )
      return &stab->syms[i];
  }

  return NULL;
}

struct msym *mon_tab_find_sym_by_name( struct symboltable *stab, struct machine *oric, char *name, int *symnum )
{
  int i, j, k, romdis;

  romdis = oric->romdis;
  if( ( oric->drivetype == DRV_JASMIN ) && ( oric->jasmin.olay ) )
    romdis = 1;

  for( i=0; i<stab->numsyms; i++ )
  {
    if( oric->type == MACH_TELESTRAT )
    {
      if( stab->syms[i].addr >= 0xc000 )
      {
        for( j=0, k=SYMF_TELEBANK0; j<8; j++, k<<=1 )
        {
          if( ((stab->syms[i].flags&k)==0) && ( oric->tele_currbank == j ) )
            break;
        }

        if( j != 8 ) continue;
      }
    } else {
      if( stab->syms[i].flags&SYMF_MICRODISC )
      {
        if( ( oric->drivetype != DRV_MICRODISC ) || ( !oric->md.diskrom ) )
          continue;
      }

      if( (stab->syms[i].flags&SYMF_JASMIN) && ( oric->drivetype != DRV_JASMIN ) )
        continue;
    
      if( romdis )
      {
        if( (stab->syms[i].flags&SYMF_ROMDIS0) != 0 )
          continue;
      } else {
        if( (stab->syms[i].flags&SYMF_ROMDIS1) != 0 )
          continue;
      }
    }

    if( !oric->symbolscase )
    {
      if( strcasecmp( name, stab->syms[i].name ) == 0 )
      {
        if( symnum ) (*symnum) = i;
        return &stab->syms[i];
      }
    } else {
      if( strcmp( name, stab->syms[i].name ) == 0 )
      {
        if( symnum ) (*symnum) = i;
        return &stab->syms[i];
      }
    }
  }

  if( symnum) (*symnum) = -1;
  return NULL;
}

struct msym *mon_find_sym_by_addr( struct machine *oric, unsigned short addr )
{
  struct msym *ret;

  ret = mon_tab_find_sym_by_addr( &usersyms, oric, addr );
  if( !ret ) ret = mon_tab_find_sym_by_addr( &oric->romsyms, oric, addr );
  if( ( !ret ) && ( oric->type == MACH_TELESTRAT ) ) ret = mon_tab_find_sym_by_addr( &oric->tele_banksyms[oric->tele_currbank], oric, addr );
  if( ( !ret ) && ( oric->disksyms ) ) ret = mon_tab_find_sym_by_addr( oric->disksyms, oric, addr );
  if( ret ) return ret;

  return mon_tab_find_sym_by_addr( &defaultsyms, oric, addr );
}

struct msym *mon_find_sym_by_name( struct machine *oric, char *name )
{
  struct msym *ret;

  ret = mon_tab_find_sym_by_name( &usersyms, oric, name, NULL );
  if( !ret ) ret = mon_tab_find_sym_by_name( &oric->romsyms, oric, name, NULL );
  if( ( !ret ) && ( oric->type == MACH_TELESTRAT ) ) ret = mon_tab_find_sym_by_name( &oric->tele_banksyms[oric->tele_currbank], oric, name, NULL );
  if( ( !ret ) && ( oric->disksyms ) ) ret = mon_tab_find_sym_by_name( oric->disksyms, oric, name, NULL );
  if( ret ) return ret;

  return mon_tab_find_sym_by_name( &defaultsyms, oric, name, NULL );
}

SDL_bool mon_addsym( struct symboltable *stab, unsigned short addr, unsigned short flags, char *name, struct msym **symptr )
{
  int i;

  if( symptr ) (*symptr) = NULL;

  // Is it a dynamic symbol table?
  if( stab->symspace == -1 ) return SDL_FALSE;

  if( ( !stab->syms ) || ( stab->symspace < (stab->numsyms+1) ) )
  {
    struct msym *newbuf;

    newbuf = malloc( sizeof( struct msym ) * (stab->symspace+64) );
    if( !newbuf ) return SDL_FALSE;

    if( ( stab->syms ) && ( stab->numsyms > 0 ) )
      memcpy( newbuf, stab->syms, sizeof( struct msym ) * stab->numsyms );

    free( stab->syms );
    stab->syms = newbuf;
    stab->symspace += 64;

    for( i=stab->numsyms; i<stab->symspace; i++ )
      stab->syms[i].name = NULL;
  }

  if( !stab->syms[stab->numsyms].name )
  {
    stab->syms[stab->numsyms].name = malloc( 128 );
    if( !stab->syms[stab->numsyms].name ) return SDL_FALSE;
  }

  stab->syms[stab->numsyms].addr = addr;
  stab->syms[stab->numsyms].flags = flags;
  strncpy( stab->syms[stab->numsyms].name, name, 128 );
  stab->syms[stab->numsyms].name[127] = 0;

  if( strlen( name ) > SNAME_LEN )
  {
    strncpy( stab->syms[stab->numsyms].sname, name, (SNAME_LEN-1) );
    stab->syms[stab->numsyms].sname[SNAME_LEN-1] = 22;
    stab->syms[stab->numsyms].sname[SNAME_LEN] = 0;
  } else {
    strcpy( stab->syms[stab->numsyms].sname, name );
  }

  if( strlen( name ) > SSNAME_LEN )
  {
    strncpy( stab->syms[stab->numsyms].ssname, name, (SSNAME_LEN-1) );
    stab->syms[stab->numsyms].ssname[SSNAME_LEN-1] = 22;
    stab->syms[stab->numsyms].ssname[SSNAME_LEN] = 0;
  } else {
    strcpy( stab->syms[stab->numsyms].ssname, name );
  }

  if( symptr ) *(symptr) = &stab->syms[stab->numsyms];

  stab->numsyms++;

  return SDL_TRUE;
}

void mon_init_symtab( struct symboltable *stab )
{
  stab->syms = NULL;
  stab->numsyms = 0;
  stab->symspace = 0;
}

void mon_freesyms( struct symboltable *stab )
{
  // Is it a dynamic table?
  if( stab->symspace == -1 ) return;
  if( stab->syms ) free( stab->syms );
  stab->syms     = NULL;
  stab->numsyms  = 0;
  stab->symspace = 0;
}

char *mon_disassemble( struct machine *oric, unsigned short *paddr, SDL_bool *looped, SDL_bool filemode )
{
  unsigned short iaddr, addr;
  unsigned char op, a1, a2;
  int i;
  char *tmpsname, *disptr;
  char sname[SNAME_LEN+1];
  struct msym *csym;

  disptr = distmp;

  iaddr = *paddr;
  if( looped ) *looped = SDL_FALSE;

  tmpsname = "";
  if( !filemode )
  {
    csym = mon_find_sym_by_addr( oric, iaddr );
    if( csym ) tmpsname = csym->sname;
  } else {
    csym = mon_find_sym_by_addr( oric, iaddr );
    if( csym )
    {
      tmpsname = csym->name;

      if( strlen( tmpsname ) > SNAME_LEN )
      {
        sprintf( distmp, "%s\n", tmpsname );
        disptr = &distmp[strlen(distmp)];
        tmpsname = "";
      }
    }
  }

  strcpy( sname, tmpsname );
  for( i=strlen(sname); i<SNAME_LEN; i++ )
    sname[i] = 32;
  sname[i] = 0;

  op = mon_read( oric, (*paddr)++ );
  switch( distab[op].amode )
  {
    case AM_IMP:
      sprintf( disptr, "%s   %04X  %02X        %s", sname, iaddr, op, distab[op].name );
      break;
    
    case AM_IMM:
      a1 = mon_read( oric, (*paddr)++ );
      sprintf( disptr, "%s   %04X  %02X %02X     %s #$%02X", sname, iaddr, op, a1, distab[op].name, a1 );
      break;

    case AM_ZP:
    case AM_ZPX:
    case AM_ZPY:
      a1 = mon_read( oric, (*paddr)++ );
      csym = mon_find_sym_by_addr( oric, a1 );
      if( csym )
        sprintf( disptr, "%s   %04X  %02X %02X     %s %s", sname, iaddr, op, a1, distab[op].name, filemode ? csym->name : csym->sname );
      else
        sprintf( disptr, "%s   %04X  %02X %02X     %s $%02X", sname, iaddr, op, a1, distab[op].name, a1 );
      if( distab[op].amode == AM_ZP ) break;
      strcat( disptr, distab[op].amode == AM_ZPX ? ",X" : ",Y" );
      break;
    
    case AM_ABS:
    case AM_ABX:
    case AM_ABY:
      a1 = mon_read( oric, (*paddr)++ );
      a2 = mon_read( oric, (*paddr)++ );
      csym = mon_find_sym_by_addr( oric, (a2<<8)|a1 );
      if( csym )
        sprintf( disptr, "%s   %04X  %02X %02X %02X  %s %s", sname, iaddr, op, a1, a2, distab[op].name, filemode ? csym->name : csym->sname );
      else
        sprintf( disptr, "%s   %04X  %02X %02X %02X  %s $%02X%02X", sname, iaddr, op, a1, a2, distab[op].name, a2, a1 );
      if( distab[op].amode == AM_ABS ) break;
      strcat( disptr, distab[op].amode == AM_ABX ? ",X" : ",Y" );
      break;

    case AM_ZIX:
      a1 = mon_read( oric, (*paddr)++ );
      csym = mon_find_sym_by_addr( oric, a1 );
      if( csym )
        sprintf( disptr, "%s   %04X  %02X %02X     %s (%s,X)", sname, iaddr, op, a1, distab[op].name, filemode ? csym->name : csym->ssname );
      else
        sprintf( disptr, "%s   %04X  %02X %02X     %s ($%02X,X)", sname, iaddr, op, a1, distab[op].name, a1 );
      break;

    case AM_ZIY:
      a1 = mon_read( oric, (*paddr)++ );
      csym = mon_find_sym_by_addr( oric, a1 );
      if( csym )     
        sprintf( disptr, "%s   %04X  %02X %02X     %s (%s),Y", sname, iaddr, op, a1, distab[op].name, filemode ? csym->name : csym->ssname );
      else
        sprintf( disptr, "%s   %04X  %02X %02X     %s ($%02X),Y", sname, iaddr, op, a1, distab[op].name, a1 );
      break;

    case AM_REL:
      a1 = mon_read( oric, (*paddr)++ );
      addr = ((*paddr)+((signed char)a1))&0xffff;
      csym = mon_find_sym_by_addr( oric, addr );
      if( csym )
        sprintf( disptr, "%s   %04X  %02X %02X     %s %s", sname, iaddr, op, a1, distab[op].name, filemode ? csym->name : csym->sname );
      else
        sprintf( disptr, "%s   %04X  %02X %02X     %s $%04X", sname, iaddr, op, a1, distab[op].name, addr );
      break;

    case AM_IND:
      a1 = mon_read( oric, (*paddr)++ );
      a2 = mon_read( oric, (*paddr)++ );
      csym = mon_find_sym_by_addr( oric, (a2<<8)|a1 );
      if( csym )
        sprintf( disptr, "%s   %04X  %02X %02X %02X  %s (%s)", sname, iaddr, op, a1, a2, distab[op].name, filemode ? csym->name : csym->sname );
      else
        sprintf( disptr, "%s   %04X  %02X %02X %02X  %s ($%02X%02X)", sname, iaddr, op, a1, a2, distab[op].name, a2, a1 );
      break;
    
    default:
      strcpy( disptr, "  WTF?" );
      break;
  }

  for( i=strlen(disptr); i<47; i++ )
    disptr[i] = 32;
  disptr[i] = 0;

  oric->cpu.anybp = SDL_FALSE;
  for( i=0; i<16; i++ )
  {
    if( oric->cpu.breakpoints[i] != -1 )
    {
      oric->cpu.anybp = SDL_TRUE;
      if( iaddr == oric->cpu.breakpoints[i] )
        disptr[SNAME_LEN+1] = '*';
    }
  }

  if( iaddr == oric->cpu.pc )
    disptr[SNAME_LEN+2] = '>';

  if( ( looped ) && ( iaddr > *paddr ) )
    *looped = SDL_TRUE;

  return distmp;
}

static void mon_regmod( int x, int y, int w )
{
  int offs, i;

  offs = y*tz[TZ_REGS]->w+x;
  for( i=0; i<w; i++, offs++ )
  {
    tz[TZ_REGS]->fc[offs] = 1;
    tz[TZ_REGS]->bc[offs] = 8;
  }
}

void mon_update_regs( struct machine *oric )
{
  int i;
  unsigned short addr, pc;
  struct msym *csym;
  char stmp[48];

  pc = oric->cpu.pc;
  
  if( ( oric->cpu.irq ) && ( oric->cpu.f_i == 0 ) )
    pc = (mon_read( oric, 0xffff )<<8) | mon_read( oric, 0xfffe );

  tzprintfpos( tz[TZ_REGS], 2, 2, "PC=%04X  SP=01%02X  A=%02X  X=%02X  Y=%02X  LPC=%04X",
    pc,
    oric->cpu.sp,
    oric->cpu.a,
    oric->cpu.x,
    oric->cpu.y,
    oric->cpu.lastpc );

  csym = mon_find_sym_by_addr( oric, pc );
  if( csym )
  {
    if( strlen( csym->name ) > 46 )
    {
      strncpy( stmp, csym->name, 46 );
      stmp[46] = 0;
    } else {
      strcpy( stmp, csym->name );
    }
    tzstrpos( tz[TZ_REGS], 2, 3, stmp );
  } else {
    tzprintfpos( tz[TZ_REGS], 2, 3, "%46s", " " );
  }

  tzprintfpos( tz[TZ_REGS], 2, 4, "CY=%09d", oric->cpu.cycles );
  tzprintfpos( tz[TZ_REGS], 2, 5, "FM=%06d RS=%03d", oric->frames, oric->vid_raster );

  tzstrpos( tz[TZ_REGS], 35, 4, (oric->cpu.irq&IRQF_VIA)  ? "VIA"  : "   "  );
  tzstrpos( tz[TZ_REGS], 35, 5, (oric->cpu.irq&IRQF_DISK) ? "DISK" : "    " );


  tzstrpos( tz[TZ_REGS], 25, 4, "NV-BDIZC" );
  tzprintfpos( tz[TZ_REGS], 25, 5, "%01X%01X1%01X%01X%01X%01X%01X",
    oric->cpu.f_n,
    oric->cpu.f_v,
    oric->cpu.f_b,
    oric->cpu.f_d,
    oric->cpu.f_i,
    oric->cpu.f_z,
    oric->cpu.f_c );

  addr = pc;
  for( i=0; i<10; i++ )
  {
    tzsetcol( tz[TZ_REGS], (addr==oric->cpu.pc) ? 1 : 2, 3 );
    tzstrpos( tz[TZ_REGS], 23, 7+i, "        " );
    tzstrpos( tz[TZ_REGS],  2, 7+i, mon_disassemble( oric, &addr, NULL, SDL_FALSE ) );
  }

  if( cpu_oldvalid )
  {
    if( cpu_old.pc     != pc )               mon_regmod(  5, 2, 4 );
    if( cpu_old.sp     != oric->cpu.sp )     mon_regmod( 14, 2, 4 );
    if( cpu_old.a      != oric->cpu.a )      mon_regmod( 22, 2, 2 );
    if( cpu_old.x      != oric->cpu.x )      mon_regmod( 28, 2, 2 );
    if( cpu_old.y      != oric->cpu.y )      mon_regmod( 34, 2, 2 );
    if( cpu_old.cycles != oric->cpu.cycles ) mon_regmod(  5, 4, 9 );
    if( frames_old     != oric->frames )     mon_regmod(  5, 5, 6 );
    if( vidraster_old  != oric->vid_raster ) mon_regmod( 15, 5, 3 );
    if( cpu_old.f_n    != oric->cpu.f_n )    mon_regmod( 25, 5, 1 );
    if( cpu_old.f_v    != oric->cpu.f_v )    mon_regmod( 26, 5, 1 );
    if( cpu_old.f_b    != oric->cpu.f_b )    mon_regmod( 28, 5, 1 );
    if( cpu_old.f_d    != oric->cpu.f_d )    mon_regmod( 29, 5, 1 );
    if( cpu_old.f_i    != oric->cpu.f_i )    mon_regmod( 30, 5, 1 );
    if( cpu_old.f_z    != oric->cpu.f_z )    mon_regmod( 31, 5, 1 );
    if( cpu_old.f_c    != oric->cpu.f_c )    mon_regmod( 32, 5, 1 );

    if( (cpu_old.irq&IRQF_VIA)  != (oric->cpu.irq&IRQF_VIA) )  mon_regmod( 35, 4, 3 );
    if( (cpu_old.irq&IRQF_DISK) != (oric->cpu.irq&IRQF_DISK) ) mon_regmod( 35, 5, 4 );
  }
}

static void mon_viamod( int x, int y, int w, struct textzone *vtz )
{
  int offs, i;

  offs = y*vtz->w+x;
  for( i=0; i<w; i++, offs++ )
  {
    vtz->fc[offs] = 1;
    vtz->bc[offs] = 8;
  }
}

void mon_update_via( struct machine *oric, struct textzone *vtz, struct via *v, struct via *old, SDL_bool *oldvalid  )
{
  tzprintfpos( vtz, 2, 2, "PCR=%02X ACR=%02X SR=%02X",
    v->pcr,
    v->acr,
    v->sr );
  tzprintfpos( vtz, 2, 3, "IFR=%02X IER=%02X",
    v->ifr,
    v->ier );  
  tzprintfpos( vtz, 2, 5, "A=%02X DDRA=%02X  B=%02X DDRB=%02X",
    (v->ora&v->ddra)|(v->ira&(~v->ddra)),
    v->ddra,
    (v->orb&v->ddrb)|(v->irb&(~v->ddrb)),
    v->ddrb );
  tzprintfpos( vtz, 2, 6, "CA1=%01X CA2=%01X   CB1=%01X CB2=%01X",
    v->ca1,
    v->ca2,
    v->cb1,
    v->cb2 );
  tzprintfpos( vtz, 2, 8, "T1L=%02X%02X T1C=%04X",
    v->t1l_h,
    v->t1l_l,
    v->t1c );
  tzprintfpos( vtz, 2, 9, "T2L=%02X%02X T2C=%04X",
    v->t2l_h,
    v->t2l_l,
    v->t2c );

  if( v == &oric->via )
  {
    tzprintfpos( vtz, 2, 11, "TAPE OFFS = %07d", oric->tapeoffs );
    tzprintfpos( vtz, 2, 12, "TAPE LEN  = %07d", oric->tapelen );
    tzprintfpos( vtz, 2, 13, "COUNT     = %07d", oric->tapecount );
    tzprintfpos( vtz, 2, 14, "BIT = %02X  DATA = %1X",
      (oric->tapebit+9)%10,
      oric->tapetime == TAPE_1_PULSE );
    tzprintfpos( vtz, 2, 15, "MOTOR = %1X", oric->tapemotor );
  }

  if( v == &oric->tele_via )
  {
    tzprintfpos( vtz, 2, 11, "BANK = %02X", oric->tele_currbank );
  }

  if( *oldvalid )
  {
    if( old->pcr   != v->pcr  ) mon_viamod(  6, 2, 2, vtz );
    if( old->acr   != v->acr  ) mon_viamod( 13, 2, 2, vtz );
    if( old->sr    != v->sr   ) mon_viamod( 19, 2, 2, vtz );
    if( old->ifr   != v->ifr  ) mon_viamod(  6, 3, 2, vtz );
    if( old->ier   != v->ier  ) mon_viamod( 13, 3, 2, vtz );
    if( old->ddra  != v->ddra ) mon_viamod( 12, 5, 2, vtz );
    if( old->ddrb  != v->ddrb ) mon_viamod( 18, 5, 2, vtz );
    if( old->ca1   != v->ca1  ) mon_viamod(  6, 6, 1, vtz );
    if( old->ca2   != v->ca2  ) mon_viamod( 12, 6, 1, vtz );
    if( old->cb1   != v->cb1  ) mon_viamod( 20, 6, 1, vtz );
    if( old->cb2   != v->cb2  ) mon_viamod( 26, 6, 1, vtz );
    if( old->t1c   != v->t1c  ) mon_viamod( 15, 8, 4, vtz );
    if( old->t2c   != v->t2c  ) mon_viamod( 15, 9, 4, vtz );

    if( ((old->t1l_h<<8)|(old->t1l_l)) != ((v->t1l_h<<8)|(v->t1l_l)) ) mon_viamod( 6, 8, 4, vtz );
    if( ((old->t2l_h<<8)|(old->t2l_l)) != ((v->t2l_h<<8)|(v->t2l_l)) ) mon_viamod( 6, 9, 4, vtz );

    if( ((v->ora&v->ddra)|(v->ira&(~v->ddra))) != ((old->ora&old->ddra)|(old->ira&(~old->ddra))) ) mon_viamod(  4, 5, 2, vtz );
    if( ((v->orb&v->ddrb)|(v->irb&(~v->ddrb))) != ((old->orb&old->ddrb)|(old->irb&(~old->ddrb))) ) mon_viamod( 26, 5, 2, vtz );
  }
}

static void mon_aymod( int x, int y, int w )
{
  int offs, i;

  offs = y*tz[TZ_AY]->w+x;
  for( i=0; i<w; i++, offs++ )
  {
    tz[TZ_AY]->fc[offs] = 1;
    tz[TZ_AY]->bc[offs] = 8;
  }
}

static void printayregbits( struct machine *oric, char *str, Sint16 x, Sint16 y, int reg, Uint8 bits )
{
  int i, o, fc, bc;
  Uint8 vnew, vold;
  struct textzone *ptz = tz[TZ_AY];

  vnew = oric->ay.eregs[reg];
  vold = ay_old.eregs[reg];

  tzprintfpos( ptz, x, y, str, vnew );
  fc = 2;
  bc = 3;
  if( ( ay_oldvalid ) && ( ay_old.eregs[reg] != oric->ay.eregs[reg] ) )
  {
    fc = 1;
    bc = 8;
    mon_aymod( x+14, y, 2 );
  }

  x+=18;
  o = (y*ptz->w)+x;
  bits = 8-bits;

  for( i=0; i<8; i++ )
  {
    if( i >= bits )
      ptz->tx[o] = (vnew&(0x80>>i)) ? '1' : '0';
    else
      ptz->tx[o] = ' ';

    if( (vnew&(0x80)>>i) != (vold&(0x80)>>i) )
    {
      ptz->fc[o] = fc;
      ptz->bc[o] = bc;
    } else {
      ptz->fc[o] = 2;
      ptz->bc[o] = 3;
    }
    o++;
  }
}

void mon_update_ay( struct machine *oric )
{
  char *strs[] = { "INACTIVE",
                   "READ    ",
                   "WRITE   ",
                   "LATCH   " };
  tzprintfpos( tz[TZ_AY], 2, 1, "CTRL=%s  REG=%02X",
    strs[oric->ay.bmode&3],
    oric->ay.creg );

  if( ay_oldvalid )
  {
    if( (ay_old.bmode&3) != (oric->ay.bmode&3) ) mon_aymod( 7, 1, 8 );
    if( ay_old.creg != oric->ay.creg ) mon_aymod( 21, 1, 2 );
  }

  printayregbits( oric, "R0 Pitch A L $%02X \%", 2, 2, AY_CHA_PER_L, 8 );
  printayregbits( oric, "R1 Pitch A H $%02X \%", 2, 3, AY_CHA_PER_H, 4 );
  printayregbits( oric, "R2 Pitch B L $%02X \%", 2, 4, AY_CHB_PER_L, 8 );
  printayregbits( oric, "R3 Pitch B H $%02X \%", 2, 5, AY_CHB_PER_H, 4 );
  printayregbits( oric, "R4 Pitch C L $%02X \%", 2, 6, AY_CHC_PER_L, 8 );
  printayregbits( oric, "R5 Pitch C H $%02X \%", 2, 7, AY_CHC_PER_H, 4 );

  printayregbits( oric, "R6 Noise     $%02X \%", 2, 9, AY_NOISE_PER, 5 );
  printayregbits( oric, "R7 Status    $%02X \%", 2,10, AY_STATUS,    7 );

  printayregbits( oric, "R8 Volume A  $%02X \%", 2,12, AY_CHA_AMP,   5 );
  printayregbits( oric, "R9 Volume B  $%02X \%", 2,13, AY_CHB_AMP,   5 );
  printayregbits( oric, "RA Volume C  $%02X \%", 2,14, AY_CHC_AMP,   5 );

  printayregbits( oric, "RB Env Per L $%02X \%", 2,16, AY_ENV_PER_L, 8 );
  printayregbits( oric, "RC Env Per H $%02X \%", 2,17, AY_ENV_PER_H, 8 );
  printayregbits( oric, "RD Env Cycle $%02X \%", 2,18, AY_ENV_CYCLE, 4 );
  printayregbits( oric, "RE Key Col   $%02X \%", 2,19, AY_PORT_A,    8 );

/*
  tzprintfpos( tz[TZ_AY], 2, 4, "A:P=%04X A=%02X   N:PER=%02X",
    (oric->ay.regs[AY_CHA_PER_H]<<8)|(oric->ay.regs[AY_CHA_PER_L]),
    oric->ay.regs[AY_CHA_AMP],
    oric->ay.regs[AY_NOISE_PER] );
  tzprintfpos( tz[TZ_AY], 2, 5, "B:P=%04X A=%02X   E:PER=%04X",
    (oric->ay.regs[AY_CHB_PER_H]<<8)|(oric->ay.regs[AY_CHB_PER_L]),
    oric->ay.regs[AY_CHB_AMP],
    (oric->ay.regs[AY_ENV_PER_H]<<8)|(oric->ay.regs[AY_ENV_PER_L]) );
  tzprintfpos( tz[TZ_AY], 2, 6, "C:P=%04X A=%02X   E:CYC=%02X",
    (oric->ay.regs[AY_CHC_PER_H]<<8)|(oric->ay.regs[AY_CHC_PER_L]),
    oric->ay.regs[AY_CHC_AMP],
    oric->ay.regs[AY_ENV_CYCLE] );
  tzprintfpos( tz[TZ_AY], 2, 8, "STATUS=%02X",
    oric->ay.regs[AY_STATUS] );
  tzprintfpos( tz[TZ_AY], 2, 9, "PORT=%02X",
    oric->ay.regs[AY_PORT_A] );  
*/
}

/*
  Uint8             r_status;
  Uint8             cmd;
  Uint8             r_track;
  Uint8             r_sector;
  Uint8             r_data;
  Uint8             c_drive;
  Uint8             c_side;
  Uint8             c_track;
  SDL_bool          last_step_in;
  void            (*setintrq)(void *);
  void            (*clrintrq)(void *);
  void             *intrqarg;
  void            (*setdrq)(void *);
  void            (*clrdrq)(void *);
  void             *drqarg;
  struct diskimage *disk[MAX_DRIVES];
  int               currentop;
  int               delayedint;
*/
void mon_update_disk( struct machine *oric )
{
  char *opname;

  if( oric->drivetype == DRV_NONE )
    return;

  opname = "Unknown      ";
  switch( oric->wddisk.currentop )
  {
    case COP_NUFFINK:       opname = "Idle         "; break;
    case COP_READ_TRACK:    opname = "Read track   "; break;
    case COP_READ_SECTOR:   opname = "Read sector  "; break;
    case COP_READ_SECTORS:  opname = "Read sectors "; break;
    case COP_WRITE_TRACK:   opname = "Write track  "; break;
    case COP_WRITE_SECTOR:  opname = "Write sector "; break;
    case COP_WRITE_SECTORS: opname = "Write sectors"; break;
    case COP_READ_ADDRESS:  opname = "Read address "; break;
  }

  tzprintfpos( tz[TZ_DISK], 2, 2, "STATUS=%02X        ROMDIS=%1X", oric->wddisk.r_status, oric->romdis );
  tzprintfpos( tz[TZ_DISK], 2, 3, "DRIVE=%02X SIDE=%02X TRACK=%02X",
    oric->wddisk.c_drive,
    oric->wddisk.c_side,
    oric->wddisk.c_track );
  tzprintfpos( tz[TZ_DISK], 2, 4, "SECTR=%02X DATA=%02X",
    oric->wddisk.r_sector,
    oric->wddisk.r_data );
  tzprintfpos( tz[TZ_DISK], 2, 5, "OP=%s", opname );
  tzprintfpos( tz[TZ_DISK], 2, 6, "IRQC=%03d DRQC=%03d",
    oric->wddisk.delayedint,
    oric->wddisk.delayeddrq );

  switch( oric->drivetype )
  {
    case DRV_MICRODISC:
      tzprintfpos( tz[TZ_DISK], 2, 8, "MDSTAT=%02X INTRQ=%1X DRQ=%1X",
        oric->md.status,
        oric->md.intrq!=0,
        oric->md.drq!=0 );
      tzprintfpos( tz[TZ_DISK], 2, 9, "EPROM=%1X", oric->md.diskrom );
      break;
    
    case DRV_JASMIN:
      tzprintfpos( tz[TZ_DISK], 2, 8, "OVRAM=%s", oric->jasmin.olay ? "ON" : "OFF" );
      break;
  }
}

void mon_store_state( struct machine *oric )
{
  int k;

  for( k=0; k<65536; k++ )
  {
    if( isram( oric, k ) )
      mwatch_old[k] = mon_read( oric, k );
  }
  mwatch_oldvalid = SDL_TRUE;

  cpu_old = oric->cpu;
  vidraster_old = oric->vid_raster;
  frames_old = oric->frames;
  cpu_oldvalid = SDL_TRUE;

  ay_old = oric->ay;
  ay_oldvalid = SDL_TRUE;

  via_old = oric->via;
  via_oldvalid = SDL_TRUE;

  via2_old = oric->tele_via;
  via2_oldvalid = SDL_TRUE;
}

void mon_state_reset( struct machine *oric )
{
  mwatch_oldvalid = SDL_FALSE;
  cpu_oldvalid = SDL_FALSE;
  ay_oldvalid = SDL_FALSE;
  via_oldvalid = SDL_FALSE;
  via2_oldvalid = SDL_FALSE;
}

void mon_update_mwatch( struct machine *oric )
{
  unsigned short addr;
  int j,k,l;
  unsigned int v;
  
  if( mw_split )
  {
    tz[TZ_MEMWATCH]->tx[ 50] = mw_which==0 ? 14 : 5;
    tz[TZ_MEMWATCH]->tx[550] = mw_which==1 ? 14 : 5;
  } else {
    int offs = 10*tz[TZ_MEMWATCH]->w+1;
    for( k=0; k<48; k++ )
      tz[TZ_MEMWATCH]->tx[offs+k] = 32;
    tz[TZ_MEMWATCH]->tx[ 50] = 5;
    tz[TZ_MEMWATCH]->tx[550] = 5;
    
  }

  addr = mw_addr[0];
  for( j=0; j<19; j++, addr+=8 )
  {
    if( ( mw_split ) && ( j == 9 ) )
    {
      int offs = 10*tz[TZ_MEMWATCH]->w+1;
      for( k=0; k<48; k++ )
      {
        tz[TZ_MEMWATCH]->tx[offs+k] = 2;
        tz[TZ_MEMWATCH]->bc[offs+k] = 3;
        tz[TZ_MEMWATCH]->fc[offs+k] = 2;
      }
      j++;
      addr = mw_addr[1];
    }
        
    sprintf( vsptmp, "%04X  ", addr );
    for( k=0; k<8; k++ )
    {
      sprintf( &vsptmp[128], "%02X ", mon_read( oric, addr+k ) );
      strcat( vsptmp, &vsptmp[128] );
    }
    l = strlen( vsptmp );
    vsptmp[l++] = 32;
    vsptmp[l++] = '\'';
    for( k=0; k<8; k++ )
    {
      v = mon_read( oric, addr+k );
      vsptmp[l++] = ((v>31)&&(v<128))?v:'.';
    }
    vsptmp[l++] = '\'';
    vsptmp[l++] = 0;
    tzstrpos( tz[TZ_MEMWATCH], 1, j+1, vsptmp );
    
    if( mwatch_oldvalid )
    {
      for( k=0; k<8; k++ )
      {
        if( isram( oric, addr+k ) && ( mwatch_old[addr+k] != mon_read( oric, addr+k ) ) )
        {
          int offs = (j+1)*tz[TZ_MEMWATCH]->w;
          tz[TZ_MEMWATCH]->fc[offs+(k*3+7)] = 1;
          tz[TZ_MEMWATCH]->fc[offs+(k*3+8)] = 1;
          tz[TZ_MEMWATCH]->fc[offs+(k+33)]  = 1;
          tz[TZ_MEMWATCH]->bc[offs+(k*3+7)] = 8;
          tz[TZ_MEMWATCH]->bc[offs+(k*3+8)] = 8;
          tz[TZ_MEMWATCH]->bc[offs+(k+33)]  = 8;
        }
      }
    }
  }
  
  if( mw_mode == 0 ) return;

  makebox( tz[TZ_MEMWATCH], 17, 8, 7, 3, 2, 3 );
  tzstrpos( tz[TZ_MEMWATCH], 18, 9, mw_ibuf );
}

void mon_update( struct machine *oric )
{
  mon_update_regs( oric );
  switch( mshow )
  {
    case MSHOW_VIA:
      mon_update_via( oric, tz[TZ_VIA], &oric->via, &via_old, &via_oldvalid );
      break;
    
    case MSHOW_VIA2:
      mon_update_via( oric, tz[TZ_VIA2], &oric->tele_via, &via2_old, &via2_oldvalid );
      break;
    
    case MSHOW_AY:
      mon_update_ay( oric );
      break;
    
    case MSHOW_DISK:
      mon_update_disk( oric );
      break;
  }

  switch( cshow )
  {
    case CSHOW_MWATCH:
      mon_update_mwatch( oric );
      break;
  }
}

void mon_render( struct machine *oric )
{
  oric->render_video( oric, SDL_FALSE );
  
  switch( mshow )
  {
    case MSHOW_VIA:
      oric->render_textzone( oric, TZ_VIA );
      break;
    
    case MSHOW_VIA2:
      oric->render_textzone( oric, TZ_VIA2 );
      break;
    
    case MSHOW_AY:
      oric->render_textzone( oric, TZ_AY );
      break;
    
    case MSHOW_DISK:
      oric->render_textzone( oric, TZ_DISK );
      break;
  }

  switch( cshow )
  {
    case CSHOW_CONSOLE:
      oric->render_textzone( oric, TZ_MONITOR );
      break;
    
    case CSHOW_DEBUG:
      oric->render_textzone( oric, TZ_DEBUG );
      break;

    case CSHOW_MWATCH:
      oric->render_textzone( oric, TZ_MEMWATCH );
      break;
  }
  oric->render_textzone( oric, TZ_REGS );
}

void mon_hide_curs( void )
{
  struct textzone *ptz = tz[TZ_MONITOR];
  int x = cursx-iloff;

  if( mon_asmmode )
  {
    ptz->fc[ptz->w*19+8+x] = 2;
    ptz->bc[ptz->w*19+8+x] = 3;
    return;
  }

  if( ( x < 0 ) || ( x > CONS_WIDTH ) ) return;
  ptz->fc[ptz->w*19+2+x] = 2;
  ptz->bc[ptz->w*19+2+x] = 3;
}

void mon_show_curs( void )
{
  struct textzone *ptz = tz[TZ_MONITOR];
  int x = cursx-iloff;

  if( mon_asmmode )
  {
    ptz->fc[ptz->w*19+8+x] = 3;
    ptz->bc[ptz->w*19+8+x] = 2;
    return;
  }

  if( ( x < 0 ) || ( x > CONS_WIDTH ) ) return;
  ptz->fc[ptz->w*19+2+x] = 3;
  ptz->bc[ptz->w*19+2+x] = 2;
}

void mon_scroll( SDL_bool above )
{
  int x, y, s, d, h;
  struct textzone *ptz = tz[TZ_MONITOR];

  h = above ? 17 : 18;

  s = ptz->w*2+1;
  d = ptz->w+1;
  for( y=0; y<h; y++ )
  {
    for( x=0; x<48; x++ )
    {
      ptz->tx[d] = ptz->tx[s];
      ptz->fc[d] = ptz->fc[s];
      ptz->bc[d++] = ptz->bc[s++];
    }
    d += 2;
    s += 2;
  }

  for( x=0; x<48; x++ )
  {
    ptz->tx[d] = ' ';
    ptz->fc[d] = 2;
    ptz->bc[d++] = 3;
  }
}

void mon_oprintf( char *fmt, ... )
{
  va_list ap;
  int i;
  char stmp[48];

  va_start( ap, fmt );
  if( vsnprintf( vsptmp, VSPTMPSIZE, fmt, ap ) != -1 )
  {
    vsptmp[VSPTMPSIZE-1] = 0;
    i = 0;
    while( strlen( &vsptmp[i] ) > 47 )
    {
      strncpy( stmp, &vsptmp[i], 47 );
      stmp[47] = 0;
      tzstrpos( tz[TZ_MONITOR], 1, 19, stmp );
      mon_scroll( SDL_FALSE );
      i += 47;
    }
    tzstrpos( tz[TZ_MONITOR], 1, 19, &vsptmp[i] );
  }
  va_end( ap );
}

void mon_printf( char *fmt, ... )
{
  va_list ap;
  int i;
  char stmp[48];

  mon_scroll( SDL_FALSE );

  va_start( ap, fmt );
  if( vsnprintf( vsptmp, VSPTMPSIZE, fmt, ap ) != -1 )
  {
    vsptmp[VSPTMPSIZE-1] = 0;
    i = 0;
    while( strlen( &vsptmp[i] ) > 46 )
    {
      strncpy( stmp, &vsptmp[i], 46 );
      stmp[46] = 0;
      tzstrpos( tz[TZ_MONITOR], 1, 19, stmp );
      mon_scroll( SDL_FALSE );
      i += 46;
    }
    tzstrpos( tz[TZ_MONITOR], 1, 19, &vsptmp[i] );
  }
  va_end( ap );
}

void mon_printf_above( char *fmt, ... )
{
  va_list ap;
  int i;
  char stmp[48];

  mon_scroll( SDL_TRUE );

  va_start( ap, fmt );
  if( vsnprintf( vsptmp, VSPTMPSIZE, fmt, ap ) != -1 )
  {
    vsptmp[VSPTMPSIZE-1] = 0;
    i = 0;
    while( strlen( &vsptmp[i] ) > 46 )
    {
      strncpy( stmp, &vsptmp[i], 46 );
      stmp[46] = 0;
      tzstrpos( tz[TZ_MONITOR], 1, 18, stmp );
      mon_scroll( SDL_TRUE );
      i += 46;
    }
    tzstrpos( tz[TZ_MONITOR], 1, 18, &vsptmp[i] );
  }
  va_end( ap );
}

void mon_str( char *str )
{
  mon_scroll( SDL_FALSE );
  tzstrpos( tz[TZ_MONITOR], 1, 19, str );
}

void mon_str_above( char *str )
{
  mon_scroll( SDL_TRUE );
  tzstrpos( tz[TZ_MONITOR], 1, 18, str );
}

void mon_start_input( void )
{
  mon_scroll( SDL_FALSE );
  ibuf[0] = 0;
  ilen = 0;
  iloff = 0;
  cursx = 0;

  if( mon_asmmode )
  {
    tzsetcol( tz[TZ_MONITOR], 1, 3 );
    tzprintfpos( tz[TZ_MONITOR], 1, 19, " %04X:", mon_addr );
    return;
  }
  tzsetcol( tz[TZ_MONITOR], 1, 3 );
  tzstrpos( tz[TZ_MONITOR], 1, 19, "]" ); 
}

void mon_enter( struct machine *oric )
{
  defaultsyms.syms     = NULL;
  defaultsyms.numsyms  = 0;
  defaultsyms.symspace = -1; // Not a dynamic symbol table
  switch( oric->type )
  {
    case MACH_ORIC1:
    case MACH_ORIC1_16K:
    case MACH_ATMOS:
      defaultsyms.syms    = defsym_atmos;
      defaultsyms.numsyms = sizeof( defsym_atmos ) / sizeof( struct msym );
      break;
    
    case MACH_TELESTRAT:
      defaultsyms.syms    = defsym_tele;
      defaultsyms.numsyms = sizeof( defsym_tele ) / sizeof( struct msym );
      break;
  }

  if( mon_bpmsg[0] )
  {
    mon_printf_above( mon_bpmsg );
    mon_bpmsg[0] = 0;
  }
}

void mon_init( struct machine *oric )
{
  defaultsyms.numsyms = 0;
  defaultsyms.symspace = 0;
  defaultsyms.syms = NULL;
  usersyms.numsyms = 0;
  usersyms.symspace = 0;
  usersyms.syms = NULL;

  mon_bpmsg[0] = 0;
  mshow = MSHOW_VIA;
  cshow = CSHOW_CONSOLE;
  mon_asmmode = SDL_FALSE;
  mon_start_input();
  mon_show_curs();
  mon_addr = oric->cpu.pc;
  lastcmd = 0;
  mw_split = SDL_FALSE;
  mw_which = 0;
  mwatch_oldvalid = SDL_FALSE;
  cpu_oldvalid = SDL_FALSE;
  ay_oldvalid = SDL_FALSE;
  via_oldvalid = SDL_FALSE;
#if LOG_DEBUG
  debug_logfile = fopen( debug_logname, "w" );
#endif
}

void mon_shut( void )
{
  mon_freesyms( &usersyms );
#if LOG_DEBUG
  if( debug_logfile ) fclose( debug_logfile );
  debug_logfile = NULL;
#endif
}

int mon_getreg( char *buf, int *off, SDL_bool addrregs, SDL_bool nregs, SDL_bool viaregs )
{
  int i;

  i = *off;

  while( isws( buf[i] ) ) i++;

  if( viaregs )
  {
    if( strncasecmp( &buf[i], "vpcr", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_PCR;
    }

    if( strncasecmp( &buf[i], "vacr", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_ACR;
    }

    if( strncasecmp( &buf[i], "vsr", 3 ) == 0 )
    {
      i+=3;
      *off = i;
      return REG_VIA_SR;
    }

    if( strncasecmp( &buf[i], "vifr", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_IFR;
    }

    if( strncasecmp( &buf[i], "vier", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_IER;
    }

    if( strncasecmp( &buf[i], "vira", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_IRA;
    }

    if( strncasecmp( &buf[i], "vora", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_ORA;
    }

    if( strncasecmp( &buf[i], "vddra", 5 ) == 0 )
    {
      i+=5;
      *off = i;
      return REG_VIA_DDRA;
    }

    if( strncasecmp( &buf[i], "virb", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_IRB;
    }

    if( strncasecmp( &buf[i], "vorb", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_ORB;
    }

    if( strncasecmp( &buf[i], "vddrb", 5 ) == 0 )
    {
      i+=5;
      *off = i;
      return REG_VIA_DDRB;
    }

    if( strncasecmp( &buf[i], "vt1l", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_T1L;
    }

    if( strncasecmp( &buf[i], "vt1c", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_T1C;
    }

    if( strncasecmp( &buf[i], "vt2l", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_T2L;
    }

    if( strncasecmp( &buf[i], "vt2c", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_T2C;
    }

    if( strncasecmp( &buf[i], "vca1", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_CA1;
    }

    if( strncasecmp( &buf[i], "vca2", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_CA2;
    }

    if( strncasecmp( &buf[i], "vcb1", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_CB1;
    }

    if( strncasecmp( &buf[i], "vcb2", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_CA2;
    }
  }

  if( addrregs )
  {
    if( strncasecmp( &buf[i], "pc", 2 ) == 0 )
    {
      i+=2;
      *off = i;
      return REG_PC;
    }

    if( strncasecmp( &buf[i], "sp", 2 ) == 0 )
    {
      i+=2;
      *off = i;
      return REG_SP;
    }
  }

  if( nregs )
  {
    switch( buf[i] )
    {
      case 'a':
      case 'A':
        i++;
        *off = i;
        return REG_A;

      case 'x':
      case 'X':
        i++;
        *off = i;
        return REG_X;

      case 'y':
      case 'Y':
        i++;
        *off = i;
        return REG_Y;
    }
  }

  return -1;
}

SDL_bool mon_getnum( struct machine *oric, unsigned int *num, char *buf, int *off, SDL_bool addrregs, SDL_bool nregs, SDL_bool viaregs, SDL_bool symbols )
{
  int i, j;
  char c;
  unsigned int v;

  i = *off;
  while( isws( buf[i] ) ) i++;

  if( ( symbols ) && ( issymstart( buf[i] ) ) )
  {
    struct msym *fsym;

    for( j=0; issymchar( buf[i+j] ); j++ ) ;
    c = buf[i+j];
    if( isws( c ) || ( c == 13 ) || ( c == 10 ) || ( c == 0 ) || ( c == ',' ) || ( c == ')' ) )
    {
      buf[i+j] = 0;
      fsym = mon_find_sym_by_name( oric, &buf[i] );
      buf[i+j] = c;
      if( fsym )
      {
        (*num) = fsym->addr;
        (*off) = i+j;
        return SDL_TRUE;
      }
    }
  }

  j = mon_getreg( buf, off, addrregs, nregs, viaregs );
  if( j != -1 )
  {
    switch( j )
    {
      case REG_A:
        *num = oric->cpu.a;
        return SDL_TRUE;
      case REG_X:
        *num = oric->cpu.x;
        return SDL_TRUE;
      case REG_Y:
        *num = oric->cpu.y;
        return SDL_TRUE;
      case REG_PC:
        *num = oric->cpu.pc;
        return SDL_TRUE;
      case REG_SP:
        *num = oric->cpu.sp|0x100;
        return SDL_TRUE;
      case REG_VIA_PCR:
        *num = oric->via.pcr;
        return SDL_TRUE;
      case REG_VIA_ACR:
        *num = oric->via.acr;
        return SDL_TRUE;
      case REG_VIA_SR:
        *num = oric->via.sr;
        return SDL_TRUE;
      case REG_VIA_IFR:
        *num = oric->via.ifr;
        return SDL_TRUE;
      case REG_VIA_IER:
        *num = oric->via.ier;
        return SDL_TRUE;
      case REG_VIA_IRA:
        *num = oric->via.ira;
        return SDL_TRUE;
      case REG_VIA_ORA:
        *num = oric->via.ora;
        return SDL_TRUE;
      case REG_VIA_DDRA:
        *num = oric->via.ddra;
        return SDL_TRUE;
      case REG_VIA_IRB:
        *num = oric->via.irb;
        return SDL_TRUE;
      case REG_VIA_ORB:
        *num = oric->via.orb;
        return SDL_TRUE;
      case REG_VIA_DDRB:
        *num = oric->via.ddrb;
        return SDL_TRUE;
      case REG_VIA_T1L:
        *num = (oric->via.t1l_h<<8)|oric->via.t1l_l;
        return SDL_TRUE;
      case REG_VIA_T1C:
        *num = oric->via.t1c;
        return SDL_TRUE;
      case REG_VIA_T2L:
        *num = (oric->via.t2l_h<<8)|oric->via.t2l_l;
        return SDL_TRUE;
      case REG_VIA_T2C:
        *num = oric->via.t2c;
        return SDL_TRUE;
      case REG_VIA_CA1:
        *num = oric->via.ca1;
        return SDL_TRUE;
      case REG_VIA_CA2:
        *num = oric->via.ca2;
        return SDL_TRUE;
      case REG_VIA_CB1:
        *num = oric->via.cb1;
        return SDL_TRUE;
      case REG_VIA_CB2:
        *num = oric->via.cb2;
        return SDL_TRUE;
    }
  }

  if( buf[i] == '%' )
  {
    // Binary
    i++;
    if( !isbin( buf[i] ) ) return SDL_FALSE;

    v = 0;
    while( isbin( buf[i] ) )
    {
      v = (v<<1) | (buf[i]-'0');
      i++;
    }

    *num = v;
    *off = i;
    return SDL_TRUE;
  }

  if( buf[i] == '$' )
  {
    // Hex
    i++;
    if( !ishex( buf[i] ) ) return SDL_FALSE;

    v = 0;
    for( ;; )
    {
      j = hexit( buf[i] );
      if( j == -1 ) break;
      v = (v<<4) | j;
      i++;
    }

    *num = v;
    *off = i;
    return SDL_TRUE;
  }

  if( !isnum( buf[i] ) ) return SDL_FALSE;

  v = 0;
  while( isnum( buf[i] ) )
  {
    v = (v*10) + (buf[i]-'0');
    i++;
  }

  *num = v;
  *off = i;
  return SDL_TRUE;    
}

Uint16 mon_sym_best_guess( struct machine *oric, Uint16 addr, char *symname, SDL_bool use_emu_state )
{
  // In the ROM area?
  if( addr >= 0xc000 )
  {
    // In a telestrat bank?
    if( oric->type == MACH_TELESTRAT )
    {
      // Labels starting t0_ are assumed to be telestrat bank 0 (etc.)
      if( ( symname[0] == 't' ) && 
          ( ( symname[1] >= '0' ) && ( symname[1] <= '7' ) ) &&
          ( symname[2] == '_' ) )
        return SYMF_TELEBANK0 << (symname[1]-'0');

      // No hint from the label name. Can we attempt to use the current telestrat bank?
      if( use_emu_state )
        return SYMF_TELEBANK0 << oric->tele_currbank;

      // Just make it valid in any telestrat bank
      return SYMF_TELEBANK0|SYMF_TELEBANK1|SYMF_TELEBANK2|SYMF_TELEBANK3|SYMF_TELEBANK4|SYMF_TELEBANK5|SYMF_TELEBANK6|SYMF_TELEBANK7;
    }

    // Its not a telestrat, so its ROM or overlay
    if( use_emu_state )
    {
      int romdis = oric->romdis;
      if( ( oric->drivetype == DRV_JASMIN ) && ( oric->jasmin.olay ) )
        romdis = 1;

      return romdis ? SYMF_ROMDIS1 : SYMF_ROMDIS0;
    }

    return SYMF_ROMDIS1;
  }

  // Normal memory
  return 0;
}

struct msym *mon_replace_or_add_symbol( struct symboltable *stab, struct machine *oric, char *symname, unsigned short flags, Uint16 addr )
{
  int i;
  char symtmp[160];
  struct msym *retval;

  for( i=0; issymchar(symname[i]); i++ )
    symtmp[i] = symname[i];
  symtmp[i] = 0;

  // Is it a dynamic symbol table?
  if( stab->symspace == -1 ) return SDL_FALSE;

  if( flags == SYM_BESTGUESS )
    flags = mon_sym_best_guess( oric, addr, symtmp, SDL_TRUE );

  for( i=0; i<stab->numsyms; i++ )
  {
    if( !oric->symbolscase )
    {
      if( strcasecmp( symtmp, stab->syms[i].name ) == 0 )
        break;
    } else {
      if( strcmp( symtmp, stab->syms[i].name ) == 0 )
        break;
    }
  }

  // Replace existing symbol?
  if( i < stab->numsyms )
  {
    stab->syms[i].addr  = addr;
    stab->syms[i].flags = flags;
    return &stab->syms[i];
  }

  mon_addsym( stab, addr, flags, symtmp, &retval );
  return retval;
}



SDL_bool mon_new_symbols( struct symboltable *stab, struct machine *oric, char *fname, unsigned short flags, SDL_bool above, SDL_bool verbose )
{
  FILE *f;
  int i, j;
  unsigned int v;

  f = fopen( fname, "r" );
  if( !f )
  {
    if( verbose )
    {
      if( above )
        mon_printf_above( "Unable to open '%s'", fname );
      else
        mon_printf( "Unable to open '%s'", fname );
    }
    return SDL_FALSE;
  }

  stab->numsyms = 0;
  while( !feof( f ) )
  {
    char linetmp[256];

    if( !fgets( linetmp, 256, f ) ) break;

    if( sscanf( linetmp, "al %06X .%s", &v, linetmp ) != 2 )
    {
        for( i=0, v=0; i<4; i++ )
        {
          j = hexit( linetmp[i] );
          if( j == -1 ) break;
          v = (v<<4)|j;
        }
        if( i < 4 ) continue;
        if( linetmp[4] != 32 ) continue;
        if( !issymstart( linetmp[5] ) ) continue;

        for( i=5,j=0; issymchar(linetmp[i]); i++,j++ )
          linetmp[j] = linetmp[i];
        linetmp[j] = 0;
    }

//    printf( "'%s' = %04X\n", linetmp, v );
    fflush( stdout );

    // Guess the flags!
    if( flags == SYM_BESTGUESS )
    {
      mon_addsym( stab, v, mon_sym_best_guess( oric, v, linetmp, SDL_FALSE ), linetmp, NULL );
    } else {
      mon_addsym( stab, v, flags, linetmp, NULL );
    }
  }

  fclose( f );
  
  if( verbose )
  {
    if( above )
      mon_printf_above( "Symbols loaded from '%s'", fname );
    else
      mon_printf( "Symbols loaded from '%s'", fname );
  }
  return SDL_TRUE;
}

SDL_bool mon_cmd( char *cmd, struct machine *oric, SDL_bool *needrender )
{
  int i, j, k, l;
  unsigned int v, w;
  SDL_bool done = SDL_FALSE;
  unsigned char *tmem;
  struct msym *tmpsym;
  FILE *f;

  i=0;
  while( isws( cmd[i] ) ) i++;
  
  if( cmd[i] != '?' )
    helpcount = 0;

  switch( cmd[i] )
  {
    case 'a':
      lastcmd = cmd[i];
      i++;

      if( mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_FALSE, SDL_FALSE, SDL_TRUE ) )
        mon_addr = v;

      mon_asmmode = SDL_TRUE;
      break;

    case 's': // Symbols
      lastcmd = 0;
      i++;
      switch( cmd[i] )
      {
        case 'c':  // Case insensitive
          oric->symbolscase = SDL_FALSE;
          mon_str( "Symbols are not case-sensitive" );
          break;

        case 'C':  // Case sensitive
          oric->symbolscase = SDL_TRUE;
          mon_str( "Symbols are case-sensitive" );
          break;
        
        case 'a':  // add
          if( !isws( cmd[i+1] ) )
          {
            mon_str( "???" );
            break;
          }

          i+=2;
          while( isws( cmd[i] ) ) i++;

          if( !issymstart( cmd[i] ) )
          {
            mon_str( "Expected symbol name and address" );
            break;
          }

          j = i;
          while( issymchar( cmd[i] ) ) i++;

          if( !isws( cmd[i] ) )
          {
            mon_str( "Expected symbol name and address" );
            break;
          }

          if( !mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_FALSE, SDL_FALSE, SDL_TRUE ) )
          {
            mon_str( "Address expected" );
            break;
          }

          tmpsym = mon_replace_or_add_symbol( &usersyms, oric, &cmd[j], SYM_BESTGUESS, v );
          if( !tmpsym )
          {
            mon_str( "Failed for some reason" );
            break;
          }

          mon_printf( "%s = %04X", tmpsym->name, tmpsym->addr );
          break;
        
        case 'k':  // kill
          if( !isws( cmd[i+1] ) )
          {
            mon_str( "???" );
            break;
          }

          i+=2;
          while( isws( cmd[i] ) ) i++;

          if( !issymstart( cmd[i] ) )
          {
            mon_str( "Symbol expected" );
            break;
          }

          j = i;
          while( issymchar( cmd[i] ) ) i++;

          if( cmd[i] != 0 )
          {
            mon_printf( "Expected end of line at '%s'", &cmd[i] );
            break;
          }

          if( !mon_tab_find_sym_by_name( &usersyms, oric, &cmd[j], &k ) )
          {
            mon_printf( "Couldn't find '%s' in the user symbol table", &cmd[j] );
            break;
          }

          for( ; k<(usersyms.numsyms-1); k++ )
            usersyms.syms[k] = usersyms.syms[k+1];
          usersyms.numsyms--;
          mon_printf( "Killed symbol '%s'", &cmd[j] );
          break;

        case 'z':  // Zap
          usersyms.numsyms = 0;
          defaultsyms.syms = NULL;
          defaultsyms.numsyms = 0;
          mon_str( "Symbols zapped!" );
          break;

        case 'l':  // Load
          if( !isws( cmd[i+1] ) )
          {
            mon_str( "???" );
            break;
          }

          i+=2;

          mon_new_symbols( &usersyms, oric, &cmd[i], SYM_BESTGUESS, SDL_FALSE, SDL_TRUE );
          break;
        
        default:
          mon_str( "???" );
          break;
      }
      break;

    case 'b':
      lastcmd = 0;
      i++;
      switch( cmd[i] )
      {
        case 'l':
          if( cmd[i+1] == 'm' )
          {
            oric->cpu.anymbp = SDL_FALSE;
            for( j=0; j<16; j++ )
            {
              if( oric->cpu.membreakpoints[j].flags )
              {
                mon_printf( "%02d: $%04X %c%c%c",
                  j,
                  oric->cpu.membreakpoints[j].addr,
                  (oric->cpu.membreakpoints[j].flags&MBPF_READ) ? 'r' : ' ',
                  (oric->cpu.membreakpoints[j].flags&MBPF_WRITE) ? 'w' : ' ',
                  (oric->cpu.membreakpoints[j].flags&MBPF_CHANGE) ? 'c' : ' ' );
                oric->cpu.anymbp = SDL_TRUE;
              }
            }
            break;
          }

          oric->cpu.anybp = SDL_FALSE;
          for( j=0; j<16; j++ )
          {
            if( oric->cpu.breakpoints[j] != -1 )
            {
              mon_printf( "%02d: $%04X", j, oric->cpu.breakpoints[j] );
              oric->cpu.anybp = SDL_TRUE;
            }
          }
          break;
        
        case 's':
          if( cmd[i+1] == 'm' )
          {
            for( j=0; j<16; j++ )
            {
              if( oric->cpu.membreakpoints[j].flags == 0 )
                break;
            }

            if( j == 16 )
            {
              mon_str( "Max 16 breakpoints" );
              break;
            }

            i += 2;
            if( !mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_TRUE, SDL_TRUE, SDL_TRUE ) )
            {
              mon_str( "Address expected" );
              break;
            }

            oric->cpu.membreakpoints[j].addr = v & 0xffff;
            oric->cpu.membreakpoints[j].lastval = mon_read( oric, v&0xffff );
            oric->cpu.membreakpoints[j].flags = 0;

            while( isws( cmd[i] ) ) i++;

            for( ;; )
            {
              switch( cmd[i] )
              {
                case 'r':
                  oric->cpu.membreakpoints[j].flags |= MBPF_READ;
                  i++;
                  continue;

                case 'w':
                  oric->cpu.membreakpoints[j].flags |= MBPF_WRITE;
                  i++;
                  continue;

                case 'c':
                  oric->cpu.membreakpoints[j].flags |= MBPF_CHANGE;
                  i++;
                  continue;
              }
              break;
            }

            if( !oric->cpu.membreakpoints[j].flags )
              oric->cpu.membreakpoints[j].flags = MBPF_READ|MBPF_WRITE;
            oric->cpu.anymbp = SDL_TRUE;

            mon_printf( "m%02d: $%04X %s%s%s", j, oric->cpu.membreakpoints[j].addr,
              (oric->cpu.membreakpoints[j].flags&MBPF_READ) ? "r" : "",
              (oric->cpu.membreakpoints[j].flags&MBPF_WRITE) ? "w" : "",
              (oric->cpu.membreakpoints[j].flags&MBPF_CHANGE) ? "c" : "" );
            break;
          }

          for( j=0; j<16; j++ )
          {
            if( oric->cpu.breakpoints[j] == -1 )
              break;
          }

          if( j == 16 )
          {
            mon_str( "Max 16 breakpoints" );
            break;
          }

          i++;
          if( !mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_TRUE, SDL_TRUE, SDL_TRUE ) )
          {
            mon_str( "Address expected" );
            break;
          }

          oric->cpu.breakpoints[j] = v & 0xffff;
          oric->cpu.anybp = SDL_TRUE;
          mon_printf( "%02d: $%04X", j, oric->cpu.breakpoints[j] );
          break;

        case 'c':
          j = 0;
          if( cmd[i+1] == 'm' )
          {
            j = 1;
            i++;
          }

          i++;
          if( !mon_getnum( oric, &v, cmd, &i, SDL_FALSE, SDL_FALSE, SDL_FALSE, SDL_FALSE ) )
          {
            mon_str( "Breakpoint ID expected" );
            break;
          }

          if( v > 15 )
          {
            mon_str( "Invalid breakpoint ID" );
            break;
          }

          if( j )
          {
            oric->cpu.membreakpoints[v].flags = 0;
            oric->cpu.anymbp = SDL_FALSE;
            for( j=0; j<16; j++ )
            {
              if( oric->cpu.membreakpoints[j].flags != 0 )
              {
                oric->cpu.anymbp = SDL_TRUE;
                break;
              }
            }
            break;
          }

          oric->cpu.breakpoints[v] = -1;
          oric->cpu.anybp = SDL_FALSE;
          for( j=0; j<16; j++ )
          {
            if( oric->cpu.breakpoints[j] != -1 )
            {
              oric->cpu.anybp = SDL_TRUE;
              break;
            }
          }
          break;
        
        case 'z':
          if( cmd[i+1] == 'm' )
          {
            for( i=0; i<16; i++ )
              oric->cpu.membreakpoints[i].flags = 0;
            oric->cpu.anymbp = SDL_FALSE;
            break;
          }

          for( i=0; i<16; i++ )
            oric->cpu.breakpoints[i] = -1;
          oric->cpu.anybp = SDL_FALSE;
          break;
        
        default:
          mon_str( "???" );
          break;
      }
      break;

    case 'm':
      lastcmd = cmd[i];
      i++;
      
      if( cmd[i] == 'w' )
      {
        lastcmd = 0;
        i++;

        if( !mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_FALSE, SDL_FALSE, SDL_TRUE ) )
        {
          mon_str( "Bad address" );
          break;
        }
        
        if( mw_split )
        {
          mw_addr[mw_which] = v;
        } else {
          mw_which = 0;
          mw_addr[0] = v;
        }
        cshow = CSHOW_MWATCH;
        break;
      }
      
      if( mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_FALSE, SDL_FALSE, SDL_TRUE ) )
        mon_addr = v;

      for( j=0; j<16; j++ )
      {
        sprintf( vsptmp, "%04X  ", mon_addr );
        for( k=0; k<8; k++ )
        {
          sprintf( &vsptmp[128], "%02X ", mon_read( oric, mon_addr+k ) );
          strcat( vsptmp, &vsptmp[128] );
        }
        l = strlen( vsptmp );
        vsptmp[l++] = 32;
        vsptmp[l++] = '\'';
        for( k=0; k<8; k++ )
        {
          v = mon_read( oric, mon_addr++ );
          vsptmp[l++] = ((v>31)&&(v<128))?v:'.';
        }
        vsptmp[l++] = '\'';
        vsptmp[l++] = 0;
        mon_str( vsptmp );
      }
      break;
    
    case 'd':
      if( cmd[i+1] == 'f' )
      {
        i+=2;

        if( !mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_FALSE, SDL_FALSE, SDL_TRUE ) )
        {
          mon_str( "Start address expected\n" );
          break;
        }

        mon_addr = v;
        if( !mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_FALSE, SDL_FALSE, SDL_TRUE ) )
        {
          mon_str( "End address expected\n" );
          break;
        }

        while( isws( cmd[i] ) ) i++;
        if( !cmd[i] )
        {
          mon_str( "Filename expected" );
          break;
        }

        f = fopen( &cmd[i], "w" );
        if( !f )
        {
          mon_printf( "Unable to open '%s'", &cmd[i] );
          break;
        }

        while( mon_addr <= v )
        {
          SDL_bool looped;
          fprintf( f, "%s\n", mon_disassemble( oric, &mon_addr, &looped, SDL_TRUE ) );
          if( looped ) break;
        }

        fclose( f );
        break;
      }

      lastcmd = cmd[i];
      i++;
      if( mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_FALSE, SDL_FALSE, SDL_TRUE ) )
        mon_addr = v;

      for( j=0; j<16; j++ )
        mon_str( mon_disassemble( oric, &mon_addr, NULL, SDL_FALSE ) );
      break;

    case 'r':
      lastcmd = 0;
      i++;

      j = mon_getreg( cmd, &i, SDL_TRUE, SDL_TRUE, SDL_TRUE );
      if( j == -1 )
      {
        mon_str( "Register expected" );
        break;
      }
      
      if( !mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_TRUE, SDL_TRUE, SDL_TRUE ) )
      {
        mon_str( "Expression expected" );
        break;
      }

      switch( j )
      {
        case REG_PC:
          oric->cpu.pc = v;
          break;

        case REG_SP:
          oric->cpu.sp = v;
          break;

        case REG_A:
          oric->cpu.a = v;
          break;

        case REG_X:
          oric->cpu.x = v;
          break;

        case REG_Y:
          oric->cpu.y = v;
          break;

        case REG_VIA_PCR:
          via_write( &oric->via, VIA_PCR, v );
          break;

        case REG_VIA_ACR:
          via_write( &oric->via, VIA_ACR, v );
          break;

        case REG_VIA_SR:
          via_write( &oric->via, VIA_SR, v );
          break;

        case REG_VIA_IFR:
          via_mon_write_ifr( &oric->via, v );
          break;

        case REG_VIA_IER:
          oric->via.ier = v&0x7f;
//          via_write( &oric->via, VIA_IER, v );
          break;

        case REG_VIA_IRA:
          via_write_porta( &oric->via, 0xff, v );
          break;

        case REG_VIA_ORA:
          via_write( &oric->via, VIA_IORA, v );
          break;

        case REG_VIA_DDRA:
          via_write( &oric->via, VIA_DDRA, v );
          break;

        case REG_VIA_IRB:
          via_write_portb( &oric->via, 0xff, v );
          break;

        case REG_VIA_ORB:
          via_write( &oric->via, VIA_IORB, v );
          break;

        case REG_VIA_DDRB:
          via_write( &oric->via, VIA_DDRB, v );
          break;

        case REG_VIA_T1L:
          via_write( &oric->via, VIA_T1L_L, v&0xff );
          via_write( &oric->via, VIA_T1L_H, (v>>8)&0xff );
          break;

        case REG_VIA_T1C:
          oric->via.t1c = v;
          break;

        case REG_VIA_T2L:
          via_write( &oric->via, VIA_T2C_L, v&0xff );
          via_write( &oric->via, VIA_T2C_H, (v>>8)&0xff );
          break;

        case REG_VIA_T2C:
          oric->via.t2c = v;
          break;

        case REG_VIA_CA1:
          via_write_CA1( &oric->via, v );
          break;

        case REG_VIA_CA2:
          via_write_CA2( &oric->via, v );
          break;

        case REG_VIA_CB1:
          via_write_CB1( &oric->via, v );
          break;

        case REG_VIA_CB2:
          via_write_CB2( &oric->via, v );
          break;
      }
      *needrender = SDL_TRUE;
      break;
    
    case 0:
      return SDL_FALSE;
    
    case 'q':
      lastcmd = cmd[i];
      switch( cmd[i+1] )
      {
        case 32:
        case 0:
        case 'm':
          mon_store_state( oric );
          setemumode( oric, NULL, EM_RUNNING );
          break;
        
        case 'e':
          done = SDL_TRUE;
          break;
        
        default:
          mon_printf( "???" );
          break;
      }
      break;
    
    case 'x':
      mon_store_state( oric );
      setemumode( oric, NULL, EM_RUNNING );
      break;
    
    case 'w':
      lastcmd = 0;
      i++;
      switch( cmd[i] )
      {
        case 'm':
          i++;
          if( !mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_TRUE, SDL_TRUE, SDL_TRUE ) )
          {
            mon_str( "Address expected" );
            break;
          }

          if( !mon_getnum( oric, &w, cmd, &i, SDL_TRUE, SDL_TRUE, SDL_TRUE, SDL_TRUE ) )
          {
            mon_str( "Size expected" );
            break;
          }

          while( isws( cmd[i] ) ) i++;
          if( !cmd[i] )
          {
            mon_str( "Filename expected" );
            break;
          }

          tmem = malloc( w );
          if( !tmem )
          {
            mon_str( "Amnesia!" );
            break;
          }

          for( j=0; j<w; j++ )
            tmem[j] = mon_read( oric, v+j );

          f = fopen( &cmd[i], "wb" );
          if( !f )
          {
            free( tmem );
            mon_printf( "Unable to open '%s'", &cmd[i] );
            break;
          }

          fwrite( tmem, w, 1, f );
          fclose( f );
          free( tmem );
          mon_printf( "Written %d bytes to '%s'", w, &cmd[i] );
          break;
      }
      break;

    case '?':
      lastcmd = cmd[i];
      switch( helpcount )
      {
        case 0:
          mon_str( "KEYS:" );
          //          |          -          |          -          |
          mon_str( "  F2 : Quit Monitor     F3 : Toggle Console" );
          mon_str( "  F4 : Toggle info      F9 : Reset cycles" );
          mon_str( "  F10: Step CPU" );
          mon_str( " " );
          mon_str( "COMMANDS:" );
          mon_str( "  a <addr>              - Assemble" );
          mon_str( "  bc <bp id>            - Clear breakpoint" );
          mon_str( "  bcm <bp id>           - Clear mem breakpoint" );
          mon_str( "  bl                    - List breakpoints" );
          mon_str( "  blm                   - List mem breakpoints" );
          mon_str( "  bs <addr>             - Set breakpoint" );
          mon_str( "  bsm <addr> [rwc]      - Set mem breakpoint" );
          mon_str( "  bz                    - Zap breakpoints" );
          mon_str( "  bzm                   - Zap mem breakpoints" );
          mon_str( "  d <addr>              - Disassemble" );
          mon_str( "  df <addr> <end> <file>- Disassemble to file" );
          mon_str( "---- MORE" );
          helpcount++;
          break;
        
        case 1:
          mon_str( "  m <addr>              - Dump memory" );
          mon_str( "  mw <addr>             - Memory watch at addr" );
          mon_str( "  r <reg> <val>         - Set <reg> to <val>" );
          mon_str( "  q, x or qm            - Quit monitor" );
          mon_str( "  qe                    - Quit emulator" );
          mon_str( "  sa <name> <addr>      - Add or move symbol" );
          mon_str( "  sk <name>             - Kill symbol" );
          mon_str( "  sc                    - Symbols not case-sens." );
          mon_str( "  sC                    - Symbols case-sensitive" );
          mon_str( "  sl <file>             - Load symbols" );
          mon_str( "  sz                    - Zap symbols" );
          mon_str( "  wm <addr> <len> <file>- Write mem to disk" );
          helpcount = 0;
          lastcmd = 0;
          break;
      }
      break;

    default:
      lastcmd = 0;
      mon_printf( "???" );
      break;
  }

  return done;
}

static void mon_write_ibuf( void )
{
  char ibtmp[CONS_WIDTH+1];

  strncpy( ibtmp, &ibuf[iloff], CONS_WIDTH );
  ibtmp[CONS_WIDTH] = 0;

  tzstrpos( tz[TZ_MONITOR], 1, 19, iloff > 0 ? "\x16" : "]" );
  tzstrpos( tz[TZ_MONITOR], 2, 19, ibtmp );
  if( strlen( &ibuf[iloff] ) > CONS_WIDTH )
  {
    tzstrpos( tz[TZ_MONITOR], 2+CONS_WIDTH, 19, "\x16" );
  } else {
    tzstr( tz[TZ_MONITOR], " " );
    tzstrpos( tz[TZ_MONITOR], 2+CONS_WIDTH, 19, " " );
  }
}

static void mon_write_ibuf_asm( void )
{
  char ibtmp[CONS_WIDTH+1];

  strncpy( ibtmp, ibuf, CONS_WIDTH );
  ibtmp[CONS_WIDTH] = 0;

  tzstrpos( tz[TZ_MONITOR], 8, 19, ibtmp );
  tzstr( tz[TZ_MONITOR], " " );
}

static void mon_set_curs_to_end( void )
{
  cursx = ilen;
  iloff = ilen-CONS_WIDTH;
  if( iloff < 0 ) iloff = 0;
}

static void mon_set_iloff( void )
{
  if( cursx < iloff ) iloff = cursx;
  if( cursx >= (iloff+CONS_WIDTH) ) iloff = cursx-CONS_WIDTH;
  if( iloff < 0 ) iloff =0;
}

/*
- AM_IMP=0,
- AM_IMM,
  AM_ZP,   <-- AM_ABS
  AM_ZPX,  <-- AM_ABX
  AM_ZPY,  <-- AM_ABY
  AM_ABS, nnnn
  AM_ABX, nnnn,x
  AM_ABY, nnnn,y
- AM_ZIX, (nn,x)
- AM_ZIY, (nn),y
  AM_REL,   <-- AM_ABS
- AM_IND  (nnnn)
*/
static SDL_bool mon_decodeoperand( struct machine *oric, char *ptr, int *type, unsigned short *val )
{
  int i;
  unsigned int v;

  i=0;
  while( isws( ptr[i] ) ) i++;

  // Implied
  if( ptr[i] == 0 )
  {
    (*type) = AM_IMP;
    return SDL_TRUE;
  }

  // Immediate
  if( ptr[i] == '#' )
  {
    i++;
    if( !mon_getnum( oric, &v, ptr, &i, SDL_FALSE, SDL_FALSE, SDL_FALSE, SDL_TRUE ) )
      return SDL_FALSE;
    (*type) = AM_IMM;
    (*val)  = v;
    return SDL_TRUE;
  }

  // ZIX, ZIY, IND
  if( ptr[i] == '(' )
  {
    i++;
    if( !mon_getnum( oric, &v, ptr, &i, SDL_TRUE, SDL_TRUE, SDL_FALSE, SDL_TRUE ) )
      return SDL_FALSE;

    if( strncasecmp( &ptr[i], ",X)", 3 ) == 0 )
    {
      (*type) = AM_ZIX;
      (*val)  = v&0xff;
      return SDL_TRUE;
    }
    
    if( strncasecmp( &ptr[i], "),Y", 3 ) == 0 )
    {
      (*type) = AM_ZIY;
      (*val)  = v&0xff;
      return SDL_TRUE;
    }

    if( ptr[i] == ')' )
    {
      (*type) = AM_IND;
      (*val)  = v;
      return SDL_TRUE;
    }
    
    return SDL_FALSE;
  }

  if( !mon_getnum( oric, &v, ptr, &i, SDL_TRUE, SDL_TRUE, SDL_FALSE, SDL_TRUE ) )
    return SDL_FALSE;

  if( strncasecmp( &ptr[i], ",X", 2 ) == 0 )
  {
    (*type) = AM_ABX;
    (*val)  = v;
    return SDL_TRUE;
  }

  if( strncasecmp( &ptr[i], ",Y", 2 ) == 0 )
  {
    (*type) = AM_ABY;
    (*val)  = v;
    return SDL_TRUE;
  }
 
  (*type) = AM_ABS;
  (*val)  = v;
  return SDL_TRUE;
}

static SDL_bool mon_assemble_line( struct machine *oric )
{
  int i, j, addsym, amode;
  unsigned short val;

  i=0;
  while( isws( ibuf[i] ) ) i++;

  if( ibuf[i] == 0 ) return SDL_TRUE;

  // Starts with a label?
  j = i;
  addsym = -1;
  if( issymstart( ibuf[j] ) )
  {
    while( issymchar( ibuf[j] ) ) j++;
    if( ibuf[j] == ':' )
    {
      addsym = i;
      i = j+1;
      while( isws( ibuf[i] ) ) i++;
    }
  }

  for( j=0; asmtab[j].name; j++ )
  {
    if( strncasecmp( asmtab[j].name, &ibuf[i], 3 ) == 0 )
      break;
  }

  if( !asmtab[j].name )
  {
    mon_oprintf( " %04X  %c%c%c  ???           ", mon_addr, ibuf[i], ibuf[i+1], ibuf[i+2] );
    return SDL_FALSE;
  }

  i += 3;
  if( !mon_decodeoperand( oric, &ibuf[i], &amode, &val ) )
  {
    mon_str( "Illegal operand" );
    return SDL_FALSE;
  }

  switch( amode )
  {
    case AM_IMP:
      if( asmtab[j].imp == -1 ) { mon_str( "Operand expected" ); return SDL_FALSE; }
      oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].imp );
      break;
    
    case AM_IMM:
      if( asmtab[j].imm == -1 ) { mon_str( "Illegal operand" ); return SDL_FALSE; }
      oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].imm );
      oric->cpu.write( &oric->cpu, mon_addr+1, val );
      break;
    
    case AM_ABS:
      if( asmtab[j].rel != -1 )
      {
        i = ((int)val)-((int)(mon_addr+2));
        if( ( i < -128 ) || ( i > 127 ) )
        {
          mon_str( "Branch out of range" );
          return SDL_FALSE;
        }
        oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].rel );
        oric->cpu.write( &oric->cpu, mon_addr+1, i&0xff );        
        break;
      }

      if( ( asmtab[j].zp != -1 ) && ( (val&0xff00)==0 ) )
      {
        oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].zp );
        oric->cpu.write( &oric->cpu, mon_addr+1, val );
        break;
      }

      if( asmtab[j].abs == -1 ) { mon_str( "Illegal operand" ); return SDL_FALSE; }
      oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].abs );
      oric->cpu.write( &oric->cpu, mon_addr+1, val&0xff );
      oric->cpu.write( &oric->cpu, mon_addr+2, (val>>8)&0xff );
      break;

    case AM_IND:
      if( asmtab[j].ind == -1 ) { mon_str( "Illegal operand" ); return SDL_FALSE; }
      oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].ind );
      oric->cpu.write( &oric->cpu, mon_addr+1, val&0xff );
      oric->cpu.write( &oric->cpu, mon_addr+2, (val>>8)&0xff );
      break;

    case AM_ZPX:
      if( asmtab[j].zpx == -1 ) { mon_str( "Illegal operand" ); return SDL_FALSE; }
      oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].zpx );
      oric->cpu.write( &oric->cpu, mon_addr+1, val );
      break;

    case AM_ZPY:
      if( asmtab[j].zpy == -1 ) { mon_str( "Illegal operand" ); return SDL_FALSE; }
      oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].zpy );
      oric->cpu.write( &oric->cpu, mon_addr+1, val );
      break;

    case AM_ZIX:
      if( asmtab[j].zix == -1 ) { mon_str( "Illegal operand" ); return SDL_FALSE; }
      oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].zix );
      oric->cpu.write( &oric->cpu, mon_addr+1, val );
      break;

    case AM_ZIY:
      if( asmtab[j].ziy == -1 ) { mon_str( "Illegal operand" ); return SDL_FALSE; }
      oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].ziy );
      oric->cpu.write( &oric->cpu, mon_addr+1, val );
      break;

    case AM_ABX:
      if( ( asmtab[j].zpx != -1 ) && ( (val&0xff00)==0 ) )
      {
        oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].zpx );
        oric->cpu.write( &oric->cpu, mon_addr+1, val );
        break;
      }

      if( asmtab[j].abx == -1 ) { mon_str( "Illegal operand" ); return SDL_FALSE; }
      oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].abx );
      oric->cpu.write( &oric->cpu, mon_addr+1, val&0xff );
      oric->cpu.write( &oric->cpu, mon_addr+2, (val>>8)&0xff );
      break;

    case AM_ABY:
      if( ( asmtab[j].zpy != -1 ) && ( (val&0xff00)==0 ) )
      {
        oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].zpy );
        oric->cpu.write( &oric->cpu, mon_addr+1, val );
        break;
      }

      if( asmtab[j].aby == -1 ) { mon_str( "Illegal operand" ); return SDL_FALSE; }
      oric->cpu.write( &oric->cpu, mon_addr, asmtab[j].aby );
      oric->cpu.write( &oric->cpu, mon_addr+1, val&0xff );
      oric->cpu.write( &oric->cpu, mon_addr+2, (val>>8)&0xff );
      break;
  }
  
  if( addsym != -1 )
    mon_replace_or_add_symbol( &usersyms, oric, &ibuf[addsym], SYM_BESTGUESS, mon_addr );

  mon_oprintf( mon_disassemble( oric, &mon_addr, NULL, SDL_FALSE ) );
  return SDL_FALSE;
}

static SDL_bool mon_console_keydown( SDL_Event *ev, struct machine *oric, SDL_bool *needrender )
{
  int i;
  SDL_bool done;

  done = SDL_FALSE;

  if( mon_asmmode )
  {
    switch( ev->key.keysym.sym )
    {
      case SDLK_LEFT:
        if( cursx > 0 )
        {
          mon_hide_curs();
          cursx--;
          mon_write_ibuf_asm();
          mon_show_curs();
          *needrender = SDL_TRUE;
        }
        break;

      case SDLK_RIGHT:
        if( cursx < ilen )
        {
          mon_hide_curs();
          cursx++;
          mon_write_ibuf_asm();
          mon_show_curs();
          *needrender = SDL_TRUE;
        }
        break;
      
      case SDLK_BACKSPACE:
        if( cursx > 0 )
        {
          mon_hide_curs();
          for( i=cursx-1; i<ilen; i++ )
            ibuf[i] = ibuf[i+1];
          cursx--;
          ilen = strlen( ibuf );
          mon_set_iloff();
          mon_write_ibuf_asm();
          mon_show_curs();
          *needrender = SDL_TRUE;
        }
        break;
      
      case SDLK_RETURN:
        mon_hide_curs();
        ibuf[ilen] = 0;

        if( mon_assemble_line( oric ) )
        {
          mon_asmmode = SDL_FALSE;
          mon_start_input();
          mon_show_curs();
          *needrender = SDL_TRUE;
          break;
        }

        mon_start_input();
        mon_show_curs();
        *needrender = SDL_TRUE;
        break;

      default:
        break;
    }

    switch( ev->key.keysym.unicode )
    {
      default:
        if( ( ev->key.keysym.unicode > 31 ) && ( ev->key.keysym.unicode < 127 ) )
        {
          if( ilen >= MAX_ASM_INPUT ) break;

          mon_hide_curs();
          for( i=ilen+1; i>cursx; i-- )
            ibuf[i] = ibuf[i-1];
          ibuf[cursx] = ev->key.keysym.unicode;
          cursx++;
          ilen++;
          mon_write_ibuf_asm();
          mon_show_curs();
          *needrender = SDL_TRUE;
        }
        break;
    }
    return done;
  }

  switch( ev->key.keysym.sym )
  {
    case SDLK_UP:
      if( histp >= (histu-1) ) break;
      mon_hide_curs();
      strcpy( ibuf, &history[++histp][0] );
      ilen = strlen( ibuf );
      mon_set_curs_to_end();
      mon_set_iloff();
      tzstrpos( tz[TZ_MONITOR], 2, 19, "                                               " );
      mon_write_ibuf();
      mon_show_curs();
      *needrender = SDL_TRUE;
      break;

    case SDLK_DOWN:
      mon_hide_curs();
      if( histp <= 0 )
      {
        histp = -1;
        ibuf[0] = 0;
        ilen = 0;
        iloff = 0;
        cursx = 0;
      } else {
        strcpy( ibuf, &history[--histp][0] );
        mon_set_curs_to_end();
        mon_set_iloff();
      }
      tzstrpos( tz[TZ_MONITOR], 2, 19, "                                               " );
      mon_write_ibuf();
      mon_show_curs();
      *needrender = SDL_TRUE;
      break;
    
    case SDLK_LEFT:
      if( cursx > 0 )
      {
        mon_hide_curs();
        cursx--;
        mon_set_iloff();
        mon_write_ibuf();
        mon_show_curs();
        *needrender = SDL_TRUE;
      }
      break;

    case SDLK_RIGHT:
      if( cursx < ilen )
      {
        mon_hide_curs();
        cursx++;
        mon_set_iloff();
        mon_write_ibuf();
        mon_show_curs();
        *needrender = SDL_TRUE;
      }
      break;
    
    default:
      break;

    case SDLK_BACKSPACE:
      if( cursx > 0 )
      {
        mon_hide_curs();
        for( i=cursx-1; i<ilen; i++ )
          ibuf[i] = ibuf[i+1];
        cursx--;
        ilen = strlen( ibuf );
        mon_set_iloff();
        mon_write_ibuf();
        mon_show_curs();
        *needrender = SDL_TRUE;
      }
      break;
    
    case SDLK_RETURN:
      mon_hide_curs();
      ibuf[ilen] = 0;
      if( ilen == 0 )
      {
        switch( lastcmd )
        {
          case '?':
          case 'm':
          case 'd':
            ibuf[cursx++] = lastcmd;
            ibuf[cursx] = 0;
            ilen = 1;
            break;
        }
      } else {
        if( ( histu > 0 ) && ( strcmp( &history[0][0], ibuf ) == 0 ) )
        {
        } else {
          if( histu > 0 )
          {
            for( i=histu; i>0; i-- )
              strcpy( &history[i][0], &history[i-1][0] );

          }
          strcpy( &history[0][0], ibuf );
          if( histu < 10 ) histu++;
        }
      }
      histp = -1;

      tzsetcol( tz[TZ_MONITOR], 2, 3 ); 
      done |= mon_cmd( ibuf, oric, needrender );
      mon_start_input();
      mon_show_curs();
      *needrender = SDL_TRUE;
      break;
  }

  switch( ev->key.keysym.unicode )
  {
    default:
      if( ( ev->key.keysym.unicode > 31 ) && ( ev->key.keysym.unicode < 127 ) )
      {
        if( ilen >= MAX_CONS_INPUT ) break;

        mon_hide_curs();
        for( i=ilen+1; i>cursx; i-- )
          ibuf[i] = ibuf[i-1];
        ibuf[cursx] = ev->key.keysym.unicode;
        cursx++;
        ilen++;
        mon_set_iloff();
        mon_write_ibuf();
        mon_show_curs();
        *needrender = SDL_TRUE;
      }
      break;
  }
  return done;
}

static SDL_bool mon_mwatch_keydown( SDL_Event *ev, struct machine *oric, SDL_bool *needrender )
{
  SDL_bool done;
  unsigned int v;
  int i, page;

  page = mw_split ? 8*8 : 8*18;

  done = SDL_FALSE;
  switch( ev->key.keysym.sym )
  {
    case SDLK_UP:
      mw_addr[mw_which] -= kshifted ? page : 8;
      *needrender = SDL_TRUE;
      break;

    case SDLK_DOWN:
      mw_addr[mw_which] += kshifted ? page : 8;
      *needrender = SDL_TRUE;
      break;

    case SDLK_PAGEUP:
      mw_addr[mw_which] -= page;
      *needrender = SDL_TRUE;
      break;

    case SDLK_PAGEDOWN:
      mw_addr[mw_which] += page;
      *needrender = SDL_TRUE;
      break;
    
    case SDLK_RETURN:
      if( mw_mode != 1 ) break;
      mw_ibuf[5] = 0;
      mw_mode = 0;
      i = 0;
      if( mon_getnum( oric, &v, mw_ibuf, &i, SDL_FALSE, SDL_FALSE, SDL_FALSE, SDL_FALSE ) )
        mw_addr[mw_which] = v;
      *needrender = SDL_TRUE;
      break;
    
    case SDLK_ESCAPE:
      mw_mode = 0;
      break;
     
    case SDLK_BACKSPACE:
      if( mw_mode != 1 ) break;
      if( mw_koffs <= 0 ) break;
      mw_ibuf[mw_koffs--] = 0;
      *needrender = SDL_TRUE;
      break;
    
    case 's':
    case 'S':
      if( mw_mode != 0 ) break;
      mw_split = !mw_split;
      if( ( !mw_split ) && ( mw_which != 0 ) )
        mw_which = 0;
      *needrender = SDL_TRUE;
      break;
    
    case SDLK_TAB:
      if( !mw_split ) break;
      if( mw_mode != 0 ) mw_mode = 0;
      mw_which ^= 1;
      *needrender = SDL_TRUE;
      break;

    default:
      break;
  }

  if( ishex( ev->key.keysym.unicode ) )
  {
    if( mw_mode != 1 )
    {
      mw_mode = 1;
      mw_ibuf[0] = '$';
      mw_ibuf[1] = 0;
      mw_koffs = 0;
    }

    if( mw_koffs < 4 )
    {
      mw_ibuf[++mw_koffs] = ev->key.keysym.unicode;
      mw_ibuf[mw_koffs+1] = 0;
    }

    *needrender = SDL_TRUE;
  }

  return done;
}

static unsigned int steppy_step( struct machine *oric )
{
  m6502_set_icycles( &oric->cpu );
  via_clock( &oric->via, oric->cpu.icycles );
  ay_ticktock( &oric->ay, oric->cpu.icycles );
  if( oric->drivetype ) wd17xx_ticktock( &oric->wddisk, oric->cpu.icycles );
  if( oric->type == MACH_TELESTRAT )
  {
    via_clock( &oric->tele_via, oric->cpu.icycles );
    acia_clock( &oric->tele_acia, oric->cpu.icycles );
  }
  m6502_inst( &oric->cpu, SDL_FALSE, mon_bpmsg );
  if( oric->cpu.rastercycles <= 0 )
  {
    ula_doraster( oric );
    oric->cpu.rastercycles += oric->cyclesperraster;
  }
  
  if( mon_bpmsg[0] )
  {
    mon_printf_above( mon_bpmsg );
    mon_bpmsg[0] = 0;
  }

  
  return oric->cpu.icycles;
}

SDL_bool mon_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender )
{
  SDL_bool done;
  SDL_bool donkey;

  done = SDL_FALSE;
  donkey = SDL_FALSE;
  switch( ev->type )
  {
    case SDL_ACTIVEEVENT:
      switch( ev->active.type )
      {
        case SDL_APPINPUTFOCUS:
        case SDL_APPACTIVE:
          *needrender = SDL_TRUE;
          break;
      }
      break;

    case SDL_KEYUP:
      switch( ev->key.keysym.sym )
      {
        case SDLK_F9:
          oric->cpu.cycles = 0;
          *needrender = SDL_TRUE;
          break;

        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
          kshifted = SDL_FALSE;
          break;

        case SDLK_F2:
          // In case we're on a breakpoint
          m6502_set_icycles( &oric->cpu );
          via_clock( &oric->via, oric->cpu.icycles );
          ay_ticktock( &oric->ay, oric->cpu.icycles );
          if( oric->drivetype ) wd17xx_ticktock( &oric->wddisk, oric->cpu.icycles );
          if( oric->type == MACH_TELESTRAT )
          {
            via_clock( &oric->tele_via, oric->cpu.icycles );
            acia_clock( &oric->tele_acia, oric->cpu.icycles );
          }
          m6502_inst( &oric->cpu, SDL_FALSE, mon_bpmsg );
          if( oric->cpu.rastercycles <= 0 )
          {
            ula_doraster( oric );
            oric->cpu.rastercycles += oric->cyclesperraster;
          }
          *needrender = SDL_TRUE;
          mon_store_state( oric );
          setemumode( oric, NULL, EM_RUNNING );
          break;

        case SDLK_F3:
          cshow = (cshow+1)%CSHOW_LAST;
          *needrender = SDL_TRUE;
          break;

        case SDLK_F4:
          mshow = (mshow+1)%MSHOW_LAST;
          if( ( mshow == MSHOW_VIA2 ) && ( oric->type != MACH_TELESTRAT ) )
            mshow = (mshow+1)%MSHOW_LAST;
          if( ( oric->drivetype == DRV_NONE ) && ( mshow == MSHOW_DISK ) )
            mshow = (mshow+1)%MSHOW_LAST;
          *needrender = SDL_TRUE;
          break;

        default:
          break;
      }
      break;

    case SDL_KEYDOWN:
      switch( ev->key.keysym.sym )
      {
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
          kshifted = SDL_TRUE;
          donkey = SDL_TRUE;
          break;

        case SDLK_F10:
          if( !kshifted )
          {
            mon_store_state( oric );
            steppy_step( oric );
            *needrender = SDL_TRUE;
            donkey = SDL_TRUE;
            break;
          }
        
        case SDLK_F11:
          if( mon_read( oric, oric->cpu.pc ) == 0x20 ) // JSR instruction?
          {
            Uint16 newpc;
            unsigned int endticks;

            newpc = oric->cpu.pc+3;
            
            mon_store_state( oric );
            endticks = SDL_GetTicks()+5000; // 5 seconds to comply
            while( ( oric->cpu.pc != newpc ) && ( SDL_GetTicks() < endticks ) )
              steppy_step( oric );

            *needrender = SDL_TRUE;
            donkey = SDL_TRUE;
            break;
          }

          mon_store_state( oric );
          steppy_step( oric );
          *needrender = SDL_TRUE;
          donkey = SDL_TRUE;
          break;
        
        case SDLK_F12:
          switch( distab[mon_read( oric, oric->cpu.pc )].amode )
          {
            case AM_IMM:
            case AM_ZP:
            case AM_ZPX:
            case AM_ZPY:
            case AM_ZIX:
            case AM_ZIY:
            case AM_REL:
              oric->cpu.pc += 2;
              break;
            
            case AM_ABS:
            case AM_ABX:
            case AM_ABY:
            case AM_IND:
              oric->cpu.pc += 3;
              break;
            

            case AM_IMP:
            default:
              oric->cpu.pc++;
              break;
          }
          *needrender = SDL_TRUE;
          donkey = SDL_TRUE;
          break;
        
        default:
          break;
      }

      if( !donkey )
      {
        switch( cshow )
        {
          case CSHOW_CONSOLE:
            done |= mon_console_keydown( ev, oric, needrender );
            break;

          case CSHOW_MWATCH:
            done |= mon_mwatch_keydown( ev, oric, needrender );
            break;
        }
      }
      break;
  }

  return done;
}
