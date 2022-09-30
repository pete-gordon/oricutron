
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


void dbg_printf(char *fmt, ...);


#include "../../system.h"
#include "oric_twilighte_board_plugin.h"

extern SDL_bool read_config_string(char* buf, char* token, char* dest, Sint32 maxlen);

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

SDL_bool get_twilighte_board_microdisc_connection(struct twilighte* twilighte)
{
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
    twilighte->microdisc=SDL_FALSE;
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


        if (read_config_int(line, "firmware", &twilighte->firmware_version, 1, 3))
        {
            twilighte->t_register = twilighte->t_register | twilighte->firmware_version;
            continue;
        }


        if( read_config_bool(   line, "microdisc",     &twilighte->microdisc ) ) continue;

        for (j = 1; j < 32; j++)
        {
            sprintf(tbtmp, "twilbankrom%02d", j);
            sprintf(tbtmpram, "twilbankram%02d", j);

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


    twilighte->IORAh = 0x07;
    twilighte->DDRA = 0b10100111;
    twilighte->current_bank = 7;
    twilighte->save_current_bank=twilighte->current_bank;

    twilighte->IORB = 0;
    twilighte->DDRB = 0b11000000;

    // It's not really the behavior of the firmware 2, because it initialize 0X314 internal register of the twilighte board to 0
    if (twilighte->firmware_version==2)
        twilighte->mirror_0x314=2;

    return  twilighte;
}


unsigned char twilighte_board_mapping_bank(struct twilighte *twilighte) {
    unsigned char bank;
    // RAM Overlay?
    if (twilighte->current_bank == 0)
        bank = 7;

    // ROM
    else if ( (twilighte->current_bank >=5) && (twilighte->current_bank <=7) )
        bank = twilighte->current_bank-1;

    // ROM/RAM fonction du bit5 de twilighte->t_register
    else if (twilighte->t_banking_register < 4)
        bank = (twilighte->current_bank-1+twilighte->t_banking_register*8);

    // ROM/RAM fonction du bit5 de twilighte->t_register
    else if (twilighte->t_banking_register >= 4)
        bank = (twilighte->current_bank+3+(twilighte->t_banking_register & 3)*8);

    // Valeurs invalides
    else
        error_printf("Banking error: bank=%d, set=%d", twilighte->current_bank, twilighte->t_banking_register);

    // error_printf("Banking[0]: set=%d, bank=%d ram/rom=%x => %d", twilighte->t_banking_register, twilighte->current_bank, (twilighte->t_register >> 5) & 1, bank);
    return bank;
}

unsigned char 	twilighteboard_oric_ROM_RAM_read(struct twilighte* twilighte, uint16_t addr) {
    unsigned char data;
    unsigned char bank;


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
        /*
        if ((twilighte->mirror_0x314&2)==0) // Test bit 0
        {
            twilighte->twilrambankdata[twilighte->current_bank][addr] = data;
            return 0;
        }
        */
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

    // In firmware mode 2, $314 is not returned by the twilighte board
    // $314 is only writen in order to have twilighte board aware of the state of the overlay ram
    // It's returned by the disk controler
    // That is why, it never returns here $314 from the twilighte board regiter

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
            // If bit 1 of $314 is equal to 0 (romdis low), then send bank 0 value
            if ((twilighte->mirror_0x314&2)==0) // Test bit 1
            {
                twilighte->save_current_bank=twilighte->current_bank;
                twilighte->current_bank=0;
            }
            else
            {
                twilighte->current_bank=6;
            }
            // If bit 2 is equal to 1, then continue and current_bank is the right value to get bank data.
        } // If it's not firmware version 2, continue

    }

    return 0;
}
