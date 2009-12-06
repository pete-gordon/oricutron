/*
**  Oriculator
**  Copyright (C) 2009 Peter Gordon
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
#define SNAME_LEN 11              // Short name for symbols (with ...)
#define SSNAME_LEN (SNAME_LEN-3)  // Short short name :)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef __SPECIFY_SDL_DIR__
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "machine.h"
#include "monitor.h"

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

#define SYMB_ROMDIS0   0
#define SYMF_ROMDIS0   (1<<SYMB_ROMDIS0)
#define SYMB_ROMDIS1   1
#define SYMF_ROMDIS1   (1<<SYMB_ROMDIS1)
#define SYMB_MICRODISC 2
#define SYMF_MICRODISC (1<<SYMB_MICRODISC)
#define SYMB_JASMIN    3
#define SYMF_JASMIN    (1<<SYMB_JASMIN)

struct msym
{
  unsigned short addr;       // Address
  unsigned short flags;
  char sname[SNAME_LEN+1];   // Short name
  char ssname[SSNAME_LEN+1]; // Short short name
  char *name;                // Full name
};


extern struct textzone *tz[];
extern char vsptmp[];

static char distmp[80];
static char ibuf[128], lastcmd;
static char history[10][128];
static int ilen, iloff=0, cursx, histu=0, histp=-1;
static unsigned short mon_addr, mon_asmmode = SDL_FALSE;
static unsigned short mw_addr;
static int mw_mode=0, mw_koffs=0;
static char mw_ibuf[8];
static SDL_bool kshifted = SDL_FALSE;
static int helpcount=0;

static int numsyms=0, symspace=0;
static struct msym *defaultsyms = NULL, *syms = NULL;

char mon_bpmsg[80];

//                                                             12345678901    12345678 
static struct msym defsym_oric1[] = { { 0xc000, SYMF_ROMDIS0, "ROMStart"   , "ROMStart", "ROMStart" },
                                      { 0, 0, { 0, }, { 0, }, NULL } };

//                                                           12345678901       12345678 
static struct msym defsym_atmos[] = { { 0xc000, SYMF_ROMDIS0, "ROMStart"      , "ROMStart"   , "ROMStart" },
                                      { 0xc000, SYMF_ROMDIS1, "Overlay"       , "Overlay"    , "Overlay" },
                                      { 0xc006, SYMF_ROMDIS0, "JumpTab"       , "JumpTab"    , "JumpTab" },
                                      { 0xc0ea, SYMF_ROMDIS0, "Keywords"      , "Keywords"   , "Keywords" },
                                      { 0xc2a8, SYMF_ROMDIS0, "ErrorMsgs"     , "ErrorMs\x16", "ErrorMsgs" },
                                      { 0xc3c6, SYMF_ROMDIS0, "FindForVar"    , "FindFor\x16", "FindForVar" },
                                      { 0xc3f4, SYMF_ROMDIS0, "VarAlloc"      , "VarAlloc"   , "VarAlloc" },
                                      { 0xc444, SYMF_ROMDIS0, "FreeMemChe\x16", "FreeMem\x16", "FreeMemCheck" },
                                      { 0xc47c, SYMF_ROMDIS0, "PrintError"    , "PrintEr\x16", "PrintError" },
                                      { 0xc4a8, SYMF_ROMDIS0, "BackToBASIC"   , "BackToB\x16", "BackToBASIC" },
                                      { 0xc4d3, SYMF_ROMDIS0, "InsDelLine"    , "InsDelL\x16", "InsDelLine" },
                                      { 0xc4e0, SYMF_ROMDIS0, "DeleteLine"    , "DeleteL\x16", "DeleteLine" },
                                      { 0xc524, SYMF_ROMDIS0, "InsertLine"    , "InsertL\x16", "InsertLine" },
                                      { 0xc55f, SYMF_ROMDIS0, "SetLineLin\x16", "SetLine\x16", "SetLineLinkPtrs" },
                                      { 0xc444, SYMF_ROMDIS0, "FreeMemChe\x16", "FreeMem\x16", "FreeMemCheck" },
                                      { 0xc592, SYMF_ROMDIS0, "GetLine"       , "GetLine"    , "GetLine" },
                                      { 0xc5e8, SYMF_ROMDIS0, "ReadKey"       , "ReadKey"    , "ReadKey" },
                                      { 0xc5fa, SYMF_ROMDIS0, "TokeniseLi\x16", "Tokenis\x16", "TokeniseLine" },
                                      { 0xc692, SYMF_ROMDIS0, "EDIT"          , "EDIT"       , "EDIT" },
                                      { 0xc6b3, SYMF_ROMDIS0, "FindLine"      , "FindLine"   , "FindLine" },
                                      { 0xc6ee, SYMF_ROMDIS0, "NEW"           , "NEW"        , "NEW" },
                                      { 0xc70d, SYMF_ROMDIS0, "CLEAR"         , "CLEAR"      , "CLEAR" },
                                      { 0xc748, SYMF_ROMDIS0, "LIST"          , "LIST"       , "LIST" },
                                      { 0xc7fd, SYMF_ROMDIS0, "LLIST"         , "LLIST"      , "LLIST" },
                                      { 0xc809, SYMF_ROMDIS0, "LPRINT"        , "LPRINT"     , "LPRINT" },
                                      { 0xc816, SYMF_ROMDIS0, "SetPrinter"    , "SetPrin\x16", "SetPrinter" },
                                      { 0xc82f, SYMF_ROMDIS0, "SetScreen"     , "SetScre\x16", "SetScreen" },
                                      { 0xc855, SYMF_ROMDIS0, "FOR"           , "FOR"        , "FOR" },
                                      { 0xc8c1, SYMF_ROMDIS0, "DoNextLine"    , "DoNextL\x16", "DoNextLine" },
                                      { 0xc915, SYMF_ROMDIS0, "DoStatement"   , "DoState\x16", "DoStatement" },
                                      { 0xc952, SYMF_ROMDIS0, "RESTORE"       , "RESTORE"    , "RESTORE" },
                                      { 0xc9a0, SYMF_ROMDIS0, "CONT"          , "CONT"       , "CONT" },
                                      { 0xc9bd, SYMF_ROMDIS0, "RUN"           , "RUN"        , "RUN" },
                                      { 0xc9c8, SYMF_ROMDIS0, "GOSUB"         , "GOSUB"      , "GOSUB" },
                                      { 0xc9e5, SYMF_ROMDIS0, "GOTO"          , "GOTO"       , "GOTO" },
                                      { 0xca12, SYMF_ROMDIS0, "RETURN"        , "RETURN"     , "RETURN" },
                                      { 0xca4e, SYMF_ROMDIS0, "FindEndOfS\x16", "FindEnd\x16", "FindEndOfStatement" },
                                      { 0xca51, SYMF_ROMDIS0, "FindEOL"       , "FindEOL"    , "FindEOL" },
                                      { 0xca70, SYMF_ROMDIS0, "IF"            , "IF"         , "IF" },
                                      { 0xca99, SYMF_ROMDIS0, "REM"           , "REM"        , "REM" },
                                      { 0xcac2, SYMF_ROMDIS0, "ON"            , "ON"         , "ON" },
                                      { 0xcae2, SYMF_ROMDIS0, "Txt2Int"       , "Txt2Int"    , "Txt2Int" },
                                      { 0xcb1c, SYMF_ROMDIS0, "LET"           , "LET"        , "LET" },
                                      { 0xcbab, SYMF_ROMDIS0, "PRINT"         , "PRINT"      , "PRINT" },
                                      { 0xcbf0, SYMF_ROMDIS0, "NewLine"       , "NewLine"    , "NewLine" },
                                      { 0xcc59, SYMF_ROMDIS0, "SetCursor"     , "SetCurs\x16", "SetCursor" },
                                      { 0xccb0, SYMF_ROMDIS0, "PrintString"   , "PrintSt\x16", "PrintString" },
                                      { 0xccce, SYMF_ROMDIS0, "ClrScr"        , "ClrScr"     , "ClrScr" },
                                      { 0xcd16, SYMF_ROMDIS0, "TRON"          , "TRON"       , "TRON" },
                                      { 0xcd46, SYMF_ROMDIS0, "GET"           , "GET"        , "GET" },
                                      { 0xcd55, SYMF_ROMDIS0, "INPUT"         , "INPUT"      , "INPUT" },
                                      { 0xcd89, SYMF_ROMDIS0, "READ"          , "READ"       , "READ" },
                                      { 0xce98, SYMF_ROMDIS0, "NEXT"          , "NEXT"       , "NEXT" },
                                      { 0xcf03, SYMF_ROMDIS0, "GetExpr"       , "GetExpr"    , "GetExpr" },
                                      { 0xcf17, SYMF_ROMDIS0, "EvalExpr"      , "EvalExpr"   , "EvalExpr" },
                                      { 0xcfac, SYMF_ROMDIS0, "DoOper"        , "DoOper"     , "DoOper" },
                                      { 0xd000, SYMF_ROMDIS0, "GetItem"       , "GetItem"    , "GetItem" },
                                      { 0xd03c, SYMF_ROMDIS0, "NOT"           , "NOT"        , "NOT" },
                                      { 0xd059, SYMF_ROMDIS0, "EvalBracket"   , "EvalBra\x16", "EvalBracket" },
                                      { 0xd07c, SYMF_ROMDIS0, "GetVarVal"     , "GetVarV\x16", "GetVarVal" },
                                      { 0xd113, SYMF_ROMDIS0, "Compare"       , "Compare"    , "Compare" },
                                      { 0xd17e, SYMF_ROMDIS0, "DIM"           , "DIM"        , "DIM" },
                                      { 0xd188, SYMF_ROMDIS0, "GetVarFrom\x16", "GetVarF\x16", "GetVarFromText" },
                                      { 0xd361, SYMF_ROMDIS0, "DimArray"      , "DimArray"   , "DimArray" },
                                      { 0xd3eb, SYMF_ROMDIS0, "GetArrayEl\x16", "GetArra\x16", "GetArrayElement" },
                                      { 0xd47e, SYMF_ROMDIS0, "FRE"           , "FRE"        , "FRE" },
                                      { 0xd4a6, SYMF_ROMDIS0, "POS"           , "POS"        , "POS" },
                                      { 0xd4ba, SYMF_ROMDIS0, "DEF"           , "DEF"        , "DEF" },
                                      { 0xd593, SYMF_ROMDIS0, "STR"           , "STR"        , "STR" },
                                      { 0xd5a3, SYMF_ROMDIS0, "SetupString"   , "SetupSt\x16", "SetupString" },
                                      { 0xd5b5, SYMF_ROMDIS0, "GetString"     , "GetStri\x16", "GetString" },
                                      { 0xd650, SYMF_ROMDIS0, "GarbageCol\x16", "Garbage\x16", "GarbageCollect" },
                                      { 0xd730, SYMF_ROMDIS0, "CopyString"    , "CopyStr\x16", "CopyString" },
                                      { 0xd767, SYMF_ROMDIS0, "StrCat"        , "StrCat"     , "StrCat" },
                                      { 0xd816, SYMF_ROMDIS0, "CHR"           , "CHR"        , "CHR" },
                                      { 0xd82a, SYMF_ROMDIS0, "LEFT"          , "LEFT"       , "LEFT" },
                                      { 0xd856, SYMF_ROMDIS0, "RIGHT"         , "RIGHT"      , "RIGHT" },
                                      { 0xd861, SYMF_ROMDIS0, "MID"           , "MID"        , "MID" },
                                      { 0xd8a6, SYMF_ROMDIS0, "LEN"           , "LEN"        , "LEN" },
                                      { 0xd8b5, SYMF_ROMDIS0, "ASC"           , "ASC"        , "ASC" },
                                      { 0xd8c5, SYMF_ROMDIS0, "GetByteExp\x16", "GetByte\x16", "GetByteExpr" },
                                      { 0xd922, SYMF_ROMDIS0, "FP2Int"        , "FP2Int"     , "FP2Int" },
                                      { 0xd938, SYMF_ROMDIS0, "PEEK"          , "PEEK"       , "PEEK" },
                                      { 0xd94f, SYMF_ROMDIS0, "POKE"          , "POKE"       , "POKE" },
                                      { 0xd958, SYMF_ROMDIS0, "WAIT"          , "WAIT"       , "WAIT" },
                                      { 0xd967, SYMF_ROMDIS0, "DOKE"          , "DOKE"       , "DOKE" },
                                      { 0xd983, SYMF_ROMDIS0, "DEEK"          , "DEEK"       , "DEEK" },
                                      { 0xd993, SYMF_ROMDIS0, "Byte2Hex"      , "Byte2Hex"   , "Byte2Hex" },
                                      { 0xd9b5, SYMF_ROMDIS0, "HEX"           , "HEX"        , "HEX" },
                                      { 0xd9de, SYMF_ROMDIS0, "LORES"         , "LORES"      , "LORES" },
                                      { 0xda0c, SYMF_ROMDIS0, "RowCalc"       , "RowCalc"    , "RowCalc" },
                                      { 0xda3f, SYMF_ROMDIS0, "SCRN"          , "SCRN"       , "SCRN" },
                                      { 0xda51, SYMF_ROMDIS0, "PLOT"          , "PLOT"       , "PLOT" },
                                      { 0xdaa1, SYMF_ROMDIS0, "UNTIL"         , "UNTIL"      , "UNTIL" },
                                      { 0xdaab, SYMF_ROMDIS0, "REPEAT"        , "REPEAT"     , "REPEAT" },
                                      { 0xdada, SYMF_ROMDIS0, "KEY"           , "KEY"        , "KEY" },
                                      { 0xdaf6, SYMF_ROMDIS0, "TxtTest"       , "TxtTest"    , "TxtTest" },
                                      { 0xdb92, SYMF_ROMDIS0, "Normalise"     , "Normali\x16", "Normalise" },
                                      { 0xdbb9, SYMF_ROMDIS0, "AddMantiss\x16", "AddMant\x16", "AddMantissas" },
                                      { 0xdcaf, SYMF_ROMDIS0, "LN"            , "LN"         , "LN" },
                                      { 0xdd51, SYMF_ROMDIS0, "UnpackFPA"     , "UnpackF\x16", "UnpackFPA" },
                                      { 0xdda7, SYMF_ROMDIS0, "FPAMult10"     , "FPAMult\x16", "FPAMult10" },
                                      { 0xddc3, SYMF_ROMDIS0, "FPADiv10"      , "FPADiv1\x16", "FPADiv10" },
                                      { 0xddd4, SYMF_ROMDIS0, "LOG"           , "LOG"        , "LOG" },
                                      { 0xde77, SYMF_ROMDIS0, "PI"            , "PI"         , "PI" },
                                      { 0xdef4, SYMF_ROMDIS0, "RoundFPA"      , "RoundFPA"   , "RoundFPA" },
                                      { 0xdf0b, SYMF_ROMDIS0, "FALSE"         , "FALSE"      , "FALSE" },
                                      { 0xdf0f, SYMF_ROMDIS0, "TRUE"          , "TRUE"       , "TRUE" },
                                      { 0xdf13, SYMF_ROMDIS0, "GetSign"       , "GetSign"    , "GetSign" },
                                      { 0xdf21, SYMF_ROMDIS0, "SGN"           , "SGN"        , "SGN" },
                                      { 0xdf4c, SYMF_ROMDIS0, "CompareFPA"    , "Compare\x16", "CompareFPA" },
                                      { 0xdf8c, SYMF_ROMDIS0, "FPA2Int"       , "FPA2Int"    , "FPA2Int" },
                                      { 0xdfbd, SYMF_ROMDIS0, "INT"           , "INT"        , "INT" },
                                      { 0xdfe7, SYMF_ROMDIS0, "GetNumber"     , "GetNumb\x16", "GetNumber" },
                                      { 0xe076, SYMF_ROMDIS0, "AddToFPA"      , "AddToFPA"   , "AddToFPA" },
                                      { 0xe0c5, SYMF_ROMDIS0, "PrintInt"      , "PrintInt"   , "PrintInt" },
                                      { 0xe22e, SYMF_ROMDIS0, "SQR"           , "SQR"        , "SQR" },
                                      { 0xe27c, SYMF_ROMDIS0, "ExpData"       , "ExpData"    , "ExpData" },
                                      { 0xe22a, SYMF_ROMDIS0, "EXP"           , "EXP"        , "EXP" },
                                      { 0xe313, SYMF_ROMDIS0, "SeriesEval"    , "SeriesE\x16", "SeriesEval" },
                                      { 0xe34f, SYMF_ROMDIS0, "RND"           , "RND"        , "RND" },
                                      { 0xe38b, SYMF_ROMDIS0, "COS"           , "COS"        , "COS" },
                                      { 0xe392, SYMF_ROMDIS0, "SIN"           , "SIN"        , "SIN" },
                                      { 0xe407, SYMF_ROMDIS0, "TrigData"      , "TrigData"   , "TrigData" },
                                      { 0xe43f, SYMF_ROMDIS0, "ATN"           , "ATN"        , "ATN" },
                                      { 0xe46f, SYMF_ROMDIS0, "ATNData"       , "ATNData"    , "ATNData" },
                                      { 0xe4ac, SYMF_ROMDIS0, "TapeSync"      , "TapeSync"   , "TapeSync" },
                                      { 0xe4e0, SYMF_ROMDIS0, "GetTapeData"   , "GetTape\x16", "GetTapeData" },
                                      { 0xe4f2, SYMF_ROMDIS0, "VERIFY"        , "VERIFY"     , "VERIFY" },
                                      { 0xe56c, SYMF_ROMDIS0, "IncTapeCou\x16", "IncTape\x16", "IncTapeCount" },
                                      { 0xe57d, SYMF_ROMDIS0, "PrintSearc\x16", "PrintSe\x16", "PrintSearching" },
                                      { 0xe585, SYMF_ROMDIS0, "PrintSavin\x16", "PrintSa\x16", "PrintSaving" },
                                      { 0xe58c, SYMF_ROMDIS0, "PrintFName"    , "PrintFN\x16", "PrintFName" },
                                      { 0xe594, SYMF_ROMDIS0, "PrintFound"    , "PrintFo\x16", "PrintFound" },
                                      { 0xe5a4, SYMF_ROMDIS0, "PrintLoadi\x16", "PrintLo\x16", "PrintLoading" },
                                      { 0xe5ab, SYMF_ROMDIS0, "PrintVerif\x16", "PrintVe\x16", "PrintVerifying" },
                                      { 0xe5b6, SYMF_ROMDIS0, "PrintMsg"      , "PrintMsg"   , "PrintMsg" },
                                      { 0xe5ea, SYMF_ROMDIS0, "ClrStatus"     , "ClrStat\x16", "ClrStatus" },
                                      { 0xe5f5, SYMF_ROMDIS0, "ClrTapeSta\x16", "ClrTape\x16", "ClrTapeStatus" },
                                      { 0xe607, SYMF_ROMDIS0, "WriteFileH\x16", "WriteFi\x16", "WriteFileHeader" },
                                      { 0xe65e, SYMF_ROMDIS0, "PutTapeByte"   , "PutTape\x16", "PutTapeByte" },
                                      { 0xe6c9, SYMF_ROMDIS0, "GetTapeByte"   , "GetTape\x16", "GetTapeByte" },
                                      { 0xe735, SYMF_ROMDIS0, "SyncTape"      , "SyncTape"   , "SyncTape" },
                                      { 0xe75a, SYMF_ROMDIS0, "WriteLeader"   , "WriteLe\x16", "WriteLeader" },
                                      { 0xe76a, SYMF_ROMDIS0, "SetupTape"     , "SetupTa\x16", "SetupTape" },
                                      { 0xe7b2, SYMF_ROMDIS0, "GetTapePar\x16", "GetTape\x16", "GetTapeParams" },
                                      { 0xe85b, SYMF_ROMDIS0, "CLOAD"         , "CLOAD"      , "CLOAD" },
                                      { 0xe903, SYMF_ROMDIS0, "CLEAR"         , "CLEAR"      , "CLEAR" },
                                      { 0xe909, SYMF_ROMDIS0, "CSAVE"         , "CSAVE"      , "CSAVE" },
                                      { 0xe946, SYMF_ROMDIS0, "CALL"          , "CALL"       , "CALL" },
                                      { 0xe987, SYMF_ROMDIS0, "STORE"         , "STORE"      , "STORE" },
                                      { 0xe9d1, SYMF_ROMDIS0, "RECALL"        , "RECALL"     , "RECALL" },
                                      { 0xeaf0, SYMF_ROMDIS0, "HiresTest"     , "HiresTe\x16", "HiresTest" },
                                      { 0xeb78, SYMF_ROMDIS0, "CheckKbd"      , "CheckKbd"   , "CheckKbd" },
                                      { 0xebce, SYMF_ROMDIS0, "HIMEM"         , "HIMEM"      , "HIMEM" },
                                      { 0xec0c, SYMF_ROMDIS0, "RELEASE"       , "RELEASE"    , "RELEASE" },
                                      { 0xec21, SYMF_ROMDIS0, "TEXT"          , "TEXT"       , "TEXT" },
                                      { 0xec33, SYMF_ROMDIS0, "HIRES"         , "HIRES"      , "HIRES" },
                                      { 0xec45, SYMF_ROMDIS0, "POINT"         , "POINT"      , "POINT" },
                                      { 0xeccc, SYMF_ROMDIS0, "StartBASIC"    , "StartBA\x16", "StartBASIC" },
                                      { 0xedc4, SYMF_ROMDIS0, "CopyMem"       , "CopyMem"    , "CopyMem" },
                                      { 0xede0, SYMF_ROMDIS0, "SetupTimer"    , "SetupTi\x16", "SetupTimer" },
                                      { 0xee1a, SYMF_ROMDIS0, "StopTimer"     , "StopTim\x16", "StopTimer" },
                                      { 0xee22, SYMF_ROMDIS0, "IRQ"           , "IRQ"        , "IRQ" },
                                      { 0xee8c, SYMF_ROMDIS0, "ResetTimer"    , "ResetTi\x16", "ResetTimer" },
                                      { 0xee1a, SYMF_ROMDIS0, "StopTimer"     , "StopTim\x16", "StopTimer" },
                                      { 0xee9d, SYMF_ROMDIS0, "GetTimer"      , "GetTime\x16", "GetTimer" },
                                      { 0xeeab, SYMF_ROMDIS0, "SetTimer"      , "SetTime\x16", "SetTimer" },
                                      { 0xeec9, SYMF_ROMDIS0, "Delay"         , "Delay"      , "Delay" },
                                      { 0xeee8, SYMF_ROMDIS0, "WritePixel"    , "WritePi\x16", "WritePixel" },
                                      { 0xeef8, SYMF_ROMDIS0, "DrawLine"      , "DrawLine"   , "DrawLine" },
                                      { 0xf0c8, SYMF_ROMDIS0, "CURSET"        , "CURSET"     , "CURSET" },
                                      { 0xf0fd, SYMF_ROMDIS0, "CURMOV"        , "CURMOV"     , "CURMOV" },
                                      { 0xf110, SYMF_ROMDIS0, "DRAW"          , "DRAW"       , "DRAW" },
                                      { 0xf11d, SYMF_ROMDIS0, "PATTERN"       , "PATTERN"    , "PATTERN" },
                                      { 0xf12d, SYMF_ROMDIS0, "CHAR"          , "CHAR"       , "CHAR" },
                                      { 0xf204, SYMF_ROMDIS0, "PAPER"         , "PAPER"      , "PAPER" },
                                      { 0xf210, SYMF_ROMDIS0, "INK"           , "INK"        , "INK" },
                                      { 0xf268, SYMF_ROMDIS0, "FILL"          , "FILL"       , "FILL" },
                                      { 0xf37f, SYMF_ROMDIS0, "CIRCLE"        , "CIRCLE"     , "CIRCLE" },
                                      { 0xf495, SYMF_ROMDIS0, "ReadKbd"       , "ReadKbd"    , "ReadKbd" },
                                      { 0xf4ef, SYMF_ROMDIS0, "Key2ASCII"     , "Key2ASC\x16", "Key2ASCII" },
                                      { 0xf523, SYMF_ROMDIS0, "FindKey"       , "FindKey"    , "FindKey" },
                                      { 0xf561, SYMF_ROMDIS0, "ReadKbdCol"    , "ReadKbd\x16", "ReadKbdCol" },
                                      { 0xf590, SYMF_ROMDIS0, "WriteToAY"     , "WriteTo\x16", "WriteToAY" },
                                      { 0xf5c1, SYMF_ROMDIS0, "PrintChar"     , "PrintCh\x16", "PrintChar" },
                                      { 0xf602, SYMF_ROMDIS0, "ControlChr"    , "Control\x16", "ControlChr" },
                                      { 0xf71a, SYMF_ROMDIS0, "ClearLine"     , "ClearLi\x16", "ClearLine" },
                                      { 0xf77c, SYMF_ROMDIS0, "Char2Scr"      , "Char2Sc\x16", "Char2Scr" },
                                      { 0xf7e4, SYMF_ROMDIS0, "PrintA"        , "PrintA"     , "PrintA" },
                                      { 0xf816, SYMF_ROMDIS0, "AltChars"      , "AltChars"   , "AltChars" },
                                      { 0xf865, SYMF_ROMDIS0, "PrintStatus"   , "PrintSt\x16", "PrintStatus" },
                                      { 0xf88f, SYMF_ROMDIS0, "Reset"         , "Reset"      , "Reset" },
                                      { 0xf8af, SYMF_ROMDIS0, "BASICStart"    , "BASICSt\x16", "BASICStart" },
                                      { 0xf8b5, SYMF_ROMDIS0, "BASICResta\x16", "BASICRe\x16", "BASICRestart" },
                                      { 0xf920, SYMF_ROMDIS0, "HiresMode"     , "HiresMo\x16", "HiresMode" },
                                      { 0xf967, SYMF_ROMDIS0, "LoresMode"     , "LoresMo\x16", "LoresMode" },
                                      { 0xf9aa, SYMF_ROMDIS0, "ResetVIA"      , "ResetVIA"   , "ResetVIA" },
                                      { 0xf9c9, SYMF_ROMDIS0, "SetupText"     , "SetupTe\x16", "SetupText" },
                                      { 0xfa14, SYMF_ROMDIS0, "RamTest"       , "RamTest"    , "RamTest" },
                                      { 0xfa9f, SYMF_ROMDIS0, "PING"          , "PING"       , "PING" },
                                      { 0xfaa7, SYMF_ROMDIS0, "PingData"      , "PingData"   , "PingData" },
                                      { 0xfab5, SYMF_ROMDIS0, "SHOOT"         , "SHOOT"      , "SHOOT" },
                                      { 0xfabd, SYMF_ROMDIS0, "ShootData"     , "ShootDa\x16", "ShootData" },
                                      { 0xfab5, SYMF_ROMDIS0, "EXPLODE"       , "EXPLODE"    , "EXPLODE" },
                                      { 0xfacb, SYMF_ROMDIS0, "ExplodeData"   , "Explode\x16", "ExplodeData" },
                                      { 0xfae1, SYMF_ROMDIS0, "ZAP"           , "ZAP"        , "ZAP" },
                                      { 0xfb06, SYMF_ROMDIS0, "ZapData"       , "ZapData"    , "ZapData" },
                                      { 0xfb14, SYMF_ROMDIS0, "KeyClickH"     , "KeyClic\x16", "KeyClickH" },
                                      { 0xfb1c, SYMF_ROMDIS0, "KeyClickHD\x16", "KeyClic\x16", "KeyClickHData" },
                                      { 0xfb2a, SYMF_ROMDIS0, "KeyClickL"     , "KeyClic\x16", "KeyClickL" },
                                      { 0xfb32, SYMF_ROMDIS0, "KeyClickLD\x16", "KeyClic\x16", "KeyClickLData" },
                                      { 0xfb14, SYMF_ROMDIS0, "KeyClickH"     , "KeyClic\x16", "KeyClickH" },
                                      { 0xfb40, SYMF_ROMDIS0, "SOUND"         , "SOUND"      , "SOUND" },
                                      { 0xfbd0, SYMF_ROMDIS0, "PLAY"          , "PLAY"       , "PLAY" },
                                      { 0xfc18, SYMF_ROMDIS0, "MUSIC"         , "MUSIC"      , "MUSIC" },
                                      { 0xfc5e, SYMF_ROMDIS0, "MusicData"     , "MusicDa\x16", "MusicData" },
                                      { 0xfc78, SYMF_ROMDIS0, "CharSet"       , "CharSet"    , "CharSet" },
                                      { 0xff78, SYMF_ROMDIS0, "KeyCodeTab"    , "KeyCode\x16", "KeyCodeTab" },
                                      { 0, 0, { 0, }, { 0, }, NULL } };

enum
{
  MSHOW_VIA=0,
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

static struct disinf distab[] = { { "BRK", AM_IMP },  // 00
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
    tzstrpos( tz[TZ_DEBUG], 1, 19, vsptmp );
#if LOG_DEBUG
    if( debug_logfile )
    {
      fprintf( debug_logfile, "%s\n", vsptmp );
      fflush( debug_logfile );
    }
#endif
  }
  va_end( ap );
}

struct msym *mon_find_sym_by_addr( struct machine *oric, unsigned short addr )
{
  int i;

  for( i=0; i<numsyms; i++ )
  {
    if( (syms[i].flags&SYMF_MICRODISC) && ( oric->drivetype != DRV_MICRODISC ) )
      continue;

    if( (syms[i].flags&SYMF_JASMIN) && ( oric->drivetype != DRV_JASMIN ) )
      continue;

    if( oric->romdis )
    {
      if( (syms[i].flags&SYMF_ROMDIS1) == 0 )
        continue;
    } else {
      if( (syms[i].flags&SYMF_ROMDIS0) == 0 )
        continue;
    }

    if( addr == syms[i].addr )
      return &syms[i];
  }

  if( !defaultsyms ) return NULL;

  for( i=0; defaultsyms[i].name; i++ )
  {
    if( (defaultsyms[i].flags&SYMF_MICRODISC) && ( oric->drivetype != DRV_MICRODISC ) )
      continue;

    if( (defaultsyms[i].flags&SYMF_JASMIN) && ( oric->drivetype != DRV_JASMIN ) )
      continue;

    if( oric->romdis )
    {
      if( (defaultsyms[i].flags&SYMF_ROMDIS1) == 0 )
        continue;
    } else {
      if( (defaultsyms[i].flags&SYMF_ROMDIS0) == 0 )
        continue;
    }

    if( addr == defaultsyms[i].addr )
      return &defaultsyms[i];
  }

  return NULL;
}

struct msym *mon_find_sym_by_name( struct machine *oric, char *name )
{
  int i;

  for( i=0; i<numsyms; i++ )
  {
    if( (syms[i].flags&SYMF_MICRODISC) && ( oric->drivetype != DRV_MICRODISC ) )
      continue;

    if( (syms[i].flags&SYMF_JASMIN) && ( oric->drivetype != DRV_JASMIN ) )
      continue;

    if( oric->romdis )
    {
      if( (syms[i].flags&SYMF_ROMDIS1) == 0 )
        continue;
    } else {
      if( (syms[i].flags&SYMF_ROMDIS0) == 0 )
        continue;
    }

    if( !oric->symbolscase )
    {
      if( strcasecmp( name, syms[i].name ) == 0 )
        return &syms[i];
    } else {
      if( strcmp( name, syms[i].name ) == 0 )
        return &syms[i];
    }
  }

  if( !defaultsyms ) return NULL;

  for( i=0; defaultsyms[i].name; i++ )
  {
    if( (defaultsyms[i].flags&SYMF_MICRODISC) && ( oric->drivetype != DRV_MICRODISC ) )
      continue;

    if( (defaultsyms[i].flags&SYMF_JASMIN) && ( oric->drivetype != DRV_JASMIN ) )
      continue;

    if( oric->romdis )
    {
      if( (defaultsyms[i].flags&SYMF_ROMDIS1) == 0 )
        continue;
    } else {
      if( (defaultsyms[i].flags&SYMF_ROMDIS0) == 0 )
        continue;
    }

    if( !oric->symbolscase )
    {
      if( strcasecmp( name, defaultsyms[i].name ) == 0 )
        return &defaultsyms[i];
    } else {
      if( strcmp( name, defaultsyms[i].name ) == 0 )
        return &defaultsyms[i];
    }
  }

  return NULL;
}

SDL_bool mon_addsym( unsigned short addr, unsigned short flags, char *name )
{
  int i;

  if( ( !syms ) || ( symspace < (numsyms+1) ) )
  {
    struct msym *newbuf;

    newbuf = malloc( sizeof( struct msym ) * (symspace+64) );
    if( !newbuf ) return SDL_FALSE;

    if( ( syms ) && ( numsyms > 0 ) )
      memcpy( newbuf, syms, sizeof( struct msym ) * numsyms );

    free( syms );
    syms = newbuf;
    symspace += 64;

    for( i=numsyms; i<symspace; i++ )
      syms[i].name = NULL;
  }

  if( !syms[numsyms].name )
  {
    syms[numsyms].name = malloc( 128 );
    if( !syms[numsyms].name ) return SDL_FALSE;
  }

  syms[numsyms].addr = addr;
  syms[numsyms].flags = flags;
  strncpy( syms[numsyms].name, name, 128 );
  syms[numsyms].name[127] = 0;

  if( strlen( name ) > SNAME_LEN )
  {
    strncpy( syms[numsyms].sname, name, (SNAME_LEN-1) );
    syms[numsyms].sname[SNAME_LEN-1] = 22;
    syms[numsyms].sname[SNAME_LEN] = 0;
  } else {
    strcpy( syms[numsyms].sname, name );
  }

  if( strlen( name ) > SSNAME_LEN )
  {
    strncpy( syms[numsyms].ssname, name, (SSNAME_LEN-1) );
    syms[numsyms].ssname[SSNAME_LEN-1] = 22;
    syms[numsyms].ssname[SSNAME_LEN] = 0;
  } else {
    strcpy( syms[numsyms].ssname, name );
  }

  numsyms++;

  return SDL_TRUE;
}

void mon_enter( struct machine *oric )
{
  defaultsyms = NULL;
  switch( oric->type )
  {
    case MACH_ORIC1:
      defaultsyms = defsym_oric1;
      break;

    case MACH_ATMOS:
      defaultsyms = defsym_atmos;
      break;
  }
}

char *mon_disassemble( struct machine *oric, unsigned short *paddr )
{
  unsigned short iaddr, addr;
  unsigned char op, a1, a2;
  int i;
  char *tmpsname;
  char sname[SNAME_LEN+1];
  struct msym *csym;

  iaddr = *paddr;

  tmpsname = "";
  csym = mon_find_sym_by_addr( oric, iaddr );
  if( csym ) tmpsname = csym->sname;

  strcpy( sname, tmpsname );
  for( i=strlen(sname); i<SNAME_LEN; i++ )
    sname[i] = 32;
  sname[i] = 0;

  op = oric->cpu.read( &oric->cpu, (*paddr)++ );
  switch( distab[op].amode )
  {
    case AM_IMP:
      sprintf( distmp, "%s   %04X  %02X        %s", sname, iaddr, op, distab[op].name );
      break;
    
    case AM_IMM:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      sprintf( distmp, "%s   %04X  %02X %02X     %s #$%02X", sname, iaddr, op, a1, distab[op].name, a1 );
      break;

    case AM_ZP:
    case AM_ZPX:
    case AM_ZPY:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      csym = mon_find_sym_by_addr( oric, a1 );
      if( csym )
        sprintf( distmp, "%s   %04X  %02X %02X     %s %s", sname, iaddr, op, a1, distab[op].name, csym->sname );
      else
        sprintf( distmp, "%s   %04X  %02X %02X     %s $%02X", sname, iaddr, op, a1, distab[op].name, a1 );
      if( distab[op].amode == AM_ZP ) break;
      strcat( distmp, distab[op].amode == AM_ZPX ? ",X" : ",Y" );
      break;
    
    case AM_ABS:
    case AM_ABX:
    case AM_ABY:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      a2 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      csym = mon_find_sym_by_addr( oric, (a2<<8)|a1 );
      if( csym )
        sprintf( distmp, "%s   %04X  %02X %02X %02X  %s %s", sname, iaddr, op, a1, a2, distab[op].name, csym->sname );
      else
        sprintf( distmp, "%s   %04X  %02X %02X %02X  %s $%02X%02X", sname, iaddr, op, a1, a2, distab[op].name, a2, a1 );
      if( distab[op].amode == AM_ABS ) break;
      strcat( distmp, distab[op].amode == AM_ABX ? ",X" : ",Y" );
      break;

    case AM_ZIX:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      csym = mon_find_sym_by_addr( oric, a1 );
      if( csym )
        sprintf( distmp, "%s   %04X  %02X %02X     %s (%s,X)", sname, iaddr, op, a1, distab[op].name, csym->ssname );
      else
        sprintf( distmp, "%s   %04X  %02X %02X     %s ($%02X,X)", sname, iaddr, op, a1, distab[op].name, a1 );
      break;

    case AM_ZIY:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      csym = mon_find_sym_by_addr( oric, a1 );
      if( csym )     
        sprintf( distmp, "%s   %04X  %02X %02X     %s (%s),Y", sname, iaddr, op, a1, distab[op].name, csym->ssname );
      else
        sprintf( distmp, "%s   %04X  %02X %02X     %s ($%02X),Y", sname, iaddr, op, a1, distab[op].name, a1 );
      break;

    case AM_REL:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      addr = ((*paddr)+((signed char)a1))&0xffff;
      csym = mon_find_sym_by_addr( oric, addr );
      if( csym )
        sprintf( distmp, "%s   %04X  %02X %02X     %s %s", sname, iaddr, op, a1, distab[op].name, csym->sname );
      else
        sprintf( distmp, "%s   %04X  %02X %02X     %s $%04X", sname, iaddr, op, a1, distab[op].name, addr );
      break;

    case AM_IND:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      a2 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      csym = mon_find_sym_by_addr( oric, (a2<<8)|a1 );
      if( csym )
        sprintf( distmp, "%s   %04X  %02X %02X %02X  %s (%s)", sname, iaddr, op, a1, a2, distab[op].name, csym->sname );
      else
        sprintf( distmp, "%s   %04X  %02X %02X %02X  %s ($%02X%02X)", sname, iaddr, op, a1, a2, distab[op].name, a2, a1 );
      break;
    
    default:
      strcpy( distmp, "  WTF?" );
      break;
  }

  for( i=strlen(distmp); i<47; i++ )
    distmp[i] = 32;
  distmp[i] = 0;

  oric->cpu.anybp = SDL_FALSE;
  for( i=0; i<16; i++ )
  {
    if( oric->cpu.breakpoints[i] != -1 )
    {
      oric->cpu.anybp = SDL_TRUE;
      if( iaddr == oric->cpu.breakpoints[i] )
        distmp[SNAME_LEN+1] = '*';
    }
  }

  if( iaddr == oric->cpu.pc )
    distmp[SNAME_LEN+2] = '>';

  return distmp;
}

void mon_update_regs( struct machine *oric )
{
  int i;
  unsigned short addr;
  struct msym *csym;
  char stmp[48];

  tzprintfpos( tz[TZ_REGS], 2, 2, "PC=%04X  SP=01%02X    A=%02X  X=%02X  Y=%02X",
    oric->cpu.pc,
    oric->cpu.sp,
    oric->cpu.a,
    oric->cpu.x,
    oric->cpu.y );

  csym = mon_find_sym_by_addr( oric, oric->cpu.pc );
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

#if CYCLECOUNT
  tzprintfpos( tz[TZ_REGS], 2, 4, "CY=%08d", oric->cpu.cycles );
  tzprintfpos( tz[TZ_REGS], 2, 5, "FM=%06d RS=%03d", oric->frames, oric->vid_raster );
#endif
  tzstrpos( tz[TZ_REGS], 30, 4, "NV-BDIZC" );
  tzprintfpos( tz[TZ_REGS], 30, 5, "%01X%01X1%01X%01X%01X%01X%01X",
    oric->cpu.f_n,
    oric->cpu.f_v,
    oric->cpu.f_b,
    oric->cpu.f_d,
    oric->cpu.f_i,
    oric->cpu.f_z,
    oric->cpu.f_c );

  addr = oric->cpu.pc;
  for( i=0; i<10; i++ )
  {
    tzsetcol( tz[TZ_REGS], (addr==oric->cpu.pc) ? 1 : 2, 3 );
    tzstrpos( tz[TZ_REGS], 23, 7+i, "        " );
    tzstrpos( tz[TZ_REGS],  2, 7+i, mon_disassemble( oric, &addr ) );
  }
}

void mon_update_via( struct machine *oric )
{
  tzprintfpos( tz[TZ_VIA], 2, 2, "PCR=%02X ACR=%02X SR=%02X",
    oric->via.pcr,
    oric->via.acr,
    oric->via.sr );
  tzprintfpos( tz[TZ_VIA], 2, 3, "IFR=%02X IER=%02X",
    oric->via.ifr,
    oric->via.ier );  
  tzprintfpos( tz[TZ_VIA], 2, 5, "A=%02X DDRA=%02X  B=%02X DDRB=%02X",
    (oric->via.ora&oric->via.ddra)|(oric->via.ira&(~oric->via.ddra)),
    oric->via.ddra,
    (oric->via.orb&oric->via.ddrb)|(oric->via.irb&(~oric->via.ddrb)),
    oric->via.ddrb );
  tzprintfpos( tz[TZ_VIA], 2, 6, "CA1=%01X CA2=%01X   CB1=%01X CB2=%01X",
    oric->via.ca1,
    oric->via.ca2,
    oric->via.cb1,
    oric->via.cb2 );
  tzprintfpos( tz[TZ_VIA], 2, 8, "T1L=%02X%02X T1C=%04X",
    oric->via.t1l_h,
    oric->via.t1l_l,
    oric->via.t1c );
  tzprintfpos( tz[TZ_VIA], 2, 9, "T2L=%02X%02X T2C=%04X",
    oric->via.t2l_h,
    oric->via.t2l_l,
    oric->via.t2c );

  tzprintfpos( tz[TZ_VIA], 2, 11, "TAPE OFFS = %07d", oric->tapeoffs );
  tzprintfpos( tz[TZ_VIA], 2, 12, "TAPE LEN  = %07d", oric->tapelen );
  tzprintfpos( tz[TZ_VIA], 2, 13, "COUNT     = %07d", oric->tapecount );
  tzprintfpos( tz[TZ_VIA], 2, 14, "BIT = %02X  DATA = %1X",
    (oric->tapebit+9)%10,
    oric->tapetime == TAPE_1_PULSE );
  tzprintfpos( tz[TZ_VIA], 2, 15, "MOTOR = %1X", oric->tapemotor );
}

void mon_update_ay( struct machine *oric )
{
  tzprintfpos( tz[TZ_AY], 2, 2, "BC1=%1X BDIR=%1X REG=%02X",
    (oric->ay.bmode & AYBMF_BC1) ? 1 : 0,
    (oric->ay.bmode & AYBMF_BDIR) ? 1 : 0,
    oric->ay.creg );
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
  if( oric->drivetype == DRV_NONE )
    return;

  tzprintfpos( tz[TZ_DISK], 2, 2, "STATUS=%02X        ROMDIS=%1X", oric->wddisk.r_status, oric->romdis );
  tzprintfpos( tz[TZ_DISK], 2, 3, "DRIVE=%02X SIDE=%02X TRACK=%02X",
    oric->wddisk.c_drive,
    oric->wddisk.c_side,
    oric->wddisk.c_track );

  switch( oric->drivetype )
  {
    case DRV_MICRODISC:
      tzprintfpos( tz[TZ_DISK], 2, 5, "MDSTAT=%02X INTRQ=%1X DRQ=%1X",
        oric->md.status,
        oric->md.intrq!=0,
        oric->md.drq!=0 );
      tzprintfpos( tz[TZ_DISK], 2, 6, "EPROM=%1X", oric->md.diskrom );
      break;
    
    case DRV_JASMIN:
      break;
  }
}

void mon_update_mwatch( struct machine *oric )
{
  unsigned short addr;
  int j,k,l;
  unsigned int v;

  addr = mw_addr;
  for( j=0; j<19; j++ )
  {
    sprintf( vsptmp, "%04X  ", addr );
    for( k=0; k<8; k++ )
    {
      sprintf( &vsptmp[128], "%02X ", oric->cpu.read( &oric->cpu, addr+k ) );
      strcat( vsptmp, &vsptmp[128] );
    }
    l = strlen( vsptmp );
    vsptmp[l++] = 32;
    vsptmp[l++] = '\'';
    for( k=0; k<8; k++ )
    {
      v = oric->cpu.read( &oric->cpu, addr++ );
      vsptmp[l++] = ((v>31)&&(v<128))?v:'.';
    }
    vsptmp[l++] = '\'';
    vsptmp[l++] = 0;
    tzstrpos( tz[TZ_MEMWATCH], 1, j+1, vsptmp );
  }

  if( mw_mode == 0 ) return;

  makebox( tz[TZ_MEMWATCH], 17, 8, 7, 3, 2, 3 );
  tzstrpos( tz[TZ_MEMWATCH], 18, 9, mw_ibuf );
}

void mon_render( struct machine *oric )
{
  video_show( oric );
  mon_update_regs( oric );
  
  switch( mshow )
  {
    case MSHOW_VIA:
      mon_update_via( oric );
      draw_textzone( tz[TZ_VIA] );
      break;
    
    case MSHOW_AY:
      mon_update_ay( oric );
      draw_textzone( tz[TZ_AY] );
      break;
    
    case MSHOW_DISK:
      mon_update_disk( oric );
      draw_textzone( tz[TZ_DISK] );
      break;
  }

  switch( cshow )
  {
    case CSHOW_CONSOLE:
      draw_textzone( tz[TZ_MONITOR] );
      break;
    
    case CSHOW_DEBUG:
      draw_textzone( tz[TZ_DEBUG] );
      break;

    case CSHOW_MWATCH:
      mon_update_mwatch( oric );
      draw_textzone( tz[TZ_MEMWATCH] );
      break;
}
  draw_textzone( tz[TZ_REGS] );
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

void mon_init( struct machine *oric )
{
  mon_bpmsg[0] = 0;
  mshow = MSHOW_VIA;
  cshow = CSHOW_CONSOLE;
  mon_asmmode = SDL_FALSE;
  mon_start_input();
  mon_show_curs();
  mon_addr = oric->cpu.pc;
  lastcmd = 0;
#if LOG_DEBUG
  debug_logfile = fopen( debug_logname, "w" );
#endif
}

void mon_shut( void )
{
  int i;
  if( syms )
  {
    for( i=0; i<symspace; i++ )
      if( syms[i].name ) free( syms[i].name );
    free( syms );
  }
  syms = NULL;
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

SDL_bool mon_new_symbols( char *fname )
{
  FILE *f;
  int i, j;
  unsigned int v;

  f = fopen( fname, "r" );
  if( !f )
  {
    mon_printf( "Unable to open '%s'", fname );
    return SDL_FALSE;
  }

  numsyms = 0;
  while( !feof( f ) )
  {
    char linetmp[256];

    if( !fgets( linetmp, 256, f ) ) break;

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

    printf( "'%s' = %04X\n", linetmp, v );
    fflush( stdout );

    if( v >= 0xc000 )
      mon_addsym( v, SYMF_ROMDIS1, linetmp );
    else
      mon_addsym( v, SYMF_ROMDIS0|SYMF_ROMDIS1, linetmp );
  }

  fclose( f );
  
  mon_printf( "Symbols loaded from '%s'", fname );
  return SDL_TRUE;
}

SDL_bool mon_cmd( char *cmd, struct machine *oric, SDL_bool *needrender )
{
  int i, j, k, l;
  unsigned int v, w;
  SDL_bool done = SDL_FALSE;
  unsigned char *tmem;
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

        case 'z':  // Zap
          numsyms = 0;
          defaultsyms = NULL;
          mon_str( "Symbols zapped!" );
          break;

        case 'l':  // Load
          if( cmd[i+1] != 32 )
          {
            mon_str( "???" );
            break;
          }

          i+=2;

          mon_new_symbols( &cmd[i] );
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
                mon_printf( "%02d: $04X %c%c%c",
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
            oric->cpu.membreakpoints[j].lastval = oric->cpu.read( &oric->cpu, v&0xffff );
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
        
        mw_addr = v;
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
          sprintf( &vsptmp[128], "%02X ", oric->cpu.read( &oric->cpu, mon_addr+k ) );
          strcat( vsptmp, &vsptmp[128] );
        }
        l = strlen( vsptmp );
        vsptmp[l++] = 32;
        vsptmp[l++] = '\'';
        for( k=0; k<8; k++ )
        {
          v = oric->cpu.read( &oric->cpu, mon_addr++ );
          vsptmp[l++] = ((v>31)&&(v<128))?v:'.';
        }
        vsptmp[l++] = '\'';
        vsptmp[l++] = 0;
        mon_str( vsptmp );
      }
      break;
    
    case 'd':
      lastcmd = cmd[i];
      i++;
      if( mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_FALSE, SDL_FALSE, SDL_TRUE ) )
        mon_addr = v;

      for( j=0; j<16; j++ )
        mon_str( mon_disassemble( oric, &mon_addr ) );
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
            tmem[j] = oric->cpu.read( &oric->cpu, v+j );

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
          mon_str( "  bsm [rwc] <addr>      - Set mem breakpoint" );
          mon_str( "  bz                    - Zap breakpoints" );
          mon_str( "  bzm                   - Zap mem breakpoints" );
          mon_str( "  d <addr>              - Disassemble" );
          mon_str( "  m <addr>              - Dump memory" );
          mon_str( "---- MORE" );
          helpcount++;
          break;
        
        case 1:
          mon_str( "  mw <addr>             - Memory watch at addr" );
          mon_str( "  r <reg> <val>         - Set <reg> to <val>" );
          mon_str( "  q, x or qm            - Quit monitor" );
          mon_str( "  qe                    - Quit emulator" );
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
  int i, j, amode;
  unsigned short val;

  i=0;
  while( isws( ibuf[i] ) ) i++;

  if( ibuf[i] == 0 ) return SDL_TRUE;

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

  mon_oprintf( mon_disassemble( oric, &mon_addr ) );
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
      
      default:
        break;
    }

    switch( ev->key.keysym.unicode )
    {
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
  }

  switch( ev->key.keysym.unicode )
  {
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
  int i;

  done = SDL_FALSE;
  switch( ev->key.keysym.sym )
  {
    case SDLK_UP:
      mw_addr -= kshifted ? 8*18 : 8;
      *needrender = SDL_TRUE;
      break;

    case SDLK_DOWN:
      mw_addr += kshifted ? 8*18 : 8;
      *needrender = SDL_TRUE;
      break;

    case SDLK_PAGEUP:
      mw_addr -= 8*18;
      *needrender = SDL_TRUE;
      break;

    case SDLK_PAGEDOWN:
      mw_addr += 8*18;
      *needrender = SDL_TRUE;
      break;
    
    case SDLK_RETURN:
      if( mw_mode != 1 ) break;
      mw_ibuf[5] = 0;
      mw_mode = 0;
      i = 0;
      if( mon_getnum( oric, &v, mw_ibuf, &i, SDL_FALSE, SDL_FALSE, SDL_FALSE, SDL_FALSE ) )
        mw_addr = v;
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
  m6502_inst( &oric->cpu, SDL_FALSE, mon_bpmsg );
  via_clock( &oric->via, oric->cpu.icycles );
  ay_ticktock( &oric->ay, oric->cpu.icycles );
  if( oric->drivetype ) wd17xx_ticktock( &oric->wddisk, oric->cpu.icycles );
  if( oric->cpu.rastercycles <= 0 )
  {
    video_doraster( oric );
    oric->cpu.rastercycles += oric->cyclesperraster;
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
    case SDL_KEYUP:
      switch( ev->key.keysym.sym )
      {
#if CYCLECOUNT
        case SDLK_F9:
          oric->cpu.cycles = 0;
          *needrender = SDL_TRUE;
          break;
#endif
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
          kshifted = SDL_FALSE;
          break;

        case SDLK_F2:
          // In case we're on a breakpoint
          m6502_inst( &oric->cpu, SDL_FALSE, mon_bpmsg );
          via_clock( &oric->via, oric->cpu.icycles );
          ay_ticktock( &oric->ay, oric->cpu.icycles );
          if( oric->drivetype ) wd17xx_ticktock( &oric->wddisk, oric->cpu.icycles );
          if( oric->cpu.rastercycles <= 0 )
          {
            video_doraster( oric );
            oric->cpu.rastercycles += oric->cyclesperraster;
          }
          *needrender = SDL_TRUE;
          setemumode( oric, NULL, EM_RUNNING );
          break;

        case SDLK_F3:
          cshow = (cshow+1)%CSHOW_LAST;
          *needrender = SDL_TRUE;
          break;

        case SDLK_F4:
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

        case SDLK_F11:
          if( oric->cpu.read( &oric->cpu, oric->cpu.pc ) == 0x20 ) // JSR instruction?
          {
            Uint16 newpc;
            unsigned int endticks;

            newpc = oric->cpu.pc+3;
            
            endticks = SDL_GetTicks()+5000; // 5 seconds to comply
            while( ( oric->cpu.pc != newpc ) && ( SDL_GetTicks() < endticks ) )
              steppy_step( oric );

            *needrender = SDL_TRUE;
            donkey = SDL_TRUE;
            break;
          }
        
        case SDLK_F10:
          steppy_step( oric );
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
