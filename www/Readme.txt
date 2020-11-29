// Web version of Oricutron

If you only want to host the provided version of Oricutron and some disks and tapes files :

Requirement: have PHP running on your website.

Copy the following files to your web directory
dir.php
index.html
Oricutron.data
oricutron.html
Oricutron.js
Oricutron.wasm

Add all your .tap and .dsk files in the same directory.

That's all folks!



If you want to compile your own version of Oricutron :

Requirements:

Install Emscripten SDK from https://emscripten.org/

https://emscripten.org/docs/getting_started/downloads.html

Note : you can use a docker image if you don't want to install everything 
(look at the end of the above linked page).

Cmake building :

; Create a build directory in the www directory :
cd www
mkdir build
; Change to this new directory
cd build
; Use emcmake and cmake to generate Makefile
emcmake cmake ..

Prepare your assets :

; create an assets directory
mkdir assets
; copy the images directory
; WARNING you'd better remove the gfx_atmoskbd.psd file as it is quite big and not used by Oricutron
cp -r ../../images assets/
; copy the roms directory (after adding the actual roms - not included in the git repository)
cp -r ../../roms assets/

Either :

; if you want to include a tap
; copy the tap
mkdir assets/tapes
cp ../../tapes/Cyclotron.tap assets/tapes

Or :

; if you want to include a dsk
; copy the dsk
mkdir assets/disks
cp ../../disks/aigledor.dsk assets/disks

Edit the configuration file (so that it autoloads the tape or the disk) :
You want to search for tapeimage and have it look like this :
tapeimage = 'assets/tapes/Cyclotron.tap'
Or you want to search for diskimage and have it look like this :
diskimage = 'assets/disks/aigledor.dsk'

You may also want to show the keyboard automatically and enable sticky mod keys 
(for the shift and ctrl keys) :
show_keyboard = yes
sticky_mod_keys = yes

Copy the configuration file :

cp ../../oricutron.cfg assets/

Building :

make

Note : if you want to change the configuration/tapes/disks you will need to do :

rm Oricutron.html
make


Testing the build :

emrun Oricutron.html 

If everything is okay :

You now need to move the following files to your website :
Oricutron.html    	->> the web page
Oricutron.js		->> the javascript launcher file
Oricutron.wasm		->> the web assembly (the machine code)
Oricutron.data		->> all the assets

If your data is very big you might want to look at the end of the following page
 to send it compressed to navigators :
https://emscripten.org/docs/compiling/WebAssembly.html


Reference documentation :

https://emscripten.org/index.html

https://lyceum-allotments.github.io/2016/06/emscripten-and-sdl-2-tutorial-part-1/

