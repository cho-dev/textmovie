/*
    framebuffer.c , part of textmovie (play movie with console. for Windows)
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

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "framebuffer.h"

#define FRAMEBUFFER_USE_W32THREADS  0
#define FRAMEBUFFER_PROC_TYPE       4

// AV_NOPTS_VALUE same as libavutil/avutil.h
#ifndef AV_NOPTS_VALUE
#define AV_NOPTS_VALUE ((int64_t)UINT64_C(0x8000000000000000))
#endif

#if FRAMEBUFFER_USE_W32THREADS
typedef HANDLE            mutexobj_t;
#define MUTEX_CREATE(x)   (x=CreateMutex(NULL,FALSE,"Framebuffer"))
#define MUTEX_DESTROY(x)  CloseHandle((x))
#define MUTEX_LOCK(x)     WaitForSingleObject((x),INFINITE)
#define MUTEX_UNLOCK(x)   ReleaseMutex((x))
#else
typedef pthread_mutex_t   mutexobj_t;
#define MUTEX_CREATE(x)   (!pthread_mutex_init(&(x),NULL))
#define MUTEX_DESTROY(x)  pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x)     pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x)   pthread_mutex_unlock(&(x))
#endif


#if FRAMEBUFFER_PROC_TYPE == 4
// type 4 ===================================================

static Framebuffer *framebufferlistvideo;
static Framebuffer *framebufferlistaudio;
static Framebuffer *framebufferlistvoid;
static Framebuffer *framebufferlistaudiowave;
static mutexobj_t  framebuffermutex;

int Framebuffer_Init(void)
{
    int ret = 0;
    
    framebufferlistvideo = NULL;
    framebufferlistaudio = NULL;
    framebufferlistvoid  = NULL;
    framebufferlistaudiowave = NULL;
    
    if(!MUTEX_CREATE(framebuffermutex)) ret = -1;
    
    return ret;
}

int Framebuffer_Uninit(void)
{
    int ret = 0;
    
    if (framebuffermutex) {
        MUTEX_DESTROY(framebuffermutex);
    } else {
        ret = -1;
    }
    
    return ret;
}

int Framebuffer_GetMaxSize(int type)
{
    int maxsize;
    
    switch (type) {
        case FRAMEBUFFER_TYPE_AUDIO:
            maxsize = FRAMEBUFFER_MAXBUFFER_AUDIO;
            break;
        case FRAMEBUFFER_TYPE_VIDEO:
            maxsize = FRAMEBUFFER_MAXBUFFER_VIDEO;
            break;
        case FRAMEBUFFER_TYPE_AUDIOWAVE:
            maxsize = FRAMEBUFFER_MAXBUFFER_AUDIOWAVE;
            break;
        case FRAMEBUFFER_TYPE_VOID:
        default:
            maxsize = FRAMEBUFFER_MAXBUFFER_VOID;
            break;
    }
    
    return maxsize;
}

Framebuffer **Framebuffer_GetList(int type)
{
    Framebuffer **buflist;
    
    switch (type) {
        case FRAMEBUFFER_TYPE_AUDIO:
            buflist = &framebufferlistaudio;
            break;
        case FRAMEBUFFER_TYPE_VIDEO:
            buflist = &framebufferlistvideo;
            break;
        case FRAMEBUFFER_TYPE_AUDIOWAVE:
            buflist = &framebufferlistaudiowave;
            break;
        case FRAMEBUFFER_TYPE_VOID:
        default:
            buflist = &framebufferlistvoid;
            break;
    }
    
    return buflist;
}

int Framebuffer_ListNum(int type)
{
    Framebuffer **buflist;
    Framebuffer *curbuf;
    int count;
    
    buflist = Framebuffer_GetList(type);
    
    MUTEX_LOCK(framebuffermutex);
    
    count = 0;
    
    if (*buflist) {
	    curbuf = (*buflist);
	    while (1) {
	        if (curbuf->type == type) count++;
	        if (curbuf->next == NULL) break;
	        curbuf = curbuf->next;
	    }
	}
	
    MUTEX_UNLOCK(framebuffermutex);
    
    return count;
}

int isFramebuffer_Full(int type)
{
    Framebuffer **buflist;
    Framebuffer *curbuf;
    int listnum;
    int maxsize;
    
    buflist = Framebuffer_GetList(type);
    maxsize = Framebuffer_GetMaxSize(type);
    
    MUTEX_LOCK(framebuffermutex);
    
    listnum = 0;
    if (*buflist) {
	    curbuf = (*buflist);
	    while (1) {
	        if (curbuf->type == type) listnum++;
	        if (curbuf->next == NULL) break;
	        curbuf = curbuf->next;
	    }
	}
    
    MUTEX_UNLOCK(framebuffermutex);
    
    if (listnum >= maxsize)
        return 1;
    else
        return 0;
}

int Framebuffer_Sendback(Framebuffer *buf)
{
    Framebuffer **buflist;
    Framebuffer *curbuf;
    int listnum;
    int maxsize;
    
    buflist = Framebuffer_GetList(buf->type);
    maxsize = Framebuffer_GetMaxSize(buf->type);
    
    MUTEX_LOCK(framebuffermutex);
    
    listnum = 0;
    if (*buflist) {
	    curbuf = (*buflist);
	    while (1) {
	        if (curbuf->type == buf->type) listnum++;
	        if (curbuf->next == NULL) break;
	        curbuf = curbuf->next;
	    }
	}
    
    if (listnum >= maxsize) {
        // printf("audio buffer overflow! \n");
        MUTEX_UNLOCK(framebuffermutex);
        return -1;
    }
    
    buf->next = (*buflist);
    *buflist = buf;
    
    MUTEX_UNLOCK(framebuffermutex);
    
    return 0;
}

int Framebuffer_Put(Framebuffer *buf)
{
    Framebuffer **buflist;
    Framebuffer *curbuf;
    int listnum;
    int maxsize;
    
    buflist = Framebuffer_GetList(buf->type);
    maxsize = Framebuffer_GetMaxSize(buf->type);
    
    MUTEX_LOCK(framebuffermutex);
    
    listnum = 0;
    if (*buflist) {
	    curbuf = (*buflist);
	    while (1) {
	        if (curbuf->type == buf->type) listnum++;
	        if (curbuf->next == NULL) break;
	        curbuf = curbuf->next;
	    }
	}
    
    if (listnum >= maxsize) {
        // printf("audio buffer overflow! \n");
        MUTEX_UNLOCK(framebuffermutex);
        return -1;
    }
    
    if (*buflist) {
	    curbuf = (*buflist);
	    while (curbuf->next != NULL )
	        curbuf = curbuf->next;
	    curbuf->next = buf;
	} else {
	    *buflist = buf;
	}
	buf->next = NULL;
    
    MUTEX_UNLOCK(framebuffermutex);
    
    return 0;
}

Framebuffer *Framebuffer_Get(int type)
{
    Framebuffer *curbuf;
    Framebuffer *prevbuf;
    Framebuffer **buflist;
    
    buflist = Framebuffer_GetList(type);
    
    MUTEX_LOCK(framebuffermutex);
    
    prevbuf = NULL;
    curbuf  = NULL;
    if (*buflist) {
	    curbuf = (*buflist);
	    while (1) {
	        if (curbuf->type == type) {
			    if (prevbuf) {
			        prevbuf->next = curbuf->next;
			    } else {
			        *buflist = curbuf->next;
			    }
			    curbuf->next = NULL;
			    break;
	        }
	        if (curbuf->next == NULL) {
	            curbuf = NULL;
	            break;
	        }
	        prevbuf = curbuf;
	        curbuf = curbuf->next;
	    }
	}
	
    MUTEX_UNLOCK(framebuffermutex);
    
    return curbuf;
}

Framebuffer *Framebuffer_GetNoRemove(int type)
{
    Framebuffer *curbuf;
    Framebuffer **buflist;
    
    buflist = Framebuffer_GetList(type);
    
    MUTEX_LOCK(framebuffermutex);
    
    curbuf  = NULL;
    if (*buflist) {
	    curbuf = (*buflist);
	    while (curbuf->type != type) {
	        if (curbuf->next == NULL) {
	            curbuf = NULL;
	            break;
	        }
	        curbuf = curbuf->next;
	    }
	}
	
    MUTEX_UNLOCK(framebuffermutex);
    
    return curbuf;
}

int64_t Framebuffer_GetPts(int type)
{
    Framebuffer *curbuf;
    Framebuffer **buflist;
    int64_t pts;
    
    buflist = Framebuffer_GetList(type);
    
    MUTEX_LOCK(framebuffermutex);
    
    curbuf = NULL;
    if (*buflist) {
	    curbuf = (*buflist);
	    while (curbuf->type != type) {
	        if (curbuf->next == NULL) {
	            curbuf = NULL;
	            break;
	        }
	        curbuf = curbuf->next;
	    }
	}
	
	if (curbuf) {
        pts =  curbuf->pts;
    } else {
        pts =  AV_NOPTS_VALUE;
    }
    
    MUTEX_UNLOCK(framebuffermutex);
    
    return pts;
}

/*
int Framebuffer_ClearPts(int type)
{
    Framebuffer **buflist;
    Framebuffer *curbuf;
    int count;
    
    buflist = Framebuffer_GetList(type);
    
    MUTEX_LOCK(framebuffermutex);
    
    count = 0;
    
    if (*buflist) {
	    curbuf = (*buflist);
	    while (1) {
	        if (curbuf->type == type) {
	            curbuf->pts = 0;
	            count++;
	        }
	        if (curbuf->next == NULL) break;
	        curbuf = curbuf->next;
	    }
	}
	
    MUTEX_UNLOCK(framebuffermutex);
    
    return count;
}
*/

Framebuffer *Framebuffer_New(int size, int clear)
{
    int i;
    Framebuffer *buf;
    uint8_t *p;
    
    buf = (Framebuffer *)malloc(sizeof(Framebuffer));
    if (buf) {
        buf->data = NULL;
        if (size) {
	        buf->data = (void *)malloc(size);
	        if (!buf->data) {
	            free(buf);
	            return NULL;
	        }
	        if (clear) {
	            p = (uint8_t *)buf->data;
	            for (i = 0; i < size; i++)
	                p[i] = 0;
	        }
	    }
	    buf->next  = NULL;
	    buf->pts   = 0;
	    buf->size  = size;
	    buf->type  = FRAMEBUFFER_TYPE_VOID;
	    buf->flags = 0;
	    buf->pos   = 0;
    }
    return buf;
}

void Framebuffer_Free(Framebuffer *buf)
{
    if (buf) {
        if (buf->data && (buf->size || (buf->flags & FRAMEBUFFER_FLAGS_DATA_FREEABLE))) {
            free(buf->data);
        }
        free(buf);
    }
}
// end of type 4 ============================================
#endif

