
#define TWILIGHTE_CARD_ORIC_EXTENSION_VDDRA            0x323
#define TWILIGHTE_CARD_ORIC_EXTENSION_DDRA             0x321

#define TWILIGHTE_CARD_ORIC_EXTENSION_VDDRB            0x322
#define TWILIGHTE_CARD_ORIC_EXTENSION_DDRB             0x320

#define TWILIGHTE_CARD_ORIC_EXTENSION_REGISTER          0x342
#define TWILIGHTE_CARD_ORIC_EXTENSION_BANKING_REGISTER  0x343



#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>


struct twilbankinfo
{
  unsigned char type;
  unsigned char *ptr;
};

struct twilighte
{
  
  unsigned char t_register;
  unsigned char t_banking_register;
  char twilrombankfiles[32][1024];
  
  unsigned char twilrambankfiles[32][1024];
 
  unsigned char twilrombankdata[32][16834];
  unsigned char twilrambankdata[32][16834];

  
  unsigned char VDDRA;
  unsigned char DDRA;

  unsigned char VDDRB;
  unsigned char DDRB;  
  unsigned char current_bank;
  
};


#include "../../system.h"

extern SDL_bool read_config_string( char *buf, char *token, char *dest, Sint32 maxlen );
 
static SDL_bool load_rom_twilighte(  char *fname, int size, unsigned char where[] )
{
  SDL_RWops *f;
  char *tmpname;

  // MinGW doesn't have asprintf :-(
  tmpname = malloc( strlen( fname ) + 10 );
  if( !tmpname )
  {
    return SDL_FALSE;
  }

  sprintf( tmpname, "%s.rom", fname );
  f = SDL_RWFromFile( tmpname, "rb" );
  if( !f )
  {
    error_printf( "Unable to open '%s'\n", tmpname );
    free( tmpname );
    return SDL_FALSE;
  }

  if( size < 0 )
  {
    int filesize;
    SDL_RWseek( f, 0, SEEK_END );
    filesize = SDL_RWtell( f );
    SDL_RWseek( f, 0, SEEK_SET );

    if( filesize > -size )
    {
      error_printf( "ROM '%s' exceeds %d bytes.\n", fname, -size );
      SDL_RWclose( f );
      free( tmpname );
      return SDL_FALSE;
    }

    where += (-size)-filesize;
    size = filesize;
  }

  if( SDL_RWread( f, where, size, 1 ) != 1 )
  {
    error_printf( "Unable to read '%s'\n", tmpname );
    SDL_RWclose( f );
    free( tmpname );
    return SDL_FALSE;
  }

  SDL_RWclose( f );
  free( tmpname );
  return SDL_TRUE;
}

struct twilighte * twilighte_oric_init(void)
{

  FILE *f;
  unsigned int i, j;
  char *result;
  char tbtmp[32];
  char tbtmpram[32];
  char line[1024];
  struct twilighte *twilighte = malloc(sizeof(struct twilighte));
  twilighte->t_banking_register=0;
  twilighte->t_register=128+1; // Firmware

  f = fopen( "plugins/twilighte_card/twilighte.cfg", "r" );
  if( !f )
  {
    error_printf( "plugins/twilighte_card/twilighte.cfg not found\n" );
    return NULL;
  }

  for( j=1; j<32; j++ )
  {
    twilighte->twilrombankfiles[j][0]=0;
    twilighte->twilrambankfiles[j][0]=0;
  }

  while( !feof( f ) )
  {
    result=fgets( line, 1024, f );
    for( j=1; j<32; j++ )
    {
      if (j > 9)
      {
        sprintf(tbtmp, "twilbankrom%d", j);
        sprintf(tbtmpram, "twilbankram%d", j);
      }
      else
      {
       sprintf(tbtmp, "twilbankrom0%d", j);
       sprintf(tbtmpram, "twilbankram0%d", j);
      }
        
      if (twilighte->twilrombankfiles[j][0]==0) 
      {
        if( read_config_string( line, tbtmp, twilighte->twilrombankfiles[j], 1024 ) ) 
        {
          break;
        }
      }
      if (twilighte->twilrambankfiles[j][0] == 0)
      {
          if (read_config_string(line, tbtmpram, twilighte->twilrambankfiles[j], 1024))
          {
              break;
          }
      }
	}
  }

  fclose( f );

  // Now load rom 
  for( i=0; i<32; i++ )
  {
    if( twilighte->twilrombankfiles[i][0]!=0 )
    { 
      if( !load_rom_twilighte( twilighte->twilrombankfiles[i], -16384, twilighte->twilrombankdata[i]  ) )
      {
        error_printf("Cannot load %s",twilighte->twilrombankfiles[i]);
        return NULL;
      }
	}
    if (twilighte->twilrambankfiles[i][0] != 0)
    {
        if (!load_rom_twilighte(twilighte->twilrambankfiles[i], -16384, twilighte->twilrambankdata[i]))
        {
            error_printf("Cannot load %s", twilighte->twilrambankfiles[i]);
            return NULL;
        }
    }
  }

  for (j=0;j<16384;j++) twilighte->twilrambankdata[0][j]=0;   
   
  twilighte->DDRA=7+128+32;
  twilighte->VDDRA=7;
  twilighte->current_bank=7;
  twilighte->DDRB=0;
  return  twilighte;
}

unsigned char 	twilighteboard_oric_ROM_RAM_read(struct twilighte *twilighte, uint16_t addr) {
  unsigned char data;
  unsigned char bank;

  if (twilighte->current_bank==0) 
  {
	  data=twilighte->twilrambankdata[0][addr];
  }
  else
  {
    if (twilighte->current_bank<5) 
    {
      if (twilighte->t_banking_register!=0)
        bank=twilighte->current_bank+8-1+((twilighte->t_banking_register-1)*4);
      else
        bank=twilighte->current_bank;
    }
    else
      bank=twilighte->current_bank;

    if ((twilighte->t_register&32)==32 && twilighte->current_bank<5) // Is it a ram bank access ?
      data=twilighte->twilrambankdata[bank][addr];
    else
      data=twilighte->twilrombankdata[bank][addr];
  }
	return data;
}

unsigned char 	twilighteboard_oric_ROM_RAM_write(struct twilighte *twilighte, uint16_t addr, unsigned char data)
{

  unsigned char bank;
  if (twilighte->current_bank==0)
  {
	  twilighte->twilrambankdata[twilighte->current_bank][addr]=data;
  }
  else
  {
    if (twilighte->current_bank<5) 
	  {
      if (twilighte->t_banking_register!=0)
        bank=twilighte->current_bank+8-1+((twilighte->t_banking_register-1)*4);
      else
        bank=twilighte->current_bank;
	  }
	  else
	    bank=twilighte->current_bank;

  	if ((twilighte->t_register&32)==32 && twilighte->current_bank<5) // Is it a ram bank access ?
	    twilighte->twilrambankdata[bank][addr]=data;
		// ROM is readonly
  }
  return 0;
}


unsigned char 	twilighteboard_oric_read(struct twilighte *twilighte, uint16_t addr)
{
  if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_REGISTER)
  {
	  return twilighte->t_register;
  }

  if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_VDDRA)
  {
	  return twilighte->VDDRA;
  }

  if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_DDRA)
  {
	  return twilighte->DDRA;
  }

  if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_VDDRB)
  {
  	return twilighte->VDDRB;;
  }

  if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_DDRB)
  {
    unsigned char data;
    data=twilighte->VDDRB;
    //twilighte->DDRB=twilighte->DDRB|1|2|4|8|16;
  	return data;

  }

  if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_BANKING_REGISTER)
  {
	  return twilighte->t_banking_register;
  }

  return 0;
}

unsigned char 	twilighteboard_oric_write(struct twilighte *twilighte, uint16_t addr, unsigned char mask, unsigned char data)
{
  if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_VDDRA)
  {
	  twilighte->VDDRA=data;
  }

  if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_DDRA)
  {

    if (mask==0) 
    { 
        twilighte->DDRA=data&(128+32+7);
    }
    if (mask==0xff)
        twilighte->DDRA = twilighte->DDRA&data;
    twilighte->current_bank=data&7;
  }

  if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_VDDRB)
  {
	  twilighte->VDDRB=data;
  }

  if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_DDRB)
  {


    if (mask==0x00)
    { 
      unsigned char lastpb6;
      unsigned char lastpb7;

      data=data&0b11011111;
      if (data==0) 
      {
        twilighte->DDRB=0;
        return 0;
      }

      lastpb6=twilighte->DDRB&64;
      lastpb7=twilighte->DDRB&128;


      if (lastpb6 && data==64) return 0;
      if (lastpb7 && data==128) return 0;
      if (lastpb7 && lastpb6 && data==128+64) return 0;

      if (!lastpb6 && data==64) twilighte->DDRB=twilighte->DDRB+64;
      if (!lastpb7 && data==128) twilighte->DDRB=twilighte->DDRB+128;
      if (!lastpb7 && !lastpb6 && data==128+64) twilighte->DDRB=128+64;

      /*
      twilighte->DDRB=data&(255);

      lastpb6=twilighte->DDRB&64;
      lastpb7=twilighte->DDRB&128;

      if (lastpb6 || lastpb7)
      */
        twilighte->DDRB=twilighte->DDRB|1|2|4|8|16;

    }




  }

  if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_REGISTER)
  {
	  twilighte->t_register=data;
  }

  if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_BANKING_REGISTER)
  {
    twilighte->t_banking_register=data;
  }

  return 0;
}
