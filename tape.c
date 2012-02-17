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
#include "6551.h"
#include "machine.h"
#include "joystick.h"
#include "filereq.h"
#include "tape.h"
#include "msgbox.h"

extern char tapefile[], tapepath[];
extern SDL_bool refreshtape;
char tmptapename[4096];
extern char filetmp[];

// Pop-up the name of the currently inserted tape
// image file (or an eject symbol if no tape image).
void tape_popup( struct machine *oric )
{
  char tmp[40];

  if( !oric->tapename[0] )
  {
    // Eject symbol
    do_popup( oric, "\x0f\x10\x13" );
    return;
  }

  // Tape image name
  sprintf( tmp, "\x0f\x10""%c %-16s", oric->tapemotor ? 18 : 17, oric->tapename );
  do_popup( oric, tmp );
}

void toggletapecap( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  char ortheader[] = { 'O', 'R', 'T', 0 };  // Oric Raw Tape, Version 0

  /* If capturing, stop */
  if( oric->tapecap )
  {
    fclose( oric->tapecap );
    oric->tapecap = NULL;
    mitem->name = "Save tape output...";
    refreshtape = SDL_TRUE;
    return;
  }

  /* Otherwise, prompt for the file to capture */
  if( !filerequester( oric, "Capture tape output", tapepath, tmptapename, FR_TAPESAVEORT ) )
  {
    // Never mind
    return;
  }
  if( tmptapename[0] == 0 ) return;

  /* If it ends in ".tap", we need to change it to ".ort", because
     we're capturing real signals here folks! */
  if( (strlen(tmptapename)>3) && (strcasecmp(&tmptapename[strlen(tmptapename)-4], ".tap")==0) )
    tmptapename[strlen(tmptapename)-4] = 0;

  /* Add .ort extension, if necessary */
  if( (strlen(tmptapename)<4) || (strcasecmp(&tmptapename[strlen(tmptapename)-4], ".ort")!=0) )
    strncat(tmptapename, ".ort", 4096);
  tmptapename[4095] = 0;

  joinpath( tapepath, tmptapename );

  /* Open the file */
  oric->tapecap = fopen( filetmp, "wb" );
  if( !oric->tapecap )
  {
    /* Oh well */
    msgbox( oric, MSGBOX_OK, "Unable to create file" );
    return;
  }

  /* Write header */
  fwrite( ortheader, 4, 1, oric->tapecap);
  
  /* Counter reset */
  oric->tapecapcount = -1;
  oric->tapecaplastbit = (oric->via.orb&oric->via.ddrb)>>7;

  /* Update menu */
  mitem->name = "Stop tape recording";
  refreshtape = SDL_TRUE;
}

/* When we're loading a .tap file (or a non-raw section of a .ort file),
** we have to do 2 things... First, we have to elongate the leader so the ROM
** can see it, and secondly we have to add a delay after the header so that the
** tape isn't several bytes into the program when the rom is ready to start
** reading it.
**
** Only call it when you think you're at a tape leader since it doesn't do many
** sanity checks.
*/
void tape_setup_header( struct machine *oric )
{
  int i;

  oric->tapedupbytes = 0;
  oric->tapehdrend   = 0;

  if( oric->tapebuf[oric->tapeoffs] != 0x16 )
    return;

  /* Does it look like a header? */
  for( i=0; oric->tapebuf[oric->tapeoffs+i]==0x16; i++ )
  {
    if( (oric->tapeoffs+i) >= oric->tapelen )
      return;
  }
  if( ( i < 3 ) || ( oric->tapebuf[oric->tapeoffs+i] != 0x24 ) )
    return;
  i++;
  if( (oric->tapeoffs+i+9) >= oric->tapelen )
    return;
  i+=9;
  while( oric->tapebuf[oric->tapeoffs+i] != 0 )
  {
    if( (oric->tapeoffs+i) >= oric->tapelen )
      return;
    i++;
  }
  i++;

  oric->tapedupbytes = 80;
  oric->tapehdrend = oric->tapeoffs+i;
}


void tape_orbchange( struct via *via )
{
  struct machine *oric = via->oric;
  unsigned char buffer[4], tapebit, bufwrite;
  unsigned int count;

  /* Capturing tape output? */
  if( !oric->tapecap ) return;
  
  /* Saving a tap section? */
  if( oric->tapecap == oric->tsavf ) return;
  
  tapebit = (via->orb & via->ddrb) >> 7;
  if( tapebit == oric->tapecaplastbit ) return;

  oric->tapecaplastbit = tapebit;

  /* Were we waiting for the first toggle? */
  if( oric->tapecapcount < 0 )
  {
    /* Well, we found it */
    oric->tapecapcount = 0;
    fputc( tapebit, oric->tapecap );
    return;
  }

  count = (oric->tapecapcount>>1);

  if( count < 0xfc )
  {
    buffer[0] = count;
    bufwrite = 1;
  }
  else if( count < 0x100 )
  {
    buffer[0] = 0xfc;
    buffer[1] = count;
    bufwrite = 2;
  }
  else if( count < 0x10000 )
  {
    buffer[0] = 0xfd;
    buffer[1] = (count>>8)&0xff;
    buffer[2] = count&0xff;
    bufwrite = 3;
  }
  else
  {
    buffer[0] = 0xfd;
    buffer[1] = 0xff;
    buffer[2] = 0xff;
    bufwrite = 3;
  }

  fwrite( buffer, bufwrite, 1, oric->tapecap );
  oric->tapecapcount = 0;
}


// Set the tape motor on or off
void tape_setmotor( struct machine *oric, SDL_bool motoron )
{
  if( motoron == oric->tapemotor )
    return;

  // Refresh the tape status icon in the status bar
  refreshtape = SDL_TRUE;

  // "Real" tape emulation?
  if( ( !oric->tapeturbo ) || ( !oric->pch_tt_available ) )
  {
    if( ( !oric->rawtape ) || ( oric->tapeoffs < oric->nonrawend ) )
    {
      // If we're stopping part way through a byte, just move
      // the current position on to the start of the next byte
      if( !motoron )
      {
        if( oric->tapebit > 0 )
          oric->tapeoffs++;
        oric->tapebit = 0;
      }

      // If we're starting the tape motor and the tape
      // is at a sync header, generate a bunch of dummy
      // sync bytes. This makes the Oric Atmos welcome tape
      // work without turbo tape enabled, for example.
      if( ( motoron ) &&
          ( oric->tapebuf ) &&
          ( oric->tapeoffs < oric->tapelen ) &&
          ( oric->tapebuf[oric->tapeoffs] == 0x16 ) )
      {
        tape_setup_header(oric);
      }
    }
  }

  // Set the new status and do a popup
  oric->tapemotor = motoron;
  if( oric->tapename[0] ) tape_popup( oric );
}

// Free up the current tape image
void tape_eject( struct machine *oric )
{
  if( oric->tapebuf ) free( oric->tapebuf );
  oric->tapebuf = NULL;
  oric->tapelen = 0;
  oric->tapename[0] = 0;
  tape_popup( oric );
  refreshtape = SDL_TRUE;
}

void tape_next_raw_count( struct machine *oric )
{
  unsigned int n;
  int nonrawend;

  if( oric->tapeoffs >= oric->tapelen )
  {
    if( oric->nonrawend != oric->tapelen )
      oric->tapehitend = 3;
    return;
  }

  n = oric->tapebuf[oric->tapeoffs++];
  if (n < 0xfc)
  {
    oric->tapecount = n<<1;
    return;
  }

  switch (n)
  {
    case 0xff: // non-raw section
      if( oric->tapeoffs >= (oric->tapelen-1) )
      {
        oric->tapeoffs = oric->tapelen;
        oric->tapehitend = 3;
        return;
      }

      nonrawend = ((oric->tapebuf[oric->tapeoffs]<<8)|oric->tapebuf[oric->tapeoffs+1]) + oric->tapeoffs+2;
      if( nonrawend > oric->tapelen )
      {
        oric->tapeoffs = oric->tapelen;
        oric->tapehitend = 3;
        return;
      }

      oric->nonrawend    = nonrawend;
      oric->tapeoffs    += 2;
      oric->tapebit      = 0;
      oric->tapecount    = 2;
      oric->tapeout      = 0;
      tape_setup_header( oric );
      return;

    case 0xfc:
      if( oric->tapeoffs >= oric->tapelen )
      {
        oric->tapehitend = 3;
        return;
      }

      oric->tapecount = oric->tapebuf[oric->tapeoffs++] << 1;
      return;
    
    case 0xfd:
      if( oric->tapeoffs >= (oric->tapelen-1) )
      {
        oric->tapeoffs = oric->tapelen;
        oric->tapehitend = 3;
        return;
      }
      
      oric->tapecount = (oric->tapebuf[oric->tapeoffs]<<9)|(oric->tapebuf[oric->tapeoffs+1]<<1);
      oric->tapeoffs += 3;
      return;
  }

  // Invalid data
  oric->tapeoffs = oric->tapelen;
  oric->tapehitend = 3;
}

// Rewind to the start of the tape
void tape_rewind( struct machine *oric )
{
  oric->nonrawend = 0;
  if( oric->rawtape )
  {
    oric->tapeout = oric->tapebuf[4];
    via_write_CB1( &oric->via, oric->tapeout );

    oric->tapeoffs = 5;
    tape_next_raw_count( oric );
  }
  else
  {
    oric->tapeoffs   = 0;
    oric->tapebit    = 0;
    oric->tapecount  = 2;
    oric->tapeout    = 0;
    oric->tapedupbytes = 0;

    if( oric->tapebuf )
      tape_setup_header( oric );
  }
  oric->tapehitend = 0;
  oric->tapedelay = 0;
  refreshtape = SDL_TRUE;
}

// This is used by the "tapsections" function. It returns the next time
// value from an ort buffer, or -1 for invalid (or if an existing tapsection
// is found)
static int next_ort_value( unsigned char *ortdata, int *offset, int end )
{
  int val=0;

  if ((*offset) >= end) return 0;

  switch (ortdata[*offset])
  {
    case 0xfc:
      if ((*offset) >= (end-1)) return 0;
      val = ortdata[(*offset)+1];
      (*offset) += 2;
      return val;
    
    case 0xfd:
      if ((*offset) >= (end-2)) return 0;
      val = (ortdata[(*offset)+1]<<8)|ortdata[(*offset)+2];
      (*offset) += 2;
      return val;
    
    case 0xfe:
    case 0xff:
      return 0;
  }

  return ortdata[(*offset)++];
}

enum
{
  TAPSEC_STATE_SEARCH = 0,
  TAPSEC_STATE_LEADER,
  TAPSEC_STATE_FINDHEADER,
  TAPSEC_STATE_HEADER,
  TAPSEC_STATE_FILENAME,
  TAPSEC_STATE_FINDDATA,
  TAPSEC_STATE_DATA
};

// This converts oric standard encoded waveforms to decoded TAP sections
static int tapsections( unsigned char *ortbuf, int ortbuflen, unsigned char *scratch )
{
  int ort_offs, tapebit, ort_val, cyccount;
  int accum, state, thisbit, leaderstart=0, leadercount=0;
  int last_offsets[20], i, bitcount, tapbytes=0;
  int data_bytes=0;

  ort_offs = 4;
  tapebit  = ortbuf[ort_offs++];
  cyccount = 0;
  state    = TAPSEC_STATE_SEARCH;
  accum    = 0;
  bitcount = 0;

  for (i=0; i<20; i++)
    last_offsets[i] = 5;

  // Loop through the ORT data
  while (ort_offs < ortbuflen)
  {
    // Shuffle the offset array
    for (i=0; i<19; i++)
      last_offsets[i] = last_offsets[i+1];
    last_offsets[19] = ort_offs;

    ort_val = next_ort_value(ortbuf, &ort_offs, ortbuflen);
    // Invalid ORT data?
    if (ort_val == -1) return ortbuflen;

    // Count these cycles
    cyccount += ort_val;

    // Invert the tape input
    tapebit ^= 1;

    // Rising edge?
    if (tapebit)
    {
      // Convert to 1/0
      thisbit = TIME_TO_BIT(cyccount);

      // Seen a value that is outside the bounds of Oric standard encoding?
      if (thisbit == -1)
      {
        // This can't be a TAP section
        state    = TAPSEC_STATE_SEARCH;
        cyccount = 0;
        accum    = 0;
      }
      else
      {
        // Accumulate this bit
        accum = (accum>>1) | (thisbit<<10);

        // What are we doing?
        switch (state)
        {
          case TAPSEC_STATE_SEARCH:
            // Look for 0x058 (0x16 encoded with start bit and parity)
            if ((accum&0x7fe) == 0x58)
            {
              leaderstart = last_offsets[8];
              leadercount = 1;
              state = TAPSEC_STATE_LEADER;
              bitcount = 0;
            }
            break;
          
          case TAPSEC_STATE_LEADER:
            bitcount = (bitcount+1)%14;
            if (!bitcount)
            {
              if ((accum&0x7fe) != 0x58)
              {
                state    = TAPSEC_STATE_SEARCH;
                cyccount = 0;
                break;
              }

              leadercount++;
              if (leadercount==4)
              {
                state = TAPSEC_STATE_FINDHEADER;
                break;
              }
            }
            break;
          
          case TAPSEC_STATE_FINDHEADER:
            bitcount = (bitcount+1)%14;
            if (!bitcount)
            {
              if ((accum&0x7fe) == 0x490) // 0x24 fully encoded
              {
                state = TAPSEC_STATE_HEADER;
                scratch[0] = 0x16;
                scratch[1] = 0x16;
                scratch[2] = 0x16;
                scratch[3] = 0x24;
                tapbytes = 4;
                state = TAPSEC_STATE_HEADER;
              }
            }
            break;

          case TAPSEC_STATE_HEADER:
            bitcount = (bitcount+1)%14;
            if (!bitcount)
            {
              // Maybe todo: check the parity bit?
              scratch[tapbytes++] = (accum>>2)&0xff;
              if (tapbytes == 13)
              {
                int load_start, load_end;

                load_start = (scratch[10]<<8)|scratch[11];
                load_end   = (scratch[ 8]<<8)|scratch[ 9];

                if (load_end <= load_start)
                {
                  state = TAPSEC_STATE_SEARCH;
                  cyccount = 0;
                  break;
                }

                data_bytes = (load_end-load_start)+1;
                state = TAPSEC_STATE_FILENAME;
              }
            }
            break;

          case TAPSEC_STATE_FILENAME:
            bitcount = (bitcount+1)%14;
            if (!bitcount)
            {
              // Maybe todo: check the parity bit?
              scratch[tapbytes++] = (accum>>2)&0xff;
              if (scratch[tapbytes-1] == 0)
              {
                state = TAPSEC_STATE_FINDDATA;
              }
            }              
            break;
          
          case TAPSEC_STATE_FINDDATA:
            // Look for a valid byte (start bits, valid parity)
            if ((accum&3)==1)
            {
              int parity = 1;
              for (i=4; i!=0x800; i<<=1)
                if (accum&i) parity ^= 1;
              if (!parity)
              {
                scratch[tapbytes++] = (accum>>2)&0xff;
                data_bytes--;
                bitcount=0;
                state = TAPSEC_STATE_DATA;
              }
            }
            break;

          case TAPSEC_STATE_DATA:
            bitcount = (bitcount+1)%14;
            if (!bitcount)
            {
              scratch[tapbytes++] = (accum>>2)&0xff;
              data_bytes--;

              // ??
              if (data_bytes < 0)
              {
                state = TAPSEC_STATE_SEARCH;
                break;
              }

              if (data_bytes > 0)
                break;

              // Got a whole standard oric tap section!
              ortbuf[leaderstart++] = 0xff;
              ortbuf[leaderstart++] = (tapbytes>>8)&0xff;
              ortbuf[leaderstart++] = (tapbytes&0xff);
              memcpy(&ortbuf[leaderstart], scratch, tapbytes);
              leaderstart += tapbytes;
              memmove(&ortbuf[leaderstart], &ortbuf[ort_offs], ortbuflen-ort_offs);
              ortbuflen -= (ort_offs-leaderstart);
              ort_offs = leaderstart;
              state = TAPSEC_STATE_SEARCH;
              cyccount = 0;
              accum = 0;
            }
            break;
        }
      }

      cyccount = 0;
    }
  }
  return ortbuflen;
}

// This is used by the wav loader routine. It gets a sample from a wav
// file (either 8 or 16bit), and returns it as a signed value. No scaling
// is performed, so it returns -128 to 127 for 8bit wavs, and -32768 to
// 32767 for 16bit wavs.
static inline signed short getsmp(unsigned char *buf, SDL_bool is8bit)
{
  return is8bit ? ((short)*buf)-128 : (short)((buf[1]<<8)|buf[0]);
}

// This converts a WAV file to Oricutron's ORT format. It should cope
// with any PCM wav file (stereo, mono, 8bit, 16bit...). It also adjusts
// any DC offset in the recording. It also calls "tapsections" to convert
// any standard oric tape format waveforms it finds into non-raw "tap"
// style sections to enable turbo loading of those parts.
static SDL_bool wav_convert( struct machine *oric )
{
  // Chunk pointers
  unsigned char *p=oric->tapebuf, *data=NULL, *ortbuf=NULL;
  unsigned int i, j, k, l, chunklen, bps=0, freq=0, smpdelta=0, datalen=0, ortlen;
  signed int smax, smin, dcoffs;
  signed short smp;
  // Cycles per sample
  double cps, count, pcount;
  SDL_bool stereo = SDL_FALSE, fmtseen = SDL_FALSE;

  // Basic validation
  i = 12;
  while ((!fmtseen) || (!data))
  {
    // Run out of data?
    if (i >= (oric->tapelen-8)) return SDL_FALSE;

    // Length of this chunk
    chunklen = (p[i+7]<<24)|(p[i+6]<<16)|(p[i+5]<<8)|p[i+4];

    // Sane length?
    if ((i+chunklen+8) > oric->tapelen)
      return SDL_FALSE;
 
    // Format chunk?
    if (memcmp(&p[i], "fmt ", 4) == 0)
    {
      // PCM?
      if ((chunklen != 16) || (((p[i+9]<<8)|p[i+8])!=1))
        return SDL_FALSE;
      
      // Channels
      switch ((p[i+11]<<8)|(p[i+10]))
      {
        case 1:  stereo = SDL_FALSE; break;
        case 2:  stereo = SDL_TRUE;  break;
        default: return SDL_FALSE;   break;
      }

      // Frequency
      freq = (p[i+15]<<24)|(p[i+14]<<16)|(p[i+13]<<8)|p[i+12];
      if( !freq ) return SDL_FALSE;

      // Sample delta
      smpdelta = (p[i+21]<<16)|p[i+20];

      // Bits per sample
      bps = (p[i+23]<<16)|p[i+22];
      if ((bps!=8)&&(bps!=16)) return SDL_FALSE;

      // Bytes per sample
      bps /= 8;

      fmtseen = SDL_TRUE;
    }
    else if (memcmp(&p[i], "data", 4) == 0)
    {
      data = &p[i+8];
      datalen = chunklen;
    }
    
    // Skip chunk
    i += chunklen+8;
  }

  smax = -70000;
  smin =  70000;

  // If we got here, we found a format and some data
  // Calculate the DC offset
  for (i=0; i<datalen; i+=smpdelta)
  {
    smp = getsmp(&data[i], bps==1);
    if (stereo) smp += getsmp(&data[i+bps], bps==1);

    if (smp < smin) smin = smp;
    if (smp > smax) smax = smp;
  }
  dcoffs = ((smax-smin)/2)+smin;

  // Now convert to a 1/0 squarewave
  j = 0;
  for (i=0; i<datalen; i+=smpdelta)
  {
    smp = getsmp(&data[i], bps==1);
    if (stereo) smp += getsmp(&data[i+bps], bps==1);

    oric->tapebuf[j++] = (smp>dcoffs) ? 1 : 0;
  }
  // Calculate cycles per sample
  cps = 500000 / ((double)freq);  // .ORT is 500khz

  // Calculate the length of the .ORT data
  ortlen = 5; // header + initial state
  i = oric->tapebuf[0];
  count = 0.0f;
  pcount = 0.0f;
  for (k=1; k<j; k++)
  {
    if (oric->tapebuf[k] != i)
    {
      i = oric->tapebuf[k];
      if (((int)count) < 1)
      {
        // Just a spike?
        if (ortlen>5) ortlen--;
        count += pcount;
      }
      else if (((int)count) < 0xfc)
      {
        ortlen++;
        pcount = count;
        count = 0.0f;
      }
      else if (((int)count) < 0x100)
      {
        ortlen+=2;
        pcount = count;
        count = 0.0f;
      }
      else
      {
        ortlen+=3;
        pcount = count;
        count = 0.0f;
      }
    }
    count+=cps;
  }

  // Allocate a buffer for the converted data
  ortbuf = malloc(ortlen);
  if (!ortbuf) return SDL_FALSE;

  // Write the header
  memcpy(ortbuf, "ORT\0", 4);

  // Write the ORT data
  i = oric->tapebuf[0];
  ortbuf[4] = i;
  count = 0.0f;
  pcount = 0.0f;
  l = 5;
  for (k=1; k<j; k++)
  {
    if (oric->tapebuf[k] != i)
    {
      i = oric->tapebuf[k];
      if (((int)count) < 1)
      {
        // Just a spike. Cancel the last swap.
        if (l==5)
          ortbuf[4] = i;
        else
          l--;
        count += pcount;
      }
      else if (((int)count) < 0xfc)
      {
        ortbuf[l++] = count;
        pcount = count;
        count = 0.0f;
      }
      else if (((int)count) < 0x100)
      {
        ortbuf[l++] = 0xfc;
        ortbuf[l++] = count;
        pcount = count;
        count = 0.0f;
      }
      else
      {
        ortbuf[l++] = 0xfd;
        ortbuf[l++] = (((int)count)>>8)&0xff;
        ortbuf[l++] = ((int)count)&0xff;
        pcount = count;
        count = 0.0f;
      }
    }
    count+=cps;
  }

  // Look for any standard oric encoded parts to convert
  // to non-raw "tap" sections
  ortlen = tapsections(ortbuf, ortlen, oric->tapebuf);

  // Substitute the ORT data for the original WAV data
  free(oric->tapebuf);
  oric->tapebuf = ortbuf;
  oric->tapelen = ortlen;

  return SDL_TRUE;
}

// Insert a new tape image
SDL_bool tape_load_tap( struct machine *oric, char *fname )
{
  FILE *f;

  // First make sure the image file exists
  f = fopen( fname, "rb" );
  if( !f ) return SDL_FALSE;

  // Eject any old image
  tape_eject( oric );

  // Get the image size
  fseek( f, 0, SEEK_END );
  oric->tapelen = ftell( f );
  fseek( f, 0, SEEK_SET );

  if( oric->tapelen <= 4 )   // Even worth loading it?
  {
    fclose( f );
    return SDL_FALSE;
  }

  // Allocate memory for the tape image and read it in
  oric->tapebuf = malloc( oric->tapelen );
  if( !oric->tapebuf )
  {
    fclose( f );
    oric->tapelen = 0;
    return SDL_FALSE;
  }

  if( fread( &oric->tapebuf[0], oric->tapelen, 1, f ) != 1 )
  {
    fclose( f );
    tape_eject( oric );
    return SDL_FALSE;
  }

  fclose( f );

  // WAV
  if ((oric->tapelen >= 36) &&
      (memcmp(oric->tapebuf,   "RIFF", 4) == 0) &&
      (memcmp(oric->tapebuf+8, "WAVE", 4) == 0))
  {
    if (!wav_convert( oric ))
    {
      msgbox( oric, MSGBOX_OK, "Invalid wav file" );
      tape_eject( oric );
      return SDL_FALSE;
    }

    oric->rawtape = SDL_TRUE;
  }
  // ORT
  else if (memcmp(oric->tapebuf, "ORT\0", 4) == 0)
  {
    oric->rawtape = SDL_TRUE;
  }
  // TAP
  else if (memcmp(oric->tapebuf, "\x16\x16\x16", 3) == 0)
  {
    oric->rawtape = SDL_FALSE;
  }
  // ???
  else
  {
    tape_eject( oric );
    msgbox( oric, MSGBOX_OK, "Unrecognised tape format" );
    return SDL_FALSE;
  }

  // Rewind the tape
  tape_rewind( oric );

  // Make a displayable version of the image filename
  if( strlen( fname ) > 31 )
  {
    strncpy( oric->tapename, &fname[strlen(fname)-31], 32 );
    oric->tapename[0] = 22;
  } else {
    strncpy( oric->tapename, fname, 32 );
  }
  oric->tapename[31] = 0;

  // Show it in the popup
  tape_popup( oric );
  return SDL_TRUE;
};

void tape_autoinsert( struct machine *oric )
{
  char *odir;
  int i;

  if( strncmp( (char *)&oric->mem[oric->pch_fd_getname_addr], oric->lasttapefile, 16 ) == 0 )
    oric->mem[oric->pch_fd_getname_addr] = 0;

  // Try and load the tape image
  strcpy( tapefile, oric->lasttapefile );
  i = strlen(tapefile);

  odir = getcwd( NULL, 0 );
  chdir( tapepath );
  tape_load_tap( oric, tapefile );
  if( !oric->tapebuf )
  {
    // Try appending .tap
    strcpy( &tapefile[i], ".tap" );
    tape_load_tap( oric, tapefile );
  }
  if( !oric->tapebuf )
  {
    // Try appending .ort
    strcpy( &tapefile[i], ".ort" );
    tape_load_tap( oric, tapefile );
  }
  
  if( oric->tapebuf )
  {
    // We already inserted this one. Don't re-insert it when we get to the end. */
    oric->lasttapefile[0] = 0;
  }
    
  chdir( odir );
  free( odir );
}

void tape_stop_savepatch( struct machine *oric )
{
  if( !oric->tsavf ) return;

  if( oric->tsavf == oric->tapecap )
  {
    unsigned char bufdata[2];
    fseek( oric->tsavf, oric->tapecapsavoffs, SEEK_SET );
    bufdata[0] = (oric->tapecapsavbytes>>8)&0xff;
    bufdata[1] = oric->tapecapsavbytes&0xff;
    fwrite( bufdata, 2, 1, oric->tsavf );
    oric->tapecapsavoffs = 0;
    oric->tapecapsavbytes = 0;
  }
  else
  {
    fclose( oric->tsavf );
  }

  oric->tsavf = NULL;
}

// Do the tape patches (must be done after every m6502 setcycles)
void tape_patches( struct machine *oric )
{
  Sint32 i, j;

  if( oric->romon )
  {
    if( oric->pch_fd_available )
    {
      // Patch CLOAD to insert a tape image specified.
      // Unfortunately the way the ROM works means we
      // only have up to 16 chars.
      if( ( oric->cpu.calcpc == oric->pch_fd_cload_getname_pc ) ||
          ( oric->cpu.calcpc == oric->pch_fd_recall_getname_pc ) )
      {
        // Read in the filename from RAM
        for( i=0; i<16; i++ )
        {
          j = oric->cpu.read( &oric->cpu, oric->pch_fd_getname_addr+i );
          if( !j ) break;
          oric->lasttapefile[i] = j;
        }
        oric->lasttapefile[i] = 0;

        if( ( oric->cpu.read( &oric->cpu, oric->pch_fd_getname_addr ) != 0 ) &&
            ( oric->autoinsert ) )
        {
          // Only do this if there is no tape inserted, or we're at the
          // end of the current tape, or the filename ends in .TAP, .ORT or .WAV
          if( ( !oric->tapebuf ) ||
              ( oric->tapeoffs >= oric->tapelen ) ||
              ( ( i > 3 ) && ( strcasecmp( &oric->lasttapefile[i-4], ".tap" ) == 0 ) ) ||
              ( ( i > 3 ) && ( strcasecmp( &oric->lasttapefile[i-4], ".ort" ) == 0 ) ) ||
              ( ( i > 3 ) && ( strcasecmp( &oric->lasttapefile[i-4], ".wav" ) == 0 ) ) )
          {
            tape_autoinsert( oric );
            oric->cpu.write( &oric->cpu, oric->pch_fd_getname_addr, 0 );
          }
        }
      }
      else if( ( ( oric->cpu.calcpc == oric->pch_fd_csave_getname_pc ) ||
                 ( oric->cpu.calcpc == oric->pch_fd_store_getname_pc ) ) &&
               ( ( !oric->tapecap ) || ( oric->tapeturbo ) ) )
      {
        // Did we miss the end of a previous one?
        if( oric->tsavf )
          tape_stop_savepatch( oric );

        // If we're doing tape capture, we can just use that
        if( oric->tapecap )
        {
          oric->tsavf = oric->tapecap;
          if( oric->tapecapcount < 0 )
            fputc( 0, oric->tapecap );
          fputc( 0xff, oric->tapecap );
          oric->tapecapsavoffs = ftell(oric->tapecap);
          fputc( 0x00, oric->tapecap );
          fputc( 0x00, oric->tapecap );
          oric->tapecapsavbytes = 0;
        }
        else
        {
          char *odir = NULL;

          // Read in the filename from RAM
          for( i=0; i<16; i++ )
          {
            j = oric->cpu.read( &oric->cpu, oric->pch_fd_getname_addr+i );
            if( !j ) break;
            tmptapename[i] = j;
          }
          tmptapename[i] = 0;

          // If no name, prompt for one
          if( tmptapename[0] == 0 ) 
          {
            if( !filerequester( oric, "Save to tape", tapepath, tmptapename, FR_TAPESAVETAP ) )
              tmptapename[0] = 0;
          }

          // If there is one, append .TAP
          if( tmptapename[0] )
          {
            if( (strlen(tmptapename) < 4) || (strcasecmp(&tmptapename[strlen(tmptapename)-4], ".tap") != 0) )
            {
              strncat(tmptapename, ".tap", sizeof(tmptapename));
              tmptapename[sizeof(tmptapename)-1] = 0;
            }
          }

          odir = getcwd( NULL, 0 );
          if( odir )
          {
            chdir( tapepath );
            oric->tsavf = fopen(tmptapename, "wb");
            chdir( odir );
            free( odir );
          }
        }
      }
      else if( ( oric->cpu.calcpc == oric->pch_tt_csave_end_pc ) ||
               ( oric->cpu.calcpc == oric->pch_tt_store_end_pc ) )
      {
        SDL_bool justtap = (oric->tsavf != oric->tapecap);
        tape_stop_savepatch( oric );
        if( justtap )
        {
          snprintf( filetmp, 32, "\x0f\x10 Saved to %s", tmptapename );
          filetmp[31] = 0;
          if (strlen(tmptapename) > 20)
          {
            filetmp[30] = '\x16';
          }
          do_popup( oric, filetmp );
        }
        tmptapename[0] = 0;
      }
    }

    if( ( oric->pch_tt_save_available ) && ( ( !oric->tapecap ) || ( oric->tapeturbo ) ) )
    {
      if( oric->cpu.calcpc == oric->pch_tt_putbyte_pc )
      {
        if( oric->tsavf )
        {
          fputc( oric->cpu.a, oric->tsavf );
          if( oric->tsavf == oric->tapecap )
            oric->tapecapsavbytes++;
        }

        oric->cpu.calcpc = oric->pch_tt_putbyte_end_pc;
        oric->cpu.calcop = oric->cpu.read( &oric->cpu, oric->cpu.calcpc );
      }
      else if( oric->cpu.calcpc == oric->pch_tt_writeleader_pc )
      {
        if( oric->tsavf )
        {
          unsigned char leader[] = { 0x16, 0x16, 0x16, 0x16 };
          fwrite( leader, 4, 1, oric->tsavf );
          if( oric->tsavf == oric->tapecap )
            oric->tapecapsavbytes += 4;
        }

        oric->cpu.calcpc = oric->pch_tt_writeleader_end_pc;
        oric->cpu.calcop = oric->cpu.read( &oric->cpu, oric->cpu.calcpc );
      }
    }
  }

// Only do turbotape past this point!

  // No tape? Motor off?
  if( ( !oric->tapebuf ) || ( !oric->tapemotor ) )
  {
    if( ( oric->pch_tt_available ) && ( oric->tapeturbo ) && ( oric->romon ) && ( oric->cpu.calcpc == oric->pch_tt_getsync_pc ) )
      oric->tapeturbo_syncstack = oric->cpu.sp;
    return;
  }

  if( ( oric->tapeturbo_forceoff ) && ( oric->pch_tt_available ) && ( oric->romon ) && ( oric->cpu.calcpc == oric->pch_tt_getsync_end_pc ) )
    oric->tapeturbo_forceoff = SDL_FALSE;

  // Don't do turbotape if we have a raw tape
  // Turbotape can't work with rawtape. TODO: Auto enable warpspeed when CLOADing/CSAVEing rawtape
  if(( oric->rawtape ) && ( oric->tapeoffs >= oric->nonrawend )) return;

  // Maybe do turbotape
  if( ( oric->pch_tt_available ) && ( oric->tapeturbo ) && ( !oric->tapeturbo_forceoff ) && ( oric->romon ) )
  {
    SDL_bool dosyncpatch;

    dosyncpatch = SDL_FALSE;

    if( oric->cpu.calcpc == oric->pch_tt_getsync_loop_pc )
    {
      // Cassette sync was called when there was no valid
      // tape to sync, so the normal cassette sync hack failed
      // and now we're stuck looking for a signal that will never
      // arrive. We have to recover back to the end of the cassette
      // sync routine.

      // Did we spot the entry into the cassette sync routine?
      if( oric->tapeturbo_syncstack == -1 )
      {
        // No. Give up.
        oric->tapeturbo_forceoff = SDL_TRUE;
        return;
      }

      // Restore the stack
      oric->cpu.sp = oric->tapeturbo_syncstack;
      oric->tapeturbo_syncstack = -1;
      dosyncpatch = SDL_TRUE;
    }

    if( ( oric->cpu.calcpc == oric->pch_tt_getsync_pc ) || ( dosyncpatch ) )
    {
      // Currently at a sync byte?
      if( oric->tapebuf[oric->tapeoffs] != 0x16 )
      {
        // Find the next sync byte
        do
        {
          oric->tapeoffs++;

          // Give up at end of image
          if( oric->tapeoffs >= oric->tapelen )
          {
            refreshtape = SDL_TRUE;
            return;
          }
        } while( oric->tapebuf[oric->tapeoffs] != 0x16 );
      }

      // "Jump" to the end of the cassette sync routine
      oric->cpu.calcpc = oric->pch_tt_getsync_end_pc;
      oric->cpu.calcop = oric->cpu.read(&oric->cpu, oric->cpu.calcpc);
      return;
    }

    if( oric->cpu.calcpc == oric->pch_tt_readbyte_pc )
    {
      // Read the next byte directly into A
      oric->cpu.a = oric->tapebuf[oric->tapeoffs++];

      // Set flags
      oric->cpu.f_z = oric->cpu.a == 0;
      oric->cpu.f_c = oric->pch_tt_readbyte_setcarry ? 1 : 0;

      // Simulate the effects of the read byte routine
      if( oric->pch_tt_readbyte_storebyte_addr != -1 ) oric->cpu.write( &oric->cpu, oric->pch_tt_readbyte_storebyte_addr, oric->cpu.a );
      if( oric->pch_tt_readbyte_storezero_addr != -1 ) oric->cpu.write( &oric->cpu, oric->pch_tt_readbyte_storezero_addr, 0x00 );

      // Jump to the end of the read byte routine
      oric->cpu.calcpc = oric->pch_tt_readbyte_end_pc;
      oric->cpu.calcop = oric->cpu.read( &oric->cpu, oric->cpu.calcpc );
      if( oric->tapeoffs >= oric->tapelen ) refreshtape = SDL_TRUE;
    }
  }
}

// Emulate the specified cpu-cycles time for the tape
void tape_ticktock( struct machine *oric, int cycles )
{
  Sint32 j;

  // Update the counter since last PB7 toggle
  if( ( oric->tapecap ) && ( oric->tapecapcount != -1 ) )
    oric->tapecapcount += cycles;

  // The VSync hack is triggered in the video emulation
  // but actually handled here, since the VSync signal
  // is read into the tape input. The video emulation
  // puts half a scanlines worth of cycles into this
  // counter, which we count down here.
  if( oric->vsync > 0 )
  {
    oric->vsync -= cycles;
    if( oric->vsync < 0 )
      oric->vsync = 0;
  }
  
  // If the VSync hack is active, we pull CB1 low
  // while the above counter is active.
  if( oric->vsynchack )
  {
    j = (oric->vsync == 0);
    if( oric->via.cb1 != j )
      via_write_CB1( &oric->via, j );
  }

  // No tape? Motor off?
  if( ( !oric->tapebuf ) || ( !oric->tapemotor ) )
    return;

  // Tape offset outside the tape image limits?
  if( ( oric->tapeoffs < 0 ) || ( oric->tapeoffs >= oric->tapelen ) )
  {
    if( oric->tapehitend > 2 )
    {
      // Try to autoinsert a tape image
      if( ( oric->lasttapefile[0] ) && ( oric->autoinsert ) )
      {
        tape_autoinsert( oric );
        oric->lasttapefile[0] = 0;
      }
    }
  }

  // Abort if turbotape is on
  if( ( oric->pch_tt_available ) && ( oric->tapeturbo ) && ( !oric->tapeturbo_forceoff ) && ( oric->romon ) && ( !oric->rawtape ) )
    return;

  if( oric->tapehitend > 2 )
    return;

  if( ( oric->tapehdrend != 0 ) && ( oric->tapeoffs == oric->tapehdrend ) )
  {
    oric->tapedelay = 1281;
    oric->tapehdrend = 0;
  }

  // No turbotape. Do "real" tape emulation.
  // Count down the cycle counter
  if( oric->tapecount > cycles )
  {
    oric->tapecount -= cycles;
    if( oric->tapedelay > 0 )
    {
      oric->tapedelay -= cycles;
      if( oric->tapedelay < 0 )
        oric->tapedelay = 1;
    }
    return;
  }

  // Toggle the tape input
  oric->tapeout ^= 1;
  if( !oric->vsynchack )
  {
    // Update the audio if tape noise is enabled
    if( oric->tapenoise )
    {
      ay_lockaudio( &oric->ay ); // Gets unlocked at the end of each frame
      if( oric->ay.do_logcycle_reset )
      {
        oric->ay.logcycle = oric->ay.newlogcycle;
        oric->ay.do_logcycle_reset = SDL_FALSE;
      }
      if( oric->ay.tlogged < TAPELOG_SIZE )
      {
        oric->ay.tapelog[oric->ay.tlogged  ].cycle = oric->ay.logcycle;
        oric->ay.tapelog[oric->ay.tlogged++].val   = oric->tapeout;
      }
    }

    // Put tape signal onto CB1
    via_write_CB1( &oric->via, oric->tapeout );
  }

  if( oric->tapedelay > 0 )
  {
    oric->tapedelay -= cycles;
    if( oric->tapedelay <= 0)
    {
      if( oric->tapebit == 1 )
        oric->tapedelay = 1;
      else
        oric->tapedelay = 0;
    }
    oric->tapecount = TAPE_1_PULSE;
    return;
  }

  if( oric->tapeoffs >= oric->tapelen )
  {
    switch( oric->tapehitend )
    {
      case 0: oric->tapecount = TAPE_1_PULSE; break;
      case 1: oric->tapecount = 0x36*2; break;
    }
    oric->tapehitend++;
    refreshtape = SDL_TRUE;
    return;
  }

  // Raw tape
  if( oric->rawtape )
  {
    // In a non-raw section?
    if( oric->tapeoffs >= oric->nonrawend )
    {
      tape_next_raw_count( oric );
      if( oric->tapehitend > 2 )
        refreshtape = SDL_TRUE;
      return;
    }
  }

  // .TAP real tape simulation
  // Tape signal rising edge
  if( oric->tapeout )
  {
    switch( oric->tapebit )
    {
      case 0:      // Start of a new byte. Send a 1 pulse
        oric->tapetime = TAPE_1_PULSE;
        break;

      case 1:     // Then a sync pulse (0)
        oric->tapetime = TAPE_0_PULSE;
        oric->tapeparity = 1;
        break;
      
      default:    // For bit numbers 2 to 9, send actual byte bits 0 to 7
        if( oric->tapebuf[oric->tapeoffs]&(1<<(oric->tapebit-2)) )
        {
          oric->tapetime = TAPE_1_PULSE;
          oric->tapeparity ^= 1;
        } else {
          oric->tapetime = TAPE_0_PULSE;
        }
        break;

      case 10:  // Then a parity bit
        if( oric->tapeparity )
          oric->tapetime = TAPE_1_PULSE;
        else
          oric->tapetime = TAPE_0_PULSE;
        break;

      case 11:
      case 12:
      case 13:
        oric->tapetime = TAPE_1_PULSE;
        break;
    }

    // Move on to the next bit
    oric->tapebit = (oric->tapebit+1)%14;
    if( !oric->tapebit )
    {
      if( oric->tapedupbytes > 0 )
        oric->tapedupbytes--;
      else
        oric->tapeoffs++;
    }
  }
  oric->tapecount = oric->tapetime;
}

