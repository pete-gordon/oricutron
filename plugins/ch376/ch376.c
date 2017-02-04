/********************************************************************
 *                                                                  *
 * Very minimal CH376 emulation (21.12.2016)                        *
 *                                                                  *
 * Code:                                                            *
 *   Jérôme 'Jede' Debrune                                          *
 *   Philippe 'OffseT' Rimauro                                      *
 *                                                                  *
 ** ch376.c *********************************************************/

/* /// "Portable includes" */

#if defined(__MORPHOS__) || defined (__AMIGA__) || defined (__AROS__)

#define __NOLIBBASE__

#include <proto/exec.h>
#include <proto/dos.h>

extern struct Library *SysBase;

#elif defined(WIN32)

#include <windows.h>
#include <stdio.h>
#include <strsafe.h>
#include <Shlwapi.h>

#elif __unix__

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/statvfs.h>
#include <sys/stat.h>

#else
#error "FixMe!"
#endif

#include "ch376.h"

/* /// */

/* /// "CH376 interface commands and constants" */

// Chip version
#define CH376_DATA_IC_VER 3

// Commands
#define CH376_CMD_NONE			    0x00
#define CH376_CMD_GET_IC_VER		0x01
#define CH376_CMD_SET_BAUDRATE 		0x02 /*Not emulated*/
#define CH376_CMD_GET_ENTER_SLEEP	0x03 /*Not emulated*/
#define CH376_CMD_RESET_ALL 		0x05 /*Not emulated*/
#define CH376_CMD_CHECK_EXIST		0x06 
#define CH376_CMD_GET_FILE_SIZE 	0x0c /*Not emulated*/
#define CH376_CMD_SET_USB_MODE		0x15
#define CH376_CMD_GET_STATUS		0x22
#define CH376_CMD_RD_USB_DATA0		0x27
#define CH376_CMD_WR_REQ_DATA		0x2d
#define CH376_CMD_SET_FILE_NAME		0x2f
#define CH376_CMD_DISK_CONNECT 		0x30 /*Not emulated*/
#define CH376_CMD_DISK_MOUNT		0x31
#define CH376_CMD_FILE_OPEN			0x32
#define CH376_CMD_FILE_ENUM_GO		0x33
#define CH376_CMD_FILE_CREATE		0x34
#define CH376_CMD_FILE_ERASE		0x35
#define CH376_CMD_FILE_CLOSE		0x36
#define CH376_CMD_BYTE_LOCATE		0x39
#define CH376_CMD_BYTE_READ			0x3a
#define CH376_CMD_BYTE_RD_GO		0x3b
#define CH376_CMD_BYTE_WRITE		0x3c
#define CH376_CMD_BYTE_WR_GO		0x3d
#define CH376_CMD_DISK_CAPACITY 	0x3e /*Not emulated*/
#define CH376_CMD_DISK_QUERY		0x3f
#define CH376_CMD_DISK_RD_GO		0x55







#define CH376_ARG_SET_USB_MODE_INVALID  0x00
#define CH376_ARG_SET_USB_MODE_SD_HOST  0x03
#define CH376_ARG_SET_USB_MODE_USB_HOST 0x06

// Status & errors
#define CH376_ERR_OPEN_DIR		0x41
#define CH376_ERR_MISS_FILE		0x42
#define CH376_ERR_FOUND_NAME 	0x43 /*Not emulated*/
#define CH376_ERR_DISK_DISCON 	0x82 /*Not emulated*/
#define CH376_ERR_LARGE_SECTOR 	0x84 /*Not emulated*/
#define CH376_ERR_TYPE_ERROR 	0x92 /*Not emulated*/
#define CH376_ERR_BPB_ERROR 	0xa1 /*Not emulated*/
#define CH376_ERR_DISK_FULL 	0xb1 /*Not emulated*/
#define CH376_ERR_FDT_OVER 		0xb2 /*Not emulated*/
#define CH376_ERR_FILE_CLOSE 	0xb4 /*Not emulated*/



#define CH376_RET_SUCCESS 0x51
#define CH376_RET_ABORT   0x5f

#define CH376_INT_SUCCESS		0x14
#define CH376_INT_CONNECT 		0x15 /*Not emulated*/
#define CH376_INT_DISCONNECT	0x16 /*Not emulated*/
#define CH376_INT_BUF_OVER 		0x17 /*Not emulated*/
#define CH376_INT_USB_READY 	0x18 /*Not emulated*/
#define CH376_INT_DISK_READ		0x1d
#define CH376_INT_DISK_WRITE 	0x1e /*Not emulated*/
#define CH376_INT_DISK_ERR 		0x1f /*Not emulated*/





/* /// */

/* /// "CH376 data structures" */

#define CMD_DATA_REQ_SIZE 0xff

// Attributes
#define DIR_ATTR_READ_ONLY 0x01
#define DIR_ATTR_HIDDEN    0x02
#define DIR_ATTR_SYSTEM    0x04
#define DIR_ATTR_VOLUME_ID 0x08
#define DIR_ATTR_DIRECTORY 0x10
#define DIR_ATTR_ARCHIVE   0x20
// Time = (Hour<<11) + (Minute<<5) + (Second>>1)
#define DIR_MAKE_FILE_TIME(h, m, s) ((h<<11) + (m<<5) + (s>>1))
// Date = ((Year-1980)<<9) + (Month<<5) + Day
#define DIR_MAKE_FILE_DATE(y, m, d) (((y-1980)<<9) + (m<<5) + d)

#pragma pack(1)

// Important: All values in all structures are stored in little-endian (either in 16 or 32 bit)
struct FatDirInfo
{
    char     DIR_Name[11];
    CH376_U8 DIR_Attr;
    CH376_U8 DIR_NTRes;
    CH376_U8 DIR_CrtTimeTenth;
    CH376_U8 DIR_CrtTime[2];
    CH376_U8 DIR_CrtDate[2];
    CH376_U8 DIR_LstAccDate[2];
    CH376_U8 DIR_FstClusHI[2];
    CH376_U8 DIR_WrtTime[2];
    CH376_U8 DIR_WrtDate[2];
    CH376_U8 DIR_FstClusLO[2];
    CH376_U8 DIR_FileSize[4];
};

struct MountInfo
{
    CH376_U8 MOUNT_DeviceType;
    CH376_U8 MOUNT_RemovableMedia;
    CH376_U8 MOUNT_Versions;
    CH376_U8 MOUNT_DataFormatAndEtc;
    CH376_U8 MOUNT_AdditionalLength;
    CH376_U8 MOUNT_Reserved1;
    CH376_U8 MOUNT_Reserved2;
    CH376_U8 MOUNT_MiscFlag;
    char     MOUNT_VendorIdStr[8];
    char     MOUNT_ProductIdStr[16];
    char     MOUNT_ProductRevStr[4];
};

struct DiskQuery
{
    CH376_U8 DISK_TotalSector[4];
    CH376_U8 DISK_FreeSector[4];
    CH376_U8 DISK_DiskFat;
};

union CommandData
{
    char              CMD_FileName[14];     // [W/O] CH376_CMD_SET_FILE_NAME
    struct MountInfo  CMD_MountInfo;        // [R/O] CH376_CMD_DISK_MOUNT
    struct FatDirInfo CMD_FatDirInfo;       // [R/O] CH376_CMD_FILE_OPEN, CH376_CMD_FILE_ENUM_GO
    struct DiskQuery  CMD_DiskQuery;        // [R/O] CH376_CMD_DISK_QUERY
    CH376_U8          CMD_FileSeek[4];      // [R/W] CH376_CMD_BYTE_LOCATE
    CH376_U8          CMD_FileReadWrite[2]; // [W/O] CH376_CMD_BYTE_READ, CH376_CMD_BYTE_WRITE
    CH376_U8          CMD_IOBuffer[255];    // [R/W] CH376_CMD_BYTE_READ, CH376_CMD_BYTE_RD_GO, CH376_CMD_BYTE_WRITE, CH376_CMD_BYTE_WR_GO
    CH376_U8          CMD_CheckByte;        // [R/W] CH376_CMD_CHECK_EXIST
};

#pragma pack()

/* /// */

/* /// "CH376 runtime structure" */

struct ch376
{
    CH376_CONTEXT context;

    CH376_U8 command;
    CH376_U8 command_status;

    CH376_U8 interface_status;
    CH376_U8 usb_mode;

    union CommandData cmd_data;
    CH376_U8 nb_bytes_in_cmd_data;
    CH376_U8 pos_rw_in_cmd_data;

    CH376_U16 bytes_to_read_write;

    CH376_LOCK root_dir_lock;
    CH376_LOCK current_dir_lock;
    CH376_FILE current_file;
    CH376_DIR current_directory_browsing;

    char *sdcard_drive_path;
    char *usb_drive_path;
};

/* /// */

/* /// "CH376 private prototypes" */

static char *clone_string(const char *string);
static void clear_structure(struct ch376 *ch376);
static void cancel_all_io(struct ch376 *ch376);
static int normalize_char(char c);
static const char * normalize_file_name(const char *file_name, char *normalized_file_name);
static const char * trim_file_name(const char *file_name, char *trimmed_file_name);
static void file_read_chunk(struct ch376 *ch376);
static void file_write_chunk(struct ch376 *ch376);

/* /// */

/* /// "Portable operating system-dependent prototypes" */

// Allocate "size" byte of memory
static void * system_alloc_mem(int size);

// Free memory previously allocated with system_alloc_mem
// It is safe to call it with NULL
static void system_free_mem(void *ptr);

// Initialize the operating system dependent context
// User data may be used if external data is required for such initialization
static CH376_BOOL system_init_context(CH376_CONTEXT *context, UNUSED void *user_data);

// Release the operating system dependent context previously initialized by system_init_context
static void system_clean_context(CH376_CONTEXT *context);

// Fill the disk_info structure with information of the disk from which the lock was obtained
static CH376_BOOL system_get_disk_info(CH376_CONTEXT *context, CH376_LOCK root_lock, struct DiskQuery *disk_info);

// Try to lock the directory from the provided lock location and return an associated lock
// Return 0 if a directory could not be locked (not a directory or not existing)
static CH376_LOCK system_obtain_directory_lock(CH376_CONTEXT *context, const char *dir_path, CH376_LOCK root_lock);

// Release a lock previouly obtained from system_obtain_directory_lock
// It is safe to call it with a 0
static void system_release_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock);

// Clone a directory lock
static CH376_LOCK system_clone_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock);

// Return a file handle corresponding to the given file located in the provited directory lock
// Return 0 if the file could not be found
static CH376_FILE system_file_open_existing(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock);

// Create a file in the provited directory lock and return its file handle
// Return 0 if the file could not be created
static CH376_FILE system_file_open_new(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock);

// Close a file opened with system_file_open_existing or system_file_open_new
static void system_file_close(CH376_CONTEXT *context, CH376_FILE file);

// Seek into a file (absolute position from beginning of the file)
static CH376_S32 system_file_seek(CH376_CONTEXT *context, CH376_FILE file, int pos);

// Read a part of a file
static CH376_S32 system_file_read(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size);

// Write some data into a file
static CH376_S32 system_file_write(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size);

// Start examining a directory
// Return information about the directory itself
static CH376_DIR system_start_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock);

// Get the next entry from a directory on which a examine session was started using system_start_examine_directory
static CH376_BOOL system_go_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock, CH376_DIR fib, struct FatDirInfo *dir_info);

// Finish a directory examine session (release all related resources)
static void system_finish_examine_directory(CH376_CONTEXT *context, CH376_DIR fib);

/* /// */

#if defined(__MORPHOS__) || defined (__AMIGA__) || defined (__AROS__)

/* /// "Amiga system functions" */

#ifdef DEBUG_CH376
#include <clib/debug_protos.h>
#define dbg_printf kprintf
#else
#define dbg_printf(...)
#endif

static void * system_alloc_mem(int size)
{
    return AllocVec(size, MEMF_ANY);
}

static void system_free_mem(void *ptr)
{
    FreeVec(ptr);
}

static CH376_BOOL system_init_context(CH376_CONTEXT *context, UNUSED void *user_data)
{
  context->DOSBase = OpenLibrary(DOSNAME, 0L);
  return (context->DOSBase != NULL);
}

static void system_clean_context(CH376_CONTEXT *context)
{
  if(context->DOSBase != NULL)
    CloseLibrary(context->DOSBase);
}

static CH376_BOOL system_get_disk_info(CH376_CONTEXT *context, CH376_LOCK root_lock, struct DiskQuery *disk_info)
{
    struct Library *DOSBase = context->DOSBase;
    BOOL got_info = CH376_FALSE;

    if(disk_info)
    {
        struct InfoData *info = system_alloc_mem(sizeof(struct InfoData));

        if(info)
        {
            if(Info(root_lock, info) == DOSTRUE)
            {
                QUAD total_sector = (info->id_NumBlocks * info->id_BytesPerBlock) / 512;
                QUAD free_sector = ((info->id_NumBlocks - info->id_NumBlocksUsed) * info->id_BytesPerBlock) / 512;

                disk_info->DISK_TotalSector[0] = (total_sector & 0x000000ff) >>  0;
                disk_info->DISK_TotalSector[1] = (total_sector & 0x0000ff00) >>  8;
                disk_info->DISK_TotalSector[2] = (total_sector & 0x00ff0000) >> 16;
                disk_info->DISK_TotalSector[3] = (total_sector & 0xff000000) >> 32;

                disk_info->DISK_FreeSector[0] = (free_sector & 0x000000ff) >>  0;
                disk_info->DISK_FreeSector[1] = (free_sector & 0x0000ff00) >>  8;
                disk_info->DISK_FreeSector[2] = (free_sector & 0x00ff0000) >> 16;
                disk_info->DISK_FreeSector[3] = (free_sector & 0xff000000) >> 32;

                disk_info->DISK_DiskFat = 0x0c; // FAT32?

                got_info = TRUE;
            }
            system_free_mem(info);
        }
    }

    return got_info;
}

static CH376_LOCK system_obtain_directory_lock(CH376_CONTEXT *context, const char *dir_path, CH376_LOCK root_lock)
{
    struct Library *DOSBase = context->DOSBase;
    BPTR lock;
    BPTR old_lock = (BPTR)0;

    dbg_printf("system_obtain_directory_lock: %s\n", dir_path);

    if(root_lock)
    {
        old_lock = CurrentDir(root_lock);
    }

    lock = Lock(dir_path, SHARED_LOCK);

    if(lock != (BPTR)0)
    {
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);

        if(fib)
        {
            if(Examine(lock, fib) == DOSFALSE || fib->fib_DirEntryType < 0)
            {
                UnLock(lock);
                lock = (BPTR)0;
            }
            FreeDosObject(DOS_FIB, fib);
        }
    }

    if(old_lock)
    {
        CurrentDir(old_lock);
    }

    return lock;
}

static void system_release_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    struct Library *DOSBase = context->DOSBase;
    UnLock(dir_lock);
}

static CH376_LOCK system_clone_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    struct Library *DOSBase = context->DOSBase;
    return DupLock(dir_lock);
}

static BPTR file_open(CH376_CONTEXT *context, CONST_STRPTR file_name, BPTR root_lock, LONG mode)
{
    struct Library *DOSBase = context->DOSBase;
    BPTR old_lock = (BPTR)0;
    BPTR file;

    if(root_lock)
    {
        old_lock = CurrentDir(root_lock);
    }

    file = Open(file_name, mode);

    if(old_lock)
    {
        CurrentDir(old_lock);
    }

    return file;
}

static CH376_FILE system_file_open_existing(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    return file_open(context, file_name, root_lock, MODE_OLDFILE);
}

static CH376_FILE system_file_open_new(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    return file_open(context, file_name, root_lock, MODE_NEWFILE);
}

static void system_file_close(CH376_CONTEXT *context, CH376_FILE file)
{
    struct Library *DOSBase = context->DOSBase;
    Close(file);
}

static CH376_S32 system_file_seek(CH376_CONTEXT *context, CH376_FILE file, int pos)
{
    struct Library *DOSBase = context->DOSBase;

    dbg_printf("system_file_seek trying to seek to position %d\n", pos);

    if(file)
        return Seek(file, pos, OFFSET_BEGINNING);
    else
        return -1;
}

static CH376_S32 system_file_read(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size)
{
    struct Library *DOSBase = context->DOSBase;

    dbg_printf("system_file_read trying to read %d bytes\n", size);

    if(file)
        return Read(file, buffer, size);
    else
        return -1;
}

static CH376_S32 system_file_write(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size)
{
    struct Library *DOSBase = context->DOSBase;

    dbg_printf("system_file_write trying to write %d bytes\n", size);

    if(file)
        return Write(file, buffer, size);
    else
        return -1;
}

static CH376_DIR system_start_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    struct Library *DOSBase = context->DOSBase;
    struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);

    if(fib)
    {
        if(Examine(dir_lock, fib) == DOSFALSE)
        {
            FreeDosObject(DOS_FIB, fib);
            fib = NULL;
        }
    }

    return fib;
}

static CH376_BOOL system_go_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock, CH376_DIR fib, struct FatDirInfo *dir_info)
{
    struct Library *DOSBase = context->DOSBase;
    BOOL is_done = FALSE;

    if(fib) while(!is_done && ExNext(dir_lock, fib) == DOSTRUE)
    {
        if(normalize_file_name(fib->fib_FileName, dir_info->DIR_Name))
        {
            dir_info->DIR_Attr = 0;

            if(fib->fib_DirEntryType > 0)
                dir_info->DIR_Attr |= DIR_ATTR_DIRECTORY;
            if((fib->fib_Protection & FIBF_WRITE) != 0)
                dir_info->DIR_Attr |= DIR_ATTR_READ_ONLY;
            if((fib->fib_Protection & FIBF_ARCHIVE) == 0)
                dir_info->DIR_Attr |= DIR_ATTR_ARCHIVE;
              //if(fib->fib_Protection & FIBF_DELETE)
              //if(fib->fib_Protection & FIBF_EXECUTE)
              //if(fib->fib_Protection & FIBF_READ)
              //if(fib->fib_Protection & FIBF_PURE)
              //if(fib->fib_Protection & FIBF_SCRIPT)
            dbg_printf("system_go_examine_directory defined file attributes: %d\n", dir_info->DIR_Attr);

            dir_info->DIR_NTRes = 0;
            dir_info->DIR_CrtTimeTenth = 0;
            dir_info->DIR_CrtTime[0] = 0;
            dir_info->DIR_CrtTime[1] = 0;
            dir_info->DIR_CrtDate[0] = 0;
            dir_info->DIR_CrtDate[1] = 0;
            dir_info->DIR_LstAccDate[0] = 0;
            dir_info->DIR_LstAccDate[1] = 0;
            dir_info->DIR_FstClusHI[0] = 0;
            dir_info->DIR_FstClusHI[1] = 0;
            dir_info->DIR_WrtTime[0] = 0;
            dir_info->DIR_WrtTime[1] = 0;
            dir_info->DIR_WrtDate[0] = 0;
            dir_info->DIR_WrtDate[1] = 0;
            dir_info->DIR_FstClusLO[0] = 0;
            dir_info->DIR_FstClusLO[1] = 0;
            dir_info->DIR_FileSize[0] = (fib->fib_Size & 0x000000ff) >>  0;
            dir_info->DIR_FileSize[1] = (fib->fib_Size & 0x0000ff00) >>  8;
            dir_info->DIR_FileSize[2] = (fib->fib_Size & 0x00ff0000) >> 16;
            dir_info->DIR_FileSize[3] = (fib->fib_Size & 0xff000000) >> 24;

            is_done = TRUE;
        }
    }

    return is_done;
}

static void system_finish_examine_directory(CH376_CONTEXT *context, CH376_DIR fib)
{
    struct Library *DOSBase = context->DOSBase;

    if(fib)
    {
        FreeDosObject(DOS_FIB, fib);
    }
}

/* /// */

#elif defined(WIN32)

/* /// "Windows system functions" */

#ifdef DEBUG_CH376
void dbg_printf(char *fmt, ...);

/*
static void dbg_printf(LPCTSTR str, ...)
{
    TCHAR buffer[256];

    va_list args;
    va_start(args, str);
    _vsnprintf_s(buffer, sizeof(buffer)-1, sizeof(buffer)-1, str, args);
    OutputDebugString(buffer);
    va_end(args);
}
*/
#else
#define dbg_printf(...)
#endif

static void * system_alloc_mem(int size)
{
    return GlobalAlloc(GMEM_FIXED, size);
}

static void system_free_mem(void *ptr)
{
    GlobalFree(ptr);
}

static CH376_BOOL system_init_context(CH376_CONTEXT *context, UNUSED void *user_data)
{
    // Nothing to do?
    return CH376_TRUE;
};

static void system_clean_context(CH376_CONTEXT *context)
{
    // Nothing to do?
}

static CH376_BOOL system_get_disk_info(CH376_CONTEXT *context, CH376_LOCK root_lock, struct DiskQuery *disk_info)
{
    BOOL got_info = FALSE;
    char volume[4];
    DWORD sectors_per_cluster;
    DWORD bytes_per_sector;
    DWORD number_of_free_clusters;
    DWORD total_number_of_clusters;

    volume[0] = (char)(PathGetDriveNumber(root_lock->path) + 'A');
    volume[1] = ':';
    volume[2] = '\\';
    volume[3] = '\0';

    if(GetDiskFreeSpace(volume, &sectors_per_cluster, &bytes_per_sector, &number_of_free_clusters, &total_number_of_clusters))
    {
        INT64 total_sector = (total_number_of_clusters * sectors_per_cluster * bytes_per_sector) / 512;
        INT64 free_sector = (number_of_free_clusters * sectors_per_cluster * bytes_per_sector) / 512;

        disk_info->DISK_TotalSector[0] = (INT8)((total_sector & 0x000000ff) >>  0);
        disk_info->DISK_TotalSector[1] = (INT8)((total_sector & 0x0000ff00) >>  8);
        disk_info->DISK_TotalSector[2] = (INT8)((total_sector & 0x00ff0000) >> 16);
        disk_info->DISK_TotalSector[3] = (INT8)((total_sector & 0xff000000) >> 32);

        disk_info->DISK_FreeSector[0] = (INT8)((free_sector & 0x000000ff) >>  0);
        disk_info->DISK_FreeSector[1] = (INT8)((free_sector & 0x0000ff00) >>  8);
        disk_info->DISK_FreeSector[2] = (INT8)((free_sector & 0x00ff0000) >> 16);
        disk_info->DISK_FreeSector[3] = (INT8)((free_sector & 0xff000000) >> 32);

        disk_info->DISK_DiskFat = 0x0c; // FAT32?

        got_info = TRUE;
    }
    return got_info;
}

static CH376_LOCK system_obtain_directory_lock(CH376_CONTEXT *context, const char *dir_path, CH376_LOCK root_lock)
{
    CH376_LOCK lock = NULL;
    TCHAR old_dir[MAX_PATH];

    if(root_lock)
    {
        GetCurrentDirectory(sizeof(old_dir), old_dir);
        SetCurrentDirectory(root_lock->path);
    };

    if(PathIsDirectory(dir_path))
    {
        lock = (CH376_LOCK)system_alloc_mem(sizeof(struct _CH376_LOCK ));
        GetFullPathName(dir_path, MAX_PATH, lock->path, NULL);
        lock->handle = INVALID_HANDLE_VALUE;
    }

    if(root_lock)
    {
        SetCurrentDirectory(old_dir);
    };

    return lock;
}

static void system_release_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    if(dir_lock != (CH376_LOCK)0)
    {
        system_free_mem(dir_lock);
    }
}

static CH376_LOCK system_clone_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    CH376_LOCK dup_lock = (CH376_LOCK)system_alloc_mem(sizeof(struct _CH376_LOCK));

    CopyMemory(dup_lock, dir_lock, sizeof(struct _CH376_LOCK));

    return dup_lock;
}

static HANDLE file_open(CH376_CONTEXT *context, const char *file_name, PCHAR root_path, DWORD creation_disposition)
{
    TCHAR old_dir[MAX_PATH];
    HANDLE file;

    if(root_path)
    {
        GetCurrentDirectory(sizeof(old_dir), old_dir);
        SetCurrentDirectory(root_path);
    }

    file = CreateFile(file_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, creation_disposition, FILE_ATTRIBUTE_NORMAL, NULL);

    if(root_path)
    {
        SetCurrentDirectory(old_dir);
    }

    if(file != INVALID_HANDLE_VALUE)
        return file;
    else
        return NULL;
}

static CH376_FILE system_file_open_existing(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    return file_open(context, file_name, root_lock->path, OPEN_EXISTING);
}

static CH376_FILE system_file_open_new(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    return file_open(context, file_name, root_lock->path, CREATE_ALWAYS);
}

static void system_file_close(CH376_CONTEXT *context, CH376_FILE file)
{
    if(file) CloseHandle(file);
}

static CH376_S32 system_file_seek(CH376_CONTEXT *context, CH376_FILE file, int pos)
{
    if(file != NULL)
        return SetFilePointer(file, pos, NULL, FILE_BEGIN);
    else
        return -1;
}

static CH376_S32 system_file_read(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size)
{
    DWORD read_bytes;

    if(ReadFile(file, buffer, size, &read_bytes, NULL))
        return read_bytes;
    else
        return -1;
}

static CH376_S32 system_file_write(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size)
{
    DWORD written_bytes;

    if(WriteFile(file, buffer, size, &written_bytes, NULL))
        return written_bytes;
    else
        return -1;
}

static CH376_DIR system_start_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    LPWIN32_FIND_DATA find_file_data = (LPWIN32_FIND_DATA)system_alloc_mem(sizeof(WIN32_FIND_DATA));
    TCHAR szDir[MAX_PATH];
    StringCchCopy(szDir, MAX_PATH, dir_lock->path);
    StringCchCat(szDir, MAX_PATH, TEXT("\\*"));

    dir_lock->handle = FindFirstFile(szDir, find_file_data);

    if(dir_lock->handle == INVALID_HANDLE_VALUE)
    {
        system_free_mem(find_file_data);
        find_file_data = NULL;
    }

    return find_file_data;
}

static CH376_BOOL system_go_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock, CH376_DIR fib, struct FatDirInfo *dir_info)
{
    BOOL is_done = FALSE;

    if(dir_lock->handle != INVALID_HANDLE_VALUE)
    {
        while(!is_done && FindNextFile(dir_lock->handle, fib))
        {
            if(normalize_file_name(fib->cFileName, dir_info->DIR_Name))
            {
                // Storing attributes :)
                dir_info->DIR_Attr = (UINT8)fib->dwFileAttributes;

                dbg_printf("system_go_examine_directory defined file attributes: %d\n", dir_info->DIR_Attr);

                dir_info->DIR_NTRes = 0;
                dir_info->DIR_CrtTimeTenth = 0;
                dir_info->DIR_CrtTime[0] = 0;
                dir_info->DIR_CrtTime[1] = 0;
                dir_info->DIR_CrtDate[0] = 0;
                dir_info->DIR_CrtDate[1] = 0;
                dir_info->DIR_LstAccDate[0] = 0;
                dir_info->DIR_LstAccDate[1] = 0;
                dir_info->DIR_FstClusHI[0] = 0;
                dir_info->DIR_FstClusHI[1] = 0;
                dir_info->DIR_WrtTime[0] = 0;
                dir_info->DIR_WrtTime[1] = 0;
                dir_info->DIR_WrtDate[0] = 0;
                dir_info->DIR_WrtDate[1] = 0;
                dir_info->DIR_FstClusLO[0] = 0;
                dir_info->DIR_FstClusLO[1] = 0;
                dir_info->DIR_FileSize[0] = (UINT8)((fib->nFileSizeLow & 0x000000ff) >>  0);
                dir_info->DIR_FileSize[1] = (UINT8)((fib->nFileSizeLow & 0x0000ff00) >>  8);
                dir_info->DIR_FileSize[2] = (UINT8)((fib->nFileSizeLow & 0x00ff0000) >> 16);
                dir_info->DIR_FileSize[3] = (UINT8)((fib->nFileSizeLow & 0xff000000) >> 24);

                is_done = TRUE;
            }
        }
    }

    return is_done;
}

static void system_finish_examine_directory(CH376_CONTEXT *context, CH376_DIR fib)
{
    if(fib)
    {
        FindClose(fib);
        system_free_mem(fib);
    }
}

/* /// */

#elif defined(__unix__)

/* /// "POSIX system functions" */

#ifdef DEBUG_CH376
#define dbg_printf(...) fprintf(stderr, __VA_ARGS__)
#else
#define dbg_printf(...)
#endif

static void * system_alloc_mem(int size)
{
    return malloc(size);
}

static void system_free_mem(void *ptr)
{
    free(ptr);
}

static CH376_BOOL system_init_context(CH376_CONTEXT *context, UNUSED void *user_data)
{
  /* Nothing to do */
	return CH376_TRUE;
}

static void system_clean_context(CH376_CONTEXT *context)
{
  /* Nothing to do */
}

static CH376_BOOL system_get_disk_info(CH376_CONTEXT *context, CH376_LOCK root_lock, struct DiskQuery *disk_info)
{
    CH376_BOOL got_info = CH376_FALSE;

    if(disk_info)
    {
        struct statvfs stats;

        if(statvfs(root_lock, &stats) == 0)
        {
            int64_t total_sector = (stats.f_blocks * stats.f_bsize) / 512;
            int64_t free_sector = (stats.f_bfree * stats.f_bsize) / 512;

            disk_info->DISK_TotalSector[0] = (total_sector & 0x000000ff) >>  0;
            disk_info->DISK_TotalSector[1] = (total_sector & 0x0000ff00) >>  8;
            disk_info->DISK_TotalSector[2] = (total_sector & 0x00ff0000) >> 16;
            disk_info->DISK_TotalSector[3] = (total_sector & 0xff000000) >> 32;

            disk_info->DISK_FreeSector[0] = (free_sector & 0x000000ff) >>  0;
            disk_info->DISK_FreeSector[1] = (free_sector & 0x0000ff00) >>  8;
            disk_info->DISK_FreeSector[2] = (free_sector & 0x00ff0000) >> 16;
            disk_info->DISK_FreeSector[3] = (free_sector & 0xff000000) >> 32;

            disk_info->DISK_DiskFat = 0x0c; // FAT32?

            got_info = CH376_TRUE;
        }
    }

    return got_info;
}

static CH376_LOCK system_obtain_directory_lock(CH376_CONTEXT *context, const char *dir_path, CH376_LOCK root_lock)
{
    CH376_LOCK lock;
    char *old_dir = NULL;
    struct stat path_stat;

    dbg_printf("system_obtain_directory_lock: %s\n", dir_path);

    if(root_lock)
    {
        if((old_dir = getcwd(NULL, 0)) != NULL)
            chdir(lock);
    }

    stat(dir_path, &path_stat);
    if(S_ISDIR(path_stat.st_mode))
    {
      lock = realpath(dir_path, NULL);
    }

    if(old_dir)
    {
        if(old_dir != NULL)
        {
            chdir(old_dir);
            system_free_mem(old_dir);
        }
    }

    return lock;
}

static void system_release_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    system_free_mem(dir_lock);
}

static CH376_LOCK system_clone_directory_lock(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    return strdup(dir_lock);
}

static FILE * file_open(const char *file_name,  CH376_LOCK root_lock, const char *mode)
{
    FILE *file;
    char *old_dir = NULL;
    struct stat path_stat;

    if(root_lock)
    {
        if((old_dir = getcwd(NULL, 0)) != NULL)
            chdir(root_lock);
    }

    file = fopen(file_name, mode);

    if(old_dir)
    {
        if(old_dir != NULL)
        {
            chdir(old_dir);
            system_free_mem(old_dir);
        }
    }

    return file;
}

static CH376_FILE system_file_open_existing(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    return file_open(file_name, root_lock, "wb+");
}

static CH376_FILE system_file_open_new(CH376_CONTEXT *context, const char *file_name, CH376_LOCK root_lock)
{
    return file_open(file_name, root_lock, "wb");
}

static void system_file_close(CH376_CONTEXT *context, CH376_FILE file)
{
	if (file)
		fclose(file);
}

static CH376_S32 system_file_seek(CH376_CONTEXT *context, CH376_FILE file, int pos)
{
    dbg_printf("system_file_seek trying to seek to position %d\n", pos);

    if(file)
        return fseek(file, pos, SEEK_SET);
    else
        return -1;
}

static CH376_S32 system_file_read(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size)
{
    dbg_printf("system_file_read trying to read %d bytes\n", size);

    if(file)
        return fread(buffer, size, 1, file);
    else
        return -1;
}

static CH376_S32 system_file_write(CH376_CONTEXT *context, CH376_FILE file, void *buffer, CH376_S32 size)
{
    dbg_printf("system_file_write trying to write %d bytes\n", size);

    if(file)
        return fwrite(buffer, size, 1, file);
    else
        return -1;
}

static CH376_DIR system_start_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock)
{
    CH376_DIR fib = system_alloc_mem(sizeof(struct _CH376_DIR));

    fib->handle = opendir(dir_lock);
    fib->entry = NULL;

    return fib;
}

static CH376_BOOL system_go_examine_directory(CH376_CONTEXT *context, CH376_LOCK dir_lock, CH376_DIR fib, struct FatDirInfo *dir_info)
{
    CH376_BOOL is_done = CH376_FALSE;

    if(fib) while((fib->entry = readdir(fib->handle)) && !is_done)
    {
        if(normalize_file_name(fib->entry->d_name, dir_info->DIR_Name))
        {
            struct stat file_stat;

            stat(fib->entry->d_name, &file_stat);

            dir_info->DIR_Attr = 0;

            if(S_ISDIR(file_stat.st_mode))
                dir_info->DIR_Attr |= DIR_ATTR_DIRECTORY;
            if((file_stat.st_mode & S_IRUSR) == 0)
                dir_info->DIR_Attr |= DIR_ATTR_READ_ONLY;

            dbg_printf("system_go_examine_directory defined file attributes: %d\n", dir_info->DIR_Attr);

            dir_info->DIR_NTRes = 0;
            dir_info->DIR_CrtTimeTenth = 0;
            dir_info->DIR_CrtTime[0] = 0;
            dir_info->DIR_CrtTime[1] = 0;
            dir_info->DIR_CrtDate[0] = 0;
            dir_info->DIR_CrtDate[1] = 0;
            dir_info->DIR_LstAccDate[0] = 0;
            dir_info->DIR_LstAccDate[1] = 0;
            dir_info->DIR_FstClusHI[0] = 0;
            dir_info->DIR_FstClusHI[1] = 0;
            dir_info->DIR_WrtTime[0] = 0;
            dir_info->DIR_WrtTime[1] = 0;
            dir_info->DIR_WrtDate[0] = 0;
            dir_info->DIR_WrtDate[1] = 0;
            dir_info->DIR_FstClusLO[0] = 0;
            dir_info->DIR_FstClusLO[1] = 0;
            dir_info->DIR_FileSize[0] = (file_stat.st_size & 0x000000ff) >>  0;
            dir_info->DIR_FileSize[1] = (file_stat.st_size & 0x0000ff00) >>  8;
            dir_info->DIR_FileSize[2] = (file_stat.st_size & 0x00ff0000) >> 16;
            dir_info->DIR_FileSize[3] = (file_stat.st_size & 0xff000000) >> 24;

            is_done = CH376_TRUE;
        }
    }

    return is_done;
}

static void system_finish_examine_directory(CH376_CONTEXT *context, CH376_DIR fib)
{
    if (fib)
    {
        closedir(fib->handle);
        system_free_mem(fib);
    }
}

/* /// */

#else
#error "FixMe!"
#endif

/* /// "CH376 private subroutines" */

static char *clone_string(const char *string)
{
    int l;
    char *s;

    for(l=0; string[l]!='\0'; l++);

    s = system_alloc_mem(l);

    for(l=0; string[l]!='\0'; l++)
        s[l] = string[l];

    s[l] = '\0';

    return s;
}

static void clear_structure(struct ch376 *ch376)
{
    ch376->command = CH376_CMD_NONE;
    ch376->command_status = 0;
    ch376->interface_status = 0;
    ch376->nb_bytes_in_cmd_data = 0;
    ch376->pos_rw_in_cmd_data = 0;
    ch376->usb_mode = CH376_ARG_SET_USB_MODE_INVALID;
    ch376->bytes_to_read_write = 0;
    ch376->root_dir_lock = (CH376_LOCK)0;
    ch376->current_dir_lock = (CH376_LOCK)0;
    ch376->current_file = (CH376_FILE)0;
    ch376->current_directory_browsing = (CH376_DIR)0;
}

static void cancel_all_io(struct ch376 *ch376)
{
    // Cancel data buffer read/write sessions
    ch376->bytes_to_read_write = 0;

    // End directory browing or file i/o sessions
    system_finish_examine_directory(&ch376->context, ch376->current_directory_browsing);
    ch376->current_directory_browsing = (CH376_DIR)0;
    system_file_close(&ch376->context, ch376->current_file);
    ch376->current_file = (CH376_FILE)0;

    // Release potential already obtained lock for root and current
    system_release_directory_lock(&ch376->context, ch376->root_dir_lock);
    ch376->root_dir_lock = (CH376_LOCK)0;
    system_release_directory_lock(&ch376->context, ch376->current_dir_lock);
    ch376->current_dir_lock = (CH376_LOCK)0;
}

// Normalize a char (capitalized and ASCII)
// Return -1 if not possible
static int normalize_char(char c)
{
    if(c > 127)
        return -1;

    if(c >='a' && c <='z')
        return c - ('a' - 'A');

    return c;
}

// Normalized a dos filename in 8.3 format
// Return NULL if not possible
static const char * normalize_file_name(const char *file_name, char *normalized_file_name)
{
    int i = 0;
    int j = 0;
    int c;

    dbg_printf("normalize_file_name: %s\n", file_name);

    while(file_name[i] != '\0' && file_name[i] != '.' && j < 8)
    {
        c = normalize_char(file_name[i++]);

        if(c == -1)
            return NULL;
        else
            normalized_file_name[j++] = (char)c;
    }

    if(file_name[i] == '\0')
    {
        for(; j<8; j++)
        {
            normalized_file_name[j] = ' ';
        }
    }
    else if(file_name[i] == '.')
    {
        i++;
        for(; j<8; j++)
        {
            normalized_file_name[j] = ' ';
        }
    }
    else
    {
        dbg_printf("normalize_file_name impossible: %s (%d=%c,%d=%c)\n", file_name, i, file_name[i], j, normalized_file_name[j]);
        return NULL;
    }

    while(file_name[i] != '\0' && file_name[i] != '.' && j < 11)
    {
        c = normalize_char(file_name[i++]);

        if(c == -1)
            return NULL;
        else
            normalized_file_name[j++] = (char)c;
    }

    if(file_name[i] != '\0')
    {
        dbg_printf("normalize_file_name impossible: %s (%d=%c,%d=%c)\n", file_name, i, file_name[i], j, normalized_file_name[j]);
        return NULL;
    }

    dbg_printf("normalize_file_name done: %s (%d=&%02x,%d=&%02x)\n", file_name, i, file_name[i], j, normalized_file_name[j]);

    for(; j<11; j++)
    {
        normalized_file_name[j] = ' ';
    }

    return normalized_file_name;
}

// Convert normalized 8.3 filename into regular filename
// (remove trailing spaces in name and extension, and clear extension trailing '.' if any)
static const char * trim_file_name(const char *file_name, char *trimmed_file_name)
{
    int i = 0;
    int j = 0;

    while(file_name[i] != ' ' && file_name[i] != '\0')
        trimmed_file_name[j++] = file_name[i++];

    while(file_name[i] == ' ')
        i++;

    while(file_name[i] != ' ' && file_name[i] != '\0')
        trimmed_file_name[j++] = file_name[i++];

    if(j > 0 && trimmed_file_name[j-1] == '.')
        j--;

    trimmed_file_name[j] = '\0';

    dbg_printf("trim_file_name from \"%s\" to \"%s\"\n", file_name, trimmed_file_name);

    return trimmed_file_name;
}

static void file_read_chunk(struct ch376 *ch376)
{
    CH376_S32 bytes_to_read_now;
    CH376_S32 bytes_actually_read;

    if(ch376->bytes_to_read_write > sizeof(ch376->cmd_data.CMD_IOBuffer))
        bytes_to_read_now = sizeof(ch376->cmd_data.CMD_IOBuffer);
    else
        bytes_to_read_now = ch376->bytes_to_read_write;

    bytes_actually_read = system_file_read(&ch376->context, ch376->current_file, &ch376->cmd_data.CMD_IOBuffer, bytes_to_read_now);

    if(bytes_actually_read >= 0)
    {
        ch376->bytes_to_read_write -= (CH376_U16)bytes_actually_read;

        ch376->nb_bytes_in_cmd_data = (CH376_U8)bytes_actually_read;
        ch376->interface_status = 127;
        // CH376 specification tells that the last buffer is returned with INT_SUCCESS
        // but it is not the case (at least with revision 4). An empty buffer is always
        // returned with INT_SUCCESS. As a consequence some routines do not read the last
        // buffer when INT_SUCCESS is returned. It is not correct regarding CH376
        // specification, but to keep compatibility with these (wrong) routines,
        // we also always return an empty buffer with INT_SUCCESS by checking
        // bytes_actually_read == 0 instead of bytes_actually_read < bytes_to_read_now
        if(bytes_to_read_now == 0 || bytes_actually_read == 0)
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_READ] all done: read operated with success (no more data)\n");
            ch376->command_status = CH376_INT_SUCCESS;
        }
        else
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_READ] all done: read operated with success (waiting data read for %d bytes out of %d)\n", ch376->nb_bytes_in_cmd_data, ch376->bytes_to_read_write);
            ch376->command_status = CH376_INT_DISK_READ;
        }
    }
    else
    {
        dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_READ] aborted: read failure\n");

        ch376->nb_bytes_in_cmd_data = 0;
        ch376->interface_status = 0;
        ch376->command_status = CH376_RET_ABORT;
    }
}

static void file_write_chunk(struct ch376 *ch376)
{
    CH376_S32 bytes_to_write_now;
    CH376_S32 bytes_actually_written;

    bytes_to_write_now = ch376->nb_bytes_in_cmd_data;

    bytes_actually_written = system_file_write(&ch376->context, ch376->current_file, &ch376->cmd_data.CMD_IOBuffer, bytes_to_write_now);

    if(bytes_actually_written == bytes_to_write_now)
    {
        ch376->bytes_to_read_write -= (CH376_U16)bytes_actually_written;

        ch376->nb_bytes_in_cmd_data = 0;
        ch376->interface_status = 127;
        if(ch376->bytes_to_read_write == 0)
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_WRITE] all done: write operated with success (no more data)\n");
            ch376->command_status = CH376_INT_SUCCESS;
        }
        else
        {
            if(ch376->bytes_to_read_write > sizeof(ch376->cmd_data.CMD_IOBuffer))
                ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_IOBuffer);
            else
                ch376->nb_bytes_in_cmd_data = (CH376_U8)ch376->bytes_to_read_write;

            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_WRITE] all done: write operated with success (waiting data write for %d bytes out of %d)\n", ch376->nb_bytes_in_cmd_data, ch376->bytes_to_read_write);
            ch376->command_status = CH376_INT_DISK_WRITE;
        }
    }
    else
    {
        dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_WRITE] aborted: write failure\n");

        ch376->nb_bytes_in_cmd_data = 0;
        ch376->interface_status = 0;
        ch376->command_status = CH376_RET_ABORT;
    }
}

/* /// */

/* /// "CH376 public read command port" */

CH376_U8 ch376_read_command_port(struct ch376 *ch376)
{
    dbg_printf("[READ][COMMAND] during command &%02x status &%02x\n", ch376->command, ch376->command_status);
    return ch376->interface_status;
}

/* /// */

/* /// "CH376 public read data port" */

CH376_U8 ch376_read_data_port(struct ch376 *ch376)
{
    CH376_U8 data_out = 0xff; // Hi-Z

    dbg_printf(">> [READ][DATA] for during command &%02x status &%02x\n", ch376->command, ch376->command_status);

    switch(ch376->command)
    {
    case CH376_CMD_CHECK_EXIST:
        data_out = ch376->cmd_data.CMD_CheckByte;
        dbg_printf("[READ][DATA][CH376_CMD_CHECK_EXIST] setting data port to &%02x\n", data_out);
        break;

    case CH376_CMD_GET_IC_VER:
        data_out = (0x40 | CH376_DATA_IC_VER) & 0x7f;
        dbg_printf("[READ][DATA][CH376_CMD_GET_IC_VER] setting data port to &%02x\n", data_out);
        break;

    case CH376_CMD_SET_USB_MODE:
        if(ch376->usb_mode == CH376_ARG_SET_USB_MODE_INVALID)
        {
            data_out = CH376_RET_ABORT;
            dbg_printf("[READ][DATA][CH376_SET_USB_MODE] aborted!\n");
        }
        else
        {
            data_out = CH376_RET_SUCCESS;
            dbg_printf("[READ][DATA][CH376_SET_USB_MODE] completed!\n");
        }
        break;

    case CH376_CMD_GET_STATUS:
        data_out = ch376->command_status;
        dbg_printf("[READ][DATA][CH376_CMD_GET_STATUS] setting data port to &%02x\n", data_out);
        break;

    case CH376_CMD_RD_USB_DATA0:
        if(ch376->nb_bytes_in_cmd_data)
        {
            if(ch376->pos_rw_in_cmd_data == CMD_DATA_REQ_SIZE)
            {
                data_out = ch376->nb_bytes_in_cmd_data;
                ch376->pos_rw_in_cmd_data = 0;
                dbg_printf("[READ][DATA][CH376_CMD_RD_USB_DATA0] read i/o buffer size: &%02x\n", data_out);
            }
            else if(ch376->nb_bytes_in_cmd_data != ch376->pos_rw_in_cmd_data)
            {
                data_out = ch376->cmd_data.CMD_IOBuffer[ch376->pos_rw_in_cmd_data];

                dbg_printf("[READ][DATA][CH376_CMD_RD_USB_DATA0] read \"%c\" (&%02x) from i/o buffer at position &%02x\n", data_out, data_out, ch376->pos_rw_in_cmd_data);

                if(++ch376->pos_rw_in_cmd_data == ch376->nb_bytes_in_cmd_data)
                    ch376->nb_bytes_in_cmd_data = 0;
            }
        }
        else
        {
            data_out = 0;
            dbg_printf("[READ][DATA][CH376_CMD_RD_USB_DATA0] nothing to read from i/o buffer\n");
        }
        break;

    case CH376_CMD_WR_REQ_DATA:
        if(ch376->nb_bytes_in_cmd_data)
        {
            if(ch376->pos_rw_in_cmd_data == CMD_DATA_REQ_SIZE)
            {
                data_out = ch376->nb_bytes_in_cmd_data;
                ch376->pos_rw_in_cmd_data = 0;
                dbg_printf("[READ][DATA][CH376_CMD_WR_REQ_DATA] read i/o buffer size: &%02x\n", data_out);
            }
        }
        else
        {
            data_out = 0;
            dbg_printf("[READ][DATA][CH376_CMD_WR_REQ_DATA] nothing to read from i/o buffer\n");
        }
        break;

    // Emulate CH376 bug which returns the 1st byte in data buffer
    // when read is performed on an unexpected command
    default:
        data_out = ch376->cmd_data.CMD_IOBuffer[0];
        break;
    }

    dbg_printf("<< [READ][DATA] for during command &%02x status &%02x\n", ch376->command, ch376->command_status);


  return data_out;
}

/* /// */

/* /// "CH376 public write command port" */

void ch376_write_command_port(struct ch376 *ch376, CH376_U8 command)
{
    dbg_printf(">> [WRITE][COMMAND] Write command &%02x status &%02x\n", command, ch376->command_status);

    ch376->interface_status = 0;

    // Emulate CH376 bug which can get the check byte
    // from the command port instead of the data port!
    if(ch376->command == CH376_CMD_CHECK_EXIST)
    {
        ch376->cmd_data.CMD_CheckByte = ~command;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_CHECK_EXIST] got check byte &%02x from command port!\n", command);
    }

    switch(command)
    {
    case CH376_CMD_CHECK_EXIST:
        ch376->command = CH376_CMD_CHECK_EXIST;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_CHECK_EXIST] waiting for check byte\n");
        break;

    case CH376_CMD_GET_IC_VER:
        ch376->command = CH376_CMD_GET_IC_VER;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_GET_IC_VER]\n");
        break;

    case CH376_CMD_SET_USB_MODE:
        ch376->command = CH376_CMD_SET_USB_MODE;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_SET_USB_MODE] waiting for usb mode data\n");
        break;

    case CH376_CMD_DISK_MOUNT:
        ch376->command = CH376_CMD_DISK_MOUNT;
        cancel_all_io(ch376);
        // If directory is available, we consider that it's mounted!
        if(ch376->usb_mode == CH376_ARG_SET_USB_MODE_SD_HOST)
            ch376->root_dir_lock = system_obtain_directory_lock(&ch376->context, ch376->sdcard_drive_path, NULL);
        else if(ch376->usb_mode == CH376_ARG_SET_USB_MODE_USB_HOST)
            ch376->root_dir_lock = system_obtain_directory_lock(&ch376->context, ch376->usb_drive_path, NULL);

        if(ch376->root_dir_lock)
        {
            ch376->cmd_data.CMD_MountInfo.MOUNT_DeviceType = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_RemovableMedia = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_Versions = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_DataFormatAndEtc = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_AdditionalLength = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_Reserved1 = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_Reserved2 = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_MiscFlag = 0;
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[0] = 'J';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[1] = 'E';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[2] = 'D';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[3] = 'E';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[4] = '+';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[5] = 'O';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[6] = 'F';
            ch376->cmd_data.CMD_MountInfo.MOUNT_VendorIdStr[7] = 'T';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 0] = 'C';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 1] = 'H';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 2] = '3';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 3] = '7';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 4] = '6';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 5] = ' ';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 6] = 'E';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 7] = 'M';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 8] = 'U';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[ 9] = 'L';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[10] = 'A';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[11] = 'T';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[12] = 'O';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[13] = 'R';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[14] = ' ';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductIdStr[15] = ' ';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductRevStr[0] = 'F';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductRevStr[1] = 'A';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductRevStr[2] = 'M';
            ch376->cmd_data.CMD_MountInfo.MOUNT_ProductRevStr[3] = 'E';

            dbg_printf("[WRITE][COMMAND][CH376_CMD_DISK_MOUNT] drive directory is found (mounted :); additional data available\n");

            ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_MountInfo);
            ch376->interface_status = 127; // Found :)
            ch376->command_status = CH376_INT_SUCCESS;
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_DISK_MOUNT] drive directory not found\n");
            ch376->command_status = 0x1f; // Don't know if it's this code
        }
        break;

    case CH376_CMD_GET_STATUS:
        ch376->command = CH376_CMD_GET_STATUS;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_GET_STATUS] waiting for status reading\n");
        break;

    case CH376_CMD_SET_FILE_NAME:
        ch376->command = CH376_CMD_SET_FILE_NAME;
        ch376->pos_rw_in_cmd_data = 0;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_SET_FILE_NAME] waiting for file name\n");
        break;

    case CH376_CMD_FILE_OPEN:
        ch376->command = CH376_CMD_FILE_OPEN;
        // mounted?
        if(ch376->root_dir_lock)
        {
            int i = 0;

            // back to root?
            if(ch376->cmd_data.CMD_FileName[i] == '/')
            {
                dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_OPEN] opening root directory\n");
                system_release_directory_lock(&ch376->context, ch376->current_dir_lock);
                ch376->current_dir_lock = system_clone_directory_lock(&ch376->context, ch376->root_dir_lock);
                i++;
            }

            if(ch376->cmd_data.CMD_FileName[i] == '\0')
            {
                // We are done
                ch376->interface_status = 127; // Found :)
                ch376->command_status = CH376_INT_SUCCESS;
            }
            else
            {
                // wildcard? (only '*' is supported for now!)
                if(ch376->cmd_data.CMD_FileName[i] == '*')
                {
                    dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_OPEN] examining directory contents\n");

                    // Start a directory examine session
                    system_finish_examine_directory(&ch376->context, ch376->current_directory_browsing);
                    ch376->current_directory_browsing = system_start_examine_directory(&ch376->context, ch376->current_dir_lock);

                    goto file_enum_go;
                }
                else
                {
                    char fixed_file_name[13]; // Max = 8 + '.' + 3 + '\0'
                    CH376_LOCK current_dir_lock;
                    trim_file_name(&ch376->cmd_data.CMD_FileName[i], fixed_file_name);

                    current_dir_lock = system_obtain_directory_lock(&ch376->context, fixed_file_name, ch376->current_dir_lock);

                    // Enter new directory?
                    if(current_dir_lock)
                    {
                        dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_OPEN] entering directory: %s\n", &ch376->cmd_data.CMD_FileName[i]);

                        system_release_directory_lock(&ch376->context, ch376->current_dir_lock);
                        ch376->current_dir_lock = current_dir_lock;

                        ch376->interface_status = 127; // Found :)
                        ch376->command_status = CH376_ERR_OPEN_DIR;
                    }
                    // Open existing file?
                    else
                    {
                        system_file_close(&ch376->context, ch376->current_file);
                        ch376->current_file = system_file_open_existing(&ch376->context, fixed_file_name, ch376->current_dir_lock);

                        dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_OPEN] opening the file: %s\n", &ch376->cmd_data.CMD_FileName[i]);

                        if(ch376->current_file)
                        {
                            ch376->interface_status = 127; // Found :)
                            ch376->command_status = CH376_INT_SUCCESS;
                        }
                        else
                        {
                            ch376->interface_status = 0;
                            ch376->command_status = CH376_ERR_MISS_FILE;
                        }
                    }
                }
            }
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_OPEN] no operation possible (device not mounted)\n");

            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

    case CH376_CMD_FILE_CREATE:
        ch376->command = CH376_CMD_FILE_CREATE;
        // mounted?
        if(ch376->root_dir_lock)
        {
            int i = 0;

            // back to root?
            if(ch376->cmd_data.CMD_FileName[i] == '/')
            {
                dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_CREATE] opening root directory\n");
                system_release_directory_lock(&ch376->context, ch376->current_dir_lock);
                ch376->current_dir_lock = system_clone_directory_lock(&ch376->context, ch376->root_dir_lock);
                i++;
            }

            if(ch376->cmd_data.CMD_FileName[i] == '\0')
            {
                // We are done
                ch376->interface_status = 127; // Found :)
                ch376->command_status = CH376_INT_SUCCESS;
            }
            else
            {
                char fixed_file_name[13]; // Max = 8 + '.' + 3 + '\0'
                trim_file_name(&ch376->cmd_data.CMD_FileName[i], fixed_file_name);

                // Create file
                system_file_close(&ch376->context, ch376->current_file);
                ch376->current_file = system_file_open_new(&ch376->context, fixed_file_name, ch376->current_dir_lock);

                dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_CREATE] creating the file: %s\n", &ch376->cmd_data.CMD_FileName[i]);

                if(ch376->current_file)
                {
                    ch376->interface_status = 127; // Found :)
                    ch376->command_status = CH376_INT_SUCCESS;
                }
                else
                {
                    ch376->interface_status = 0;
                    ch376->command_status = CH376_ERR_MISS_FILE;
                }
            }
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_CREATE] no operation possible (device not mounted)\n");

            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

	case CH376_CMD_FILE_ERASE:
		ch376->command = CH376_CMD_FILE_ERASE;
		if (ch376->root_dir_lock)
		{
			/*Add here Erase file emulation*/
		}
		else
		{
			dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_ERASE] no operation possible (device not mounted)\n");

			ch376->interface_status = 0;
			ch376->command_status = CH376_RET_ABORT;
		}
		break;

		break;

    case CH376_CMD_RD_USB_DATA0:
        ch376->command = CH376_CMD_RD_USB_DATA0;
        ch376->pos_rw_in_cmd_data = CMD_DATA_REQ_SIZE; // Will be reset when size is sent
        dbg_printf("[WRITE][COMMAND][CH376_CMD_RD_USB_DATA0] waiting for i/o buffer read\n");
        break;

    case CH376_CMD_WR_REQ_DATA:
        ch376->command = CH376_CMD_WR_REQ_DATA;
        ch376->pos_rw_in_cmd_data = CMD_DATA_REQ_SIZE; // Will be reset when size is sent
        dbg_printf("[WRITE][COMMAND][CH376_CMD_WR_REQ_DATA] waiting for i/o buffer write\n");
        break;

    case CH376_CMD_FILE_ENUM_GO:
        ch376->command = CH376_CMD_FILE_ENUM_GO;
file_enum_go:
        if(system_go_examine_directory(&ch376->context, ch376->current_dir_lock, ch376->current_directory_browsing, &ch376->cmd_data.CMD_FatDirInfo))
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_ENUM_GO] next directory entry in buffer\n");
            ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_FatDirInfo);
            ch376->interface_status = 127;
            ch376->command_status = CH376_INT_DISK_READ;
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_ENUM_GO] directory browing finished\n");

            system_finish_examine_directory(&ch376->context, ch376->current_directory_browsing);
            ch376->current_directory_browsing = (CH376_DIR)0;

            ch376->interface_status = 0;
            ch376->command_status = CH376_ERR_OPEN_DIR;
        }
        break;

    case CH376_CMD_DISK_QUERY:
        ch376->command = CH376_CMD_DISK_QUERY;
        // mounted?
        if(system_get_disk_info(&ch376->context, ch376->root_dir_lock, &ch376->cmd_data.CMD_DiskQuery))
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_DISK_CAPACITY] sector capacity in buffer\n");
            ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_DiskQuery);
            ch376->interface_status = 127;
            ch376->command_status = CH376_INT_SUCCESS;
        }
        else
        {
            dbg_printf("[WRITE][COMMAND][CH376_CMD_DISK_CAPACITY] no operation possible (device not mounted)\n");
            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

    case CH376_CMD_BYTE_LOCATE:
        ch376->command = CH376_CMD_BYTE_LOCATE;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_BYTE_LOCATE] waiting for seek position\n");
        ch376->pos_rw_in_cmd_data = 0;
        break;

    case CH376_CMD_BYTE_READ:
        ch376->command = CH376_CMD_BYTE_READ;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_BYTE_READ] waiting for read size\n");
        ch376->pos_rw_in_cmd_data = 0;
        break;

    case CH376_CMD_BYTE_RD_GO:
        ch376->command = CH376_CMD_BYTE_RD_GO;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_BYTE_RD_GO] waiting for next chunk to read\n");
        file_read_chunk(ch376);
        break;

    case CH376_CMD_BYTE_WRITE:
        ch376->command = CH376_CMD_BYTE_WRITE;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_BYTE_WRITE] waiting for write size\n");
        ch376->pos_rw_in_cmd_data = 0;
        break;

    case CH376_CMD_BYTE_WR_GO:
        ch376->command = CH376_CMD_BYTE_WR_GO;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_BYTE_WR_GO] waiting for next chunk to write\n");
        file_write_chunk(ch376);
        break;

    case CH376_CMD_FILE_CLOSE:
        ch376->command = CH376_CMD_FILE_CLOSE;
        dbg_printf("[WRITE][COMMAND][CH376_CMD_FILE_CLOSE] waiting for close mode\n");
        break;

    default:
        dbg_printf("[WRITE][COMMAND][Unsupported] command &%02x not implemented\n", ch376->command);
        ch376->interface_status = 0;
        ch376->command_status = CH376_RET_ABORT;
        break;
    }

    dbg_printf("<< [WRITE][COMMAND] Write command &%02x status &%02x\n", ch376->command, ch376->command_status);
}

/* /// */

/* /// "CH376 public write data port" */

void ch376_write_data_port(struct ch376 *ch376, CH376_U8 data)
{
  dbg_printf(">> [WRITE][DATA] Write data &%02x status &%02x\n", data, ch376->command_status);

  switch(ch376->command)
  {
    case CH376_CMD_CHECK_EXIST:
        ch376->cmd_data.CMD_CheckByte = ~data;
        dbg_printf("[WRITE][DATA][CH376_CMD_CHECK_EXIST] got check byte &%02x\n", data);
        break;

    case CH376_CMD_SET_USB_MODE:
        cancel_all_io(ch376);
        switch(data)
        {
        case CH376_ARG_SET_USB_MODE_USB_HOST:
            ch376->usb_mode = CH376_ARG_SET_USB_MODE_USB_HOST;
            dbg_printf("[WRITE][DATA][CH376_SET_USB_MODE] USB host set\n");
            break;
        case CH376_ARG_SET_USB_MODE_SD_HOST:
            ch376->usb_mode = CH376_ARG_SET_USB_MODE_SD_HOST;
            dbg_printf("[WRITE][DATA][CH376_SET_USB_MODE] SD card set\n");
            break;
        default:
            ch376->usb_mode = CH376_ARG_SET_USB_MODE_INVALID;
            dbg_printf("[WRITE][DATA][CH376_SET_USB_MODE_CODE_INVALID] set\n");
            break;
        }
        break;

    case CH376_CMD_SET_FILE_NAME:
        dbg_printf("[WRITE][DATA][CH376_CMD_SET_FILE_NAME] got file name character \"%c\" (&%02x) for position %ld\n", data, data, ch376->pos_rw_in_cmd_data);
        // protect buffer overflow
        if(ch376->pos_rw_in_cmd_data < sizeof(ch376->cmd_data.CMD_FileName))
        {
            // store new filename character
            ch376->cmd_data.CMD_FileName[ch376->pos_rw_in_cmd_data++] = data;
            ch376->cmd_data.CMD_FileName[ch376->pos_rw_in_cmd_data] = '\0';
        }
        break;

    case CH376_CMD_BYTE_LOCATE:
        if(ch376->pos_rw_in_cmd_data < sizeof(ch376->cmd_data.CMD_FileSeek))
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_LOCATE] got byte #%d with value &%02x\n", ch376->pos_rw_in_cmd_data, data);
            ch376->cmd_data.CMD_FileSeek[ch376->pos_rw_in_cmd_data] = data;

            if(++ch376->pos_rw_in_cmd_data == sizeof(ch376->cmd_data.CMD_FileSeek))
            {
                CH376_S32 file_seek_pos = (ch376->cmd_data.CMD_FileSeek[0] <<  0)
                                        | (ch376->cmd_data.CMD_FileSeek[1] <<  8)
                                        | (ch376->cmd_data.CMD_FileSeek[2] << 16)
                                        | (ch376->cmd_data.CMD_FileSeek[3] << 24);

                file_seek_pos = system_file_seek(&ch376->context, ch376->current_file, file_seek_pos);

                if(file_seek_pos >= 0)
                {
                    dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_LOCATE] all done: seek operated with success (waiting for data read)\n");

                    ch376->cmd_data.CMD_FileSeek[0] = (CH376_U8)((file_seek_pos & 0x000000ff) >>  0);
                    ch376->cmd_data.CMD_FileSeek[1] = (CH376_U8)((file_seek_pos & 0x0000ff00) >>  8);
                    ch376->cmd_data.CMD_FileSeek[2] = (CH376_U8)((file_seek_pos & 0x00ff0000) >> 16);
                    ch376->cmd_data.CMD_FileSeek[3] = (CH376_U8)((file_seek_pos & 0xff000000) >> 24);

                    ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_FileSeek);
                    ch376->interface_status = 127;
                    ch376->command_status = CH376_INT_SUCCESS;
                }
                else
                {
                    dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_LOCATE] all done: seek failure\n");

                    ch376->interface_status = 0;
                    ch376->command_status = CH376_RET_ABORT;
                }
            }
        }
        break;

    case CH376_CMD_WR_REQ_DATA:
        if(ch376->nb_bytes_in_cmd_data
        && ch376->pos_rw_in_cmd_data != CMD_DATA_REQ_SIZE)
        {
            CH376_U8 *raw = (CH376_U8 *)&ch376->cmd_data;

            if(ch376->nb_bytes_in_cmd_data != ch376->pos_rw_in_cmd_data)
            {
                raw[ch376->pos_rw_in_cmd_data] = data;

                dbg_printf("[WRITE][DATA][CH376_CMD_WR_REQ_DATA] write \"%c\" (&%02x) to i/o buffer at position &%02x\n", data, data, ch376->pos_rw_in_cmd_data);

                if(++ch376->pos_rw_in_cmd_data == ch376->nb_bytes_in_cmd_data)
                {
                    ch376->interface_status = 0;
                    ch376->command_status = CH376_RET_SUCCESS;
                }
            }
        }
        else
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_WR_REQ_DATA] nothing to write to i/o buffer\n");

            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;

    case CH376_CMD_BYTE_READ:
        if(ch376->pos_rw_in_cmd_data < sizeof(ch376->cmd_data.CMD_FileReadWrite))
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_READ] got byte #%d with value &%02x\n", ch376->pos_rw_in_cmd_data, data);
            ch376->cmd_data.CMD_FileReadWrite[ch376->pos_rw_in_cmd_data] = data;

            if(++ch376->pos_rw_in_cmd_data == sizeof(ch376->cmd_data.CMD_FileReadWrite))
            {
                ch376->bytes_to_read_write = (ch376->cmd_data.CMD_FileReadWrite[0] <<  0)
                                           | (ch376->cmd_data.CMD_FileReadWrite[1] <<  8);
                file_read_chunk(ch376);
            }
        }
        break;

    case CH376_CMD_BYTE_WRITE:
        if(ch376->pos_rw_in_cmd_data < sizeof(ch376->cmd_data.CMD_FileReadWrite))
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_BYTE_WRITE] got byte #%d with value &%02x\n", ch376->pos_rw_in_cmd_data, data);
            ch376->cmd_data.CMD_FileReadWrite[ch376->pos_rw_in_cmd_data] = data;

            if(++ch376->pos_rw_in_cmd_data == sizeof(ch376->cmd_data.CMD_FileReadWrite))
            {
                ch376->bytes_to_read_write = (ch376->cmd_data.CMD_FileReadWrite[0] <<  0)
                                           | (ch376->cmd_data.CMD_FileReadWrite[1] <<  8);

                if(ch376->bytes_to_read_write > sizeof(ch376->cmd_data.CMD_IOBuffer))
                    ch376->nb_bytes_in_cmd_data = sizeof(ch376->cmd_data.CMD_IOBuffer);
                else
                    ch376->nb_bytes_in_cmd_data = (CH376_U8)ch376->bytes_to_read_write;

                ch376->interface_status = 127;
                ch376->command_status = CH376_INT_SUCCESS;
            }
        }
        break;

    case CH376_CMD_FILE_CLOSE:
        // Note: close mode is not implemented; size if always updated
        dbg_printf("[WRITE][DATA][CH376_CMD_FILE_CLOSE] update size: %s\n", ch376->pos_rw_in_cmd_data == 0 ? "no" : "yes");
        if(ch376->current_file)
        {
            system_file_close(&ch376->context, ch376->current_file);
            ch376->current_file = (CH376_FILE)0;

            ch376->interface_status = 127;
            ch376->command_status = CH376_INT_SUCCESS;
        }
        else
        {
            dbg_printf("[WRITE][DATA][CH376_CMD_FILE_CLOSE] failure: not file opened\n");

            ch376->interface_status = 0;
            ch376->command_status = CH376_RET_ABORT;
        }
        break;
    }

    dbg_printf("<< [WRITE][DATA] Write data &%02x status &%02x\n", data, ch376->command_status);

}

/* /// */

/* /// "CH376 public init & clean" */

struct ch376 * ch376_create(void *user_data)
{
    struct ch376 *ch376 = system_alloc_mem(sizeof(struct ch376));

    if(ch376)
    {
        if(system_init_context(&ch376->context, user_data))
        {
            ch376->sdcard_drive_path = clone_string("ch376_sdcard_drive/");
            ch376->usb_drive_path = clone_string("ch376_usb_drive/");
            clear_structure(ch376);
        }
        else
        {
            system_free_mem(ch376);
            ch376 = NULL;
        }
    }
    return ch376;
}

void ch376_destroy(struct ch376 *ch376)
{
    cancel_all_io(ch376);
    system_free_mem(ch376->sdcard_drive_path);
    system_free_mem(ch376->usb_drive_path);
    system_clean_context(&ch376->context);
    system_free_mem(ch376);
}

void ch376_reset(struct ch376 *ch376)
{
    cancel_all_io(ch376);
    clear_structure(ch376);
}

/* /// */

/* /// "CH376 public configuration" */

void ch376_set_sdcard_drive_path(struct ch376 *ch376, const char *path)
{
    dbg_printf("ch376_set_sdcard_drive_path: %s\n", path);
    cancel_all_io(ch376);
    system_free_mem(ch376->sdcard_drive_path);
    ch376->sdcard_drive_path = clone_string(path);
}

void ch376_set_usb_drive_path(struct ch376 *ch376, const char *path)
{
    dbg_printf("ch376_set_usb_drive_path: %s\n", path);
    cancel_all_io(ch376);
    system_free_mem(ch376->usb_drive_path);
    ch376->usb_drive_path = clone_string(path);
}

const char * ch376_get_sdcard_drive_path(struct ch376 *ch376)
{
    return ch376->sdcard_drive_path;
}

const char * ch376_get_usb_drive_path(struct ch376 *ch376)
{
    return ch376->usb_drive_path;
}
/* /// */
