# Introduction #

This page describes how to compile Oricutron for Ubuntu (instructions correct for Ubuntu 12.04)

  * Install SDL and GTK developer files with:
`sudo apt-get install libsdl-dev libgtk-3-dev`
  * Make sure you have the GNU C++ compiler support installed:
`sudo apt-get install g++`
  * Either download and extract a source package from the [download page](http://www.petergordon.org.uk/oricutron/), or [check out](http://code.google.com/p/oriculator/source/checkout) the current subversion trunk.
  * Open a shell, and navigate to the Oricutron sourcecode
  * Type:
`make`

Hopefully, an Oricutron executable should come out the other end.