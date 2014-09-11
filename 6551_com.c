/*
 * *  Oricutron
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
 **  6551 ACIA emulation - com back-end
 */

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"

#ifdef BACKEND_COM

static struct machine* oric = NULL;

static int com_baudrate = 115200;
static int com_bits = 8;
static char com_parity = 'N';
static int com_stopbits = 1;
static char com_name[ACIA_BACKEND_NAME_LEN];

/* forward definitions */
static SDL_bool com_validate_param(void);
static SDL_bool com_open(void);
static SDL_bool com_close(void);
static SDL_bool com_write(Uint8 data);
static SDL_bool com_read(Uint8* data);
static SDL_bool com_peek(Uint8* data);

static void com_done( struct acia* acia )
{
  com_close();
  acia_init_none( acia );
}

static Uint8 com_stat(Uint8 stat)
{
  return (stat | (ASTF_CARRIER|ASTF_DSR));
}

static SDL_bool com_has_byte(Uint8* data)
{
  return com_peek(data);
}

static SDL_bool com_get_byte(Uint8* data)
{
  return com_read(data);
}

static SDL_bool com_put_byte(Uint8 data)
{
  return com_write(data);
}

SDL_bool acia_init_com( struct acia* acia )
{
  oric = acia->oric;

  acia->done = com_done;
  acia->stat = com_stat;
  acia->has_byte = com_has_byte;
  acia->get_byte = com_get_byte;
  acia->put_byte = com_put_byte;


  if(5 == sscanf(oric->aciabackendname, "com:%d,%d,%c,%d,%s",
    &com_baudrate, &com_bits, &com_parity, &com_stopbits, com_name))
  {
    //printf("com: %d, %d, %c, %d, %s\n", com_baudrate, com_bits, com_parity, com_stopbits, com_name);
    if(com_validate_param())
    {
      if(com_open())
      {
        oric->aciabackend = ACIA_TYPE_COM;
        return SDL_TRUE;
      }
    }
  }

  // fall-back to none
  acia_init_none( acia );
  return SDL_FALSE;
}

// simple - com library

#define COM_FIFO_SIZE   4096
static int com_fifo = 0;
static Uint8 com_fifo_buf[COM_FIFO_SIZE];


#ifdef __LINUX__
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#define min(a,b)  (((a)<(b))?(a):(b))

static int com_fd = -1;
static int com_param = 0;

static SDL_bool com_validate_param(void)
{
  com_param = 0;
  switch(com_baudrate)
  {
    case 50: com_param = B50; break;
    case 75: com_param = B75; break;
    case 110: com_param = B110; break;
    case 134: com_param = B134; break;
    case 150: com_param = B150; break;
    case 300: com_param = B300; break;
    case 600: com_param = B600; break;
    case 1200: com_param = B1200; break;
    case 1800: com_param = B1800; break;
    case 2400: com_param = B2400; break;
//  case 3600: com_param = B3600; break;
    case 4800: com_param = B4800; break;
//  case 7200: com_param = B7200; break;
    case 9600: com_param = B9600; break;
    case 19200: com_param = B19200; break;
    case 38400: com_param = B38400; break;
    case 57600: com_param = B57600; break;
    case 115200: com_param = B115200; break;
    default:
      return SDL_FALSE;
      break;
  }

  switch(com_bits)
  {
    case 5: com_param |= CS5; break;
    case 6: com_param |= CS6; break;
    case 7: com_param |= CS7; break;
    case 8: com_param |= CS8; break;
    default:
      return SDL_FALSE;
      break;
  }

  switch(tolower(com_parity))
  {
    case 'n': /* */ break;
    case 'o': com_param |= PARENB|PARODD; break;
    case 'e': com_param |= PARENB; break;
    default:
      return SDL_FALSE;
      break;
  }

  switch(com_stopbits)
  {
    case 2: com_param |= CSTOPB; break;
    case 1: /* */ break;
    default:
      return SDL_FALSE;
      break;
  }

  com_param |= (CLOCAL|CREAD);
  return SDL_TRUE;
}

static SDL_bool com_open(void)
{
  struct termios options;

  com_fifo = 0;

  com_fd = open(com_name, O_RDWR | O_NOCTTY | O_NDELAY);
  if( -1 == com_fd )
  {
    //printf("open failed\n");
    return SDL_FALSE;
  }

  fcntl(com_fd, F_SETFL, FNDELAY);

  tcgetattr(com_fd, &options);
  options.c_cflag  = com_param;
  tcsetattr(com_fd, TCSANOW, &options);

  return SDL_TRUE;
}

static SDL_bool com_close(void)
{
  if( -1 != com_fd )
    close( com_fd );
  com_fd = -1;
  return SDL_TRUE;
}

static SDL_bool com_write(Uint8 data)
{
  if( -1 != com_fd )
  {
    if( 0 < write(com_fd, &data, 1) )
      return SDL_TRUE;
  }
  return SDL_FALSE;
}

static void com_fill_fifo(void)
{
  if( com_fifo < COM_FIFO_SIZE )
  {
    int len = 0;
    if( -1 != ioctl(com_fd, FIONREAD, &len) )
    {
      len = min(len, COM_FIFO_SIZE - com_fifo);
      len = read( com_fd, com_fifo_buf + com_fifo, len);
      com_fifo += len;
    }
  }
}

static SDL_bool com_read(Uint8* data)
{
  if( -1 != com_fd )
  {
    com_fill_fifo();

    if( 0 < com_fifo )
    {
      *data = com_fifo_buf[0];
      memmove(com_fifo_buf, com_fifo_buf+1, --com_fifo);
      return SDL_TRUE;
    }
  }
  return SDL_FALSE;
}

static SDL_bool com_peek(Uint8* data)
{
  if( -1 != com_fd )
  {
    com_fill_fifo();

    if( 0 < com_fifo )
    {
      *data = com_fifo_buf[0];
      return SDL_TRUE;
    }
  }
  return SDL_FALSE;
}
#endif /* __LINUX__ */

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static HANDLE com_fd = INVALID_HANDLE_VALUE;
static DCB com_dcb;

static SDL_bool com_validate_param(void)
{
  memset(&com_dcb, 0, sizeof(com_dcb));
  switch(com_baudrate)
  {
//  case 50: com_dcb.BaudRate = CBR_50; break;
//  case 75: com_dcb.BaudRate = CBR_75; break;
    case 110: com_dcb.BaudRate = CBR_110; break;
//  case 134: com_dcb.BaudRate = CBR_134; break;
//  case 150: com_dcb.BaudRate = CBR_150; break;
    case 300: com_dcb.BaudRate = CBR_300; break;
    case 600: com_dcb.BaudRate = CBR_600; break;
    case 1200: com_dcb.BaudRate = CBR_1200; break;
//  case 1800: com_dcb.BaudRate = CBR_1800; break;
    case 2400: com_dcb.BaudRate = CBR_2400; break;
//  case 3600: com_dcb.BaudRate = CBR_3600; break;
    case 4800: com_dcb.BaudRate = CBR_4800; break;
//  case 7200: com_dcb.BaudRate = CBR_7200; break;
    case 9600: com_dcb.BaudRate = CBR_9600; break;
    case 19200: com_dcb.BaudRate = CBR_19200; break;
    case 38400: com_dcb.BaudRate = CBR_38400; break;
    case 57600: com_dcb.BaudRate = CBR_57600; break;
    case 115200: com_dcb.BaudRate = CBR_115200; break;
    default:
      return SDL_FALSE;
      break;
  }

  switch(com_bits)
  {
    case 5:
    case 6:
    case 7:
    case 8: com_dcb.ByteSize = com_bits; break;
    default:
      return SDL_FALSE;
      break;
  }

  switch(tolower(com_parity))
  {
    case 'n': com_dcb.fParity = 0; com_dcb.Parity = 0; break;
    case 'o': com_dcb.fParity = 1; com_dcb.Parity = 1; break;
    case 'e': com_dcb.fParity = 1; com_dcb.Parity = 2; break;
    default:
      return SDL_FALSE;
      break;
  }

  switch(com_stopbits)
  {
    case 2: com_dcb.StopBits = 2; break;
    case 1: com_dcb.StopBits = 0; break;
    default:
      return SDL_FALSE;
      break;
  }

  return SDL_TRUE;
}

static SDL_bool com_open(void)
{
  DCB dcb;
  COMMTIMEOUTS com_timeouts;

  com_fd = CreateFile(com_name, GENERIC_READ|GENERIC_WRITE, 0,0, OPEN_EXISTING, 0,0);
  if ( INVALID_HANDLE_VALUE == com_fd )
    return SDL_FALSE;

  if (!GetCommState(com_fd, &dcb) )
  {
    com_close();
    return SDL_FALSE;
  }

  dcb.BaudRate = com_dcb.BaudRate;
  dcb.ByteSize = com_dcb.ByteSize;
  dcb.fParity = com_dcb.fParity;
  dcb.Parity = com_dcb.Parity;
  dcb.StopBits = com_dcb.StopBits;

  dcb.fOutxCtsFlow=FALSE;
  dcb.fOutxDsrFlow=FALSE;
  dcb.fDtrControl=DTR_CONTROL_DISABLE;
  dcb.fTXContinueOnXoff=TRUE;

  memmove(&com_dcb, &dcb, sizeof(com_dcb));

  if (!SetCommState(com_fd, &com_dcb) )
  {
    com_close();
    return SDL_FALSE;
  }

  com_timeouts.ReadIntervalTimeout=0;
  com_timeouts.ReadTotalTimeoutConstant=0;
  com_timeouts.ReadTotalTimeoutMultiplier=0;
  com_timeouts.WriteTotalTimeoutConstant=0;
  com_timeouts.WriteTotalTimeoutMultiplier=0;
  SetCommTimeouts( com_fd, &com_timeouts );

  return SDL_TRUE;
}

static SDL_bool com_close(void)
{
  if ( INVALID_HANDLE_VALUE != com_fd )
    CloseHandle( com_fd );
  com_fd = INVALID_HANDLE_VALUE;
  return SDL_TRUE;
}

static SDL_bool com_write(Uint8 data)
{
  if( INVALID_HANDLE_VALUE != com_fd )
  {
    DWORD len = 0;
    if( 0 < WriteFile( com_fd, &data, 1, &len, NULL) )
      return SDL_TRUE;
  }
  return SDL_FALSE;
}

static void com_fill_fifo(void)
{
  if( com_fifo < COM_FIFO_SIZE )
  {
    DWORD err;
    COMSTAT com_st;
    ClearCommError( com_fd, &err, &com_st );
    if( 0 < (int) com_st.cbInQue )
    {
      DWORD len = min( com_st.cbInQue, COM_FIFO_SIZE - com_fifo );
      if( 0 < ReadFile( com_fd, com_fifo_buf + com_fifo, len, &len, NULL) )
      {
        com_fifo += len;
      }
    }
  }
}

static SDL_bool com_read(Uint8* data)
{
  if( INVALID_HANDLE_VALUE != com_fd )
  {
    com_fill_fifo();

    if( 0 < com_fifo )
    {
      *data = com_fifo_buf[0];
      memmove(com_fifo_buf, com_fifo_buf+1, com_fifo-1);
      com_fifo--;
      return SDL_TRUE;
    }
  }
  return SDL_FALSE;
}

static SDL_bool com_peek(Uint8* data)
{
  if( INVALID_HANDLE_VALUE != com_fd )
  {
    com_fill_fifo();

    if( 0 < com_fifo )
    {
      *data = com_fifo_buf[0];
      return SDL_TRUE;
    }
  }
  return SDL_FALSE;
}

#endif /* WIN32 */

#endif /* BACKEND_COM */
