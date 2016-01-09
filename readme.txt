-------------------------------------------------------------------
Contents of this package
-------------------------------------------------------------------

repository: https://github.com/cho-dev/textmovie.git

######## Document
how_to_build(jpn).txt  ===> How to build textmovie.exe (japanese)
Eula(GPLv3).txt   ===> license (GPL version3)
readme.txt  ===> this file

######## source file of textmovie
audiowave.c
audiowave.h
framebuffer.c
framebuffer.h
playlist.c
playlist.h
textmovie.c
textscreen.c
textscreen.h
version.h

appiconset.ico  ===> application icon data
resource.rc  ===> resource file
buildcount.sh  ===> build count utility
Makefile  ===> Makefile

textmovie.ini  ===> sample .ini file for textmovie.exe


-------------------------------------------------------------------
License
-------------------------------------------------------------------
GPL version3 or later. see Eula(GPLv3).txt


-------------------------------------------------------------------
Other source file to make binary
-------------------------------------------------------------------
Binary textmovie.exe(build 423) uses following library.

FFmpeg: (ffmpeg2.5 or later)
ffmpeg-2.5.4.tar.bz2 from http://ffmpeg.org/

SDL: (SDL1.2.15)
SDL-1.2.15.tar.gz from https://www.libsdl.org/

