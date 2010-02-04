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
**  WD17xx, Microdisc and Jasmin emulation
*/

/******************** MICRODISC *********************/
#define MAX_DRIVES 4

// Status bits (0x314)
#define MDSB_INTENA  0
#define MDSF_INTENA  (1<<MDSB_INTENA)
#define MDSB_ROMDIS  1
#define MDSF_ROMDIS  (1<<MDSB_ROMDIS)
#define MDSB_DENSITY 3
#define MDSF_DENSITY (1<<MDSB_DENSITY)
#define MDSB_SIDE    4
#define MDSF_SIDE    (1<<MDSB_SIDE)
#define MDSF_DRIVE   0x60
#define MDSB_EPROM   7                  // Write
#define MDSF_EPROM   (1<<MDSB_EPROM)
#define MDSB_INTRQ   7                  // Read
#define MDSF_INTRQ   (1<<MDSB_INTRQ)

// 0x318
#define MB_DRQ 7
#define MF_DRQ (1<<MB_DRQ)

/******************** WD17xx *********************/

// WD17xx status
#define WSB_BUSY     0
#define WSF_BUSY     (1<<WSB_BUSY)
#define WSBI_PULSE   1
#define WSFI_PULSE   (1<<WSBI_PULSE)     // Type I only
#define WSB_DRQ      1
#define WSF_DRQ      (1<<WSB_DRQ)
#define WSBI_TRK0    2
#define WSFI_TRK0    (1<<WSBI_TRK0)      // Type I only
#define WSB_LOSTDAT  2
#define WSF_LOSTDAT  (1<<WSB_LOSTDAT)
#define WSB_CRCERR   3
#define WSF_CRCERR   (1<<WSB_CRCERR)
#define WSBI_SEEKERR 4
#define WSFI_SEEKERR (1<<WSBI_SEEKERR)   // Type I only
#define WSB_RNF      4
#define WSF_RNF      (1<<WSB_RNF)
#define WSBI_HEADL   5
#define WSFI_HEADL   (1<<WSBI_HEADL)     // Type I only
#define WSBR_RECTYP  5
#define WSFR_RECTYP  (1<<WSBR_RECTYP)    // Read sector only
#define WSB_WRITEERR 5
#define WSF_WRITEERR (1<<WSB_WRITEERR)
#define WSB_WRPROT   6
#define WSF_WRPROT   (1<<WSB_WRPROT)
#define WSB_NOTREADY 7
#define WSF_NOTREADY (1<<WSB_NOTREADY)

// Current operation
enum
{
  COP_NUFFINK=0,      // Not doing anything, guv
  COP_READ_TRACK,     // Reading a track
  COP_READ_SECTOR,    // Reading a sector
  COP_WRITE_TRACK,    // Writing a track
  COP_WRITE_SECTOR,   // Writing a sector
  COP_READ_ADDRESS    // Reading a sector header
};

struct densityinfo
{
  int tracksize;
  Uint8 idmark;
  Uint8 datamark;
  Uint8 deldatamark;
};

struct mfmsector
{
  Uint8 *id_ptr;
  Uint8 *data_ptr;
};

struct diskimage
{
  Uint32   numtracks;
  Uint32   numsides;
  Uint32   geometry;
  int      cachedtrack, cachedside;
  Uint32   numsectors;
  struct   mfmsector sector[32];
  Uint8   *rawimage;
  Uint32   rawimagelen;
  struct   densityinfo *dinf;
};

struct wd17xx
{
  Uint8             r_status;
  Uint8             cmd;
  Uint8             r_track;
  Uint8             r_sector;
  Uint8             r_data;
  Uint8             c_drive;
  Uint8             c_side;
  Uint8             c_track;
  Uint8             c_sector;
  Uint8             sectype;
  SDL_bool          last_step_in;
  void            (*setintrq)(void *);
  void            (*clrintrq)(void *);
  void             *intrqarg;
  void            (*setdrq)(void *);
  void            (*clrdrq)(void *);
  void             *drqarg;
  struct diskimage *disk[MAX_DRIVES];
  int               currentop;
  struct mfmsector *currsector;
  int               currseclen;
  int               curroffs;
  int               delayedint;
  int               delayeddrq;
  int               distatus;
};

struct microdisc
{
  Uint8 status;
  Uint8 intrq, drq;
  struct wd17xx *wd;
  struct machine *oric;
  SDL_bool diskrom;
};

struct jasmin
{
  Uint8 olay;     // 0x3fa
  Uint8 romdis;   // 0x3fb
  struct wd17xx *wd;
  struct machine *oric;
};

SDL_bool diskimage_load( struct machine *oric, char *fname, int drive );

void wd17xx_ticktock( struct wd17xx *wd, int cycles );

void microdisc_init( struct microdisc *md, struct wd17xx *wd, struct machine *oric );
void microdisc_free( struct microdisc *md );
unsigned char microdisc_read( struct microdisc *md, unsigned short addr );
void microdisc_write( struct microdisc *md, unsigned short addr, unsigned char data );

void jasmin_init( struct jasmin *j, struct wd17xx *wd, struct machine *oric );
void jasmin_free( struct jasmin *j );
unsigned char jasmin_read( struct jasmin *j, unsigned short addr );
void jasmin_write( struct jasmin *j, unsigned short addr, unsigned char data );
