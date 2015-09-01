# Introduction #

This page describes how to compile Oricutron for Ubuntu (instructions correct for Debian unstable)

  * Install SDL, GTK and OpenGL developer files with (as root):
`apt-get install libsdl-dev libgtk-3-dev libgl1-mesa-dev`
  * Make sure you have the GNU C++ compiler support installed (as root):
`apt-get install g++`
  * Either download and extract a source package from the [download page](http://www.petergordon.org.uk/oricutron/), or [check out](http://code.google.com/p/oriculator/source/checkout) the current subversion trunk.
  * Open a shell, and navigate to the Oricutron sourcecode
  * Type:
`make`

Hopefully, an Oricutron executable should come out the other end.