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
 **  6551 ACIA emulation - modem back-end
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

#ifdef BACKEND_MODEM

static struct machine* oric = NULL;

#define CMD_BUF_SIZE    4
static Uint8 mdm_cmd_buf[CMD_BUF_SIZE];
static Uint64 mdm_time_buf[CMD_BUF_SIZE];

#define DATA_BUF_SIZE   4096
static int mdm_in = 0;
static Uint8 mdm_in_buf[DATA_BUF_SIZE];
static int mdm_out = 0;
static Uint8 mdm_out_buf[DATA_BUF_SIZE];

static SDL_bool connected = SDL_FALSE;
static SDL_bool listening = SDL_FALSE;
static SDL_bool escaped = SDL_FALSE;

static int srv_sck = -1;
static int cnt_sck = -1;

/* Forward definitions */
static SDL_bool socket_init(void);
static void socket_done(void);
static int socket_create(int* sock);
static int socket_bind(int sock, int port);
static int socket_listen(int sock);
static int socket_accept(int sock, int* sck);
static int socket_connect(int sock, const char* ip, int port);
static int socket_write(int sock, const unsigned char* data, int len);
static int socket_read(int sock, unsigned char* data, int* len);
static int socket_close(int sock);

static Uint64 time_getmillisec(void);

static char* trim(char* line)
{
  if(line && *line)
  {
    int l = strlen(line);
    while(0 <= l && line[l-1] == ' ')
      line[--l] = '\0';
    while(0 <= l && line[0] == ' ')
      memmove(line, line+1, l--);
  }
  return line;
}

static void send_str(const char* s)
{
  while(s && *s)
  {
    mdm_out_buf[mdm_out] = *s++;
    mdm_out++;
    if(mdm_out == DATA_BUF_SIZE)
    {
      memmove(mdm_out_buf, mdm_out_buf+1, DATA_BUF_SIZE-1);
      mdm_in--;
    }
  }
}

static void send_responce(const char* s)
{
  send_str("\r\n");
  if(s && *s)
  {
    send_str(s);
    send_str("\r\n");
  }
}

static void send_responce_ok(void)
{
  send_responce("OK");
}

static void send_responce_error(void)
{
  send_responce("ERROR");
}

static void mdm_hangup(void)
{
  connected = SDL_FALSE;
  escaped = SDL_FALSE;
  socket_close(cnt_sck);
  cnt_sck = -1;
}

static void mdm_answeroff(void)
{
  if( listening )
  {
    listening = SDL_FALSE;
    socket_close(srv_sck);
    srv_sck = -1;
  }
}

static void mdm_answeron(void)
{
  if( !listening )
  {
    if(socket_create(&srv_sck) &&
      socket_bind(srv_sck, oric->aciabackendcfgport) &&
      socket_listen(srv_sck))
      listening = SDL_TRUE;
  }
}

static void mdm_connect(const char* s)
{
  char* ip = trim(strdup(s));
  char* p = strchr(ip, ':');
  int port = 0;

  if(p)
  {
    *p++ = '\0';
    port = atoi(p);
  }
  // default telnet port
  if(0 == port) port = ACIA_TYPE_MODEM_DEFAULT_PORT;

  if( socket_create(&cnt_sck) )
  {
    if(socket_connect(cnt_sck, ip, port))
    {
      send_responce("CONNECT");
      connected = SDL_TRUE;
      return;
    }
    else
      send_responce("BUSY");
  }
  else
    send_responce("NO DIALTINE");

  mdm_hangup();
  free(ip);
}

static void parse_command(const char* s)
{
  if(!s)
    return;

  if( escaped )
  {
    if('\0' == s[0])
      send_responce("");
    else if(!strcasecmp("ATA", s))
      escaped = SDL_FALSE;
    else if(!strcasecmp("ATO", s))
      escaped = SDL_FALSE;
    else if(!strcasecmp("ATH1", s))
      escaped = SDL_FALSE;
    else if(!strcasecmp("ATH0", s))
    {
      mdm_hangup();
      send_responce_ok();
    }
    else if(!strncasecmp("ATZ", s, 3))
    {
      mdm_hangup();
      send_responce_ok();
    }
    else
      send_responce_error();
  }
  else
  {
    if('\0' == s[0])
      send_responce("");
    else if(!strcasecmp("AT", s))
      send_responce_ok();
    else if(!strcasecmp("ATA", s))
    {
      mdm_answeron();
      if( !listening )
        send_responce_error();
      else
        send_responce("AUTOANSWER ON");
    }
    else if(!strncasecmp("ATS0=", s, 5))
    {
      char* p = trim(strdup(s+5));
      int on = atoi(p);
      free(p);
      
      if(0<on)
      {
        mdm_answeron();
        if( !listening )
          send_responce_error();
        else
          send_responce("AUTOANSWER ON");
      }
      else
      {
        mdm_answeroff();
        send_responce("AUTOANSWER OFF");
      }
    }
    else if(!strcasecmp("ATS0?", s))
    {
      if( listening )
        send_responce("AUTOANSWER ON");
      else
        send_responce("AUTOANSWER OFF");
    }
    else if(!strcasecmp("ATH0", s))
    {
      mdm_hangup();
      send_responce_ok();
    }
    else if(!strcasecmp("ATH1", s))
      send_responce_ok();
    else if(!strncasecmp("ATZ", s, 3))
      send_responce_ok();
    else if(!strncasecmp("AT&F", s, 4))
      send_responce_ok();
    else if(!strncasecmp("ATD", s, 3))
    {
      s = s + 3;
      switch(tolower(*s))
      {
        case 't':
        case 'p':
          s++;
          break;
      }
      if(*s)
        mdm_connect(s);
      else
        send_responce_error();
    }
    else
      send_responce_error();
  }
}

static void modem_done( struct acia* acia )
{
  mdm_hangup();

  socket_close(srv_sck);
  srv_sck = -1;

  socket_done();
  acia_init_none( acia );
}

static Uint8 modem_stat(Uint8 stat)
{
  if( connected && !escaped )
  {
    int len = DATA_BUF_SIZE - mdm_out;
    if(0 < len)
    {
      if(!socket_read(cnt_sck, mdm_out_buf + mdm_out, &len))
      {
        mdm_hangup();
        send_responce("NO CARRIER");
      }
      else
        mdm_out += len;
    }
  }

  if( !connected && listening )
  {
    if(socket_accept(srv_sck, &cnt_sck))
    {
      send_responce("CONNECT");
      connected = SDL_TRUE;
    }
  }

  if( connected )
    return (stat & ~(ASTF_CARRIER|ASTF_DSR));

  if( listening )
    return (stat & ~(ASTF_DSR));

  return (stat | (ASTF_CARRIER|ASTF_DSR));
}

static SDL_bool modem_has_byte(Uint8* data)
{
  if(0 < mdm_out)
  {
    *data = mdm_out_buf[0];
    return SDL_TRUE;
  }

  return SDL_FALSE;
}

static SDL_bool modem_get_byte(Uint8* data)
{
  if(0 < mdm_out)
  {
    *data = mdm_out_buf[0];
    memmove(mdm_out_buf, mdm_out_buf+1, mdm_out);
    mdm_out--;
    return SDL_TRUE;
  }

  return SDL_FALSE;
}

static void mdm_escape(Uint8 data)
{
  mdm_cmd_buf[0] = mdm_cmd_buf[1];
  mdm_cmd_buf[1] = mdm_cmd_buf[2];
  mdm_cmd_buf[2] = mdm_cmd_buf[3];
  mdm_cmd_buf[3] = data;
  
  mdm_time_buf[0] = mdm_time_buf[1];
  mdm_time_buf[1] = mdm_time_buf[2];
  mdm_time_buf[2] = mdm_time_buf[3];
  mdm_time_buf[3] = time_getmillisec();
  
  if( mdm_cmd_buf[1] == '+' && mdm_cmd_buf[2] == '+' && mdm_cmd_buf[3] == '+')
  {
    if(1000 < (mdm_time_buf[3] - mdm_time_buf[0]))
    {
      escaped = SDL_TRUE;
    }
  }  
}

static SDL_bool modem_put_byte(Uint8 data)
{
  if( connected && !escaped )
  {
    if(!socket_write(cnt_sck, &data, 1))
    {
      mdm_hangup();
      send_responce("NO CARRIER");
    }
    mdm_escape(data);
  }
  else
  {
    mdm_in_buf[mdm_in] = data;
    mdm_in++;
    if(mdm_in == DATA_BUF_SIZE)
    {
      memmove(mdm_in_buf, mdm_in_buf+1, DATA_BUF_SIZE-1);
      mdm_in--;
    }
    
    // including 0x7e as BACKSPACE:
    // this is tribute to Vagelis Blathras
    // for the  excellent terminal program
    if(data == 0x08 || data == 0x7f || data == 0x7e)
    {
      switch(mdm_in)
      {
        case 0:
          break;
        case 1:
          mdm_in = 0;
          break;
        default:
          mdm_in -= 2;
          break;
      }
    }
    else if(data == 0x0d)
    {
      mdm_in_buf[mdm_in-1] = 0x00;
      parse_command((const char*)mdm_in_buf);
      mdm_in = 0;
    }
    else
    {
      // local echo
      if(mdm_out < DATA_BUF_SIZE)
        mdm_out_buf[mdm_out++] = data;
    }
  }
  return SDL_TRUE;
}

SDL_bool acia_init_modem( struct acia* acia )
{
  oric = acia->oric;

  mdm_in = 0;
  mdm_out = 0;

  mdm_in_buf[0] = 0x00;
  mdm_out_buf[0] = 0x00;
  memset(mdm_cmd_buf, 0, sizeof(Uint8)*CMD_BUF_SIZE);
  memset(mdm_time_buf, 0, sizeof(Uint64)*CMD_BUF_SIZE);
  
  acia->done = modem_done;
  acia->stat = modem_stat;
  acia->has_byte = modem_has_byte;
  acia->get_byte = modem_get_byte;
  acia->put_byte = modem_put_byte;

  srv_sck = -1;
  cnt_sck = -1;

  connected = SDL_FALSE;
  listening = SDL_FALSE;
  escaped = SDL_FALSE;
  
  if(socket_init())
  {
    oric->aciabackend = ACIA_TYPE_MODEM;
    return SDL_TRUE;
  }

  // fall-back to none
  acia_init_none( acia );
  return SDL_FALSE;
}

// simple socket library

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>


#ifdef __LINUX__
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#define _recvdata(a, b, c) read(a, b, c)
#define _closesocket close
#endif

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#define _recvdata(a, b, c) recv(a, b, c, 0)
#define _closesocket closesocket
#endif // WIN32

static SDL_bool socket_initialized = SDL_FALSE;

static SDL_bool socket_init(void)
{
  if(!socket_initialized)
  {
    #ifdef WIN32
    WSADATA wsadata;
    if( (WSAStartup(MAKEWORD(2,2), &wsadata) == SOCKET_ERROR) ||
      (WSAStartup(MAKEWORD(1,1), &wsadata) == SOCKET_ERROR) )
      return SDL_FALSE;
    #endif
  }
  socket_initialized = SDL_TRUE;
  return SDL_TRUE;
}

static void socket_done(void)
{
  #ifdef WIN32
  WSACleanup();
  #endif
}

static int socket_create(int* sock)
{
  int on = 1;
  *sock = socket(AF_INET, SOCK_STREAM, 0);
  if(*sock == -1)
    return 0;

  if(setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, (const char*) &on, sizeof(on)) == -1)
    return 0;

  return 1;
}

static int socket_bind(int sock, int port)
{
  int on = 1;
  struct sockaddr_in srv_addr;
  memset((void*)&srv_addr, 0, sizeof(srv_addr));
  srv_addr.sin_family = AF_INET;
  srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  srv_addr.sin_port = htons(port);

  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on));

  if(bind(sock, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) == -1)
    return 0;
  else
  {
    #ifdef __LINUX__
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    #endif

    #ifdef WIN32
    u_long imode = 1;
    ioctlsocket(sock, FIONBIO, &imode);
    #endif
  }
  return 1;
}

static int socket_listen(int sock)
{
  if(listen(sock, 1) == -1)
    return 0;

  return 1;
}

static int socket_accept(int sock, int* sck)
{
  struct sockaddr_in cli_addr;
  socklen_t cli_addr_len = sizeof(cli_addr);

  *sck = accept(sock, (struct sockaddr*) &cli_addr, &cli_addr_len);
  if(*sck == -1)
    return 0;
  else
  {
    #ifdef __LINUX__
    fcntl(*sck, F_SETFL, fcntl(*sck, F_GETFL, 0) | O_NONBLOCK);
    #endif

    #ifdef WIN32
    u_long imode = 1;
    ioctlsocket(*sck, FIONBIO, &imode);
    #endif
  }

  return 1;
}

static int socket_write(int sock, const unsigned char* data, int len)
{
  if(send(sock, (char*)data, len, 0) == -1)
    return 0;
  return 1;
}

static int socket_read(int sock, unsigned char* data, int* len)
{
  int ret = _recvdata(sock, (char*)data, *len);
  *len = 0;
  if(0 == ret)
    return 0;
  else if(0 < ret)
    *len = ret;
  return 1;
}

static void hostname_to_ip(const char * host, char* ip, int len)
{
  struct hostent *he = gethostbyname( host );
  if( NULL != he )
  {
    struct in_addr **addr_list = (struct in_addr **) he->h_addr_list;
    if( NULL != addr_list[0] )
    {
      strncpy(ip, inet_ntoa(*addr_list[0]), len);
      return;
    }
  }
  strncpy(ip, host, len);
}

static int socket_connect(int sock, const char* host, int port)
{
  char ip[64];
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  hostname_to_ip(host, ip, 64);
  addr.sin_addr.s_addr = inet_addr(ip);

  if(connect(sock, (struct sockaddr*) &addr, sizeof(addr)) != 0)
    return 0;
  else
  {
    #ifdef __LINUX__
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
    #endif

    #ifdef WIN32
    u_long imode = 1;
    ioctlsocket(sock, FIONBIO, &imode);
    #endif
  }

  return 1;
}

int socket_close(int sock)
{
  if(_closesocket(sock) == -1)
    return 0;

  return 1;
}

static Uint64 time_getmillisec(void)
{
  #ifdef __LINUX__
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ((ts.tv_sec * 1000000000) + ts.tv_nsec)/1000;
  #endif
  #ifdef WIN32
  return GetTickCount();
  #endif
}

#endif /* BACKEND_MODEM */
