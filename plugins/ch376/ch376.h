/********************************************************************
 *                                                                  *
 * Very minimal CH376 emulation                                     *
 *                                                                  *
 * Code:                                                            *
 *   Jérôme 'Jede' Debrune                                          *
 *   Philippe 'OffseT' Rimauro of Futurs'                           *
 *                                                                  *
 ** ch376.h *********************************************************/

#ifndef LOCAL_CH376_H
#define LOCAL_CH376_H

/* #define DEBUG_CH376 */

/* /// "Portable types" */

#if defined(__MORPHOS__) || defined (__AMIGA__) || defined (__AROS__)

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif
#ifndef DOS_DOS_H
#include <dos/dos.h>
#endif

typedef UBYTE CH376_U8;
typedef BYTE CH376_S8;
typedef UWORD CH376_U16;
typedef WORD CH376_S16;
typedef ULONG CH376_U32;
typedef LONG CH376_S32;
typedef UQUAD CH376_U64;
typedef QUAD CH376_S64;
typedef BOOL CH376_BOOL;
typedef BPTR CH376_FILE;
typedef BPTR CH376_LOCK;
typedef struct FileInfoBlock * CH376_DIR;
typedef struct { struct Library *DOSBase; } CH376_CONTEXT;

#define CH376_TRUE  TRUE
#define CH376_FALSE FALSE

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

#elif defined(WIN32)

#include <windows.h>

typedef UINT8 CH376_U8;
typedef INT8 CH376_S8;
typedef UINT16 CH376_U16;
typedef INT16 CH376_S16;
typedef UINT32 CH376_U32;
typedef INT32 CH376_S32;
typedef UINT64 CH376_U64;
typedef INT64 CH376_S64;
typedef BOOL CH376_BOOL;
typedef HANDLE CH376_FILE;
typedef struct _CH376_LOCK { TCHAR path[MAX_PATH]; HANDLE handle; } * CH376_LOCK;
typedef WIN32_FIND_DATA * CH376_DIR;

typedef struct { void *dummy; } CH376_CONTEXT;

#define CH376_TRUE  TRUE
#define CH376_FALSE FALSE

#define UNUSED

#elif defined(__unix__)

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>

typedef uint8_t CH376_U8;
typedef int8_t CH376_S8;
typedef uint16_t CH376_U16;
typedef int16_t CH376_S16;
typedef uint32_t CH376_U32;
typedef int32_t CH376_S32;
typedef uint64_t CH376_U64;
typedef int64_t CH376_S64;
typedef bool CH376_BOOL;
typedef FILE * CH376_FILE;
typedef char * CH376_LOCK;
typedef struct _CH376_DIR { DIR *handle; struct dirent *entry; } * CH376_DIR;
typedef struct { void *dummy; } CH376_CONTEXT;

#define CH376_TRUE  true
#define CH376_FALSE false

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

#else
#error "FixMe!"
#endif

/* /// */

/* /// "CH376 public I/O API" */

struct ch376;

// Initialization
struct ch376 * ch376_create(void *user_data);
void ch376_destroy(struct ch376 *ch376);
void ch376_reset(struct ch376 *ch376);

// Configuration
void ch376_set_sdcard_drive_path(struct ch376 *ch376, const char *path);
void ch376_set_usb_drive_path(struct ch376 *ch376, const char *path);
const char * ch376_get_sdcard_drive_path(struct ch376 *ch376);
const char * ch376_get_usb_drive_path(struct ch376 *ch376);

// Runtime
void ch376_write_command_port(struct ch376 *ch376, CH376_U8 command);
void ch376_write_data_port(struct ch376 *ch376, CH376_U8 data);
CH376_U8 ch376_read_command_port(struct ch376 *ch376);
CH376_U8 ch376_read_data_port(struct ch376 *ch376);

/* /// */

#endif
