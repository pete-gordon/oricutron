Oricutron 1.1
-------------

(c)2009-2013 Peter Gordon (pete@petergordon.org.uk)

This is a work in progress.


Current status
==============

  6502:  100% done (apart from any unknown bugs :)
  VIA:   95% done.
  AY:    99% done.
  Video: 100% done
  Tape:  99% done (.TAP, .ORT and .WAV supported)
  Disk:  Reading/Writing sectors works. No track read/write.



Credits
=======

  Programming
  -----------

  Peter Gordon


  Additional Programming
  ----------------------

  Francois Revol
  Alexandre Devert
  Stefan Haubenthal
  Ibisum
  Kamel Biskri
  Iss


  Amiga & Windows ports
  ---------------------

  Peter Gordon


  MacOS X port
  ------------

  Francois Revol
  Kamel Biskri


  MorphOS & AROS ports
  --------------------

  Stefan Haubenthal


  Linux port
  ----------

  Francois Revol
  Ibisum
  Alexandre Devert


  Pandora port
  ------------

  Ibisum


  Pravetz disk support
  --------------------

  Iss




Thanks
======

Thanks to DBug and Twilighte for letting me distribute their demos and
games with Oricutron.

Thanks to DBug, Twilighte, Chema, kamelito, Yicker, JamesD, Algarbi, ibisum,
jede, thrust26 and everyone else for their help and feedback!



AVI export notes
================

The AVI export uses the MRLE codec. Your favourite player might not support
it, but MPlayer plays it, ffmpeg converts it and you can upload it directly
to youtube.

Note that the MRLE codec shows up some endian-issues on the Amiga OS4 port
of MPlayer, so it will sound crappy and have wrong colours until those bugs
are fixed :-(


Command line
============

You can specify certain options on the command line. All options have
both short and long versions. For example:

  -mblah

  or

  --machine blah

Is the same thing. Note that the short version doesn't have a space, but
the long version does.

Here are all the options:


  -m / --machine     = Specify machine type. Valid types are:

                       "atmos" or "a" for Oric atmos
                       "oric1" or "1" for Oric-1
                       "o16k" for Oric-1 16k
                       "telestrat" or "t" for Telestrat
                       "pravetz", "pravetz8d" or "p" for Pravetz 8D

  -d / --disk        = Specify a disk image to use in drive 0
  -t / --tape        = Specify a tape image to use
  -k / --drive       = Specify a disk drive controller. Valid types are:

                       "microdisc" or "m" for Microdisc
                       "jasmin" or "j" for Jasmin

  -s / --symbols     = Load symbols from a file
  -f / --fullscreen  = Run oricutron fullscreen
  -w / --window      = Run oricutron in a window
  -R / --rendermode  = Render mode. Valid modes are:

                       "soft" for software rendering
                       "opengl" for OpenGL

  -b / --debug       = Start oricutron in the debugger
  -r / --breakpoint  = Set a breakpoint
  -h / --help        = Print command line help and quit

  --turbotape on|off = Enable or disable turbotape
  --lightpen on|off  = Enable or disable lightpen
  --vsynchack on|off = Enable or disable VSync hack
  --scanlines on|off = Enable or disable scanline simulation

NOTE: If you are not sure what machine or drive type is required for a disk or
tape image, just pass the filename without any options and Oricutron will
try and autodetect for you.


Examples:

oricutron tapes/tape_image.tap
oricutron disks/disk_image.dsk
oricutron --machine atmos --tape "tape files/foo.tap" --symbols "my files/symbols"
oricutron -m1 -tBUILD/foo.tap -sBUILD/symbols -b
oricutron --drive microdisc --disk demos/barbitoric.dsk --fullscreen
oricutron -ddemos/barbitoric.dsk -f
oricutron --turbotape off tapes/hobbit.tap



Keys
====

  In emulator
  -----------

  F1       - Bring up the menu
  F2       - Go to debugger/monitor
  F3       - Reset button (NMI)
  F4       - Hard reset
  Shift+F4 - Jasmin reset
  F5       - Toggle FPS
  F6       - Toggle warp speed
  F7       - Save all modified disks
  Shift+F7 - Save all modified disks to new disk images
  F9       - Save tape output
  F10      - Start/Stop AVI capture
  Help     - Show guide


  In menus
  --------

  Cursors   - Navigate
  Enter     - Perform option
  Backspace - Go back
  Escape    - Exit menus
  (or use the mouse)


  In Debugger/Monitor
  -------------------

  F2      - Return to the emulator
  F3      - Toggle console/debug output/memwatch
  F4      - Toggle VIA/AY/disk information
  F9      - Reset cycle count
  F10     - Step over code
  F11     - Step over code without tracing into
            subroutines.
  F12     - Skip instruction


  In the console:
  ---------------

  Up/Down - Command history


  In memwatch:
  ------------

  Up/Down           - Scroll (+shift for page up/down)
  Page Up/Page Down - Page up/down
  Hex digits        - Enter address
  S                 - Toggle split mode
  Tab               - Switch windows in split mode


Monitor instructions
====================

In the monitor, number arguments are decimal by default, or prefixed with $ for
hex or % for binary. Pretty much everything is output in hex.

In most places where you can enter a number or address, you can pass a CPU or
VIA register. (VIA registers are prefixed with V, e.g. VDDRA). Anywhere you can
pass an address, you can also use a symbol.

Commands:

  ?                     - Help
  a <addr>              - Assemble
  bc <bp id>            - Clear breakpoint
  bcm <bp id>           - Clear mem breakpoint
  bl                    - List breakpoints
  blm                   - List mem breakpoints
  bs <addr>             - Set breakpoint
  bsm <addr> [rwc]      - Set mem breakpoint
  bz                    - Zap breakpoints
  bzm                   - Zap mem breakpoints
  d <addr>              - Disassemble
  df <addr> <end> <file>- Disassemble to file
  m <addr>              - Dump memory
  mm <addr> <value>     - Modify memory
  mw <addr>             - Memory watch at addr
  nl <file>             - Load snapshot
  ns <file>             - Save snapshot
  r <reg> <val>         - Set <reg> to <val>
  q, x or qm            - Quit monitor
  qe                    - Quit emulator
  sa <name> <addr>      - Add or move user symbol
  sk <name>             - Kill user symbol
  sc                    - Symbols not case-sensitive
  sC                    - Symbols case-sensitive
  sl <file>             - Load user symbols
  sx <file>             - Export user symbols
  sz                    - Zap user symbols
  wm <addr> <len> <file>- Write mem to disk



Breakpoints
===========

There are two types of breakpoints. "Normal" breakpoints trigger when the CPU
is about to execute an instruction at the breakpoint address. "Memory" breakpoints
trigger when the breakpoint address is accessed or modified.

There are three ways a memory breakpoint can be triggered; when the CPU is about
to read the address (r), and the CPU is about to write the address (w), or after the
value at the address changes for any reason (c).

You specify which ways you'd like the breakpoint to trigger when you set the memory
breakpoint:

bsm r $0c00        <-- Break when the CPU is about to read from $0c00
bsm rw $0c00       <-- Break when the CPU is about to access $0c00
bsm c $0c00        <-- Break after then contents of $0c00 change
bsm rwc $0c00      <-- Break just before the CPU accesses $0c00, or just after it
                       changes for any reason.
