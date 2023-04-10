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

    char twilrambankfiles[32][1024];

    unsigned char twilrombankdata[32][16834];
    unsigned char twilrambankdata[32][16834];

    int firmware_version;

    SDL_bool microdisc;

    unsigned char DDRA;
    unsigned char IORAh;

    unsigned char DDRB;
    unsigned char IORB;
    unsigned char current_bank;
    unsigned char mirror_0x314;
    unsigned char mirror_0x314_write_detected;
};

struct twilighte * twilighte_oric_init(void);

unsigned char twilighte_board_mapping_bank(struct twilighte *twilighte);
unsigned char twilighte_board_mapping_software_bank(struct twilighte *twilighte);

unsigned char twilighteboard_oric_ROM_RAM_read(struct twilighte *twilighte, uint16_t addr) ;
unsigned char twilighteboard_oric_ROM_RAM_write(struct twilighte *twilighte, uint16_t addr, unsigned char data);

unsigned char twilighteboard_oric_write(struct twilighte *twilighte, uint16_t addr,unsigned char mask, unsigned char data);
unsigned char twilighteboard_oric_read(struct twilighte* twilighte, uint16_t addr);


