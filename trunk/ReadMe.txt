Oriculator 0.0.x
----------------

All code (c)2009 Peter Gordon (pete@petergordon.org.uk)

This is a work in progress.


Current status:

  6502:  100% done (apart from any unknown bugs :)
  VIA:   All done except shift register (which is on my todo list)
  AY:    I/O done. Sound works, but isn't great.
  Video: Almost done. 60Hz mode missing.
  Tape:  ".TAP" file support (no WAV yet)
  Disk:  Microdisc reading is partially working. Jasmin is not done at all.

The Telestrat is not emulated at all yet. Also, "Turbo Tape" only works in
Atmos mode. Tape noise only works if Turbo Tape is disabled.



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


  -m / --machine    = Specify machine type. Valid types are:

                      "atmos" or "a" for Oric atmos
		      "oric1" or "1" for Oric 1
		      "o16k" for Oric 1 16k

  -d / --disk       = Specify a disk image to use in drive 0
  -t / --tape       = Specify a tape image to use
  -k / --drive      = Specify a disk drive controller. Valid types are:
  
                      "microdisc" or "m" for Microdisc
		      "jasmin" or "j" for Jasmin

  -s / --symbols    = Load symbols from a file


Examples:

oriculator --machine atmos --tape "tape files/foo.tap" --symbols "my files/symbols"
oriculator -m1 -tBUILD/foo.tap -sBUILD/symbols
oriculator --drive microdisc --disk demos/barbitoric.dsk
oriculator -ddemos/barbitoric.dsk



Keys
====

In emulator
-----------

  F1 - Bring up the menu
  F2 - Go to debugger/monitor
  F5 - Toggle FPS
  F6 - Toggle warp speed


In menus
--------

  Cursors - Navigate
  Enter   - Perform option
  Escape  - Go back
  (or use the mouse)


In Debugger/Monitor
-------------------

  F2      - Return to the emulator
  F3      - Toggle console/debug output/memwatch
  F4      - Toggle VIA/AY information
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

  bs <address>               - Set breakpoint
  bsm <address> [rwc]        - Set mem breakpoint
  bc <bp id>                 - Clear breakpoint
  bcm <bp id>                - Clear mem breakpoint
  bz                         - Zap breakpoints
  bzm                        - Zap mem breakpoint
  bl                         - List breakpoints
  blm                        - List mem breakpoints
  m <address>                - Dump memory
  mw <address>               - Memory watch at <address>
  sc                         - Symbols are not case sensitive
  sC                         - Symbols are case sensitive
  sl <file>                  - Load symbols
  sz                         - Zap symbols
  d <address>                - Disassemble
  r <reg> <value>            - Set register <reg> to <value>
  q, x or qm                 - Quit monitor (back to emulator)
  qe                         - Quit emulator
  wm <addr> <len> <filename> - Write Oric memory to disk (as seen from the 6502).


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
