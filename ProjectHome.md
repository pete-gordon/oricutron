## Oricutron ##

Oricutron (formerly known as Oriculator) is an emulator for the Oric series of computers, and the Pravetz 8D. It is written in plain C, and uses SDL. It is designed to be portable.

If you want to discuss Oricutron, or Oric in general, why not join us in #oric on IRCNet?

## Downloads ##

Google are removing the download functionality from Google Code! From now on, downloads will be hosted here:

http://www.petergordon.org.uk/oricutron/

## Thanks ##

Thanks to ISS for the awesome work on Pravetz 8D and serial emulation!

Thanks to Stefan Haubenthal, Kamel Biskri, Francois Revol, Alexandre Devert, Ibisum, Patrice Torguet, and Christian for their contributions and ports!

Thanks to DBug and Twilighte for letting me distribute their Oric software with Oricutron.


## Latest Release ##

Oricutron v1.2 was released on the 1st November 2014.

### Changes since v1.1: ###

All:

  * Fixed memory access breakpoints which were broken in (at least) v1.1.
  * Added snapshot files to the filetype autodetection
  * Added a virtual on-screen keyboard, and the ability to remap keys `<torguet`>
  * Fixed autobooting of Jasmin disks `<christian`>
  * ACIA 6551 serial port emulation, including a virtual modem which lets you connect over TCP/IP `<iss`>
  * Fixed a bug in the V flag emulation for SBC/ADC `<christian`>
  * Detects invalid .tap images (encoded length larger than tape image length)


MacOS X:

  * Corrected a bug which prevented opening of the save box for snapshots. `<torguet`>
  * Works on OS X 10.9 `<torguet`>
  * Added logos for retina displays `<torguet`>
  * .wav and .ort tapes show up in the tape file requesters `<torguet`>


Amiga:

  * Compiles for AmigaOS 2.x `<stefan h`>


Haiku:

  * Added HVIF icon `<revolf`>


Linux:

  * Added copy to clipboard `<iss`>


Windows:

  * Added copy to clipboard `<iss`>


### Notes about AVI capture ###

The AVI export uses the MRLE codec. Your favourite player might not support
it, but MPlayer plays it, ffmpeg converts it and you can upload it directly
to youtube.