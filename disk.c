/*
**  Oricutron
**  Copyright (C) 2009-2014 Peter Gordon
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
**  WD17xx, Microdisc and Jasmin emulation
*/

/* The microdisc ROM has a bug where if no disk */
/* is inserted at boot, the disk detection routine */
/* has an unbalanced stack and there is a chance */
/* that it will end up executing garbage when you */
/* insert a disk. Euphoric has a fudge to work */
/* around this. It pretends a disk is in the drive */
/* but leaves the "read sector" command lingering */
/* until you insert a disk. Set this define to */
/* do the same fudge */
#define MICRODISC_FUDGE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "disk_pravetz.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"
#include "msgbox.h"
#include "filereq.h"

extern char diskfile[], diskpath[], filetmp[];
extern char telediskfile[], telediskpath[];
extern char pravdiskfile[], pravdiskpath[];
extern SDL_bool refreshdisks;

#define GENERAL_DISK_DEBUG 0
#define DEBUG_SECTOR_DUMP  0

#if DEBUG_SECTOR_DUMP
static char sectordumpstr[64];
static int sectordumpcount;
#endif

#ifdef MICRODISC_FUDGE
void microdisc_setintrq( void *md );
#endif

static Uint16 calc_crc( Uint16 crc, Uint8 value)
{
  crc  = ((unsigned char)(crc >> 8)) | (crc << 8);
  crc ^= value;
  crc ^= ((unsigned char)(crc & 0xff)) >> 4;
  crc ^= (crc << 8) << 4;
  crc ^= ((crc & 0xff) << 4) << 1;
  return crc;
}

// Pop up disk image information for a couple of seconds
void disk_popup( struct machine *oric, int drive )
{
  char tmp[40];
  if( !oric->diskname[drive][0] )
  {
    do_popup( oric, "\x14\x15\x13" );
    return;
  }

  sprintf( tmp, "\x14\x15""%d %-16s", drive, oric->diskname[drive] );
  do_popup( oric, tmp );
}

// Free a disk image and clear the pointer to it
static void diskimage_free( struct machine *oric, struct diskimage **dimg )
{
  char *dpath, *dfile;

  if( !dimg ) return;
  if( !(*dimg) ) return;

  if( oric->type != MACH_TELESTRAT )
  {
    switch (oric->drivetype)
    {
      case DRV_PRAVETZ:
        dpath = pravdiskpath;
        dfile = pravdiskfile;
        break;

      default:
        dpath = diskpath;
        dfile = diskfile;
        break;
    }
  }
  else
  {
    dpath = telediskpath;
    dfile = telediskfile;
  }

  if( (*dimg)->modified )
  {
    char modmsg[128];
    sprintf( modmsg, "The disk in drive %d is modified.\nWould you like to save the changes?", (*dimg)->drivenum );
    if( msgbox( oric, MSGBOX_YES_NO, modmsg ) )
    {
      sprintf( modmsg, "Save disk %d", (*dimg)->drivenum );
      if( filerequester( oric, modmsg, dpath, dfile, FR_DISKSAVE ) )
      {
        joinpath( dpath, dfile );
        diskimage_save( oric, filetmp, (*dimg)->drivenum );
      }
    }
  }

  if( (*dimg)->rawimage )
  {
    free( (*dimg)->rawimage );
    (*dimg)->rawimage = NULL;
  }

  free( *dimg );
  *dimg = NULL;
}

// Read a raw integer from a disk image file
static Uint32 diskimage_rawint( struct diskimage *dimg, Uint32 offset )
{
  if( !dimg ) return 0;
  if( ( !dimg->rawimage ) || ( dimg->rawimagelen < 4 ) ) return 0;
  if( offset > (dimg->rawimagelen-4) ) return 0;

  return (dimg->rawimage[offset+3]<<24)|(dimg->rawimage[offset+2]<<16)|(dimg->rawimage[offset+1]<<8)|dimg->rawimage[offset];
}

// Allocate and initialise a disk image structure
// If "rawimglen" isn't zero, it will also allocate
// ram for a raw disk image
static struct diskimage *diskimage_alloc( Uint32 rawimglen )
{
  struct diskimage *dimg;
  Uint8 *buf = NULL;

  if( rawimglen )
  {
    // FIXME: this is temporary solution to allow
    // low-level formatting up to 128 track per side
    if( rawimglen < 128*2*6400+256 ) rawimglen = 128*2*6400+256;
    buf = malloc( rawimglen );
    if( !buf ) return NULL;
  }

  dimg = malloc( sizeof( struct diskimage ) );
  if( !dimg )
  {
    free( buf );
    return NULL;
  }

  dimg->drivenum    = -1;
  dimg->numtracks   = 0;
  dimg->numsides    = 0;
  dimg->geometry    = 0;
  dimg->cachedtrack = -1;
  dimg->cachedside  = -1;
  dimg->numsectors  = 0;
  dimg->rawimage    = buf;
  dimg->rawimagelen = rawimglen;
  dimg->modified    = SDL_FALSE;
  dimg->modified_time = 0;
  return dimg;
}

// Eject a disk from a drive
void disk_eject( struct machine *oric, int drive )
{
  diskimage_free( oric, &oric->wddisk.disk[drive] );
  oric->wddisk.disk[drive] = NULL;

  oric->pravetz.drv[drive].pimg  = NULL;
  oric->pravetz.drv[drive].byte  = 0;
  oric->pravetz.drv[drive].dirty = SDL_FALSE;
  oric->pravetz.drv[drive].half_track = 0;
  oric->diskname[drive][0] = 0;
  disk_popup( oric, drive );
}

// Whenever a seek operation occurs, the track where the head ends up
// is "cached". The track is located within the raw image file, and
// all the sector address and data markers are found, and pointers
// are remembered for each.
void diskimage_cachetrack( struct diskimage *dimg, int track, int side )
{
  Uint8 *ptr, *eot;
  Uint32 sectorcount, n;

  // If this track is already cached, don't waste time doing it again
  if( ( dimg->cachedtrack == track ) &&
      ( dimg->cachedside == side ) )
    return;

  // reset cached info
  for( n=0; n<32; n++ )
  {
    dimg->sector[n].id_ptr = NULL;
    dimg->sector[n].data_ptr = NULL;
  }
  dimg->numsectors = 0;
  dimg->cachedtrack = -1;
  dimg->cachedside  = -1;

  if( side >= dimg->numsides )
    return;

  // Find the start and end locations of the track within the disk image
  ptr = &dimg->rawimage[(side*dimg->numtracks+track)*6400+256];
  eot = &ptr[6400];

  // Scan through the track looking for sectors
  sectorcount = 0;
  while( ptr < eot )
  {
    // Search for ID mark
    while( (ptr<eot) && (ptr[0]!=0xfe) ) ptr++;

    // Don't exceed the bounds of this track
    if( ptr >= eot ) break;

    // Store ID pointer
    dimg->sector[sectorcount].id_ptr = ptr;
    dimg->sector[sectorcount].data_ptr = NULL;
    sectorcount++;

    // Get N value
    n = ptr[4];

    // Skip ID field and CRC
    ptr+=7;

    // Search for data ID
    while( (ptr<eot) && (ptr[0]!=0xfb) && (ptr[0]!=0xf8) ) ptr++;
    if( ptr >= eot ) break;

    // Store pointer
    dimg->sector[sectorcount-1].data_ptr = ptr;

    // Skip data field and ID
    ptr += (1<<(n+7))+3;
  }

  if( 0 < sectorcount )
  {
    // Remember how many sectors we have successfully cached
    dimg->numsectors = sectorcount;
    dimg->cachedtrack = track;
    dimg->cachedside  = side;
  }
}

// This saves a diskimage back to disk.
// Since the disk image is always kept in standard format, there is
// no processing of the image in this routine, it is just dumped from
// memory back to disk.
SDL_bool diskimage_save( struct machine *oric, char *fname, int drive )
{
  FILE *f;

  // Make sure there is a disk in the drive!
  if( !oric->wddisk.disk[drive] ) return SDL_FALSE;

  if( oric->drivetype == DRV_PRAVETZ )
    disk_pravetz_write_image(&oric->pravetz.drv[drive]);

  // Open the file for writing
  f = fopen( fname, "wb" );
  if( !f )
  {
    do_popup( oric, "\x14\x15Save failed" );
    return SDL_FALSE;
  }

  // Dump it to disk
  if( fwrite( oric->wddisk.disk[drive]->rawimage, oric->wddisk.disk[drive]->rawimagelen, 1, f ) != 1 )
  {
    fclose( f );
    do_popup( oric, "\x14\x15Save failed" );
    return SDL_FALSE;
  }

  // All done!
  fclose( f );

  // If we are not just overwriting the original file, remember the new filename
  if( fname != oric->wddisk.disk[drive]->filename )
  {
    strncpy( oric->wddisk.disk[drive]->filename, fname, 4096+512-1 );
    oric->wddisk.disk[drive]->filename[4096+512-1] = 0;
  }

  // The image in memory is no longer different to the last saved version
  oric->wddisk.disk[drive]->modified = SDL_FALSE;
  oric->wddisk.disk[drive]->modified_time = 0;

  // Remember to update the GUI
  refreshdisks = SDL_TRUE;
  return SDL_TRUE;
}

// This routine "inserts" a disk image into a virtual drive
SDL_bool diskimage_load( struct machine *oric, char *fname, int drive )
{
  FILE *f;
  Uint32 len;

  // Open the file
  f = fopen( fname, "rb" );
  if( !f ) return SDL_FALSE;

  // The file exists, so eject any currently inserted disk
  disk_eject( oric, drive );

  // Determine the size of the disk image
  fseek( f, 0, SEEK_END );
  len = (int)ftell( f );
  fseek( f, 0, SEEK_SET );

  // Empty file!?
  if( len <= 0 )
  {
    fclose( f );
    return SDL_FALSE;
  }

  // Allocate a new disk image structure and space for the raw image
  oric->wddisk.disk[drive] = diskimage_alloc( len );
  if( !oric->wddisk.disk[drive] )
  {
    do_popup( oric, "\x14\x15""Out of memory" );
    fclose( f );
    return SDL_FALSE;
  }

  // Read the image file into memory
  if( fread( oric->wddisk.disk[drive]->rawimage, len, 1, f ) != 1 )
  {
    fclose( f );
    disk_eject( oric, drive );
    do_popup( oric, "\x14\x15""Read error" );
    return SDL_FALSE;
  }

  fclose( f );

  if( oric->drivetype == DRV_PRAVETZ )
  {
    Uint16  t_idx;
    Uint16  s_idx;
    Uint16  sb_idx;
    Uint16  tb_idx;

    oric->pravetz.drv[drive].pimg = oric->wddisk.disk[drive];

    if( drive >= 2 )
    {
      disk_eject( oric, drive );
      do_popup( oric, "\x14\x15""Invalid drive number" );
      return SDL_FALSE;
    }

    if( len != 143360 )
    {
      disk_eject( oric, drive );
      do_popup( oric, "\x14\x15""Invalid pravetz disk image" );
      return SDL_FALSE;
    }

    // Get some basic image info
    for (t_idx = 0; t_idx < PRAV_TRACKS_PER_DISK; t_idx++)
    {
      tb_idx = 0;

      for (s_idx = 0; s_idx < PRAV_SECTORS_PER_TRACK; s_idx++)
      {
        for (sb_idx = 0; sb_idx < PRAV_RAW_BYTES_PER_SECTOR; sb_idx++, tb_idx++)
        {
          oric->pravetz.drv[drive].image[t_idx][tb_idx] =
            disk_pravetz_image_raw_byte(oric, drive, t_idx, s_idx, sb_idx);
        }
      }

      while (tb_idx < PRAV_RAW_TRACK_SIZE)
      {
        oric->pravetz.drv[drive].image[t_idx][tb_idx] = 0xFF;
        tb_idx++;
      }
    }

    oric->pravetz.drv[drive].byte       = 0;
    oric->pravetz.drv[drive].half_track = 0;
  }
  else
  {
    // Check for the header ID
    if( strncmp( (char *)oric->wddisk.disk[drive]->rawimage, "MFM_DISK", 8 ) != 0 )
    {
      disk_eject( oric, drive );
      do_popup( oric, "\x14\x15""Invalid disk image" );
      return SDL_FALSE;
    }

    // Get some basic image info
    oric->wddisk.disk[drive]->drivenum  = drive;
    oric->wddisk.disk[drive]->numsides  = diskimage_rawint( oric->wddisk.disk[drive], 8 );
    oric->wddisk.disk[drive]->numtracks = diskimage_rawint( oric->wddisk.disk[drive], 12 );
    oric->wddisk.disk[drive]->geometry  = diskimage_rawint( oric->wddisk.disk[drive], 16 );

    // Is the disk sane!?
    if( ( oric->wddisk.disk[drive]->numsides < 1 ) ||
      ( oric->wddisk.disk[drive]->numsides > 2 ) )
    {
      disk_eject( oric, drive );
      do_popup( oric, "\x14\x15""Invalid disk image" );
      return SDL_FALSE;
    }
  }

  // Nobody has written to this disk yet
  oric->wddisk.disk[drive]->modified = SDL_FALSE;
  oric->wddisk.disk[drive]->modified_time = 0;

  // Remember the filename of the image for this drive
  strncpy( oric->wddisk.disk[drive]->filename, fname, 4096+512-1 );
  oric->wddisk.disk[drive]->filename[4096+512-1] = 0;

  // Come up with a suitable short name for popups etc.
  if( strlen( fname ) > 31 )
  {
    strncpy( oric->diskname[drive], &fname[strlen(fname)-31], 32 );
    oric->diskname[drive][0] = 22;
  } else {
    strncpy( oric->diskname[drive], fname, 32 );
  }
  oric->diskname[drive][31] = 0;

  // Do the popup
  disk_popup( oric, drive );

  // Mark the disk status icons as needing a refresh
  refreshdisks = SDL_TRUE;
  return SDL_TRUE;
};

// This routine does nothing. It is just used as a default for the callback routines
// and is usually replaced by the microdisc/jasmin/whatever implementations.
void wd17xx_dummy( void *nothing )
{
}

// Initialise a WD17xx controller instance
void wd17xx_init( struct wd17xx *wd )
{
  wd->r_status = 0;
  wd->r_track  = 0;
  wd->r_sector = 0;
  wd->r_data   = 0;
  wd->c_drive  = 0;
  wd->c_side   = 0;
  wd->c_track  = 0;
  wd->c_sector = 0;
  wd->last_step_in = SDL_FALSE;
  wd->setintrq = wd17xx_dummy;
  wd->clrintrq = wd17xx_dummy;
  wd->intrqarg = NULL;
  wd->setdrq   = wd17xx_dummy;
  wd->clrdrq   = wd17xx_dummy;
  wd->drqarg   = NULL;
  wd->currentop = COP_NUFFINK;
  wd->delayedint = 0;
  wd->delayeddrq = 0;
  wd->distatus   = -1;
  wd->ddstatus   = -1;
  refreshdisks = SDL_TRUE;
}

// This routine emulates some cycles of disk activity.
void wd17xx_ticktock( struct wd17xx *wd, int cycles )
{
#ifdef MICRODISC_FUDGE
  if ((wd->currentop == COP_READ_SECTORS_FUDGE) ||
      (wd->currentop == COP_READ_SECTOR_FUDGE))
  {
    if (wd->disk[wd->c_drive])
    {
      wd->curroffs   = 0;
      wd->currsector = wd17xx_find_sector( wd, wd->r_sector );
      if( !wd->currsector )
      {
        wd->r_status = WSF_RNF;
        wd->clrdrq( wd->drqarg );
        wd->setintrq( wd->intrqarg );
        wd->currentop = COP_NUFFINK;
        refreshdisks = SDL_TRUE;
#if GENERAL_DISK_DEBUG
        dbg_printf( "DISK: Sector %d not found.", wd->r_sector );
#endif
      }
      else
      {
        wd->currseclen = 1<<(wd->currsector->id_ptr[4]+7);
        wd->r_status   = WSF_BUSY|WSF_NOTREADY;
        wd->delayeddrq = 60;
        wd->currentop  = (wd->currentop == COP_READ_SECTORS_FUDGE) ? COP_READ_SECTORS : COP_READ_SECTOR;
        wd->crc        = 0xe295;
        refreshdisks = SDL_TRUE;
#if DEBUG_SECTOR_DUMP
        sectordumpcount = 0;
        sectordumpstr[0] = 0;
#endif
      }
    }
  }
#endif

  // Is there a pending INTRQ?
  if( wd->delayedint > 0 )
  {
    // Count down the INTRQ timer!
    wd->delayedint -= cycles;

    // Time to assert INTRQ?
    if( wd->delayedint <= 0 )
    {
      // Yep! Stop timing.
      wd->delayedint = 0;

      // Need to update the status register?
      if( wd->distatus != -1 )
      {
        // Yep. Do so.
        wd->r_status = wd->distatus;
        wd->distatus = -1;
      }

      // Assert INTRQ (this function pointer is set up by the microdisc/jasmin/whatever controller)
      wd->setintrq( wd->intrqarg );
#if GENERAL_DISK_DEBUG
      dbg_printf( "DISK: Delayed INTRQ" );
#endif
    }
  }

  // Is there a pending DRQ?
  if( wd->delayeddrq > 0 )
  {
    // Count down the DRQ timer!
    wd->delayeddrq -= cycles;

    // Time to assert DRQ?
    if( wd->delayeddrq <= 0 )
    {
      // Yep! Stop timing.
      wd->delayeddrq = 0;

      // Need to update the status register?
      if( wd->ddstatus != -1 )
      {
        // Yep. Do so.
        wd->r_status = wd->ddstatus;
        wd->ddstatus = -1;
      }

      // Assert DRQ
      wd->r_status |= WSF_DRQ;
      wd->setdrq( wd->drqarg );
    }
  }
}

// This routine seeks to the specified track. It is used by the SEEK and STEP commands.
void wd17xx_seek_track( struct wd17xx *wd, Uint8 track, SDL_bool dofudge )
{
  // Is there a disk in the drive?
  if( wd->disk[wd->c_drive] )
  {
    // Yes. If we are trying to seek to a non-existant track, just seek as far as we can
    if( track >= wd->disk[wd->c_drive]->numtracks )
    {
      track = (wd->disk[wd->c_drive]->numtracks>0)?wd->disk[wd->c_drive]->numtracks-1:0;
      wd->distatus = WSFI_HEADL|WSFI_SEEKERR;
    }
    else
    {
      wd->distatus = WSFI_HEADL|WSFI_PULSE;
    }

    // Cache the new track
    diskimage_cachetrack( wd->disk[wd->c_drive], track, wd->c_side );

    // Update our status
    wd->c_track = track;
    wd->c_sector = 0;
    wd->r_track = track;

    // Assert INTRQ in 20 cycles time and update the status accordingly
    // (note: 20 cycles is waaaaaay faster than any real drive could seek. The actual
    // delay would depend how far the head had to seek, and what stepping speed was
    // currently set).
    wd->delayedint = 20;
    if( wd->c_track == 0 ) wd->distatus |= WSFI_TRK0;
#if GENERAL_DISK_DEBUG
    dbg_printf( "DISK: At track %u (%u sectors)", track, wd->disk[wd->c_drive]->numsectors );
#endif
    return;
  }

  // No disk in drive
  // Set INTRQ because the operation has finished.
  wd->setintrq( wd->intrqarg );
  wd->r_track = 0;

#if GENERAL_DISK_DEBUG
  dbg_printf( "DISK: Seek fail" );
#endif

#ifdef MICRODISC_FUDGE
  // Euphoric style workaround for the sedoric bug...
  if ((dofudge) && (wd->setintrq == microdisc_setintrq))
  {
      wd->r_status = 0x44;
      return;
  }
#endif

  // Set error state
  wd->r_status = WSF_NOTREADY|WSFI_SEEKERR;
}

// This routine looks for the sector with the specified ID in the current track.
// It returns NULL if there is no such sector, or a structure with pointers to
// the ID and data fields if the sector is found.
struct mfmsector *wd17xx_find_sector( struct wd17xx *wd, Uint8 secid )
{
  int revs=0;
  struct diskimage *dimg;

  // Save some typing...
  dimg = wd->disk[wd->c_drive];

  // No disk image? No sectors!
  if( !dimg ) return NULL;

  // Make sure the current track is cached
  diskimage_cachetrack( dimg, wd->c_track, wd->c_side );

  // No sectors on this track? Someone needs to format their disk...
  if( dimg->numsectors < 1 )
    return NULL;

  // We do this more realistically than we need to since this is not
  // a super-accurate emulation (for now). Never mind. Lets go
  // around the track up to two times.
  while( revs < 2 )
  {
    // Move on to the next sector
    wd->c_sector = (wd->c_sector+1)%dimg->numsectors;

    // If we passed through the start of the track, set the pulse bit in the status register
    if( !wd->c_sector )
    {
      revs++;
      wd->r_status |= WSFI_PULSE;
    }

    // Found the required sector?
    if( dimg->sector[wd->c_sector].id_ptr[3] == secid )
      return &dimg->sector[wd->c_sector];
  }

  // The search failed :-(
#if GENERAL_DISK_DEBUG
  dbg_printf( "Couldn't find sector %u", secid );
#endif
  return NULL;
}

// This returns the first valid sector in the current track, or NULL if
// there aren't any sectors.
struct mfmsector *wd17xx_first_sector( struct wd17xx *wd )
{
  struct diskimage *dimg;

  dimg = wd->disk[wd->c_drive];

  // No disk? no sector...
  if( !dimg ) return NULL;

  // Make sure the current track is cached
  diskimage_cachetrack( dimg, wd->c_track, wd->c_side );

  // No sectors?!
  if( dimg->numsectors < 1 )
    return NULL;

  // We're at the first sector!
  wd->c_sector = 0;
  wd->r_status = WSFI_PULSE;

  // Return the sector pointers
  return &dimg->sector[wd->c_sector];
}

// Move on to the next sector
struct mfmsector *wd17xx_next_sector( struct wd17xx *wd )
{
  struct diskimage *dimg;

  dimg = wd->disk[wd->c_drive];

  // No disk? No sectors!
  if( !dimg ) return NULL;

  // Make sure the current track is cached
  diskimage_cachetrack( dimg, wd->c_track, wd->c_side );

  // No sectors?
  if( dimg->numsectors < 1 )
    return NULL;

  // Get the next sector number
  wd->c_sector = (wd->c_sector+1)%dimg->numsectors;

  // If we are at the start of the track, set the pulse bit
  if( !wd->c_sector ) wd->r_status |= WSFI_PULSE;

  // Return the sector pointers
  return &dimg->sector[wd->c_sector];
}

// Perform a read operation on a WD17xx register
unsigned char wd17xx_read( struct wd17xx *wd, unsigned short addr )
{
  // Which register?!
  switch( addr )
  {
    case 0: // Status register
      wd->clrintrq( wd->intrqarg );   // Reading the status register clears INTRQ
      return wd->r_status;

    case 1: // Track register
      return wd->r_track;

    case 2: // Sector register
      return wd->r_sector;

    case 3: // Data register
      // What are we currently doing?
      switch( wd->currentop )
      {
#ifdef MICRODISC_FUDGE
        case COP_READ_SECTOR_FUDGE:
        case COP_READ_SECTORS_FUDGE:
          return 0;
#endif

        case COP_READ_SECTOR:
        case COP_READ_SECTORS:
          // We somehow started a sector read operation without a valid sector.
          if( !wd->currsector )
          {
            // Abort.
            wd->r_status &= ~WSF_DRQ;
            wd->r_status |= WSF_RNF;
            wd->clrdrq( wd->drqarg );
            wd->currentop = COP_NUFFINK;
            refreshdisks = SDL_TRUE;
            break;
          }

          // If this is the first read of a read operation, remember the record type for later
          if( wd->curroffs == 0 ) wd->sectype = (wd->currsector->data_ptr[wd->curroffs++]==0xf8)?WSFR_RECTYP:0x00;

          // Get the next byte from the sector
          wd->r_data = wd->currsector->data_ptr[wd->curroffs++];
          wd->crc = calc_crc( wd->crc, wd->r_data );

          // Clear any previous DRQ
          wd->r_status &= ~WSF_DRQ;
          wd->clrdrq( wd->drqarg );

#if DEBUG_SECTOR_DUMP
          sprintf( &sectordumpstr[sectordumpcount*2], "%02X", wd->r_data );
          sectordumpstr[34+sectordumpcount] = ((wd->r_data>31)&&(wd->r_data<127)) ? wd->r_data : '.';
          sectordumpcount++;
          if( sectordumpcount >= 16 )
          {
            sectordumpstr[32] = ' ';
            sectordumpstr[33] = '\'';
            sectordumpstr[50] = '\'';
            sectordumpstr[51] = 0;
            dbg_printf( "%s", sectordumpstr );
            sectordumpcount = 0;
          }
#endif

          // Has the whole sector been read?
          if( wd->curroffs > wd->currseclen )
          {
            // If you want to do CRC checking, wd->crc should equal (wd->currsector->data_ptr[wd_curroffs]<<8)|wd->currsector->data_ptr[wd_curroffs+1] right here
#if DEBUG_SECTOR_DUMP
            if( sectordumpcount )
            {
              sectordumpstr[33] = '\'';
              sectordumpstr[34+sectordumpcount] = '\'';
              sectordumpstr[35+sectordumpcount] = 0;
              for( sectordumpcount*=2; sectordumpcount<33; sectordumpcount++ )
                sectordumpstr[sectordumpcount*2] = ' ';
              dbg_printf( "%s", sectordumpstr );
              sectordumpcount = 0;
            }
#endif

            // We've got to the end of the current sector. IF it is a multiple sector
            // operation, we need to move on!
            if( wd->currentop == COP_READ_SECTORS )
            {
#if GENERAL_DISK_DEBUG
              dbg_printf( "Next sector..." );
#endif
              // Get the next sector, and carry on!
              wd->r_sector++;
              wd->curroffs   = 0;
              wd->currsector = wd17xx_find_sector( wd, wd->r_sector );
              wd->crc        = 0xe295;

              // If we hit the end of the track, thats fine, it just means the operation
              // is finished.
              if( !wd->currsector )
              {
                wd->delayedint = 20;           // Assert INTRQ in 20 cycles time
                wd->distatus   = wd->sectype;  // ...and when doing so, set the status to reflect the record type
                wd->currentop = COP_NUFFINK;   // No longer in the middle of an operation
                wd->r_status &= (~WSF_DRQ);    // Clear DRQ (no data to read)
                wd->clrdrq( wd->drqarg );
                refreshdisks = SDL_TRUE;       // Turn off the disk LED in the status bar
                break;
              }

              // We've got the next sector lined up. Assert DRQ in 180 cycles time (simulate a bit of a delay
              // between sectors. Note that most of these values have been pulled out of thin air and might need
              // adjusting for some pickier loaders).
              wd->delayeddrq = 180;
              break;
            }

            // Just reading one sector so..
            wd->delayedint = 32;           // INTRQ in a little while because we're finished
            wd->distatus   = wd->sectype;  // Set the status accordingly
            wd->currentop = COP_NUFFINK;   // Finished the op
            wd->r_status &= (~WSF_DRQ);    // Clear DRQ (no more data)
            wd->clrdrq( wd->drqarg );
            refreshdisks = SDL_TRUE;       // Turn off disk LED
          } else {
            wd->delayeddrq = 32;           // More data ready. DRQ to let them know!
          }
          break;

        case COP_READ_TRACK:

          wd->r_data = wd->disk[wd->c_drive]->rawimage[(wd->c_side*wd->disk[wd->c_drive]->numtracks + wd->c_track)*6400+256 + wd->curroffs];
          wd->curroffs++;
          wd->r_status &= ~WSF_DRQ;
          wd->clrdrq( wd->drqarg );

          // Has the whole track been read?
          if( wd->curroffs >= 6400 )
          {
            wd->delayedint = 20;
            wd->distatus   = 0;
            wd->currentop = COP_NUFFINK;
            refreshdisks = SDL_TRUE;
          } else {
            wd->delayeddrq = 32;
          }

          break;

        case COP_READ_ADDRESS:
          if( !wd->currsector )
          {
            wd->r_status &= ~WSF_DRQ;
            wd->clrdrq( wd->drqarg );
            wd->currentop = COP_NUFFINK;
            refreshdisks = SDL_TRUE;
            break;
          }
          if( wd->curroffs == 0 ) wd->r_sector = wd->currsector->id_ptr[1];
          wd->r_data = wd->currsector->id_ptr[++wd->curroffs];
          wd->r_status &= ~WSF_DRQ;
          wd->clrdrq( wd->drqarg );
          if( wd->curroffs >= 6 )
          {
            wd->delayedint = 20;
            wd->distatus   = 0;
            wd->currentop = COP_NUFFINK;
            refreshdisks = SDL_TRUE;
          } else {
            wd->delayeddrq = 32;
          }
          break;
      }

      return wd->r_data;
  }

  return 0; // ??
}

static SDL_bool last_step_in = SDL_FALSE;
void wd17xx_write( struct machine *oric, struct wd17xx *wd, unsigned short addr, unsigned char data )
{
  switch( addr )
  {
    case 0: // Command register
      wd->clrintrq( wd->intrqarg );
      switch( data & 0xe0 )
      {
        case 0x00:  // Restore or seek
          switch( data & 0x10 )
          {
            case 0x00:  // Restore (Type I)
#if GENERAL_DISK_DEBUG
              dbg_printf( "DISK: (%04X) Restore", oric->cpu.pc-1 );
#endif
              wd->r_status = WSF_BUSY;
              if( data & 8 ) wd->r_status |= WSFI_HEADL;
              wd17xx_seek_track( wd, 0, oric->cpu.calcpc == 0xe3a1 );
              wd->currentop = COP_NUFFINK;
              refreshdisks = SDL_TRUE;
              break;

            case 0x10:  // Seek (Type I)
#if GENERAL_DISK_DEBUG
              dbg_printf( "DISK: (%04X) Seek", oric->cpu.pc-1 );
#endif
              wd->r_status = WSF_BUSY;
              if( data & 8 ) wd->r_status |= WSFI_HEADL;
              wd17xx_seek_track( wd, wd->r_data, SDL_FALSE );
              wd->currentop = COP_NUFFINK;
              refreshdisks = SDL_TRUE;
              break;
          }
          break;

        case 0x20:  // Step (Type I)
#if GENERAL_DISK_DEBUG
          dbg_printf( "DISK: (%04X) Step", oric->cpu.pc-1 );
#endif
          wd->r_status = WSF_BUSY;
          if( data & 8 ) wd->r_status |= WSFI_HEADL;
          if( last_step_in )
            wd17xx_seek_track( wd, wd->c_track+1, SDL_FALSE );
          else
            wd17xx_seek_track( wd, wd->c_track > 0 ? wd->c_track-1 : 0, SDL_FALSE );
          wd->currentop = COP_NUFFINK;
          refreshdisks = SDL_TRUE;
          break;

        case 0x40:  // Step-in (Type I)
#if GENERAL_DISK_DEBUG
          dbg_printf( "DISK: (%04X) Step-In (%d,%d)", oric->cpu.pc-1, wd->c_track, wd->c_track+1 );
#endif
          wd->r_status = WSF_BUSY;
          if( data & 8 ) wd->r_status |= WSFI_HEADL;
          wd17xx_seek_track( wd, wd->c_track+1, SDL_FALSE );
          last_step_in = SDL_TRUE;
          wd->currentop = COP_NUFFINK;
          refreshdisks = SDL_TRUE;
          break;

        case 0x60:  // Step-out (Type I)
#if GENERAL_DISK_DEBUG
          dbg_printf( "DISK: Step-Out" );
#endif
          wd->r_status = WSF_BUSY;
          if( data & 8 ) wd->r_status |= WSFI_HEADL;
          if( wd->c_track > 0 )
            wd17xx_seek_track( wd, wd->c_track-1, SDL_FALSE );
          last_step_in = SDL_FALSE;
          wd->currentop = COP_NUFFINK;
          refreshdisks = SDL_TRUE;
          break;

        case 0x80:  // Read sector (Type II)
#if GENERAL_DISK_DEBUG
          switch( oric->drivetype )
          {
            case DRV_MICRODISC:
              dbg_printf( "DISK: (%04X) Read sector %u (CODE=%02X,ROM=%s,EPROM=%s)", oric->cpu.pc-1, wd->r_sector, data, oric->romdis?"OFF":"ON", oric->md.diskrom?"ON":"OFF" );
              break;

            default:
              dbg_printf( "DISK: (%04X) Read sector %u (CODE=%02X)", oric->cpu.pc-1, wd->r_sector, data );
              break;
          }
#endif
#ifdef MICRODISC_FUDGE
          if (!wd->disk[wd->c_drive])
          {
            wd->currentop = (data&0x10) ? COP_READ_SECTORS_FUDGE : COP_READ_SECTOR_FUDGE;
            break;
          }
#endif

          wd->curroffs   = 0;
          wd->currsector = wd17xx_find_sector( wd, wd->r_sector );
          if( !wd->currsector )
          {
            wd->r_status = WSF_RNF;
            wd->clrdrq( wd->drqarg );
            wd->setintrq( wd->intrqarg );
            wd->currentop = COP_NUFFINK;
            refreshdisks = SDL_TRUE;
#if GENERAL_DISK_DEBUG
            dbg_printf( "DISK: Sector %d not found.", wd->r_sector );
#endif
//            setemumode( oric, NULL, EM_DEBUG );
            break;
          }

          wd->currseclen = 1<<(wd->currsector->id_ptr[4]+7);
          wd->r_status   = WSF_BUSY|WSF_NOTREADY;
          wd->delayeddrq = 60;
          wd->currentop  = (data&0x10) ? COP_READ_SECTORS : COP_READ_SECTOR;
          wd->crc        = 0xe295;
          refreshdisks = SDL_TRUE;
#if DEBUG_SECTOR_DUMP
          sectordumpcount = 0;
          sectordumpstr[0] = 0;
#endif
          break;

        case 0xa0:  // Write sector (Type II)
#if GENERAL_DISK_DEBUG
          switch( oric->drivetype )
          {
            case DRV_MICRODISC:
              dbg_printf( "DISK: (%04X) Write sector %u (CODE=%02X,ROM=%s,EPROM=%s)", oric->cpu.pc-1, wd->r_sector, data, oric->romdis?"OFF":"ON", oric->md.diskrom?"ON":"OFF" );
              break;

            default:
              dbg_printf( "DISK: (%04X) Write sector %u (CODE=%02X)", oric->cpu.pc-1, wd->r_sector, data );
              break;
          }
#endif
          wd->curroffs   = 0;
          wd->currsector = wd17xx_find_sector( wd, wd->r_sector );
          if( !wd->currsector )
          {
            wd->r_status = WSF_RNF;
            wd->clrdrq( wd->drqarg );
            wd->setintrq( wd->intrqarg );
            wd->currentop = COP_NUFFINK;
            refreshdisks = SDL_TRUE;
#if GENERAL_DISK_DEBUG
            dbg_printf( "DISK: Sector %d not found.", wd->r_sector );
#endif
//            setemumode( oric, NULL, EM_DEBUG );
            break;
          }

          wd->currseclen = 1<<(wd->currsector->id_ptr[4]+7);
          wd->r_status   = WSF_BUSY|WSF_NOTREADY;
          wd->delayeddrq = 500;
          wd->currentop  = (data&0x10) ? COP_WRITE_SECTORS : COP_WRITE_SECTOR;
          wd->crc        = 0xe295;
          refreshdisks = SDL_TRUE;
          break;

        case 0xc0:  // Read address / Force IRQ
          switch( data & 0x10 )
          {
            case 0x00: // Read address (Type III)
#if GENERAL_DISK_DEBUG
              dbg_printf( "DISK: (%04X) Read address", oric->cpu.pc-1 );
#endif
              wd->curroffs = 0;
              if( !wd->currsector )
                wd->currsector = wd17xx_first_sector( wd );
              else
                wd->currsector = wd17xx_next_sector( wd );

              if( !wd->currsector )
              {
                wd->r_status = WSF_RNF;
                wd->clrdrq( wd->drqarg );
                wd->currentop = COP_NUFFINK;
                wd->setintrq( wd->intrqarg );
                refreshdisks = SDL_TRUE;
#if GENERAL_DISK_DEBUG
                dbg_printf( "DISK: No sectors on this track?" );
#endif
                break;
              }

#if GENERAL_DISK_DEBUG
              dbg_printf( "DISK: %02X,%02X,%02X,%02X,%02X,%02X",
                wd->currsector->id_ptr[1],
                wd->currsector->id_ptr[2],
                wd->currsector->id_ptr[3],
                wd->currsector->id_ptr[4],
                wd->currsector->id_ptr[5],
                wd->currsector->id_ptr[6] );
#endif
              wd->r_status = WSF_NOTREADY|WSF_BUSY|WSF_DRQ;
              wd->setdrq( wd->drqarg );
              wd->currentop = COP_READ_ADDRESS;
              refreshdisks = SDL_TRUE;
              break;

            case 0x10: // Force Interrupt (Type IV)
#if GENERAL_DISK_DEBUG
              dbg_printf( "DISK: (%04X) Force int", oric->cpu.pc-1 );
#endif
              wd->r_status = 0;
              wd->clrdrq( wd->drqarg );
              wd->setintrq( wd->intrqarg );
              wd->delayedint = 0;
              wd->delayeddrq = 0;
              wd->currentop = COP_NUFFINK;
              refreshdisks = SDL_TRUE;
              break;
          }
          break;

        case 0xe0:  // Read track / Write track
          switch( data & 0x10 )
          {
            case 0x00: // Read track (Type III)
#if GENERAL_DISK_DEBUG
              dbg_printf( "DISK: (%04X) Read track", oric->cpu.pc-1 );
#endif
              wd->curroffs = 0;
              wd->r_status   = WSF_BUSY|WSF_NOTREADY;
              wd->delayeddrq = 60;

              wd->currentop = COP_READ_TRACK;
              refreshdisks = SDL_TRUE;
              break;

            case 0x10: // Write track (Type III)
#if GENERAL_DISK_DEBUG
              dbg_printf( "DISK: (%04X) Write track", oric->cpu.pc-1 );
#endif
              wd->curroffs = 0;
              wd->r_status = WSF_NOTREADY|WSF_BUSY;
              wd->delayeddrq = 500;
              wd->clrdrq( wd->drqarg );
              wd->clrintrq( wd->intrqarg );
              wd->delayedint = 0;

              wd->currentop = COP_WRITE_TRACK;
              refreshdisks = SDL_TRUE;
              break;
          }
          break;
      }
      break;

    case 1: // Track register
      wd->r_track = data;
      break;

    case 2: // Sector register
      wd->r_sector = data;
      break;

    case 3: // Data register
      wd->r_data = data;

      switch( wd->currentop )
      {
        case COP_WRITE_SECTOR:
        case COP_WRITE_SECTORS:
          if( !wd->currsector )
          {
            wd->r_status &= ~WSF_DRQ;
            wd->r_status |= WSF_RNF;
            wd->clrdrq( wd->drqarg );
            wd->currentop = COP_NUFFINK;
            refreshdisks = SDL_TRUE;
            break;
          }
          if( wd->curroffs == 0 ) wd->currsector->data_ptr[wd->curroffs++]=0xfb;
          wd->currsector->data_ptr[wd->curroffs++] = wd->r_data;
          wd->crc = calc_crc( wd->crc, wd->r_data );
          if( !wd->disk[wd->c_drive]->modified ) refreshdisks = SDL_TRUE;
          wd->disk[wd->c_drive]->modified = SDL_TRUE;
          wd->disk[wd->c_drive]->modified_time = 0;
          wd->r_status &= ~WSF_DRQ;
          wd->clrdrq( wd->drqarg );

          if( wd->curroffs > wd->currseclen )
          {
            wd->currsector->data_ptr[wd->curroffs++] = wd->crc>>8;
            wd->currsector->data_ptr[wd->curroffs++] = wd->crc;
            if( wd->currentop == COP_WRITE_SECTORS )
            {
#if GENERAL_DISK_DEBUG
              dbg_printf( "Next sector..." );
#endif
              // Get the next sector, and carry on!
              wd->r_sector++;
              wd->curroffs   = 0;
              wd->currsector = wd17xx_find_sector( wd, wd->r_sector );
              wd->crc        = 0xe295;

              if( !wd->currsector )
              {
                wd->delayedint = 20;
                wd->distatus   = wd->sectype;
                wd->currentop = COP_NUFFINK;
                wd->r_status &= (~WSF_DRQ);
                wd->clrdrq( wd->drqarg );
                refreshdisks = SDL_TRUE;
                break;
              }
              wd->delayeddrq = 180;
              break;
            }

            wd->delayedint = 32;
            wd->distatus   = wd->sectype;
            wd->currentop = COP_NUFFINK;
            wd->r_status &= (~WSF_DRQ);
            wd->clrdrq( wd->drqarg );
            refreshdisks = SDL_TRUE;
          } else {
            wd->delayeddrq = 32;
          }
          break;

        case COP_WRITE_TRACK:
          switch(data)
          {
            // All bytes > 0xF4 are control bytes
            case 0xf5:
              // MFM: Initialize CRC generator
              // FM : Not allowed
#if GENERAL_DISK_DEBUG
              dbg_printf( "\tCOP_WRITE_TRACK: %02X -> SYNC (clear crc) -> 0xA1", wd->r_data);
#endif
              wd->crc = 0x968b;
              wd->r_data = 0xa1;
              wd->crc = calc_crc( wd->crc, wd->r_data );
              // wd->crc = 0xcdb4;
              break;

            case 0xf6:
              // FM : Not allowed
#if GENERAL_DISK_DEBUG
              dbg_printf( "\tCOP_WRITE_TRACK: %02X -> SYNC -> 0xC2", wd->r_data);
#endif
              wd->r_data = 0xc2;
              wd->crc = calc_crc( wd->crc, wd->r_data );
              break;

            case 0xf7:
#if GENERAL_DISK_DEBUG
              dbg_printf( "\tCOP_WRITE_TRACK: %02X -> CRC -> %04X", wd->r_data, wd->crc);
#endif
              wd->disk[wd->c_drive]->rawimage[(wd->c_side*wd->disk[wd->c_drive]->numtracks + wd->c_track)*6400+256 + wd->curroffs] = wd->crc >> 8;
              wd->curroffs++;
              wd->r_data = wd->crc & 0xff;
              break;

#if GENERAL_DISK_DEBUG
            // MFM: 0xf8 -> 0xff: write control byte
            // FM : 0xf8 -> 0xfe: Initialize CRC generator
            case 0xf8:
              dbg_printf( "\tCOP_WRITE_TRACK: %02X -> Data Address Mark (deleted)", wd->r_data);
              wd->crc = calc_crc( wd->crc, wd->r_data );
              // next bytes: data, <CRC>;
              break;

            case 0xf9:
              dbg_printf( "\tCOP_WRITE_TRACK: %02X -> Data Mark", wd->r_data);
              wd->crc = calc_crc( wd->crc, wd->r_data );
              break;

            case 0xfa:
              dbg_printf( "\tCOP_WRITE_TRACK: %02X -> Data Mark", wd->r_data);
              wd->crc = calc_crc( wd->crc, wd->r_data );
              break;

            case 0xfb:
              dbg_printf( "\tCOP_WRITE_TRACK: %02X -> Data Address Mark (normal)", wd->r_data);
              wd->crc = calc_crc( wd->crc, wd->r_data );
              // next bytes: data, <CRC>;
              break;

            case 0xfc:
              dbg_printf( "\tCOP_WRITE_TRACK: %02X -> Data Mark", wd->r_data);
              wd->crc = calc_crc( wd->crc, wd->r_data );
              break;

            case 0xfd:
              dbg_printf( "\tCOP_WRITE_TRACK: %02X -> Data Mark", wd->r_data);
              wd->crc = calc_crc( wd->crc, wd->r_data );
              break;

            case 0xfe:
              dbg_printf( "\tCOP_WRITE_TRACK: %02X -> Index Address Mark", wd->r_data);
              wd->crc = calc_crc( wd->crc, wd->r_data );
              // next 6 bytes: track, side, sector, size, <CRC>;
              break;

            case 0xff:
              dbg_printf( "\tCOP_WRITE_TRACK: %02X -> ???", wd->r_data);
              wd->crc = calc_crc( wd->crc, wd->r_data );
              break;
#endif

            default:
              wd->crc = calc_crc( wd->crc, wd->r_data );

          }

         // Write byte to disk image
#if GENERAL_DISK_DEBUG
          dbg_printf("\tCOP_WRITE_TRACK: write byte: %02X '%c'", wd->r_data, wd->r_data);
#endif
          wd->disk[wd->c_drive]->rawimage[(wd->c_side*wd->disk[wd->c_drive]->numtracks + wd->c_track)*6400+256 + wd->curroffs] = wd->r_data;
          wd->curroffs++;

          wd->r_status &= ~WSF_DRQ;
          wd->clrdrq( wd->drqarg );

          wd->disk[wd->c_drive]->modified = SDL_TRUE;
          wd->disk[wd->c_drive]->modified_time = 0;
          refreshdisks = SDL_TRUE;

          // Has the whole track been written?
	  if (wd->curroffs >= 6400)
	  {
#if GENERAL_DISK_DEBUG
            dbg_printf("\tCOP_WRITE_TRACK: track full (%d)", wd->curroffs);
#endif
            wd->delayedint = 32;
            wd->currentop = COP_NUFFINK;
            // wd->r_status &= (~WSF_DRQ);
            wd->r_status = 0;
            wd->clrdrq( wd->drqarg );
	  }
	  else
            wd->delayeddrq = 64;

          break;
      }
      break;
  }
}

// Microdisc interface handlers
void microdisc_setdrq( void *md )
{
  struct microdisc *mdp = (struct microdisc *)md;
  mdp->drq = 0;
}

void microdisc_clrdrq( void *md )
{
  struct microdisc *mdp = (struct microdisc *)md;
  mdp->drq = MF_DRQ;
}

void microdisc_setintrq( void *md )
{
  struct microdisc *mdp = (struct microdisc *)md;

  mdp->intrq = 0; //MDSF_INTRQ;
  if( mdp->status & MDSF_INTENA )
    mdp->oric->cpu.irq |= IRQF_DISK;
}

void microdisc_clrintrq( void *md )
{
  struct microdisc *mdp = (struct microdisc *)md;

  mdp->intrq = MDSF_INTRQ;
  mdp->oric->cpu.irq &= ~IRQF_DISK;
}

void microdisc_init( struct microdisc *md, struct wd17xx *wd, struct machine *oric )
{
  wd17xx_init( wd );
  wd->setintrq = microdisc_setintrq;
  wd->clrintrq = microdisc_clrintrq;
  wd->intrqarg = (void*)md;
  wd->setdrq   = microdisc_setdrq;
  wd->clrdrq   = microdisc_clrdrq;
  wd->drqarg   = (void*)md;

  md->status  = 0;
  md->intrq   = 0;
  md->drq     = 0;
  md->wd      = wd;
  md->oric    = oric;
  md->diskrom = SDL_TRUE;
}

void microdisc_free( struct microdisc *md )
{
  int i;
  for( i=0; i<MAX_DRIVES; i++ )
    disk_eject( md->oric, i );
}

unsigned char microdisc_read( struct microdisc *md, unsigned short addr )
{
//  dbg_printf( "DISK: (%04X) Read from %04X", md->oric->cpu.pc-1, addr );
  if( ( addr >= 0x310 ) && ( addr < 0x314 ) )
    return wd17xx_read( md->wd, addr&3 );

  switch( addr )
  {
    case 0x314:
      return md->intrq|0x7f;

    case 0x318:
      return md->drq|0x7f;

    default:
      break;
  }

  return via_read( &md->oric->via, addr );
}

void microdisc_write( struct microdisc *md, unsigned short addr, unsigned char data )
{
//  dbg_printf( "DISK: (%04X) Write %02X to %04X", md->oric->cpu.pc-1, data, addr );
  if( ( addr >= 0x310 ) && ( addr < 0x314 ) )
  {
    wd17xx_write( md->oric, md->wd, addr&3, data );
    return;
  }

  switch( addr )
  {
    case 0x314:
      md->status = data;

      // Interrupts enabled, and /INTRQ == 0 ?
      if( ( data&MDSF_INTENA ) && ( md->intrq == 0 ) )
      {
        md->oric->cpu.irq |= IRQF_DISK;
      } else {
        md->oric->cpu.irq &= ~IRQF_DISK;
      }

      md->wd->c_drive  = (data&MDSF_DRIVE)>>5;
      md->wd->c_side   = (data&MDSF_SIDE) ? 1 : 0;
      md->oric->romdis = (data&MDSF_ROMDIS) ? SDL_FALSE : SDL_TRUE;
      md->diskrom      = (data&MDSF_EPROM) ? SDL_FALSE : SDL_TRUE;
      break;

    case 0x318:
      md->drq = (data&MF_DRQ);
      break;

    default:
      via_write( &md->oric->via, addr, data );
      break;
  }
}

// Byte Drive 500 interface handlers
void bd500_setdrq( void *bd )
{
  struct bd500 *bdp = (struct bd500 *)bd;
  bdp->drq = 0x80;
}

void bd500_clrdrq( void *bd )
{
  struct bd500 *bdp = (struct bd500 *)bd;
  bdp->drq = 0;
}

void bd500_setintrq( void *bd )
{
  struct bd500 *bdp = (struct bd500 *)bd;
  bdp->intrq = 0x40;
}

void bd500_clrintrq( void *bd )
{
  struct bd500 *bdp = (struct bd500 *)bd;
  bdp->intrq = 0;
}

void bd500_init( struct bd500 *bd, struct wd17xx *wd, struct machine *oric )
{
  wd17xx_init( wd );
  wd->setintrq = bd500_setintrq;
  wd->clrintrq = bd500_clrintrq;
  wd->intrqarg = (void*)bd;
  wd->setdrq   = bd500_setdrq;
  wd->clrdrq   = bd500_clrdrq;
  wd->drqarg   = (void*)bd;

  bd->status  = 0;
  bd->intrq   = 0;
  bd->drq     = 0;
  bd->wd      = wd;
  bd->oric    = oric;
  bd->diskrom = SDL_TRUE;
}

void bd500_free( struct bd500 *bd )
{
  int i;
  for( i=0; i<MAX_DRIVES; i++ )
    disk_eject( bd->oric, i );
}

unsigned char bd500_read( struct bd500 *bd, unsigned short addr )
{
//  dbg_printf( "DISK: (%04X) Read from %04X", bd->oric->cpu.pc-1, addr );

  if( ( addr >= 0x320 ) && ( addr < 0x324 ) )
    return wd17xx_read( bd->wd, addr&3 );

  switch( addr )
  {
    case 0x312:
      return (bd->drq | bd->intrq);
      break;

    case 0x313:
      bd->oric->romdis = SDL_FALSE;
      break;

    case 0x314:
      bd->oric->romdis = SDL_TRUE;
      break;

    case 0x310:
    case 0x311:
    case 0x315:
    case 0x316:
      bd->diskrom = SDL_FALSE;
      break;

    case 0x317:
      // 64k/56k mode depend on ROM chips
      bd->diskrom = bd->oric->rom16? SDL_FALSE : SDL_TRUE;
      break;

    default:
      break;
  }

  return 0xff;
}

void bd500_write( struct bd500 *bd, unsigned short addr, unsigned char data )
{
//  dbg_printf( "DISK: (%04X) Write %02X to %04X", bd->oric->cpu.pc-1, data, addr );

  if( ( addr >= 0x320 ) && ( addr < 0x324 ) )
  {
    wd17xx_write( bd->oric, bd->wd, addr&3, data );
    return;
  }

  switch( addr )
  {
    case 0x31a:
      switch( data & 0xe0 )
      {
        default:
        case 0x20:
          bd->wd->c_drive = 0;
          break;
        case 0x40:
          bd->wd->c_drive = 1;
          break;
        case 0x80:
          bd->wd->c_drive = 2;
          break;
        case 0xc0:
          bd->wd->c_drive = 3;
          break;
      }
      break;
  }
}

// Jasmin interface handlers
void jasmin_setdrq( void *j )
{
  struct jasmin *jp = (struct jasmin *)j;
  jp->oric->cpu.irq |= IRQF_DISK;
}

void jasmin_clrdrq( void *j )
{
  struct jasmin *jp = (struct jasmin *)j;
  jp->oric->cpu.irq &= ~IRQF_DISK;
}

void jasmin_setintrq( void *j )
{
}

void jasmin_clrintrq( void *j )
{
}

void jasmin_init( struct jasmin *j, struct wd17xx *wd, struct machine *oric )
{
  wd17xx_init( wd );
  wd->setintrq = jasmin_setintrq;
  wd->clrintrq = jasmin_clrintrq;
  wd->intrqarg = (void*)j;
  wd->setdrq   = jasmin_setdrq;
  wd->clrdrq   = jasmin_clrdrq;
  wd->drqarg   = (void*)j;

  j->olay     = 0;
  j->romdis   = 0;
  j->wd       = wd;
  j->oric     = oric;
}

void jasmin_free( struct jasmin *j )
{
  int i;
  for( i=0; i<MAX_DRIVES; i++ )
    disk_eject( j->oric, i );
}

unsigned char jasmin_read( struct jasmin *j, unsigned short addr )
{
//  dbg_printf( "DISK: (%04X) Read from %04X", md->oric->cpu.pc-1, addr );
  if( ( addr >= 0x3f4 ) && ( addr < 0x3f8 ) )
    return wd17xx_read( j->wd, addr&3 );

  switch( addr )
  {
    case 0x3f8:  // Side select
      return j->wd->c_side ? 1 : 0;

    case 0x3f9:  // Disk controller reset
    case 0x3fc:
    case 0x3fd:
    case 0x3fe:
    case 0x3ff:
      return 0;

    case 0x3fa:  // Overlay RAM
      return j->olay;

    case 0x3fb:
      return j->romdis;

    default:
      break;
  }

  return via_read( &j->oric->via, addr );
}

void jasmin_write( struct jasmin *j, unsigned short addr, unsigned char data )
{
  if( ( addr >= 0x3f4 ) && ( addr < 0x3f8 ) )
  {
    wd17xx_write( j->oric, j->wd, addr&3, data );
    return;
  }

  switch( addr )
  {
    case 0x3f8: // side select
      j->wd->c_side = data&1;
      break;

    case 0x3f9: // reset
      // ...
      break;

    case 0x3fa: // overlay RAM
      j->olay = data&1;
      break;

    case 0x3fb: // romdis
      j->romdis = data&1;
      j->oric->romdis = (data!=0) ? SDL_TRUE : SDL_FALSE;
      break;

    case 0x3fc: // Drive 0
    case 0x3fd: // Drive 1
    case 0x3fe: // Drive 2
    case 0x3ff: // Drive 3
      j->wd->c_drive = addr&3;
      break;

    default:
      via_write( &j->oric->via, addr, data );
      break;
  }
}

// Pravetz
void pravetz_init( struct pravetz *p, struct machine *oric )
{
  int i;

  p->olay = 0;
  p->romdis = 1;
  p->extension = 0;
  p->oric = oric;

  for( i=0; i<MAX_DRIVES; i++)
  {
      p->drv[i].write_ready = 0x80;
      p->drv[i].volume      = 254;
      p->drv[i].select      = 0;
      p->drv[i].motor_on    = 0;
      p->drv[i].pimg        = NULL;
  }
}

void pravetz_free( struct pravetz *p )
{
  int i;
  for( i=0; i<MAX_DRIVES; i++ )
    disk_eject( p->oric, i );
}

unsigned char pravetz_read( struct pravetz *p, unsigned short addr )
{
  unsigned char data = 0;
  data = disk_pravetz_read( p->oric, addr );
  p->oric->wddisk.currentop = p->drv[p->oric->wddisk.c_drive].motor_on ? COP_READ_SECTOR : COP_NUFFINK;
  return data;
}

void pravetz_write( struct pravetz *p, unsigned short addr, unsigned char data )
{
  disk_pravetz_write( p->oric, addr, data );
  p->oric->wddisk.currentop = p->drv[p->oric->wddisk.c_drive].motor_on ? COP_READ_SECTOR : COP_NUFFINK;
}

