/*
    playlist.c , part of textmovie (play movie with console. for Windows)
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

#include <stdint.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#else
#include <sys/time.h>
#include <unistd.h>
#ifndef MAX_PATH
#define MAX_PATH   260
#endif
#endif

#include "playlist.h"


static PlaylistData *gPlaylist[PLAYLIST_ARRAY_MAX + 1] = { NULL };   // +1 : space for NULL terminate
static int gPlaylistCurrentPlay = -1;

static uint64_t tickcount(void)
{
#ifdef _WIN32
    return (uint64_t)timeGetTime();
#else
    struct timeval time;
    struct timezone zone;
    gettimeofday(&time, &zone);
    return (uint64_t)time.tv_sec*1000 + (uint64_t)time.tv_usec/1000;
#endif
}

static size_t GetFilenameFromFullpath(char *filename, int filenamelen , const char *fullpathname) {
    int  i;
    int  len, head;
    char ch, ch2;
    
    // find last '/' or '\', and set 'head' to head of name part
    head = 0;
    len = strlen(fullpathname);
    for (i = 0; i < len; i++) {
        ch = fullpathname[i];
        if ((ch == '/') || (ch == '\\')) {
            if ((i + 1) < len) {
                head = i + 1;
            }
        }
        if ( (((unsigned char)ch >= 0x81) && ((unsigned char)ch <= 0x9f)) || 
             (((unsigned char)ch >= 0xe0) && ((unsigned char)ch <= 0xfc)) ) { // Japanese Shift-JIS code check
            if ((i + 1) < len) {
                ch2 = fullpathname[i+1];
                if (((unsigned char)ch2 >= 0x40) && ((unsigned char)ch2 <= 0xfc)) {
                    i++;
                }
            }
        }
    }
    
    snprintf(filename, filenamelen, "%s", (fullpathname + head));
    
    return strlen((fullpathname + head));
}

// lower case -> upper case
// less than 8digit -> 8digit
// ex. moe1.mp4  -> MOE00000001.MP00000004   moe23-45.mp4 -> MOE00000023-00000045.MP00000004
static size_t GetFilenameForSort(char *exfilename, int filenamelen, const char *infilename)
{
    int  i;
    int  len, index, n, pad;
    char ch;
    int  shiftjis;
    int  exdigit = 8;
    
    len = strlen(infilename);
    index = 0;
    n = 0;
    shiftjis = 0;
    for (i = 0; i < len; i++) {
        ch = infilename[i];
        if (shiftjis) {
            shiftjis = 0;
        } else {
            if (((unsigned int)ch >= 'a') && ((unsigned int)ch <= 'z')) ch -= 0x20; // to upper case
            
            if ( (((unsigned char)ch >= 0x81) && ((unsigned char)ch <= 0x9f)) || 
                 (((unsigned char)ch >= 0xe0) && ((unsigned char)ch <= 0xfc)) ) { // Japanese Shift-JIS code check
                shiftjis = 1;
            }
        }
        // if (((unsigned int)ch >= 'a') && ((unsigned int)ch <= 'z')) ch -= 0x20;
        pad = 0;
        if (!n) {
            while (((unsigned int)infilename[i+n] >= '0') && ((unsigned int)infilename[i+n] <= '9')) n++;
            if (n) {
               pad = exdigit - n;
               if (pad < 0) pad = 0;
            }
        }
        while (pad--) {
            exfilename[index++] = '0';
            if (index >= filenamelen - 1) {
               exfilename[index] = 0;
               return index;
            }
        }
        exfilename[index++] = ch;
        if (index >= filenamelen - 1) {
            break;
        }
        if (n > 0) n--;
    }
    exfilename[index] = 0;
    return index;
}

PlaylistData *Playlist_CreateData(void)
{
    PlaylistData *pdata;
    
    pdata = (PlaylistData *)malloc(sizeof(PlaylistData));
    if (pdata) {
        pdata->filename[0] = 0;
        pdata->duration = 0;
        pdata->start_time = 0;
        pdata->audio_codec_id = 0;
        pdata->video_codec_id = 0;
        pdata->audio = 0;
        pdata->video = 0;
        pdata->audio_timebase = 1;
        pdata->audio_itunsmpb = 0;
        pdata->audio_smpb_edelay = 0;
        pdata->audio_smpb_zeropad = 0;
        pdata->audio_smpb_length = 0;
        pdata->video_avg_frame_rate_den = 1;
        pdata->video_avg_frame_rate_num = 1;
    }
    
    return pdata;
}

void Playlist_FreeData(PlaylistData *pdata)
{
    if (pdata) free(pdata);
}

void Playlist_Shuffle(void)
{
    int currentplay;
    int nb_playlist;
    int i, j;
    int dstindex;
    
    currentplay = Playlist_GetCurrentPlay();
    if (currentplay == -1) return;
    nb_playlist = Playlist_GetNumData();
    
    srand((int)(tickcount() % 0xffff));
    for (i = 0; i < nb_playlist; i++) {
        j = 2;
        while (j--) {
            dstindex = rand() % nb_playlist;
            if ((i != currentplay) && (dstindex != currentplay)) {
                Playlist_SwapData(i, dstindex);
                break;
            }
        }
        /*
        Playlist_SwapData(i, dstindex);
        if (currentplay == i) {
            currentplay = dstindex;
        } else if (currentplay == dstindex) {
            currentplay = i;
        }
        */
    }
    Playlist_SetCurrentPlay(currentplay);
}

int Playlist_SortCompare(int p1, int p2, int direction)
{
    char strbuf[MAX_PATH];
    char strexnum1[MAX_PATH];
    char strexnum2[MAX_PATH];
    PlaylistData *pd1, *pd2;
    int cmp;
    
    pd1 = gPlaylist[p1];
    pd2 = gPlaylist[p2];
    
    GetFilenameFromFullpath(strbuf, MAX_PATH - 1, pd1->filename);
    GetFilenameForSort(strexnum1, MAX_PATH - 1, strbuf);
    GetFilenameFromFullpath(strbuf, MAX_PATH - 1, pd2->filename);
    GetFilenameForSort(strexnum2, MAX_PATH - 1, strbuf);
    
    cmp = strcmp(strexnum1, strexnum2);
    if (direction) {
        cmp = -cmp;
    }
    
    return cmp;
}


void Playlist_SortFB(int start, int end, int direction)
{
    int mid = (end - start) / 2 + start;
    int i, k1, k2;
    
    if (mid - start >= 2) Playlist_SortFB(start, mid, direction);
    if (end - mid   >= 2) Playlist_SortFB(mid, end, direction);
    
    k2 = mid;
    for (k1 = start; k1 < end; k1++) {
        if ((k1 == k2) || (k2 >= end)) break;
        if (Playlist_SortCompare(k1, k2, direction) > 0) {
            PlaylistData *tmp;
            tmp = gPlaylist[k2];
            for (i = k2; i > k1; i--) {
                gPlaylist[i] = gPlaylist[i-1];
            }
            gPlaylist[k1] = tmp;
            k2++;
        }
    }
}

/*
int Playlist_SortFB(int start, int end, int direction, int currentplay)
{
    int mid = (end - start) / 2 + start;
    int top = mid - start;
    int bottom = end - mid;
    
    if (1) {
        if (top >= 3) {
            currentplay = Playlist_SortFB(start, mid, direction, currentplay);
        } else if (top == 2) {
            if (Playlist_SortCompare(start, start+1, direction) > 0) {
                Playlist_SwapData(start, start+1);
                if (currentplay == start) {
                    currentplay = start+1;
                } else if (currentplay == start+1) {
                    currentplay = start;
                }
            }
        }
        
        if (bottom >= 3) {
            currentplay = Playlist_SortFB(mid, end, direction, currentplay);
        } else if (bottom == 2) {
            if (Playlist_SortCompare(mid, mid+1, direction) > 0) {
                Playlist_SwapData(mid, mid+1);
                if (currentplay == mid) {
                    currentplay = mid+1;
                } else if (currentplay == mid+1) {
                    currentplay = mid;
                }
            }
        }
    }
    
    {
        int i, j, k2, rf;
        
        k2 = mid;
        for (i = start; i < end; i++) {
            if (i == k2)   break;
            if (k2 >= end) break;
            if (Playlist_SortCompare(i, k2, direction) > 0) {
                PlaylistData *tmp;
                tmp = gPlaylist[k2];
                if (currentplay == k2) rf = 1; else rf = 0;
                for (j = k2; j > i; j--) {
                    gPlaylist[j] = gPlaylist[j-1];
                    if (currentplay == j-1) {
                        currentplay = j;
                    }
                }
                gPlaylist[i] = tmp;
                if (rf) {
                    currentplay = i;
                }
                k2++;
            }
        }
    }
    return currentplay;
}
*/

void Playlist_QuickSort(int start, int end, int direction)
{
    int pivot1, pivot2;
    int i, mid;
    
    mid = start + (end-start) / 2;
    if (end - start >= 2) {
        if (Playlist_SortCompare(mid, end, direction) > 0)
            Playlist_SwapData(mid, end);
        if (Playlist_SortCompare(start, mid, direction) > 0)
            Playlist_SwapData(start, mid);
        if (Playlist_SortCompare(mid, end, direction) < 0)
            Playlist_SwapData(mid, end);
    }
    
    pivot1 = end;
    pivot2 = end;
    for (i = start; i < pivot1; i++) {
        if (Playlist_SortCompare(i, pivot1, direction) >= 0) {
            pivot1--;
            Playlist_SwapData(i, pivot1);
            if (Playlist_SortCompare(pivot1, pivot2, direction) != 0) {
                Playlist_SwapData(pivot1, pivot2);
                pivot2--;
            }
            i--;
        }
    }
    pivot1--;
    pivot2++;
    
    if (pivot1 - start > 0) {
        Playlist_QuickSort(start, pivot1, direction);
    }
    if (end - pivot2 > 0) {
        Playlist_QuickSort(pivot2, end, direction);
    }
}

/*
void Playlist_QuickSortB(int start, int end, int direction)
{
    int pivot = end;
    int i;
    
    for (i = start; i < pivot; i++) {
        if (Playlist_SortCompare(i, pivot, direction) > 0) {
            Playlist_SwapData(i, pivot - 1);
            Playlist_SwapData(pivot - 1, pivot);
            pivot--;
            i--;
        }
    }
    if ((pivot - 1) - start > 0) {
        Playlist_QuickSort(start, pivot - 1, direction);
    }
    if (end - (pivot + 1) > 0) {
        Playlist_QuickSort(pivot + 1, end, direction);
    }
}
*/

void Playlist_Sort(int direction)
{
    int nb_playlist;
    int currentplay;
    int i;
    PlaylistData *pd;
    
    currentplay = Playlist_GetCurrentPlay();
    nb_playlist = Playlist_GetNumData();
    
    for (i = 0; i < nb_playlist; i++) {
        pd = gPlaylist[i];
        if (currentplay == i) {
            pd->sortflag = 1;
        } else {
            pd->sortflag = 0;
        }
    }
    
    //Playlist_SortFB(0, nb_playlist, direction);
    //Playlist_QuickSortB(0, nb_playlist - 1, direction);
    Playlist_QuickSort(0, nb_playlist - 1, direction);
    
    currentplay = 0;
    for (i = 0; i < nb_playlist; i++) {
        pd = gPlaylist[i];
        if (pd->sortflag == 1) {
            currentplay = i;
            break;
        }
    }
    
    Playlist_SetCurrentPlay(currentplay);
}


/*
void Playlist_Sort(int direction)
{
    int currentplay;
    int nb_playlist;
    int i, j, cmp;
    PlaylistData *pd1, *pd2;
    char strbuf[MAX_PATH];
    char strexnum1[MAX_PATH];
    char strexnum2[MAX_PATH];
    
    currentplay = Playlist_GetCurrentPlay();
    if (currentplay == -1) return;
    nb_playlist = Playlist_GetNumData();
    
    // bubble sort
    for (i = 0; i < nb_playlist - 1; i++) {
        for (j = 0; j < nb_playlist - 1 - i; j++) {
            pd1 = gPlaylist[j];
            pd2 = gPlaylist[j+1];
            //pd1 = Playlist_GetData(j);
            //pd2 = Playlist_GetData(j+1);
            if (pd1 && pd2) {
                
                GetFilenameFromFullpath(strbuf, MAX_PATH - 1, pd1->filename);
                GetFilenameForSort(strexnum1, MAX_PATH - 1, strbuf);
                GetFilenameFromFullpath(strbuf, MAX_PATH - 1, pd2->filename);
                GetFilenameForSort(strexnum2, MAX_PATH - 1, strbuf);
                
                //GetFilenameFromFullpath(strexnum1, MAX_PATH - 1, pd1->filename);
                //GetFilenameFromFullpath(strexnum2, MAX_PATH - 1, pd2->filename);
                
                if (direction) {
                   cmp = (strcmp(strexnum1, strexnum2) < 0);
                } else {
                   cmp = (strcmp(strexnum1, strexnum2) > 0);
                }
                
                if(cmp) {
                    Playlist_SwapData(j, j+1);
                    if (currentplay == j) {
                        currentplay = j+1;
                    } else if (currentplay == j+1) {
                        currentplay = j;
                    }
                }
            }
        }
    }
    Playlist_SetCurrentPlay(currentplay);
}
*/

void Playlist_SwapData(int index1, int index2)
{
    PlaylistData *tmp;
    
    if ((index1 < 0) || (index1 >= Playlist_GetNumData())) return;
    if ((index2 < 0) || (index2 >= Playlist_GetNumData())) return;
    
    tmp = gPlaylist[index1];
    gPlaylist[index1] = gPlaylist[index2];
    gPlaylist[index2] = tmp;
}

int Playlist_GetCurrentPlay(void)
{
    return gPlaylistCurrentPlay;
}

void Playlist_SetCurrentPlay(int index)
{
    if ((index < -1) || (index >= Playlist_GetNumData())) return;
    
    gPlaylistCurrentPlay = index;
}

int Playlist_SetNextCurrentPlay(void)
{
    int num;
    
    if (gPlaylistCurrentPlay == -1) return -1;
    num = Playlist_GetNumData();
    gPlaylistCurrentPlay++;
    if (gPlaylistCurrentPlay >= num) gPlaylistCurrentPlay = 0;
    return gPlaylistCurrentPlay;
}

int Playlist_SetPreviousCurrentPlay(void)
{
    int num;
    
    if (gPlaylistCurrentPlay == -1) return -1;
    num = Playlist_GetNumData();
    gPlaylistCurrentPlay--;
    if (gPlaylistCurrentPlay < 0) gPlaylistCurrentPlay = num - 1;
    return gPlaylistCurrentPlay;
}

int Playlist_GetNumData(void)
{
    int index;
    
    index = 0;
    while (gPlaylist[index]) index++;
    
    return index;
}

PlaylistData *Playlist_GetData(int index)
{
    if ((index < 0) || (index >= Playlist_GetNumData())) {
        return NULL;
    }
    
    return gPlaylist[index];
}

int Playlist_AddData(PlaylistData *pdata)
{
    int index;
    
    index = 0;
    while (gPlaylist[index]) {
        index++;
        if (index >= PLAYLIST_ARRAY_MAX) {
            //Playlist_FreeData(pdata);
            return -1;
        }
    }
    gPlaylist[index] = pdata;
    gPlaylist[index + 1] = NULL;
    
    return 0;
}

int Playlist_DelData(int index)
{
    if ((index < 0) || (index >= Playlist_GetNumData())) {
        return -1;
    }
    if (gPlaylist[index])
        Playlist_FreeData(gPlaylist[index]);
    index++;
    while (gPlaylist[index]) {
        gPlaylist[index - 1] = gPlaylist[index];
        index++;
    }
    gPlaylist[index - 1] = NULL;
    return 0;
}

void Playlist_ClearList(void)
{
    int index;
    
    index = 0;
    while(gPlaylist[index]) {
        Playlist_FreeData(gPlaylist[index]);
        gPlaylist[index] = NULL;
        index++;
    }
    Playlist_SetCurrentPlay(-1);
}


