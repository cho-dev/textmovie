#
# textmovie makefile (for Windows)
#     by Coffey (c)2015  version 20150815,  (since 20150218)
#
######## tool list
CC        = gcc
STRIP     = strip
RESC      = windres

BUILDCOUNT = buildcount.sh

######### executable and source list
PROGS     = textmovie.exe
PROGSG    = textmovie_g.exe
SRCS      = textmovie.c textscreen.c framebuffer.c playlist.c audiowave.c
#SRCS      = $(wildcard *.c)
HEADERS   = textscreen.h framebuffer.h playlist.h audiowave.h
RESOURCE  = resource.rc
VERSIONFILE = version.h

######### object and library list
OBJS     := $(subst .c,.o,$(SRCS))
RESOBJ   := $(subst .rc,.o,$(RESOURCE))
OBJS     += $(RESOBJ)

# ffmpeg basic library
LDLIBS   += -lavdevice -lavformat -lavfilter -lavcodec \
            -lswresample -lswscale -lpostproc -lavutil

# ffmpeg external library
#LDLIBS   += -lbs2b -lfreetype -lfdk-aac -lvo-aacenc -lmp3lame \
#            -lvorbis -logg -lvorbisenc -lopus -lx264 -lx265
#            -lwebp -lopencore-amrnb -lopencore-amrwb -lvo-amrwbenc

# SDL library (mingw32: need -lmingw32 before SDL to link WinMain)
LDLIBS   += -lmingw32 -lSDLmain -lSDL

# other library
LDLIBS   += -liconv -lz -lbz2 -lws2_32 -lpthread -lstdc++
LDLIBS   += -lwinmm -lm -luser32 -lgdi32 -ldxguid

######### option list
CFLAGS   += -O2 -Wall -std=c99
LDFLAGS  += -static -mconsole

#CFLAGS   += -Wall -std=c99 -coverage
#LDFLAGS  += -static -mconsole -coverage

######### rule list
.PHONY:     all clean all-g

$(PROGS):   $(OBJS)
			$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
			$(STRIP) $@
			./$(BUILDCOUNT)

$(PROGSG):  $(OBJS)
			$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
			./$(BUILDCOUNT)

textmovie.o: textmovie.c $(VERSIONFILE) $(HEADERS)
			$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c $(HEADERS)
			$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.rc $(VERSIONFILE)
			$(RESC) -i $< -o $@

all:        $(PROGS)

all-g:      $(PROGS) $(PROGSG)

clean:
			-rm -f $(OBJS) $(PROGS)
			-rm -f $(PROGSG)

