
enum
{
  TZ_MONITOR = 0,
  TZ_REGS,
  TZ_VIA,
  TZ_AY,
  TZ_MENU,
  TZ_FILEREQ,
  TZ_LAST
};

struct textzone
{
  int x, y, w, h;
  int px, py, cfc, cbc;
  unsigned char *tx;
  unsigned char *fc, *bc;
};

#define OSDMENUBAR ((char *)-1)

struct osdmenuitem
{
  char *name;
  void (*func)(struct machine *,struct osdmenuitem *,int);
  int arg;
};

struct osdmenu
{
  char *title;
  int citem;
  struct osdmenuitem *items;
};

#define VSPTMPSIZE 1024

void do_popup( char *str );
void makebox( struct textzone *ptz, int x, int y, int w, int h, int fg, int bg );
void tzstr( struct textzone *ptz, char *text );
void tzstrpos( struct textzone *ptz, int x, int y, char *text );
void tzsetcol( struct textzone *ptz, int fc, int bc );
void tzprintf( struct textzone *ptz, char *fmt, ... );
void tzprintfpos( struct textzone *ptz, int x, int y, char *fmt, ... );
void draw_textzone( struct textzone *ptz );
void printstr( int x, int y, Uint16 fc, Uint16 bc, char *str );
void gotomenu( struct machine *oric, struct osdmenuitem *mitem, int menunum );
SDL_bool menu_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender );
void setmenutoggles( struct machine *oric );

void render( void );
void preinit_gui( void );
SDL_bool init_gui( void );
void shut_gui( void );
