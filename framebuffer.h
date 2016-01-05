/*
    framebuffer.h , part of textmovie (play movie with console. for Windows)
    Copyright (C) 2015-2016  by Coffey

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef FRAMEBUFFER_FRAMEBUFFER_H
#define FRAMEBUFFER_FRAMEBUFFER_H


// this flag is "freeable member 'void *data' by Framebuffer_Free()"
// if not set this flag, 'data' will not free automatically by Framebuffer_Free()
#define FRAMEBUFFER_FLAGS_DATA_FREEABLE 0x0001

#define FRAMEBUFFER_TYPE_VOID        0
#define FRAMEBUFFER_TYPE_AUDIO       1
#define FRAMEBUFFER_TYPE_VIDEO       2
#define FRAMEBUFFER_TYPE_AUDIOWAVE   3

#define FRAMEBUFFER_MAXBUFFER_AUDIO  32
#define FRAMEBUFFER_MAXBUFFER_VIDEO  8
#define FRAMEBUFFER_MAXBUFFER_VOID   8
#define FRAMEBUFFER_MAXBUFFER_AUDIOWAVE   8

typedef struct Framebuffer {
    struct Framebuffer *next;
    int64_t pts;
    int type;
    int size;
    int pos;
    int flags;
    int playnum;
    void *data;
} Framebuffer;

int Framebuffer_Init(void);
int Framebuffer_Uninit(void);
int Framebuffer_ListNum(int type);
int isFramebuffer_Full(int type);
int Framebuffer_Sendback(Framebuffer *buf);
int Framebuffer_Put(Framebuffer *buf);
Framebuffer *Framebuffer_Get(int type);
Framebuffer *Framebuffer_GetNoRemove(int type);
int64_t Framebuffer_GetPts(int type);
// int Framebuffer_ClearPts(int type);
Framebuffer *Framebuffer_New(int size, int clear);
void Framebuffer_Free(Framebuffer *buf);

#endif
