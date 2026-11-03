# pngdec.ldg

Library using the LDG system and the PNG decoder functions from LIBPNG.

Used by:

* MapEdit to load and save tiles into APNG as containor.
* MAKE_MAP to slice large PNG into tiles and map array.

Other programs can use it, please read these sources and P2SM GFA source.

# installation for makefiles

- pre-requisite: different targets of libldg.a in /opt/cross-mint/m68k-atari-mint/lib/

- get /pngdec.ldg/ from [codecs_r5_src.zip](https://ptonthat.fr/files/pngdec/codecs_r5_src.zip) and unpack the contents to ./

- install [libcmini/lib/](https://github.com/freemint/libcmini/releases/) a files on a location outside /opt/cross-mint/m68k-atari-mint/lib/ (ie in your workspace besides other projects) and fix $CMINI path in the Makefile.

- pngdec.ldg.xcodeproj is for Xcode 26.3, you may not need it if you use something else.
