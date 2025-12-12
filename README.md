# pngdec.ldg

Library using the LDG system and the PNG decoder functions from LIBPNG.

Used by:

* MapEdit to load and save tiles into APNG as containor.
* MAKE_MAP to slice large PNG into tiles and map array.

Other programs can use it, please read these sources and P2SM GFA source.

# installation for makefiles

- pre-requisite: different targets of libldg.a in /opt/cross-mint/m68k-atari-mint/lib/

- in an empty folder,  
   ```mkdir ./build/68000```  
   ```mkdir ./build/68020```  
   ```mkdir ./build/ColdFire```  

- get /pngdec.ldg/ from [codecs_r4_src.zip](https://ptonthat.fr/files/pngdec/codecs_r4_src.zip) and unpack the contents to ./

- pngdec.ldg.xcodeproj is for Xcode 26.1, you may not need it if you use something else.
