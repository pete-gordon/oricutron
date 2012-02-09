/*
**  Oricutron
**  Copyright (C) 2009-2012 Peter Gordon
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

// WD17xx status bits
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
  COP_READ_SECTORS,   // Reading multiple sectors
  COP_WRITE_TRACK,    // Writing a track
  COP_WRITE_SECTOR,   // Writing a sector
  COP_WRITE_SECTORS,  // Writing multiple sectors
  COP_READ_ADDRESS    // Reading a sector header
};

// This is just a convenient structure in which to
// store pointers to the actual data for a sector
// in a disk image.
struct mfmsector
{
  Uint8 *id_ptr;
  Uint8 *data_ptr;
};

// A disk image in memory
// When the disk controller seeks to a track, we "cache" the entire
// track in the sector array. This is just an array of pointers to
// the actual sectors within the track.
struct diskimage
{
  Sint16   drivenum;              // The drive this disk is inserted into, or -1
  Uint32   numtracks;             // Number of tracks per side
  Uint32   numsides;              // Number of sides in the image
  Uint32   geometry;              // Geometry. I don't know what this is.
  Sint16   cachedtrack;           // Currently cached track (or -1 for none)
  Sint16   cachedside;            // Currently cached side (or -1 for none)
  Uint32   numsectors;            // Number of sectors cached (= number of valid sectors in the current track)
  struct   mfmsector sector[32];  // Cache of pointers to sectors
  Uint8   *rawimage;              // The raw disk image file loaded into memory
  Uint32   rawimagelen;           // Size of the raw image file
  SDL_bool modified;              // Set to TRUE if the image in memory has been modified
  Sint32   modified_time;         // Cycles since it was last modified
  char     filename[4096+512];    // Full path and filename of the current image file
};

// Current state of the WD17xx controller
struct wd17xx
{
  Uint8             r_status;          // Status register
  Uint8             r_track;           // Track register
  Uint8             r_sector;          // Sector register
  Uint8             r_data;            // Data register
  Uint8             c_drive;           // Currently active drive
  Uint8             c_side;            // Currently active side
  Uint8             c_track;           // Currently selected track
  Uint8             c_sector;          // Currently selected sector ID
  Uint8             sectype;           // When reading a sector, this is used to remember if it was marked as deleted
  SDL_bool          last_step_in;      // Set to TRUE if the last seek operation stepped the head inwards
  void            (*setintrq)(void *); // Function pointer called by the WD17xx core when INTRQ is set (used for microdisc/jasmin integration)
  void            (*clrintrq)(void *); // Called when INTRQ is cleared
  void             *intrqarg;          // Userdata passed to setintrq/clrintrq
  void            (*setdrq)(void *);   // Called when DRQ is set
  void            (*clrdrq)(void *);   // Called when DRQ is cleared
  void             *drqarg;            // Userdata passed to setdrq/clrdrq
  struct diskimage *disk[MAX_DRIVES];  // Loaded disk images
  int               currentop;         // Current operation in progress
  struct mfmsector *currsector;        // Pointers to the current sector in the disk image being used by an active read or write operation
  int               currseclen;        // The length of the current sector
  int               curroffs;          // Current offset into the above sector
  int               delayedint;        // A cycle counter for simulating a delay before INTRQ is asserted
  int               delayeddrq;        // A cycle counter for simulating a delay before DRQ is asserted
  int               distatus;          // The new contents for r_status when delayedint expires (or -1 to leave it untouched)
  int               ddstatus;          // The new contents for r_status when delayeddrq expires (or -1 to leave it untouched)
  Uint16            crc;
};

// Current state of the Microdisc hardware
struct microdisc
{
  Uint8 status;           // Status register (write to 0x314)
  Uint8 intrq;            // intrq register (read from 0x314)
  Uint8 drq;              // drq register (read/write 0x318)
  struct wd17xx *wd;      // Pointer to the WD17xx structure
  struct machine *oric;   // Pointer to the Oric structure
  SDL_bool diskrom;       // TRUE if the diskrom is enabled
};


// Current state of the Jasmin hardware
struct jasmin
{
  Uint8 olay;              // Overlay RAM enable (0x3fa)
  Uint8 romdis;            // ROMDIS (0x3fb)
  struct wd17xx *wd;       // Pointer to the WD17xx structure
  struct machine *oric;    // Pointer to the Oric structure
};

// Functions to read/write diskimages
SDL_bool diskimage_load( struct machine *oric, char *fname, int drive ); 
SDL_bool diskimage_save( struct machine *oric, char *fname, int drive );
void diskimage_cachetrack( struct diskimage *dimg, int track, int side );
struct mfmsector *wd17xx_find_sector( struct wd17xx *wd, Uint8 secid );

// Call this to emulate some cycles of disk activity
void wd17xx_ticktock( struct wd17xx *wd, int cycles );

// Microdisc interface
void microdisc_init( struct microdisc *md, struct wd17xx *wd, struct machine *oric );
void microdisc_free( struct microdisc *md );
unsigned char microdisc_read( struct microdisc *md, unsigned short addr );
void microdisc_write( struct microdisc *md, unsigned short addr, unsigned char data );

// Jasmin interface
void jasmin_init( struct jasmin *j, struct wd17xx *wd, struct machine *oric );
void jasmin_free( struct jasmin *j );
unsigned char jasmin_read( struct jasmin *j, unsigned short addr );
void jasmin_write( struct jasmin *j, unsigned short addr, unsigned char data );

