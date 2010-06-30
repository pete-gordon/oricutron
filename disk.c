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
**  WD17xx, Microdisc and Jasmin emulation
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "machine.h"
#include "msgbox.h"
#include "filereq.h"

extern char diskfile[], diskpath[], filetmp[];
extern char telediskfile[], telediskpath[];
extern SDL_bool refreshdisks;

#define GENERAL_DISK_DEBUG 1
#define DEBUG_SECTOR_DUMP  0

#if DEBUG_SECTOR_DUMP
static char sectordumpstr[64];
static int sectordumpcount;
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
    do_popup( "\x14\x15\x13" );
    return;
  }

  sprintf( tmp, "\x14\x15""%d %-16s", drive, oric->diskname[drive] );
  do_popup( tmp );
}

// Free a disk image and clear the pointer to it
void diskimage_free( struct machine *oric, struct diskimage **dimg )
{
  char *dpath, *dfile;

  if( !(*dimg) ) return;

  if( oric->type != MACH_TELESTRAT )
  {
    dpath = diskpath;
    dfile = diskfile;
  } else {
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

  if( (*dimg)->rawimage ) free( (*dimg)->rawimage );
  free( *dimg );
  (*dimg) = NULL;
}

// Read a raw integer from a disk image file
Uint32 diskimage_rawint( struct diskimage *dimg, Uint32 offset )
{
  if( !dimg ) return 0;
  if( ( !dimg->rawimage ) || ( dimg->rawimagelen < 4 ) ) return 0;
  if( offset > (dimg->rawimagelen-4) ) return 0;

  return (dimg->rawimage[offset+3]<<24)|(dimg->rawimage[offset+2]<<16)|(dimg->rawimage[offset+1]<<8)|dimg->rawimage[offset];
}

// Allocate and initialise a disk image structure
// If "rawimglen" isn't zero, it will also allocate
// ram for a raw disk image
struct diskimage *diskimage_alloc( Uint32 rawimglen )
{
  struct diskimage *dimg;
  Uint8 *buf=NULL;

  if( rawimglen )
  {
    buf = malloc( rawimglen );
    if( !buf ) return NULL;
  }

  dimg = malloc( sizeof( struct diskimage ) );
  if( !dimg ) return NULL;

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
  return dimg;
}

// Eject a disk from a drive
void disk_eject( struct machine *oric, int drive )
{
  diskimage_free( oric, &oric->wddisk.disk[drive] );
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

  // Remember how many sectors we have successfully cached
  dimg->numsectors = sectorcount;
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

  // Open the file for writing
  f = fopen( fname, "wb" );
  if( !f )
  {
    do_popup( "\x14\x15Save failed" );
    return SDL_FALSE;
  }

  // Dump it to disk
  if( fwrite( oric->wddisk.disk[drive]->rawimage, oric->wddisk.disk[drive]->rawimagelen, 1, f ) != 1 )
  {
    fclose( f );
    do_popup( "\x14\x15Save failed" );
    return SDL_FALSE;
  }

  // All done!
  fclose( f );

  // If we are not just overwriting the original file, remember the new filename
  if( fname != oric->wddisk.disk[drive]->filename )
  {
    strncpy( oric->wddisk.disk[drive]->filename, fname, 4096+512 );
    oric->wddisk.disk[drive]->filename[4096+511] = 0;
  }

  // The image in memory is no longer different to the last saved version
  oric->wddisk.disk[drive]->modified = SDL_FALSE;

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
  len = ftell( f );
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
    do_popup( "\x14\x15""Out of memory" );
    fclose( f );
    return SDL_FALSE;
  }

  // Read the image file into memory
  if( fread( oric->wddisk.disk[drive]->rawimage, len, 1, f ) != 1 )
  {
    fclose( f );
    disk_eject( oric, drive );
    do_popup( "\x14\x15""Read error" );
    return SDL_FALSE;
  }

  fclose( f );

  // Check for the header ID
  if( strncmp( (char *)oric->wddisk.disk[drive]->rawimage, "MFM_DISK", 8 ) != 0 )
  {
    disk_eject( oric, drive );
    do_popup( "\x14\x15""Invalid disk image" );
    return SDL_FALSE;
  }

  // Get some basic image info
  oric->wddisk.disk[drive]->drivenum  = drive;
  oric->wddisk.disk[drive]->numsides  = diskimage_rawint( oric->wddisk.disk[drive], 8 );
  oric->wddisk.disk[drive]->numtracks = diskimage_rawint( oric->wddisk.disk[drive], 12 );
  oric->wddisk.disk[drive]->geometry  = diskimage_rawint( oric->wddisk.disk[drive], 16 );

  // Is the disk sane!?
  if( ( oric->wddisk.disk[drive]->numsides < 1 ) &&
      ( oric->wddisk.disk[drive]->numsides > 2 ) )
  {
    disk_eject( oric, drive );
    do_popup( "\x14\x15""Invalid disk image" );
    return SDL_FALSE;
  }

  // Nobody has written to this disk yet
  oric->wddisk.disk[drive]->modified = SDL_FALSE;

  // Remember the filename of the image for this drive
  strncpy( oric->wddisk.disk[drive]->filename, fname, 4096+512 );
  oric->wddisk.disk[drive]->filename[4096+511] = 0;

  // Come up with a suitable short name for popups etc.
  if( strlen( fname ) > 15 )
  {
    strncpy( oric->diskname[drive], &fname[strlen(fname)-15], 16 );
    oric->diskname[drive][0] = 22;
  } else {
    strncpy( oric->diskname[drive], fname, 16 );
  }
  oric->diskname[drive][15] = 0;

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
void wd17xx_seek_track( struct wd17xx *wd, Uint8 track )
{
  // Is there a disk in the drive?
  if( wd->disk[wd->c_drive] )
  {
    // Yes. If we are trying to seek to a non-existant track, just seek as far as we can
    if( track >= wd->disk[wd->c_drive]->numtracks )
      track = (wd->disk[wd->c_drive]->numtracks>0)?wd->disk[wd->c_drive]->numtracks-1:0;

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
    wd->distatus = WSFI_HEADL|WSFI_PULSE;
    if( wd->c_track == 0 ) wd->distatus |= WSFI_TRK0;
#if GENERAL_DISK_DEBUG
    dbg_printf( "DISK: At track %u (%u sectors)", track, wd->disk[wd->c_drive]->numsectors );
#endif
    return;
  }

  // No disk in drive
  // Set INTRQ because the operation has finished.
  wd->setintrq( wd->intrqarg );

  // Set error state
  wd->r_status = WSF_NOTREADY|WSFI_SEEKERR;
  wd->r_track = 0;
#if GENERAL_DISK_DEBUG
  dbg_printf( "DISK: Seek fail" );
#endif
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
              wd17xx_seek_track( wd, 0 );
              wd->currentop = COP_NUFFINK;
              refreshdisks = SDL_TRUE;
              break;
            
            case 0x10:  // Seek (Type I)
#if GENERAL_DISK_DEBUG
              dbg_printf( "DISK: (%04X) Seek", oric->cpu.pc-1 );
#endif
              wd->r_status = WSF_BUSY;
              if( data & 8 ) wd->r_status |= WSFI_HEADL;
              wd17xx_seek_track( wd, wd->r_data );
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
            wd17xx_seek_track( wd, wd->c_track+1 );
          else
            wd17xx_seek_track( wd, wd->c_track > 0 ? wd->c_track-1 : 0 );
          wd->currentop = COP_NUFFINK;
          refreshdisks = SDL_TRUE;
          break;
        
        case 0x40:  // Step-in (Type I)
#if GENERAL_DISK_DEBUG
          dbg_printf( "DISK: (%04X) Step-In (%d,%d)", oric->cpu.pc-1, wd->c_track, wd->c_track+1 );
#endif
          wd->r_status = WSF_BUSY;
          if( data & 8 ) wd->r_status |= WSFI_HEADL;
          wd17xx_seek_track( wd, wd->c_track+1 );
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
            wd17xx_seek_track( wd, wd->c_track-1 );
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
              wd->currentop = COP_READ_TRACK;
              refreshdisks = SDL_TRUE;
              break;
            
            case 0x10: // Write track (Type III)
#if GENERAL_DISK_DEBUG
              dbg_printf( "DISK: (%04X) Write track", oric->cpu.pc-1 );
#endif
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
          if( wd->curroffs == 0 ) wd->currsector->data_ptr[wd->curroffs++]=0xf8;
          wd->currsector->data_ptr[wd->curroffs++] = wd->r_data;
          wd->crc = calc_crc( wd->crc, wd->r_data );
          if( !wd->disk[wd->c_drive]->modified ) refreshdisks = SDL_TRUE;
          wd->disk[wd->c_drive]->modified = SDL_TRUE;
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
      }
      break;
  }
}

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
      return via_read( &md->oric->via, addr );
  }

  return 0;
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
      return via_read( &j->oric->via, addr );
  }

  return 0;
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
