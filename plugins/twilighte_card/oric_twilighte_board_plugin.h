
struct twilighte * twilighte_oric_init(void);

unsigned char twilighteboard_oric_ROM_RAM_read(struct twilighte *twilighte, uint16_t addr) ;
unsigned char twilighteboard_oric_ROM_RAM_write(struct twilighte *twilighte, uint16_t addr, unsigned char data);

unsigned char twilighteboard_oric_write(struct twilighte *twilighte, uint16_t addr,unsigned char mask, unsigned char data);
unsigned char twilighteboard_oric_read(struct twilighte* twilighte, uint16_t addr);
