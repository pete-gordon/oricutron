Oriculator 0.0.2
----------------

All code (c)2009 Peter Gordon (pete@petergordon.org.uk)

This is a very early preview of my upcoming portable Oric emulator. Every
part of this emulator has been written from scratch.

It is not really ready for release, I just thought i'd upload a preview for
people to look at.

Current status:

  6502:  100% done (apart from any unknown bugs :)
  VIA:   All done except shift register (which is on my todo list)
  AY:    I/O done. Sound works, but isn't great.
  Video: Almost done. 60Hz mode missing.
  Tape:  ".TAP" file support (no WAV yet)
  Disk:  Not a single line of code yet.

The Telestrat is not emulated at all yet. Also, "Turbo Tape" only works in
Atmos mode. Tape noise only works if Turbo Tape is disabled.


Keys:

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
  F4      - Toggle console/debug output/memwatch
  F3      - Toggle VIA/AY information
  F9      - Reset cycle count
  F10     - Step over code

  In the console:
  ---------------

  Up/Down - Command history


  In memwatch:
  ------------

  Up/Down           - Scroll (+shift for page up/down)
  Page Up/Page Down - Page up/down
  Hex digits        - Enter address


Monitor instructions
--------------------

In the monitor, number arguments are decimal by default, or prefixed with $ for
hex or % for binary. Pretty much everything is output in hex.

In most places where you can enter a number or address, you can pass a CPU or
VIA register. (VIA registers are prefixed with V, e.g. VDDRA).

Commands:

  bs <address>               - Set breakpoint
  bc <bp id>                 - Clear breakpoint
  bz                         - Zap breakpoints
  bl                         - List breakpoints
  m <address>                - Dump memory
  d <address>                - Disassemble
  r <reg> <value>            - Set register <reg> to <value>
  q, x or qm                 - Quit monitor (back to emulator)
  qe                         - Quit emulator
  wm <addr> <len> <filename> - Write Oric memory to disk (as seen from the 6502).
