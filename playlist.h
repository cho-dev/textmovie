/*
    playlist.h , part of textmovie (play movie with console. for Windows)
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

#ifndef PLAYLIST_PLAYLIST_H
#define PLAYLIST_PLAYLIST_H


#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#else
#ifndef MAX_PATH
#define MAX_PATH   260
#endif
#endif

#define PLAYLIST_ARRAY_MAX  2048

typedef struct PlaylistData {
    char    filename[MAX_PATH];
    wchar_t filename_w[MAX_PATH];
    int     sortflag;
    int64_t duration;
    int64_t start_time;
    int     audio_codec_id;
    int     video_codec_id;
    int     audio;
    int     video;
    int     audio_sample_rate;
    int     audio_timebase;
    int     audio_itunsmpb;
    uint32_t     audio_smpb_edelay;
    uint32_t     audio_smpb_zeropad;
    uint64_t     audio_smpb_length;
    int    video_avg_frame_rate_den;
    int    video_avg_frame_rate_num;
} PlaylistData;

PlaylistData *Playlist_CreateData(void);
void Playlist_FreeData(PlaylistData *pdata);
void Playlist_Shuffle(void);
void Playlist_Sort(int direction);
void Playlist_SwapData(int index1, int index2);
int  Playlist_GetCurrentPlay(void);
void Playlist_SetCurrentPlay(int index);
int  Playlist_SetNextCurrentPlay(void);
int  Playlist_SetPreviousCurrentPlay(void);
int  Playlist_GetNumData(void);
PlaylistData *Playlist_GetData(int index);
int  Playlist_AddData(PlaylistData *pdata);
int  Playlist_DelData(int index);
void Playlist_ClearList(void);
#endif

