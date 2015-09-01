# Introduction #

This page describes how to compile Oricutron for Windows with [MinGW](http://www.mingw.org/).

Note that this method involves just lazily copying the libSDL files right into your MinGW installation, which isn't necessarily the right way to do it (its just the easiest). If you want to do it properly, there are several guides online if you google "SDL MinGW".


  * Download the latest MinGW installer from the [MinGW sourceforge site](http://sourceforge.net/projects/mingw/files/).
  * Run the installer. Pick any components you like, but the only components you strictly need for Oricutron are the "C compiler" and "C++ compiler".
  * Download the latest SDL development libraries for MinGW from [the libSDL 1.2 download page](http://www.libsdl.org/download-1.2.php).
  * Navigate to the extracted SDL folder (so that you can see "bin", and "include" directories for example).
  * Hit Ctrl+A to select all files and Ctrl+C to copy
  * Navigate to the MinGW dir (C:\MinGW by default)
  * Hit Ctrl+V to paste. Let it merge the contents of directories
  * Go to your environmental variables
    * For Windows XP, hit Windows+Break, select the advanced tab, and click "Environment Variables"
    * For Windows 7, hit Windows+Break, click "Advanced system settings", then click "Environment variables"
    * Add the MinGW bin directory to your path variable (defaults to "C:\MinGW\bin")
  * Either download and extract a source package from the [download page](http://www.petergordon.org.uk/oricutron/), or [check out](http://code.google.com/p/oriculator/source/checkout) the current subversion trunk.
  * Open a command prompt, and navigate to the Oricutron sourcecode
  * Type "`mingw32-make`". On Windows XP, you will probably have to type "`mingw32-make PLATFORM=win32`" to override the platform autodetection. On Windows 7 64 bits you need to type "`mingw32-make PLATFORM=win32 HOSTOS=win32`" to override the platform autodetection and to let make know that even though HOSTOS is win64 it is compatible with win32.

Hopefully, an Oricutron executable should come out the other end.