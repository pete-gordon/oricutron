
#define TWILIGHTE_CARD_ORIC_EXTENSION_MIRROR_314        0x314

#define TWILIGHTE_CARD_ORIC_EXTENSION_DDRA              0x323
#define TWILIGHTE_CARD_ORIC_EXTENSION_IORAh             0x321

#define TWILIGHTE_CARD_ORIC_EXTENSION_DDRB              0x322
#define TWILIGHTE_CARD_ORIC_EXTENSION_IORB              0x320

#define TWILIGHTE_CARD_ORIC_EXTENSION_REGISTER          0x342
#define TWILIGHTE_CARD_ORIC_EXTENSION_BANKING_REGISTER  0x343

/*
 * Port B
 * 7 6 5 4 3 2 1 0
 *       U D F L R
 *   +-> select left port
 * +---> select right port
 *
 * Port A
 * 7 6 5 4 3 2 1 0
 *           +---+---> Bank #
 *     +--> Fire 3
 * +------> Fire 2
 *
 * DDRx: bit à 0 => bit en entrée
 *       bit à 1 => bit en sortie
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../system_sdl.h"
SDL_bool read_config_int(char* buf, char* token, int* dest, int min, int max);
SDL_bool read_config_bool(char* buf, char* token, SDL_bool* dest);




struct twilbankinfo
{
    unsigned char type;
    unsigned char* ptr;
};

struct twilighte
{

    unsigned char t_register;
    unsigned char t_banking_register;
    char twilrombankfiles[32][1024];

    char twilrambankfiles[32][1024];

    unsigned char twilrombankdata[32][16834];
    unsigned char twilrambankdata[32][16834];

    int firmware_version;

    unsigned char mirror_0x314;

    unsigned char DDRA;
    unsigned char IORAh;

    unsigned char DDRB;
    unsigned char IORB;
    unsigned char current_bank;

};





#include "../../system.h"

extern SDL_bool read_config_string(char* buf, char* token, char* dest, Sint32 maxlen);
///extern SDL_bool load_rom( struct machine *oric, char *fname, int size, unsigned char *where, struct symboltable *stab, int symflags );

static SDL_bool load_rom_twilighte(char* fname, int size, unsigned char where[])
{
    SDL_RWops* f;
    char* tmpname;

    // MinGW doesn't have asprintf :-(
    tmpname = malloc(strlen(fname) + 10);
    if (!tmpname) {

        return SDL_FALSE;
    }

    sprintf(tmpname, "%s.rom", fname);
    f = SDL_RWFromFile(tmpname, "rb");
    if (!f)
    {
        error_printf("Unable to open '%s'\n", tmpname);
        free(tmpname);
        return SDL_FALSE;
    }

    if (size < 0)
    {
        int filesize;
        SDL_RWseek(f, 0, SEEK_END);
        filesize = SDL_RWtell(f);
        SDL_RWseek(f, 0, SEEK_SET);

        if (filesize > -size)
        {
            error_printf("ROM '%s' exceeds %d bytes.\n", fname, -size);
            SDL_RWclose(f);
            free(tmpname);
            return SDL_FALSE;
        }

        where += (-size) - filesize;
        size = filesize;
    }

    if (SDL_RWread(f, where, size, 1) != 1)
    {
        error_printf("Unable to read '%s'\n", tmpname);
        SDL_RWclose(f);
        free(tmpname);
        return SDL_FALSE;
    }

    SDL_RWclose(f);
    free(tmpname);
    return SDL_TRUE;
}

struct twilighte* twilighte_oric_init(void)
{

    FILE* f;
    unsigned int i, j;
    char* result;
    char tbtmp[32];
    char tbtmpram[32];
    char line[1024];
    struct twilighte* twilighte = malloc(sizeof(struct twilighte));
    twilighte->t_banking_register = 0;

    twilighte->t_register = 128 + 1; // Firmware

    f = fopen("plugins/twilighte_board/twilighte.cfg", "r");
    if (!f)
    {
        error_printf("plugins/twilighte_board/twilighte.cfg not found\n");
        return NULL;
    }

    for (j = 1; j < 32; j++)
    {
        twilighte->twilrombankfiles[j][0] = 0;
        twilighte->twilrambankfiles[j][0] = 0;
    }

    while (!feof(f))
    {
        result = fgets(line, 1024, f);
        if( result )
        {
          // FIXME: do something to silence the compiler warning ...
        }


        if (read_config_int(line, "twilighte_firmware", &twilighte->firmware_version, 1, 3))
        {
            twilighte->t_register = twilighte->t_register | twilighte->firmware_version;
            continue;
        }

        for (j = 1; j < 32; j++)
        {
            sprintf(tbtmp, "twilbankrom%02d", j);
            sprintf(tbtmpram, "twilbankram%02d", j);
            /*
                  if (j>9)
                  {
                      sprintf( tbtmp, "twilbankrom%d", j);
                      sprintf(tbtmpram, "twilbankram%d", j);
                  }
                  else
                  {
                    sprintf( tbtmp, "twilbankrom0%d", j);
                    sprintf(tbtmpram, "twilbankram0%d", j);
                  }
            */
            if (twilighte->twilrombankfiles[j][0] == 0)
            {
                if (read_config_string(line, tbtmp, twilighte->twilrombankfiles[j], 1024))
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

    fclose(f);

    // Now load rom
    for (i = 0; i < 32; i++)
    {
        if (twilighte->twilrombankfiles[i][0] != 0)
        {
            //error_printf("Load bank (ROM) #%d: %s", i, twilighte->twilrombankfiles[i]);
            if (!load_rom_twilighte(twilighte->twilrombankfiles[i], -16384, twilighte->twilrombankdata[i]))
            {
                error_printf("Cannot load %s", twilighte->twilrombankfiles[i]);
                return NULL;
            }
        }

        if (twilighte->twilrambankfiles[i][0] != 0)
        {
            //error_printf("Load bank (RAM) #%d: %s", i, twilighte->twilrambankfiles[i]);
            if (!load_rom_twilighte(twilighte->twilrambankfiles[i], -16384, twilighte->twilrambankdata[i]))
            {
                error_printf("Cannot load %s", twilighte->twilrambankfiles[i]);
                return NULL;
            }
        }

    }
    /*

    */
    // Flush sram (first bank)
    for (j = 0; j < 16384; j++) twilighte->twilrambankdata[0][j] = 0;

    twilighte->IORAh = 0x07;
    twilighte->DDRA = 0b10100111;
    twilighte->current_bank = 7;
    twilighte->IORB = 0;
    twilighte->DDRB = 0b11000000;

    if (twilighte->firmware_version==2)
        twilighte->mirror_0x314=2;


    return  twilighte;
}

unsigned char 	twilighteboard_oric_ROM_RAM_read(struct twilighte* twilighte, uint16_t addr) {
    unsigned char data;
    unsigned char bank;

    if (twilighte->firmware_version==2)
    {
        // If bit 1 of $314 is equal to 0 (romdis low), then send bank 0 value
        if ((twilighte->mirror_0x314&2)==0) // Test bit 0
        {
            data = twilighte->twilrambankdata[0][addr];
            return data;
        }
        // If bit 2 is equal to 1, then continue and current_bank is the right value to get bank data.
    } // If it's not firmware version 2, continue

    if (twilighte->current_bank == 0)
    {
        data = twilighte->twilrambankdata[0][addr];
    }
    else
    {
        if (twilighte->current_bank < 5)
        {
            if (twilighte->t_banking_register != 0)
                bank = twilighte->current_bank + 8 + ((twilighte->t_banking_register - 1) * 4);
            else
                bank = twilighte->current_bank;
        }
        else
            bank = twilighte->current_bank;

        if ((twilighte->t_register & 32) == 32 && twilighte->current_bank < 5) // Is it a ram bank access ?
            data = twilighte->twilrambankdata[bank][addr];
        else
            data = twilighte->twilrombankdata[bank][addr];
    }
    return data;
}

unsigned char 	twilighteboard_oric_ROM_RAM_write(struct twilighte* twilighte, uint16_t addr, unsigned char data)
{

    unsigned char bank;

    if (twilighte->firmware_version==2)
    {
        // If bit 1 of $314 is equal to 0 (romdis low), then send bank 0 value
        if ((twilighte->mirror_0x314&2)==0) // Test bit 0
        {
            twilighte->twilrambankdata[twilighte->current_bank][addr] = data;
            return 0;
        }
        // If bit 2 is equal to 1, then continue and current_bank is the right value to get bank data.
    } // If it's not firmware version 2, continue


    if (twilighte->current_bank == 0)
    {
        twilighte->twilrambankdata[twilighte->current_bank][addr] = data;
    }
    else
    {
        if (twilighte->current_bank < 5)
        {
            if (twilighte->t_banking_register != 0)
                bank = twilighte->current_bank + 8 + ((twilighte->t_banking_register - 1) * 4);
            else
                bank = twilighte->current_bank;
        }
        else
            bank = twilighte->current_bank;

        if ((twilighte->t_register & 32) == 32 && twilighte->current_bank < 5) // Is it a ram bank access ?
            twilighte->twilrambankdata[bank][addr] = data;
        // ROM is readonly
    }
    return 0;
}


unsigned char 	twilighteboard_oric_read(struct twilighte* twilighte, uint16_t addr)
{
    if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_REGISTER)
    {
        return twilighte->t_register;
    }

    if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_DDRA)
    {
        return twilighte->DDRA;
    }

    if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_IORAh)
    {
        return twilighte->IORAh;
    }

    if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_DDRB)
    {
        return twilighte->DDRB;
    }

    if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_IORB)
    {
        return twilighte->IORB;
    }

    if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_BANKING_REGISTER)
    {
        return twilighte->t_banking_register;
    }

    return 0;
}

unsigned char 	twilighteboard_oric_write(struct twilighte* twilighte, uint16_t addr, unsigned char mask, unsigned char data)
{
    // mask: $00 => lecture/écriture via
    // mask: $ff => mise à jour du port joystick

    if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_DDRA)
    {
        twilighte->DDRA = data;
    }

    if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_IORAh)
    {

        if (mask == 0)
            twilighte->IORAh = data & twilighte->DDRA;
        else
            twilighte->IORAh = (twilighte->IORAh & twilighte->DDRA) | (data & (twilighte->DDRA ^ 0xff));

        twilighte->current_bank = twilighte->IORAh & 7;
    }

    if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_DDRB)
    {
        twilighte->DDRB = data;
    }

    if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_IORB)
    {
        if (mask == 0)
            twilighte->IORB = data & twilighte->DDRB;
        else
            twilighte->IORB = (twilighte->IORB & twilighte->DDRB) | (data & (twilighte->DDRB ^ 0xff));
    }

    if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_REGISTER)
    {
        twilighte->t_register = data;
    }

    if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_BANKING_REGISTER)
    {
        twilighte->t_banking_register = data;
    }

    if (twilighte->firmware_version==2)
    {
        if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_MIRROR_314)
        {
            twilighte->mirror_0x314 = data;
        }
    }

    return 0;
}
