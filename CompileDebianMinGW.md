# Introduction #

This page describes how to cross-compile Oricutron for Windows with [MinGW](http://www.mingw.org/) under Debian (instructions correct for Debian unstable).

  * Install MinGW cross-compiler with (as root):
`apt-get install mingw32`
  * Download the latest SDL development libraries for MinGW from [the libSDL 1.2 download page](http://www.libsdl.org/download-1.2.php).
  * Install the SDL development libraries to /usr/local/i586-mingw32msvc/SDL-1.2.15 FIXME!
  * Either download and extract a source package from the [download page](http://www.petergordon.org.uk/oricutron/), or [check out](http://code.google.com/p/oriculator/source/checkout) the current subversion trunk.
  * Open a shell, and navigate to the Oricutron sourcecode
  * To compile we must override the platform autodetection and pass over the path to SDL headers and libs. Type:
`make PLATFORM=win32 SDL_PREFIX=/usr/local/i586-mingw32msvc/SDL-1.2.15`

Hopefully, an Oricutron executable should come out the other end.