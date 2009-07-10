
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL/SDL.h>

#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "machine.h"

extern char diskfile[], diskpath[];

void disk_popup( struct machine *oric )
{
  char tmp[40];
  if( !oric->diskname[0] )
  {
    do_popup( "\x14\x15\x13" );
    return;
  }

  sprintf( tmp, "\x14\x15"" %-16s", oric->diskname );
  do_popup( tmp );
}

void disk_eject( struct machine *oric )
{
  if( oric->diskimg ) free( oric->diskimg );
  oric->diskimglen = 0;
  oric->diskname[0] = 0;
  disk_popup( oric );
}

SDL_bool disk_load_dsk( struct machine *oric, char *fname )
{
  FILE *f;

  f = fopen( fname, "rb" );
  if( !f ) return SDL_FALSE;

  disk_eject( oric );

  fseek( f, 0, SEEK_END );
  oric->diskimglen = ftell( f );
  fseek( f, 0, SEEK_SET );

  if( oric->diskimglen <= 0 ) return SDL_FALSE;

  oric->diskimg = malloc( oric->diskimglen );
  if( !oric->diskimg )
  {
    fclose( f );
    oric->diskimglen = 0;
    return SDL_FALSE;
  }

  if( fread( &oric->diskimg[0], oric->diskimglen, 1, f ) != 1 )
  {
    fclose( f );
    disk_eject( oric );
    return SDL_FALSE;
  }

  fclose( f );
  strncpy( oric->diskname, fname, 16 );
  oric->diskname[15] = 0;
  disk_popup( oric );
  return SDL_TRUE;
};

void wd17xx_dummy( void *nothing )
{
}

void wd17xx_init( struct wd17xx *wd )
{
  wd->cmd      = 0;
  wd->r_status = 0;
  wd->r_track  = 0;
  wd->r_sector = 0;
  wd->r_data   = 0;
  wd->setintrq = wd17xx_dummy;
  wd->clrintrq = wd17xx_dummy;
  wd->intrqarg = NULL;
}

unsigned char wd17xx_read( struct wd17xx *wd, unsigned short addr )
{
  switch( addr )
  {
    case 0: // Status register
      wd->clrintrq( wd->intrqarg );
      return wd->r_status;
    
    case 1: // Track register
      return wd->r_track;
    
    case 2: // Sector register
      return wd->r_sector;
    
    case 3: // Data register
      return wd->r_data;
  }

  return 0; // ??
}

void wd17xx_write( struct wd17xx *wd, unsigned short addr, unsigned char data )
{
  switch( addr )
  {
    case 0: // Command register
      wd->cmd = data;
      switch( data & 0xe0 )
      {
        case 0x00:  // Restore or seek
          switch( data & 0x10 )
          {
            case 0x00:  // Restore
              break;
            
            case 0x10:  // Seek
              break;
          }
          break;
        
        case 0x20:  // Step
          break;
        
        case 0x40:  // Step-in
          break;
        
        case 0x60:  // Step-out
          break;
        
        case 0x80:  // Read sector
          break;
        
        case 0xa0:  // Write sector
          break;
        
        case 0xc0:  // Read address / Force IRQ
          switch( data & 0x10 )
          {
            case 0x00: // Read address
              break;
            
            case 0x10: // Force IRQ
              break;
          }
          break;
        
        case 0xe0:  // Read track / Write track
          switch( data & 0x10 )
          {
            case 0x00: // Read track
              break;
            
            case 0x10: // Write track
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
      break;
  }
}

void microdisc_setintrq( void *md )
{
  ((struct microdisc *)md)->status |= MDSF_INTRQ;
}

void microdisc_clrintrq( void *md )
{
  ((struct microdisc *)md)->status &= ~MDSF_INTRQ;
}

void microdisc_init( struct microdisc *md, struct wd17xx *wd, struct machine *oric )
{
  wd17xx_init( wd );
  wd->setintrq = microdisc_setintrq;
  wd->clrintrq = microdisc_clrintrq;
  wd->intrqarg = (void*)md;

  md->status = 0;
  md->eprom  = 0;
  md->drq    = 0;
  md->wd     = wd;
  md->oric   = oric;
}

unsigned char microdisc_read( struct microdisc *md, unsigned short addr )
{
  if( ( addr >= 0x310 ) && ( addr < 0x314 ) )
    return wd17xx_read( md->wd, addr&3 );

  switch( addr )
  {
    case 0x314:
      return md->status;

    case 0x318:
      return md->drq;
  }

  return 0;
}

void microdisc_write( struct microdisc *md, unsigned short addr, unsigned char data )
{
  if( ( addr >= 0x310 ) && ( addr < 0x314 ) )
  {
    wd17xx_write( md->wd, addr&3, data );
    return;
  }

  switch( addr )
  {
    case 0x314:
      md->status = (md->status&MDSF_INTRQ)|(data&~MDSF_INTRQ);
      md->eprom  = data&MDSF_EPROM;

      md->oric->romdis = (md->status&MDSF_ROMDIS) ? SDL_FALSE : SDL_TRUE;
      break;

    case 0x318:
      md->drq = (data&MF_DRQ);
      break;
  }
}
