/*
    textmovie.c , part of textmovie (play movie with console. for Windows)
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


    decode code is referred to ffmpeg sample source 'filtering_video.c' 'filtering_audio.c'
*/

#define TEXTMOVIE_TEXTMOVIE_VERSION  20160227
#ifdef _WIN64
#define TEXTMOVIE_TEXTMOVIE_NAME  "textmovie64.exe"
#else
#define TEXTMOVIE_TEXTMOVIE_NAME  "textmovie.exe"
#endif

#define TEXTMOVIE_TEXTMOVIE_DEBUG_DECODE 0
#define TEXTMOVIE_TEXTMOVIE_INITFILE_NAME  "textmovie.ini"

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <conio.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
// need winmm.lib (link option -lwinmm)
#include <mmsystem.h>
#endif

// ffmpeg libraries
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <SDL/SDL.h>

#include "textscreen.h"
#include "framebuffer.h"
#include "playlist.h"
#include "audiowave.h"
#include "version.h"

#include <pthread.h>
typedef pthread_mutex_t   mutexobj_t;
#define MUTEX_CREATE(x)   (!pthread_mutex_init(&(x),NULL))
#define MUTEX_DESTROY(x)  pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x)     pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x)   pthread_mutex_unlock(&(x))


#define DEFAULT_PLAYBACK_AUDIO_SAMPLE  44100

// use timeGetTime() instead of GetTimeCount() (include mmsystem.h)
#define GetTickCount timeGetTime

// video codec, filter context
static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *dec_ctx = NULL;
static AVFilterContext *buffersink_ctx;
static AVFilterContext *buffersrc_ctx;
static AVFilterGraph *filter_graph = NULL;
static int video_stream_index = -1;
static AVFrame *frame;
static AVFrame *filter_frame;

// audio codec, filter context
static AVFormatContext *afmt_ctx = NULL;
static AVCodecContext *adec_ctx = NULL;
static AVFilterContext *abuffersink_ctx;
static AVFilterContext *abuffersrc_ctx;
static AVFilterGraph *afilter_graph = NULL;
static int audio_stream_index = -1;
static AVFrame *aframe;
static AVFrame *afilter_frame;
static AVPacket apacket;
static AVPacket apacket0;

// text bitmap handle
static TextScreenBitmap *gBitmap;
static TextScreenBitmap *gBitmapWave;
static TextScreenBitmap *gBitmapLastVideo = NULL;
static TextScreenBitmap *gBitmapClip = NULL;
static TextScreenBitmap *gBitmapList = NULL;

static int64_t gAudioCurrentPts;
static int     gAudioCurrentPlaynum;

static int     gVolume = 100;
static int64_t gStartTime = 0;
static int64_t gPauseTime = 0;
static int64_t gADiff = 0;
static int64_t gVDiff = 0;
static wchar_t gFilename[MAX_PATH];
static int     gAudioLevel = 0;
static int     gAudioClip = 0;
static int     gQuitFlag = 0;
static int     gPause = 0;
static int     gShowWave = 0;
static int     gDebugDecode = 0;
static int     gSeeked = 0;
static int     gShowInfo = 0;
static int     gShowPlaylist = 0;
static int     gReadDoneAudio = 0;
static int     gReadDoneVideo = 0;
static int     gOptionShuffle = 0;
static int     gFrameDrop = 0;
static int     gBarMode = 0;
static int     gSampleRate = DEFAULT_PLAYBACK_AUDIO_SAMPLE;
static int     gInitSampleRate = DEFAULT_PLAYBACK_AUDIO_SAMPLE;

//static int64_t gCallPrevTime = 0;  // test for callback
//static int64_t gCallDiff = 0;      // test for callback

static pthread_t  gAudioWaveTid;
static mutexobj_t gMutexBitmapWave;


typedef struct MediaInfo {
    wchar_t filename[MAX_PATH];
    int64_t duration;
    int64_t start_time;
    int  audio_codec_id;
    int  video_codec_id;
    int  audio;
    int  video;
    int  audio_sample_rate;
    int  audio_timebase;
    int  audio_itunsmpb;
    uint32_t  audio_smpb_edelay;
    uint32_t  audio_smpb_zeropad;
    uint64_t  audio_smpb_length;
    int    video_avg_frame_rate_den;
    int    video_avg_frame_rate_num;
} MediaInfo;


void ReadInitFile(const char *lpFileName)
{
    char    *lpAppName = "SETTINGS";
    
    gBarMode = (int)GetPrivateProfileInt(lpAppName, "BarMode", 0, lpFileName);
    gBarMode = (!!gBarMode);
    
    gShowInfo = (int)GetPrivateProfileInt(lpAppName, "ShowInfo", 0, lpFileName);
    if (gShowInfo < 0) gShowInfo = 0;
    if (gShowInfo > 2) gShowInfo = 2;
    
    gShowWave = (int)GetPrivateProfileInt(lpAppName, "ShowWave", 0, lpFileName);
    gShowWave = (!!gShowWave);
    
    gOptionShuffle = (int)GetPrivateProfileInt(lpAppName, "Shuffle", 0, lpFileName);
    gOptionShuffle = (!!gOptionShuffle);
    
    AudioWave_SetWaveType((int)GetPrivateProfileInt(lpAppName, "AudioWaveType", 0, lpFileName));
    
    AudioWave_SetSpectrumBase((int)GetPrivateProfileInt(lpAppName, "SpectrumBase", 3, lpFileName));
    
    gShowPlaylist = (int)GetPrivateProfileInt(lpAppName, "ShowPlaylist", 0, lpFileName);
    gShowPlaylist = (!!gShowPlaylist);
    
    gVolume = (int)GetPrivateProfileInt(lpAppName, "Volume", 100, lpFileName);
    if (gVolume < 0) gVolume = 0;
    if (gVolume > 500) gVolume = 500;
    gVolume = (gVolume / 5) * 5;
    
    gInitSampleRate = (int)GetPrivateProfileInt(lpAppName, "SampleRate", DEFAULT_PLAYBACK_AUDIO_SAMPLE, lpFileName);
    if (gInitSampleRate < 22050) gInitSampleRate = 22050;
    if (gInitSampleRate > 48000) gInitSampleRate = 48000;
}

void Clear_Cuedata(int type)
{
    Framebuffer *buf;
    
    switch (type) {
        case FRAMEBUFFER_TYPE_AUDIO:
            while(Framebuffer_ListNum(FRAMEBUFFER_TYPE_AUDIO)) {
                buf = Framebuffer_Get(FRAMEBUFFER_TYPE_AUDIO);
                Framebuffer_Free(buf);
            }
            break;
        case FRAMEBUFFER_TYPE_VIDEO:
            while(Framebuffer_ListNum(FRAMEBUFFER_TYPE_VIDEO)) {
                buf = Framebuffer_Get(FRAMEBUFFER_TYPE_VIDEO);
                TextScreen_FreeBitmap((TextScreenBitmap *)buf->data);
                Framebuffer_Free(buf);
            }
            break;
        case FRAMEBUFFER_TYPE_VOID:
            while(Framebuffer_ListNum(FRAMEBUFFER_TYPE_VOID)) {
                buf = Framebuffer_Get(FRAMEBUFFER_TYPE_VOID);
                Framebuffer_Free(buf);
            }
            break;
        case FRAMEBUFFER_TYPE_AUDIOWAVE:
            while(Framebuffer_ListNum(FRAMEBUFFER_TYPE_AUDIOWAVE)) {
                buf = Framebuffer_Get(FRAMEBUFFER_TYPE_AUDIOWAVE);
                Framebuffer_Free(buf);
            }
            break;
        default:
            break;
    }
}

void GetItunsmpbValues(char *smpbstr, uint32_t *edelay, uint32_t *zeropad, uint64_t *length)
{
    int i, j, index, n;
    char strbuf[128];
    int smpblen;
    uint64_t val, k;
    
    smpblen = strlen(smpbstr);
    for (i = 0; i < smpblen; i++) {
        if ((smpbstr[i] >= 'a') && (smpbstr[i] <= 'f')) smpbstr[i] -= 0x20;
    }
    
    index = 0;
    for (i = 0; i < 4; i++) {
        n = 0;
        while( !(((smpbstr[index] >= '0') && (smpbstr[index] <= '9')) ||
               ((smpbstr[index] >= 'A') && (smpbstr[index] <= 'F'))) ) {
            index++;
            if (index >= smpblen) break;
        }
        while( ((smpbstr[index] >= '0') && (smpbstr[index] <= '9')) || 
               ((smpbstr[index] >= 'A') && (smpbstr[index] <= 'F'))  ) {
            strbuf[n++] = smpbstr[index++];
            if (index >= smpblen) break;
        }
        strbuf[n] = 0;
        //printf("i=%d: %s\n", i, strbuf);
        val = 0;
        k = 1;
        for (j = n - 1; j >= 0; j--) {
            if ((strbuf[j] >= '0') && (strbuf[j] <= '9')) strbuf[j] -= '0';
            if ((strbuf[j] >= 'A') && (strbuf[j] <= 'F')) strbuf[j] = strbuf[j] - 'A' + 10;
            if (strbuf[j] > 15) strbuf[j] = 15;
            if (strbuf[j] < 0)  strbuf[j] = 0;
            val = val + k * (uint64_t)strbuf[j];
            k *= 16;
        }
        if (i == 1) *edelay = val;
        if (i == 2) *zeropad = val;
        if (i == 3) *length = val;
    }
}

int isVideoStillPicture(int video_codec_id)
{
    if (video_codec_id == AV_CODEC_ID_MJPEG) return 1;
    if (video_codec_id == AV_CODEC_ID_PNG) return 1;
    if (video_codec_id == AV_CODEC_ID_BMP) return 1;
    if (video_codec_id == AV_CODEC_ID_WEBP) return 1;
    if (video_codec_id == AV_CODEC_ID_TIFF) return 1;
    if (video_codec_id == AV_CODEC_ID_TARGA) return 1;
    if (video_codec_id == AV_CODEC_ID_GIF) return 1;
    if (video_codec_id == AV_CODEC_ID_JPEG2000) return 1;
    
    return 0;
}

/*
void CP932toUTF8(char *str_utf8, int len, const char *str_cp932)
{
    wchar_t str_wchar[1024];
    
    //printf("cp932 filename = %s\n", str_cp932);
    MultiByteToWideChar(CP_ACP, 0, str_cp932, strlen(str_cp932)+1, str_wchar, sizeof(str_wchar));
    WideCharToMultiByte(CP_UTF8, 0, str_wchar, -1, str_utf8, len, NULL, NULL);
    //printf("utf8 filename = %s\n", str_utf8);
}
*/

// int isMediaFile(const char *filename)
int isMediaFile(const wchar_t *filename, MediaInfo *minfo)
{
    int ret;
    AVCodec *dec;
    AVFormatContext *lfmt_ctx;
    AVCodecContext  *ldec_ctx;
    AVCodecContext  *ladec_ctx;
    int  isAudio;
    int  isVideo;
    int  stream_index;
    char filename_utf8[MAX_PATH * 2];
    
    //CP932toUTF8(filename_utf8, sizeof(filename_utf8), filename);
    WideCharToMultiByte(CP_UTF8, 0, filename, -1, filename_utf8, sizeof(filename_utf8), NULL, NULL);
    
    isAudio = 0;
    isVideo = 0;
    
    //snprintf(minfo->filename, MAX_PATH - 1, "%s", filename);
    snwprintf(minfo->filename, MAX_PATH - 1, L"%s", filename);
    minfo->filename[MAX_PATH - 1] = 0;
    minfo->audio = 0;
    minfo->video = 0;
    minfo->audio_codec_id = 0;
    minfo->video_codec_id = 0;
    minfo->audio_sample_rate = 1;
    minfo->audio_timebase = 1;
    minfo->audio_itunsmpb = 0;
    minfo->audio_smpb_edelay = 0;
    minfo->audio_smpb_zeropad = 0;
    minfo->audio_smpb_length = 0;
    minfo->video_avg_frame_rate_den = 1;
    minfo->video_avg_frame_rate_num = 1;
    
    lfmt_ctx = NULL;
    ldec_ctx = NULL;
    ladec_ctx = NULL;
    
    //if ((ret = avformat_open_input(&lfmt_ctx, filename, NULL, NULL)) < 0) {
    if ((ret = avformat_open_input(&lfmt_ctx, filename_utf8, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return 0;
    }
    
    if ((ret = avformat_find_stream_info(lfmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        avformat_close_input(&lfmt_ctx);
        return 0;
    }
    
    minfo->duration = lfmt_ctx->duration;
    minfo->start_time = (lfmt_ctx->start_time == AV_NOPTS_VALUE) ? 0: lfmt_ctx->start_time;
    
    // find video stream
    if ((ret = av_find_best_stream(lfmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
    } else {
        stream_index = ret;
        ldec_ctx = lfmt_ctx->streams[stream_index]->codec;
        av_opt_set_int(ldec_ctx, "refcounted_frames", 1, 0);
        
        if ((ret = avcodec_open2(ldec_ctx, dec, NULL)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
        } else {
            isVideo = 1;
            minfo->video = 1;
            minfo->video_codec_id = ldec_ctx->codec_id;
            // 100000 = AV_TIME_BASE / 10
            if ((ldec_ctx->codec_id == AV_CODEC_ID_MJPEG) && (lfmt_ctx->duration <= 100000)) isVideo = 0;
            if ((ldec_ctx->codec_id == AV_CODEC_ID_PNG) && (lfmt_ctx->duration <= 100000)) isVideo = 0;
            if ((ldec_ctx->codec_id == AV_CODEC_ID_BMP) && (lfmt_ctx->duration <= 100000)) isVideo = 0;
            if ((ldec_ctx->codec_id == AV_CODEC_ID_WEBP) && (lfmt_ctx->duration <= 100000)) isVideo = 0;
            if ((ldec_ctx->codec_id == AV_CODEC_ID_TIFF) && (lfmt_ctx->duration <= 100000)) isVideo = 0;
            if ((ldec_ctx->codec_id == AV_CODEC_ID_TARGA) && (lfmt_ctx->duration <= 100000)) isVideo = 0;
            if ((ldec_ctx->codec_id == AV_CODEC_ID_GIF) && (lfmt_ctx->duration <= 100000)) isVideo = 0;
            if ((ldec_ctx->codec_id == AV_CODEC_ID_JPEG2000) && (lfmt_ctx->duration <= 100000)) isVideo = 0;
            if (ldec_ctx->codec_id == AV_CODEC_ID_ANSI) isVideo = 0;
            if ((ldec_ctx->codec_id >= AV_CODEC_ID_FIRST_SUBTITLE) && (ldec_ctx->codec_id < AV_CODEC_ID_PROBE)) isVideo = 0;
            
            minfo->video_avg_frame_rate_den = lfmt_ctx->streams[stream_index]->avg_frame_rate.den;
            minfo->video_avg_frame_rate_num = lfmt_ctx->streams[stream_index]->avg_frame_rate.num;
            
            avcodec_close(ldec_ctx);
        }
    }
    
    // find audio stream
    if ((ret = av_find_best_stream(lfmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a audio stream in the input file\n");
    } else {
        stream_index = ret;
        ladec_ctx = lfmt_ctx->streams[stream_index]->codec;
        av_opt_set_int(ladec_ctx, "refcounted_frames", 1, 0);
        
        if ((ret = avcodec_open2(ladec_ctx, dec, NULL)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
        } else {
            AVDictionary      *dic;
            AVDictionaryEntry *tag = NULL;
            uint32_t edelay, zeropad;
            uint64_t length;
            AVRational  timebase;
            
            isAudio = 1;
            minfo->audio = 1;
            minfo->audio_codec_id = ladec_ctx->codec_id;
            minfo->audio_sample_rate = ladec_ctx->sample_rate;
            timebase = lfmt_ctx->streams[stream_index]->time_base;
            if (timebase.num) {
                minfo->audio_timebase = timebase.den / timebase.num;
            } else {
                minfo->audio_timebase = 1;
            }
            
            // get ITUNSMPB playback info (for gapless play)
            dic = lfmt_ctx->metadata;
            tag = av_dict_get(dic, "ITUNSMPB", tag, AV_DICT_IGNORE_SUFFIX);
            if (tag) {
                edelay = 0;
                zeropad = 0;
                length = 0;
                GetItunsmpbValues(tag->value, &edelay, &zeropad, &length);
                minfo->audio_itunsmpb = 1;
                minfo->audio_smpb_edelay = edelay;
                minfo->audio_smpb_zeropad = zeropad;
                minfo->audio_smpb_length = length;
            }
            avcodec_close(ladec_ctx);
            //printf("timebase=%d,smpb=%d,edelay=%d,zeropad=%d,length=%d\n", minfo->audio_timebase, minfo->audio_itunsmpb,
            //                            (int)minfo->audio_smpb_edelay, (int)minfo->audio_smpb_zeropad, (int)minfo->audio_smpb_length);
        }
    }
    avformat_close_input(&lfmt_ctx);
    
    return (isAudio || isVideo);
}

int VideoStream_OpenFile(const wchar_t *filename)
{
    int ret;
    AVCodec *dec;
    char filename_utf8[MAX_PATH * 2];
    
    //CP932toUTF8(filename_utf8, sizeof(filename_utf8), filename);
    WideCharToMultiByte(CP_UTF8, 0, filename, -1, filename_utf8, sizeof(filename_utf8), NULL, NULL);
    
    video_stream_index = -1;
    fmt_ctx = NULL;
    dec_ctx = NULL;
    
    //if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)) < 0) {
    if ((ret = avformat_open_input(&fmt_ctx, filename_utf8, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        avformat_close_input(&fmt_ctx);
        return ret;
    }
    
    if ((ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        avformat_close_input(&fmt_ctx);
        return ret;
    }
    
    video_stream_index = ret;
    dec_ctx = fmt_ctx->streams[video_stream_index]->codec;
    av_opt_set_int(dec_ctx, "refcounted_frames", 1, 0);
    
    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
        avformat_close_input(&fmt_ctx);
        return ret;
    }
    
    return 0;
}

int AudioStream_OpenFile(const wchar_t *filename)
{
    int ret;
    AVCodec *dec;
    char filename_utf8[MAX_PATH * 2];
    
    //CP932toUTF8(filename_utf8, sizeof(filename_utf8), filename);
    WideCharToMultiByte(CP_UTF8, 0, filename, -1, filename_utf8, sizeof(filename_utf8), NULL, NULL);
    
    afmt_ctx = NULL;
    adec_ctx = NULL;
    
    //if ((ret = avformat_open_input(&afmt_ctx, filename, NULL, NULL)) < 0) {
    if ((ret = avformat_open_input(&afmt_ctx, filename_utf8, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    
    if ((ret = avformat_find_stream_info(afmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        avformat_close_input(&afmt_ctx);
        return ret;
    }
    
    if ((ret = av_find_best_stream(afmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a audio stream in the input file\n");
        avformat_close_input(&afmt_ctx);
        return ret;
    }
    audio_stream_index = ret;
    adec_ctx = afmt_ctx->streams[audio_stream_index]->codec;
    av_opt_set_int(adec_ctx, "refcounted_frames", 1, 0);
    
    if ((ret = avcodec_open2(adec_ctx, dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
        avformat_close_input(&afmt_ctx);
        return ret;
    }
    
    apacket0.data = NULL;
    apacket.data = NULL;
    
    return 0;
}

int Stream_OpenFile(const wchar_t *filename)
{
    if (video_stream_index != -1) {
        avcodec_close(dec_ctx);
        avformat_close_input(&fmt_ctx);
        video_stream_index = -1;
    }
    if (audio_stream_index != -1) {
        avcodec_close(adec_ctx);
        avformat_close_input(&afmt_ctx);
        audio_stream_index = -1;
    }
    
    gReadDoneAudio = 0;
    gReadDoneVideo = 0;
    
    if (AudioStream_OpenFile(filename) < 0) {
        gReadDoneAudio = 1;
    }
    if (VideoStream_OpenFile(filename) < 0) {
        gReadDoneVideo = 1;
    }
    
    return 0;
}

int VideoStream_InitFilters(const char *filters_descr)
{
    char args[512];
    int ret = 0;
    AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_GRAY8, AV_PIX_FMT_NONE };

    avfilter_graph_free(&filter_graph);
    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
            time_base.num, time_base.den,
            dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);
    
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }
    
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }
    
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }
    
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;
    
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;
    
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                    &inputs, &outputs, NULL)) < 0)
        goto end;
    
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

int AudioStream_InitFilters(const char *filters_descr)
{
    char args[512];
    int ret = 0;
    AVFilter *abuffersrc  = avfilter_get_by_name("abuffer");
    AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    static const enum AVSampleFormat out_sample_fmts[] = { AV_SAMPLE_FMT_S16, -1 };
    static const int64_t out_channel_layouts[] = { AV_CH_LAYOUT_STEREO, -1 };
    int out_sample_rates[] = { gSampleRate, -1 };
    const AVFilterLink *outlink;
    AVRational time_base = afmt_ctx->streams[audio_stream_index]->time_base;
    
    avfilter_graph_free(&afilter_graph);
    afilter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !afilter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    if (!adec_ctx->channel_layout)
        adec_ctx->channel_layout = av_get_default_channel_layout(adec_ctx->channels);
    snprintf(args, sizeof(args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
             time_base.num, time_base.den, adec_ctx->sample_rate,
             av_get_sample_fmt_name(adec_ctx->sample_fmt), adec_ctx->channel_layout);
    ret = avfilter_graph_create_filter(&abuffersrc_ctx, abuffersrc, "in",
                                       args, NULL, afilter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        goto end;
    }
    
    ret = avfilter_graph_create_filter(&abuffersink_ctx, abuffersink, "out",
                                       NULL, NULL, afilter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
        goto end;
    }
    
    ret = av_opt_set_int_list(abuffersink_ctx, "sample_fmts", out_sample_fmts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
        goto end;
    }
    
    ret = av_opt_set_int_list(abuffersink_ctx, "channel_layouts", out_channel_layouts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
        goto end;
    }
    
    ret = av_opt_set_int_list(abuffersink_ctx, "sample_rates", out_sample_rates, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
        goto end;
    }
    
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = abuffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;
    
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = abuffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;
    
    if ((ret = avfilter_graph_parse_ptr(afilter_graph, filters_descr,
                                        &inputs, &outputs, NULL)) < 0)
        goto end;
    
    if ((ret = avfilter_graph_config(afilter_graph, NULL)) < 0)
        goto end;
    
    outlink = abuffersink_ctx->inputs[0];
    av_get_channel_layout_string(args, sizeof(args), -1, outlink->channel_layout);
    av_log(NULL, AV_LOG_INFO, "Output: srate:%dHz fmt:%s chlayout:%s\n",
           (int)outlink->sample_rate,
           (char *)av_x_if_null(av_get_sample_fmt_name(outlink->format), "?"),
           args);

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    
    return ret;
}

void Stream_Seek(int64_t delta)
{
    int64_t current_ts, seek_min, seek_target, seek_max;
    int     flags;
    Framebuffer *buf;

    flags = 0;
    
    current_ts  = (int64_t)GetTickCount() * 1000 - gStartTime;
    // current_ts  = gAudioCurrentPts;
    seek_target = current_ts + delta;
    seek_min    = (delta > 0) ? current_ts + 100 : INT64_MIN;
    seek_max    = (delta < 0) ? current_ts - 100 : INT64_MAX;
    flags       = 0;  // AVSEEK_FLAG_ANY;
    
    if (audio_stream_index != -1) {
        SDL_PauseAudio(1);
        //avformat_seek_file(afmt_ctx, audio_stream_index, seek_min, seek_target, seek_max, flags);
        avformat_seek_file(afmt_ctx, -1, seek_min, seek_target, seek_max, flags);
        SDL_LockAudio();
        while(Framebuffer_ListNum(FRAMEBUFFER_TYPE_AUDIO)) {
            buf = Framebuffer_Get(FRAMEBUFFER_TYPE_AUDIO);
            Framebuffer_Free(buf);
        }
        SDL_UnlockAudio();
        SDL_PauseAudio(0);
    }
    if (video_stream_index != -1) {
        //avformat_seek_file(fmt_ctx, video_stream_index, seek_min, seek_target, seek_max, flags);
        avformat_seek_file(fmt_ctx, -1, seek_min, seek_target, seek_max, flags);
        while(Framebuffer_ListNum(FRAMEBUFFER_TYPE_VIDEO)) {
            buf = Framebuffer_Get(FRAMEBUFFER_TYPE_VIDEO);
            TextScreen_FreeBitmap((TextScreenBitmap *)buf->data);
            Framebuffer_Free(buf);
        }
    }
    gStartTime = gStartTime - delta;
    gSeeked = 1;
}

void AudioStream_VolumeAdjust(int16_t *stream16buf, int stream16len)
{
    int32_t sdat32;
    int32_t volume;
    int count;
    
    gAudioLevel = gAudioLevel * 7 / 8;  // adjust peak level release time
    volume = gVolume;
    
    for (count = 0; count < stream16len; count++) {
        sdat32 = (int32_t)stream16buf[count];
        sdat32 = (sdat32 * volume) / 100;
        if (sdat32 > 0x7fff) {
            sdat32 = 0x7fff;
            gAudioClip = 2;
        }
        if (sdat32 < -0x8000) {
            sdat32 = -0x8000;
            gAudioClip = 2;
        }
        stream16buf[count] = (int16_t)sdat32;
        
        // check peak level
        sdat32 = (sdat32 >= 0) ? sdat32: -sdat32;
        if (sdat32 > gAudioLevel) gAudioLevel = sdat32;
    }
}

static void CheckClockDifference(int64_t pts)
{
    int64_t diff;
    
    diff = (int64_t)GetTickCount()*1000 - gStartTime - pts;
    if ((diff > 40000) || (diff < -40000)) {                   // re-synchronize when pts has large difference
        gStartTime = (int64_t)GetTickCount()*1000 - pts + 0;   // +n -> pos:video delay, neg:audio delay
    }
    gADiff = diff;
}

static void AudioStream_SDLCallback(void *userdata, uint8_t *stream, int len)
{
    Framebuffer *abuf;
    int16_t     *stream16buf;
    int         stream16len;
    int16_t     *data;
    int         count;
    int         pos;
    int         diffcheck;
    
    //gCallDiff = GetTickCount() - gCallPrevTime; // for test
    //gCallPrevTime = GetTickCount();             // for test
    
    stream16buf = (int16_t *)malloc(sizeof(int16_t) * len / 2);
    if (!stream16buf) return;
    
    stream16len = len / 2;
    count = 0;
    diffcheck = 1;
    abuf = NULL;
    pos = 0;
    
    while (count < stream16len) {
        if (!gPause) {
            abuf = Framebuffer_GetNoRemove(FRAMEBUFFER_TYPE_AUDIO);
        }
        if (abuf) {
            if (diffcheck) {
                // 48000Hz, 2ch, 16bit  ->  pts delay = (data byte) * 1000000 / 48000 / 4
                CheckClockDifference(abuf->pts + ((int64_t)abuf->pos * 1000000L / (int64_t)gSampleRate / 4));
                diffcheck = 0;
            }
            gAudioCurrentPts = abuf->pts + ((int64_t)abuf->pos * 1000000L / (int64_t)gSampleRate / 4);
            gAudioCurrentPlaynum = abuf->playnum;
            
            data = (int16_t *)abuf->data;
            pos = abuf->pos;
            while ((pos < abuf->size) && (count < stream16len)) {
                *(stream16buf + count) = data[(pos/2)];
                count++;
                pos += 2;
            }
            if (pos >= abuf->size) {
                abuf = Framebuffer_Get(FRAMEBUFFER_TYPE_AUDIO);
                Framebuffer_Free(abuf);
                abuf = NULL;
            }
        } else {
            if (!gPause) {
                gAudioCurrentPts = (int64_t)GetTickCount()*1000 - gStartTime;
                gAudioCurrentPlaynum = Playlist_GetCurrentPlay();
            }
            
            while (count < stream16len) {
                *(stream16buf + count) = 0;
                count++;
            }
        }
    }
    
    if (abuf) {
        abuf->pos = pos;
    }
    
    AudioStream_VolumeAdjust(stream16buf, stream16len);  // volume adjust
    
    // copy audio data to buffer for draw wave
    abuf = Framebuffer_New(stream16len * 2, 0);
    if (abuf) {
        data = (int16_t *)abuf->data;
        abuf->pts =gAudioCurrentPts;
        abuf->type = FRAMEBUFFER_TYPE_AUDIOWAVE;
        abuf->playnum = gAudioCurrentPlaynum;
        memcpy(abuf->data, stream16buf, len);
        if (Framebuffer_Put(abuf)) {
            Framebuffer_Free(abuf);
        }
    }
    
    memcpy(stream, stream16buf, len);
    free(stream16buf);
    
    //gCallDiff = GetTickCount() - gCallPrevTime; // for test
}

static void exit_proc(void)
{
    gQuitFlag = 1;
    
    // close SDL
    SDL_PauseAudio(1);
    SDL_CloseAudio();
    SDL_Quit();
    
    // Thread destroy
    pthread_join(gAudioWaveTid , NULL );
    MUTEX_DESTROY(gMutexBitmapWave);
    
    // free all cue data
    Clear_Cuedata(FRAMEBUFFER_TYPE_AUDIO);
    Clear_Cuedata(FRAMEBUFFER_TYPE_VIDEO);
    Clear_Cuedata(FRAMEBUFFER_TYPE_VOID);
    Clear_Cuedata(FRAMEBUFFER_TYPE_AUDIOWAVE);
    
    Framebuffer_Uninit();
    
    Playlist_ClearList();
    
    // free text bitmap
    TextScreen_FreeBitmap(gBitmap);
    TextScreen_FreeBitmap(gBitmapWave);
    if (gBitmapClip) TextScreen_FreeBitmap(gBitmapClip);
    if (gBitmapList) TextScreen_FreeBitmap(gBitmapList);
    if (gBitmapLastVideo) TextScreen_FreeBitmap(gBitmapLastVideo);
    
    // free audio, video context
    avfilter_graph_free(&filter_graph);
    avcodec_close(dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_frame_free(&frame);
    av_frame_free(&filter_frame);
    
    avfilter_graph_free(&afilter_graph);
    avcodec_close(adec_ctx);
    avformat_close_input(&afmt_ctx);
    av_frame_free(&aframe);
    av_frame_free(&afilter_frame);
    
    SetThreadExecutionState(ES_CONTINUOUS);
    
    // TEST: end timer resolution
    /*
    if (0) {
        timeEndPeriod(1);
    }
    */
    
    ExitProcess(0);
}

static BOOL MyConsoleCtrlHandler(DWORD ctrltype)
{
    switch(ctrltype)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
        default:
            gQuitFlag = 1;  // Quit signal(flag) to main thread. main thread will terminate correctly.
            Sleep(1000);
            exit_proc();    // if still running, do force terminate.
            break;
    }
    return TRUE;
}

int AudioStream_ReadAndBuffer(void)
{
    //AVPacket apacket;
    AVRational time_base;
    int ret;
    int got_frame;
    int64_t pts_time;
    static int ignore_audio = 0;
    static int64_t ptsoffset = 0;
    
    ret = 0;
    
    if (gSeeked) {
        PlaylistData *ppd;
        
        ignore_audio = 4;
        ppd = Playlist_GetData( Playlist_GetCurrentPlay() );
        if (ppd) {
            if (ppd->audio_codec_id == AV_CODEC_ID_WMALOSSLESS) { // avoid noise (decode bug ?)
                ignore_audio = 24;
            }
        }
    }
    
    if (isFramebuffer_Full(FRAMEBUFFER_TYPE_AUDIO)) return 0;
    
    if (!apacket0.data) {
        if ((ret = av_read_frame(afmt_ctx, &apacket)) < 0) {
            return -1;
        } else {
            apacket0 = apacket;
            ptsoffset = 0;
        }
    }
    
    if (apacket.stream_index == audio_stream_index) {
        got_frame = 0;
        ret = avcodec_decode_audio4(adec_ctx, aframe, &got_frame, &apacket);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error decoding audio\n");
            av_log(NULL, AV_LOG_ERROR, "ret=%d:%016x\n", ret, ret);
            // av_free_packet(&apacket0);
            av_packet_unref(&apacket0);
            apacket0.data = NULL;
            return 0;
        }
        
        apacket.size -= ret;
        apacket.data += ret;
        
        if (!got_frame) {
            if (apacket.size <= 0) {
                // av_free_packet(&apacket0);
                av_packet_unref(&apacket0);
                apacket0.data = NULL;
            }
            return 0;
        }
        
        if (av_buffersrc_add_frame_flags(abuffersrc_ctx, aframe, 0) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
            // av_free_packet(&apacket0);
            av_packet_unref(&apacket0);
            apacket0.data = NULL;
            return -1;
        }
        
        while (1) {
            int         samples;
            int16_t     *p;
            Framebuffer *abuf;
            int16_t     *data;
            int         i;
            
            ret = av_buffersink_get_frame(abuffersink_ctx, afilter_frame);
            
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            }
            if (ret < 0) {
                av_frame_unref(aframe);
                // av_free_packet(&apacket0);
                av_packet_unref(&apacket0);
                apacket0.data = NULL;
                return -1;
            }
            
            afilter_frame->pts = av_frame_get_best_effort_timestamp(afilter_frame);
            // time_base = abuffersink_ctx->inputs[0]->time_base;
            time_base = afmt_ctx->streams[audio_stream_index]->time_base;
            pts_time = av_rescale_q(afilter_frame->pts, time_base, AV_TIME_BASE_Q) + ptsoffset;
            
            //samples = afilter_frame->nb_samples * 
            //                av_get_channel_layout_nb_channels(av_frame_get_channel_layout(afilter_frame));
            samples = afilter_frame->nb_samples * 2;
            p = (int16_t *)afilter_frame->data[0];
            
            ptsoffset += (int64_t)(samples / 2) * 1000000L / (int64_t)gSampleRate;
            
            if (gDebugDecode) {
                //TextScreen_Wait(100);
                gStartTime = (int64_t)GetTickCount()*1000 - pts_time;
                printf("AudioBuffer:time=%d.%03d:pts=%"PRId64":TB=%d/%d:sample=%d:ch=%d\n", 
                                    (int)(pts_time / 1000000), (int)((pts_time % 1000000) / 1000), 
                                    afilter_frame->pts, time_base.num, time_base.den,
                                    samples, av_get_channel_layout_nb_channels(av_frame_get_channel_layout(afilter_frame)));
            } else {  // make audio cue data
                if (ignore_audio) {
                    ignore_audio--;
                } else {
                    {  // experimental 20150305 gapless play test (for iTunSMPB tag):
                        PlaylistData *pd;
                        int64_t  sample48len;
                        int64_t  currentsample;
                        int64_t  resample_comp;
                        
                        pd = NULL;
                        if (Playlist_GetCurrentPlay() != -1) {
                            pd = Playlist_GetData(Playlist_GetCurrentPlay());
                            //if (pd->audio_itunsmpb && !(pd->video)) {
                            if (pd->audio_itunsmpb) {
                                resample_comp = 0;
                                if (pd->audio_sample_rate != gSampleRate) {  // compensation for resample
                                    resample_comp = 16;
                                }
                                // +edelay for compare pts (currentsample)
                                sample48len = ((int64_t)pd->audio_smpb_length + (int64_t)pd->audio_smpb_edelay + resample_comp)
                                                 * gSampleRate / pd->audio_sample_rate;
                                //sample48len = ((int64_t)pd->audio_smpb_length) * gSampleRate / pd->audio_sample_rate;
                                currentsample = pts_time * gSampleRate / 1000000;
                                //printf("%d:%d ",(int)currentsample, (int)sample48len);
                                if (currentsample + (samples / 2) > sample48len) {
                                    samples = (sample48len - currentsample - 1) * 2;
                                    if (samples < 0) samples = 0;
                                    // printf("s=%d   ", samples);
                                }
                            }
                        }
                    }
                    
                    if (samples) {
                        abuf = Framebuffer_New(samples * 2, 0);
                        if (abuf) {
                            
                            data = (int16_t *)abuf->data;
                            abuf->pts = pts_time;
                            abuf->type = FRAMEBUFFER_TYPE_AUDIO;
                            abuf->flags = 0;
                            abuf->playnum = Playlist_GetCurrentPlay();
                            for (i = 0; i < samples; i++) {
                                data[i] = *p;
                                /*
                                if((i == 0) || (i == 1)) {  // test marker
                                    data[i] = 10000;
                                }
                                */
                                p++;
                            }
                            
                            SDL_LockAudio();
                            Framebuffer_Put(abuf);
                            SDL_UnlockAudio();
                        }
                    }
                }
            }
            av_frame_unref(afilter_frame);
        }
        av_frame_unref(aframe);
        
        if (apacket.size <= 0) {
            // av_free_packet(&apacket0);
            av_packet_unref(&apacket0);
            apacket0.data = NULL;
        }
    } else {
        // av_free_packet(&apacket0);
        av_packet_unref(&apacket0);
    }
    
    /*
    if (apacket.size <= 0) {
        // av_free_packet(&apacket0);
        av_packet_unref(&apacket0);
        apacket0.data = NULL;
    }
    */
    
    return 0;
}

int VideoStream_ReadAndBuffer(void)
{
    AVPacket packet;
    AVRational time_base;
    int ret;
    int got_frame;
    int64_t pts_time;
    static int64_t prev_pts_time = 0;
    static int ignore_video = 0;
    int decodecount;
    
    ret = 0;
    
    if (gSeeked) {
        ignore_video = 8;
    }
    
    if (isFramebuffer_Full(FRAMEBUFFER_TYPE_VIDEO)) return 0;
    
    if ((ret = av_read_frame(fmt_ctx, &packet)) < 0) return -1;
    if (packet.stream_index == video_stream_index) {
        got_frame = 0;
        decodecount = 64;
        
        while(!got_frame && decodecount--) {  // experimental 20150321 for PNG decode
            ret = avcodec_decode_video2(dec_ctx, frame, &got_frame, &packet);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error decoding video\n");
                // av_free_packet(&packet);
                av_packet_unref(&packet);
                if (ret == AVERROR_INVALIDDATA) {
                    return 0;
                } else {
                    return -1;
                }
            }
        }
        
        if (!got_frame) {
            //av_free_packet(&packet);
            av_packet_unref(&packet);
            //printf("png! ret=%d", ret);
            return 0;
        }
        
        if (av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            return -1;
        }
        
        while (1) {
            uint8_t *p0, *p;
            int x, y;
            TextScreenBitmap *tmp;
            Framebuffer *vbuf;
            
            ret = av_buffersink_get_frame(buffersink_ctx, filter_frame);

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_unref(filter_frame);
                break;
            }
            if (ret < 0) {
                av_frame_unref(filter_frame);
                av_frame_unref(frame);
                // av_free_packet(&packet);
                av_packet_unref(&packet);
                return -1;
            }
            
            filter_frame->pts = av_frame_get_best_effort_timestamp(filter_frame);
            time_base = buffersink_ctx->inputs[0]->time_base;
            pts_time = av_rescale_q(filter_frame->pts, time_base, AV_TIME_BASE_Q);
            // printf("pts_time:%"PRId64"\n", pts_time);
            
            if (filter_frame->pts == AV_NOPTS_VALUE) pts_time = 0;
            
            if (pts_time < prev_pts_time) {
                Clear_Cuedata(FRAMEBUFFER_TYPE_VIDEO);
            }
            prev_pts_time = pts_time;
            
            if (gDebugDecode) {
                //TextScreen_Wait(100);
                printf("VideoBuffer:time=%d.%03d:pts=%"PRId64":TB=%d/%d:width=%d:height=%d\n", 
                                  (int)(pts_time / 1000000), (int)((pts_time % 1000000) / 1000), filter_frame->pts, time_base.num, 
                                  time_base.den, filter_frame->width, filter_frame->height);
            } else {  // make video cue data
                /*
                if (ignore_video) {
                    // pts_time = (int64_t)GetTickCount() * 1000 - gStartTime;
                    ignore_video--;
                } else {}
                */
                {
                    if (ignore_video) {
                        pts_time = -1000LL*1000LL*1000LL;
                        ignore_video--;
                    }
                    
                    p0 = filter_frame->data[0];
                    for (y = 0; y < filter_frame->height; y++) {
                        p = p0;
                        for (x = 0; x < filter_frame->width; x++)
                            TextScreen_PutCell(gBitmap, x, y, " .-:+*H#"[*(p++) / 32]);
                        p0 += filter_frame->linesize[0];
                    }
                    tmp = TextScreen_DupBitmap(gBitmap);
                    vbuf = Framebuffer_New(0, 0);
                    if (vbuf) {
                        vbuf->type = FRAMEBUFFER_TYPE_VIDEO;
                        vbuf->pts  = pts_time;
                        vbuf->data = (void *)tmp;
                        vbuf->playnum = Playlist_GetCurrentPlay();
                        Framebuffer_Put(vbuf);
                    }
                }
            }
            av_frame_unref(filter_frame);
        }
        av_frame_unref(frame);
    }
    // av_free_packet(&packet);
    av_packet_unref(&packet);
    
    return 0;
}

void Do_Clipboard_Copy(int reverse) {
    TextScreenBitmap *bitmap;
    HGLOBAL cdata;
    SIZE_T  size;
    char *p;
    char cell;
    int i, x, y, index;
    char neg[256];
    
    bitmap = gBitmapClip;
    if (!bitmap) return;
    
    // make negative table
    for (i = 0; i < 256; i++) {
        neg[i] = '.';
    }
    neg[(unsigned char)' '] = '#';
    neg[(unsigned char)'.'] = 'M';
    neg[(unsigned char)'-'] = 'H';
    neg[(unsigned char)':'] = '*';
    neg[(unsigned char)'+'] = '+';
    neg[(unsigned char)'*'] = ':';
    neg[(unsigned char)'H'] = '-';
    neg[(unsigned char)'#'] = '.';
    
    
    if (OpenClipboard( GetConsoleWindow() )) {
        size = (bitmap->width + 2) * bitmap->height + 2;
        cdata = GlobalAlloc(GMEM_MOVEABLE, size);
        if (cdata) {
            p= (char *)GlobalLock(cdata);
            index = 0;
            for (y = 0; y < bitmap->height; y++) {
                for (x = 0; x < bitmap->width; x++) {
                    cell = TextScreen_GetCell(bitmap, x, y);
                    if (!reverse) {
                        p[index++] = cell;
                    } else {
                        p[index++] = neg[(unsigned char)cell];
                    }
                }
                p[index++] = 0x0d;
                p[index++] = 0x0a;
            }
            p[index++] = 0x00;
            GlobalUnlock(cdata);
            EmptyClipboard();
            if (SetClipboardData(CF_TEXT, cdata) == NULL) {
                // printf("Err:%d\n", GetLastError());
                GlobalFree(cdata);
            }
        }
        CloseClipboard();
    }
}

void Stream_Restart(const wchar_t *filename, int seamless)
{
    if (!seamless) {
        SDL_PauseAudio(1);
    }
    snwprintf(gFilename, MAX_PATH, L"%s", filename);
    
    { // close current stream
        int ret;
        char strbuf[256];
        
        if (video_stream_index != -1) {
            avcodec_close(dec_ctx);
            avformat_close_input(&fmt_ctx);
            dec_ctx = NULL;
            fmt_ctx = NULL;
            video_stream_index = -1;
        }
        if (audio_stream_index != -1) {
            avcodec_close(adec_ctx);
            avformat_close_input(&afmt_ctx);
            adec_ctx = NULL;
            afmt_ctx = NULL;
            audio_stream_index = -1;
        }
        
        if (!seamless) {
            SDL_LockAudio();
            Clear_Cuedata(FRAMEBUFFER_TYPE_AUDIO);
            SDL_UnlockAudio();
        }
        Clear_Cuedata(FRAMEBUFFER_TYPE_VIDEO);
        Clear_Cuedata(FRAMEBUFFER_TYPE_VOID);
        
        if (gBitmapLastVideo) TextScreen_FreeBitmap(gBitmapLastVideo);
        gBitmapLastVideo = NULL;
        
        gReadDoneAudio = 0;
        gReadDoneVideo = 0;
        // initialize audio stream (open file, init filters)
        //snprintf(strbuf, sizeof(strbuf), "aresample=%d,aformat=sample_fmts=s16:channel_layouts=stereo", (int)gSampleRate);
        snprintf(strbuf, sizeof(strbuf), "anull");
        if ((ret = AudioStream_OpenFile(gFilename)) < 0) {
            gReadDoneAudio = 1;
        } else {
            if ((ret = AudioStream_InitFilters(strbuf)) < 0 )
                exit_proc();
        }
        
        // initialize video stream (open file, init filters)
        snprintf(strbuf, sizeof(strbuf), "scale=%d:%d", gBitmap->width, gBitmap->height);
        if ((ret = VideoStream_OpenFile(gFilename)) < 0) {
            gReadDoneVideo = 1;
        } else {
            if ((ret = VideoStream_InitFilters(strbuf)) < 0 )
                exit_proc();
            if (VideoStream_ReadAndBuffer() < 0) {
                gReadDoneVideo = 1;
            }
        }
    }
    
    if (!seamless) {
        SDL_PauseAudio(0);
    }
    gStartTime = (int64_t)GetTickCount() * 1000;
}

int Get_ConsoleSize(int *width, int *height)
{
    HANDLE stdouth;
    CONSOLE_SCREEN_BUFFER_INFO info;
    int ret;
    
    stdouth = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!stdouth) {
        *width  = 80;
        *height = 25;
        return -1;
    }
    ret = GetConsoleScreenBufferInfo(stdouth, &info);
    if (ret) {
        *width = info.srWindow.Right-info.srWindow.Left + 1;
        *height = info.srWindow.Bottom-info.srWindow.Top + 1;
    } else {
        *width  = 80;
        *height = 25;
        return -1;
    }
    return 0;
}

int isFolder(wchar_t *filename)
{
    DWORD attr;
    
    attr = GetFileAttributesW(filename);
    if ((attr != 0xFFFFFFFF) && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        return 1;
    }
    return 0;
}

size_t Get_FilenameFromFullpath(char *filename, int filenamelen , const char *fullpathname) {
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
    filename[filenamelen - 1] = 0;
    
    return strlen((fullpathname + head));
}

size_t Get_DirNameFromFullpath(char *filename, int filenamelen , const char *fullpathname) {
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
    
    snprintf(filename, filenamelen, "%s", fullpathname);
    filename[filenamelen - 1] = 0;
    if (filenamelen > head) {
        filename[head] = 0;
    }
    
    return head;
}

void CopyMediaInfo2PlaylistData(PlaylistData *ppd, MediaInfo *minfo)
{
    ppd->duration = minfo->duration;
    ppd->start_time = minfo->start_time;
    ppd->audio = minfo->audio;
    ppd->video = minfo->video;
    ppd->audio_codec_id = minfo->audio_codec_id;
    ppd->video_codec_id = minfo->video_codec_id;
    ppd->audio_sample_rate = minfo->audio_sample_rate;
    ppd->audio_timebase = minfo->audio_timebase;
    ppd->audio_itunsmpb = minfo->audio_itunsmpb;
    ppd->audio_smpb_edelay = minfo->audio_smpb_edelay;
    ppd->audio_smpb_zeropad = minfo->audio_smpb_zeropad;
    ppd->audio_smpb_length = minfo->audio_smpb_length;
    ppd->video_avg_frame_rate_den = minfo->video_avg_frame_rate_den;
    ppd->video_avg_frame_rate_num = minfo->video_avg_frame_rate_num;
}

void CheckOptionFilename(const wchar_t *fullpath_w)
{
    char fullpath[MAX_PATH];
    char filename[MAX_PATH];
    
    WideCharToMultiByte(CP_ACP, 0, fullpath_w, -1, fullpath, sizeof(fullpath), NULL, NULL);
    Get_FilenameFromFullpath(filename, MAX_PATH, fullpath);
    
    if (!strcmp(filename, TEXTMOVIE_TEXTMOVIE_INITFILE_NAME)) { // read .ini file
        ReadInitFile(fullpath);
    }
    if (!strcmp(filename, "SHUFFLE")) {  // shuffle mode
        gOptionShuffle = 1;
    }
}

int Playlist_Make(wchar_t *dirname, int count)
{
    DWORD attr;
    HANDLE hdl;
    WIN32_FIND_DATAW fd;
    wchar_t dpath[MAX_PATH];
    wchar_t fullpath[MAX_PATH];
    char strbuf[128];
    PlaylistData *ppd;
    MediaInfo minfo;
    
    //Playlist_ClearList();
    // count = 0;
    snwprintf(dpath, MAX_PATH - 1, L"%s\\*.*", dirname);
    dpath[MAX_PATH - 1] = 0;
    attr = GetFileAttributesW(dirname);
    if ((attr != 0xFFFFFFFF) && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
        hdl = FindFirstFileW(dpath, &fd);
        if (hdl != INVALID_HANDLE_VALUE) {
            if ( !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                !(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) &&
                !(fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ) {
                //CheckOptionFilename(fd.cFileName);
                snwprintf(fullpath, MAX_PATH - 1, L"%s\\%s", dirname, fd.cFileName);
                fullpath[MAX_PATH - 1] = 0;
                CheckOptionFilename(fullpath);
                if (isMediaFile(fullpath, &minfo)) {
                    ppd = Playlist_CreateData();
                    if (ppd) {
                        snwprintf(ppd->filename_w, MAX_PATH - 1, L"%s", fullpath);
                        ppd->filename_w[MAX_PATH - 1] = 0;
                        WideCharToMultiByte(CP_ACP, 0, fullpath, -1, ppd->filename, sizeof(ppd->filename), NULL, NULL);
                        CopyMediaInfo2PlaylistData(ppd, &minfo);
                        if (Playlist_AddData(ppd) == -1) {
                            Playlist_FreeData(ppd);
                        } else {
                            count++;
                        }
                        //printf("Filename1=%s\n", ppd->filename);
                        //_getch();
                    }
                }
            } else 
            if ( (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                !(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) &&
                !(fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ) {
                if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) { 
                    snwprintf(fullpath, MAX_PATH - 1, L"%s\\%s", dirname, fd.cFileName);
                    fullpath[MAX_PATH - 1] = 0;
                    count = Playlist_Make(fullpath, count);  // recursive file search
                    //printf("Filename2=%s\n", fullpath);
                    //_getch();
                }
            }
            while ( FindNextFileW(hdl, &fd) ) {
                if ( !(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    !(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) &&
                    !(fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ) {
                    //CheckOptionFilename(fd.cFileName);
                    snwprintf(fullpath, MAX_PATH - 1, L"%s\\%s", dirname, fd.cFileName);
                    fullpath[MAX_PATH - 1] = 0;
                    CheckOptionFilename(fullpath);
                    if (isMediaFile(fullpath, &minfo)) {
                        ppd = Playlist_CreateData();
                        if (ppd) {
                            snwprintf(ppd->filename_w, MAX_PATH - 1, L"%s", fullpath);
                            ppd->filename_w[MAX_PATH - 1] = 0;
                            WideCharToMultiByte(CP_ACP, 0, fullpath, -1, ppd->filename, sizeof(ppd->filename), NULL, NULL);
                            CopyMediaInfo2PlaylistData(ppd, &minfo);
                            if (Playlist_AddData(ppd) == -1) {
                                Playlist_FreeData(ppd);
                            } else {
                                count++;
                            }
                            //printf("Filename3=%s\n", ppd->filename);
                            //_getch();
                        }
                    }
                    if (count >= PLAYLIST_ARRAY_MAX) break;
                    
                    if ((count % 20 == 0) && gBitmap) {
                        TextScreen_ClearBitmap(gBitmap);
                        TextScreen_DrawText(gBitmap, 0, 0, "Search valid media files in a folder. Wait a moment please.");
                        snprintf(strbuf, sizeof(strbuf), "  %d files found.", count);
                        TextScreen_DrawText(gBitmap, 0, 1, strbuf);
                        TextScreen_ShowBitmap(gBitmap, 0, 0);
                        TextScreen_ClearBitmap(gBitmapList);
                    }
                } else
                if ( (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    !(fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) &&
                    !(fd.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ) {
                    if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) { 
                        snwprintf(fullpath, MAX_PATH - 1, L"%s\\%s", dirname, fd.cFileName);
                        fullpath[MAX_PATH - 1] = 0;
                        count = Playlist_Make(fullpath, count);  // recursive file search
                        //printf("Filename4=%s\n", fullpath);
                        //_getch();
                    }
                    
                    if (count >= PLAYLIST_ARRAY_MAX) break;
                    /*
                    if ((count % 20 == 0) && gBitmap) {
                        TextScreen_ClearBitmap(gBitmap);
                        TextScreen_DrawText(gBitmap, 0, 0, "Search valid media files in a folder. Wait a moment please.");
                        snprintf(strbuf, sizeof(strbuf), "  %d files found.", count);
                        TextScreen_DrawText(gBitmap, 0, 1, strbuf);
                        TextScreen_ShowBitmap(gBitmap, 0, 0);
                        TextScreen_ClearBitmap(gBitmapList);
                    }
                    */
                }
            }
            FindClose(hdl);
        }
    }
    /*
    if (count) {
        ppd = Playlist_GetData(0);
        snprintf(gFilename, MAX_PATH - 1, "%s", ppd->filename);
        gFilename[MAX_PATH - 1] = 0;
        Playlist_SetCurrentPlay(0);
    }
    */
    return count;
}

void Do_DrawPlaylist(TextScreenBitmap *bitmap)
{
    int height, center;
    int y, index;
    int scrollmode;
    int nb_playlist;
    int currentplay;
    char filename[MAX_PATH];
    char timestr[16];
    char strbuf[512];  // MAX_PATH + alpha
    PlaylistData *ppd;
    
    currentplay = Playlist_GetCurrentPlay();
    //currentplay = gAudioCurrentPlaynum;
    if (currentplay == -1) {
        gShowPlaylist = 0;
        return;
    }
    
    height = bitmap->height;
    center = height / 2;
    
    nb_playlist = Playlist_GetNumData();
    
    scrollmode = 0;
    if (nb_playlist > height) scrollmode = 1;
    y = 0;
    index = 0;
    if (scrollmode && (currentplay > center)) {
        index = currentplay - center;
        if (nb_playlist < index + height) {
            index = nb_playlist - height;
        }
    }
    
    while ( y < height ) {
        ppd = Playlist_GetData(index);
        if (!ppd) break;
        Get_FilenameFromFullpath(filename, MAX_PATH - 1, ppd->filename);
        // printf("%s : %s\n", filename, ppd->filename);
        {
            int sec, min, hour, sign;
            //int dec;
            
            sec = ppd->duration / 1000;
            sign = (sec >= 0);
            sec = sign ? sec : -sec;
            //dec = sec % 1000;
            sec = sec / 1000;
            hour = sec / 3600;
            min = (sec / 60) % 60;
            sec = sec % 60;
            //snprintf(timestr, sizeof(timestr),"%c%02d:%02d:%02d.%03d", sign ? ' ' : '-', hour, min, sec, dec);
            snprintf(timestr, sizeof(timestr),"%c%02d:%02d:%02d", sign ? ' ' : '-', hour, min, sec);
            if (ppd->duration < 0) {
                snprintf(timestr, sizeof(timestr), "%s", " --------");
            }
        }
        //snprintf(strbuf, sizeof(strbuf), "%c : %c%c :%s : %s : (%s/%s)(%d/%d)", (index == currentplay) ? '@' : ' ',
        //                           (ppd->audio) ? 'A' : ' ', (ppd->video) ? 'V' : ' ', timestr, filename,
        //                           avcodec_get_name(ppd->audio_codec_id), avcodec_get_name(ppd->video_codec_id), ppd->audio_codec_id, ppd->video_codec_id);
        snprintf(strbuf, sizeof(strbuf), "%c : %c%c :%s : %s", (index == currentplay) ? '@' : ' ',
                                   (ppd->audio) ? 'A' : ' ', (ppd->video) ? 'V' : ' ', timestr, filename);
        
        TextScreen_DrawText(bitmap, 0, y, strbuf);
        y++;
        index++;
    }
}

void Do_Keyboard_GetDDFilename(wchar_t *filename, int len)
{
    int  index;
    unsigned int dt;
    int  ch;
    
    index = 1;
    dt = GetTickCount();
    
    while (filename[index]) index++;
    
    while (1) {  // collect input character in 200ms
        if (_kbhit()) {
            ch = _getwch();
            filename[index++] = ch;
            if (index >= len - 6) {
                filename[index++] = 0;
                break;
            }
        }
        if (GetTickCount() - dt > 200) {
            filename[index++] = 0;
            break;
        }
        if (GetTickCount() < dt) dt = GetTickCount();
    }
    
    // extract first 1 file name
    if (filename[0] == '"') {    // double quote file
        for (index = 1; index < len - 6; index++) {
            if (filename[index] == '"') {
                filename[index] = 0;
                break;
            }
        }
    } else {
        for (index = 1; index < len - 6; index++) {
            if (filename[index] == ' ') {
                filename[index] = 0;
                break;
            }
        }
    }
    
    index = 0;
    do {  // delete filename[0]
        filename[index] = filename[index+1];
        index++;
    } while (filename[index]);
    
    while (_kbhit()) _getwch();
}

void Do_PlayPrevious(void)
{
    PlaylistData *ppd;
    
    if (Playlist_GetCurrentPlay() != -1) {  // previous playlist
        //Playlist_SetCurrentPlay(gAudioCurrentPlaynum);
        Playlist_SetPreviousCurrentPlay();
        ppd = Playlist_GetData(Playlist_GetCurrentPlay());
        Stream_Restart(ppd->filename_w, 0);
        if (gShowPlaylist ) { // for key repeat: experimental 20150303
            TextScreen_ClearBitmap(gBitmap);
            Do_DrawPlaylist(gBitmap);
            TextScreen_ShowBitmap(gBitmap, 0, 0);
            if (gBitmapList) TextScreen_FreeBitmap(gBitmapList);
            gBitmapList = TextScreen_DupBitmap(gBitmap);
        }
    }
}

void Do_PlayNext(void)
{
    PlaylistData *ppd;
    
    if (Playlist_GetCurrentPlay() != -1) {  // next playlist
        //Playlist_SetCurrentPlay(gAudioCurrentPlaynum);
        Playlist_SetNextCurrentPlay();
        ppd = Playlist_GetData(Playlist_GetCurrentPlay());
        Stream_Restart(ppd->filename_w, 0);
        if (gShowPlaylist ) { // for key repeat: experimental 20150303
            TextScreen_ClearBitmap(gBitmap);
            Do_DrawPlaylist(gBitmap);
            TextScreen_ShowBitmap(gBitmap, 0, 0);
            if (gBitmapList) TextScreen_FreeBitmap(gBitmapList);
            gBitmapList = TextScreen_DupBitmap(gBitmap);
        }
    }
}

void Do_PlayRestart(void)
{
    PlaylistData *ppd;
    
    Playlist_SetCurrentPlay(gAudioCurrentPlaynum);
    ppd = Playlist_GetData(Playlist_GetCurrentPlay());
    Stream_Restart(ppd->filename_w, 0);  // restart current play
}

void Do_Keyboard_Check(void)
{
    //static int64_t       savepts;
    static int           ch = 0;
    static int           savech[4] = {0};
    static unsigned int  lastcht;
    int  i;
    PlaylistData *ppd;
    MediaInfo  minfo;
    
    while (_kbhit()) {
        ch      = _getwch();
        lastcht = GetTickCount();
        savech[0] = ch;
        
        //  if stdin is ([A-Z]:\) try to get filename.
        if ((savech[2] >= 'A') && (savech[2] <= 'Z') && (savech[1] == ':') && (savech[0] == '\\')) {
            char filename[MAX_PATH];
            wchar_t filename_w[MAX_PATH];
            
            filename_w[0] = savech[3];
            filename_w[1] = savech[2];
            filename_w[2] = savech[1];
            filename_w[3] = savech[0];
            filename_w[4] = 0;
            //Do_Keyboard_GetDDFilename(filename, MAX_PATH);
            //MultiByteToWideChar(CP_ACP, 0, filename, strlen(filename)+1, filename_w, sizeof(filename_w));
            
            Do_Keyboard_GetDDFilename(filename_w, MAX_PATH);
            WideCharToMultiByte(CP_ACP, 0, filename_w, -1, filename, sizeof(filename), NULL, NULL);
            
            if (isFolder(filename_w)) {
                if (Playlist_Make(filename_w, 0)) {
                    //ppd = Playlist_GetData(Playlist_GetCurrentPlay());
                    //Stream_Restart(ppd->filename, 0);
                }
            } else {
                CheckOptionFilename(filename_w);
                if (isMediaFile(filename_w, &minfo)) {
                    ppd = Playlist_CreateData();
                    snwprintf(ppd->filename_w, MAX_PATH - 1, L"%s", filename_w);
                    ppd->filename_w[MAX_PATH - 1] = 0;
                    snprintf(ppd->filename, MAX_PATH - 1, "%s", filename);
                    ppd->filename[MAX_PATH - 1] = 0;
                    CopyMediaInfo2PlaylistData(ppd, &minfo);
                    if (Playlist_AddData(ppd) == -1) {
                        Playlist_FreeData(ppd);
                    }
                    /*
                    Playlist_SetCurrentPlay(-1);
                    Stream_Restart(filename, 0);
                    */
                }
            }
            
            //for (i = 0; i < sizeof(savech); i++) savech[i] = 0;
            for (i = 0; i < 4; i++) savech[i] = 0;
            ch = 0;
        }
        
        //  if stdin is (\\*) try to get filename.
        if ((savech[1] == '\\') && (savech[0] == '\\')) {
            char filename[MAX_PATH];
            wchar_t filename_w[MAX_PATH];
            
            filename_w[0] = savech[2];
            filename_w[1] = savech[1];
            filename_w[2] = savech[0];
            filename_w[3] = 0;
            //Do_Keyboard_GetDDFilename(filename, MAX_PATH);
            //MultiByteToWideChar(CP_ACP, 0, filename, strlen(filename)+1, filename_w, sizeof(filename_w));
            
            Do_Keyboard_GetDDFilename(filename_w, MAX_PATH);
            WideCharToMultiByte(CP_ACP, 0, filename_w, -1, filename, sizeof(filename), NULL, NULL);
            
            if (isFolder(filename_w)) {
                if (Playlist_Make(filename_w, 0)) {
                    //ppd = Playlist_GetData(Playlist_GetCurrentPlay());
                    //Stream_Restart(ppd->filename, 0);
                }
            } else {
                if (isMediaFile(filename_w, &minfo)) {
                    ppd = Playlist_CreateData();
                    snwprintf(ppd->filename_w, MAX_PATH - 1, L"%s", filename_w);
                    ppd->filename_w[MAX_PATH - 1] = 0;
                    snprintf(ppd->filename, MAX_PATH - 1, "%s", filename);
                    ppd->filename[MAX_PATH - 1] = 0;
                    CopyMediaInfo2PlaylistData(ppd, &minfo);
                    if (Playlist_AddData(ppd) == -1) {
                        Playlist_FreeData(ppd);
                    }
                    /*
                    Playlist_SetCurrentPlay(-1);
                    Stream_Restart(filename, 0);
                    */
                }
            }
            
            //for (i = 0; i < sizeof(savech); i++) savech[i] = 0;
            for (i = 0; i < 4; i++) savech[i] = 0;
            ch = 0;
        }
        
        if (ch == 0xe0) {  // scancode 'e0' 'xx'
            int ch2;
            ch2 = _getwch();
            if (ch2 == 0x48) {   // up arrow key
                gVolume += 5;
                if (gVolume > 500) gVolume = 500;
            }
            if (ch2 == 0x50) {   // down arrow key
                gVolume -= 5;
                if (gVolume < 0) gVolume = 0;
            }
            if (ch2 == 0x4b && !gPause) {   // left arrow key
                {  // experimental 20150310  rewind through previous media
                    int64_t pts;
                    int64_t seekpos;
                    int64_t seekdelta = 10 * 1000000L;  // -10 sec
                    
                    ppd = Playlist_GetData(gAudioCurrentPlaynum);
                    pts = gAudioCurrentPts - ppd->start_time;
                    if (pts < 3*1000*1000) {
                        Playlist_SetCurrentPlay(gAudioCurrentPlaynum);
                        Playlist_SetPreviousCurrentPlay();
                        ppd = Playlist_GetData(Playlist_GetCurrentPlay());
                        Stream_Restart(ppd->filename_w, 0);
                        seekpos = ppd->start_time + ppd->duration - seekdelta;
                        if (seekpos < 0) seekpos = 0;
                        Stream_Seek(seekpos);
                        gAudioCurrentPlaynum = Playlist_GetCurrentPlay();
                        gAudioCurrentPts = seekpos;
                    } else {
                        if (gAudioCurrentPlaynum == Playlist_GetCurrentPlay()) {
                            if (gReadDoneAudio) {
                                ppd = Playlist_GetData(gAudioCurrentPlaynum);
                                Stream_Restart(ppd->filename_w, 0);
                                seekpos = pts - seekdelta;
                                if (seekpos < 0) seekpos = 0;
                                Stream_Seek(seekpos);
                                gAudioCurrentPlaynum = Playlist_GetCurrentPlay();
                                gAudioCurrentPts = seekpos;
                            } else {
                                Stream_Seek( -seekdelta);
                            }
                        } else {
                            Playlist_SetCurrentPlay(gAudioCurrentPlaynum);
                            ppd = Playlist_GetData(Playlist_GetCurrentPlay());
                            Stream_Restart(ppd->filename_w, 0);
                            seekpos = pts - seekdelta;
                            if (seekpos < 0) seekpos = 0;
                            Stream_Seek(seekpos);
                            gAudioCurrentPlaynum = Playlist_GetCurrentPlay();
                            gAudioCurrentPts = seekpos;
                        }
                    }
                }
            }
            if (ch2 == 0x4d && !gPause) {   // right arrow key
                {
                    int64_t  seekdelta = 10 * 1000000L;  // +10 sec
                    
                    if (gAudioCurrentPlaynum == Playlist_GetCurrentPlay()) {
                        Stream_Seek( seekdelta );
                    } else {
                        ppd = Playlist_GetData(Playlist_GetCurrentPlay());
                        Stream_Restart(ppd->filename_w, 0);
                    }
                }
            }
            if (ch2 == 0x49 && !gPause) {   // page up key
                Do_PlayPrevious();
            }
            if (ch2 == 0x51 && !gPause) {   // page down key
                Do_PlayNext();
            }
            if (ch2 == 0x47 && !gPause) {   // home key
                Do_PlayRestart();
            }
            if (ch2 == 0x4f && !gPause) {   // end key
            }
            if (ch2 == 0x52 && !gPause) {   // insert key
            }
            if (ch2 == 0x53 && !gPause) {   // delete key
            }
            
            // printf("%02x", ch2);
            ch = 0;
        }
        //for (i = sizeof(savech) - 1; i > 0; i--) savech[i]  = savech[i-1];
        for (i = 4 - 1; i > 0; i--) savech[i]  = savech[i-1];
    }
    
    //if (ch) printf("key = %04x", ch);
    
    if (GetTickCount() - lastcht > 50) {  // normal key check delayed 50ms to detect drag'n drop file name
        if (ch == 'Q' || ch == 'q') gQuitFlag = 1;
        if (ch == 'W' || ch == 'w') gShowWave = !gShowWave;
        if (ch == 'E' || ch == 'e') {
            AudioWave_NextWaveType();
            MUTEX_LOCK(gMutexBitmapWave);
            TextScreen_ClearBitmap(gBitmapWave);
            MUTEX_UNLOCK(gMutexBitmapWave);
        }
        if (ch == 'I' || ch == 'i') {
            gShowInfo++;
            if (gShowInfo > 2) gShowInfo = 0;
        }
        if (ch == 'L' || ch == 'l') {
            if (!gShowPlaylist) {
                if (Playlist_GetCurrentPlay() != -1) {
                    gShowPlaylist = 1;
                    if (gBitmapList) TextScreen_ClearBitmap(gBitmapList);
                }
            } else {
                gShowPlaylist = 0;
            }
        }
        if ((ch == 's') && gShowPlaylist) {
            Playlist_Shuffle();
        }
        if ((ch == 'S') && gShowPlaylist) {
            Playlist_Sort(0);
        }
        if ((ch == 'N' || ch == 'n') && !gPause) {
            Do_PlayNext();
        }
        if ((ch == 'B' || ch == 'b') && !gPause) {
            Do_PlayPrevious();
        }
        if ((ch == 'H' || ch == 'h') && !gPause) {
            Do_PlayRestart();
        }
        if (ch == 'V' || ch == 'v') gBarMode = !gBarMode;
        
        /*
        if (ch == 'G') {
            gDebugDecode = !gDebugDecode;
            if (gDebugDecode) {
                SDL_LockAudio();
                Clear_Cuedata(FRAMEBUFFER_TYPE_AUDIO);
                SDL_UnlockAudio();
                Clear_Cuedata(FRAMEBUFFER_TYPE_VIDEO);
                Clear_Cuedata(FRAMEBUFFER_TYPE_VOID);
                Clear_Cuedata(FRAMEBUFFER_TYPE_AUDIOWAVE);
                av_log_set_level(AV_LOG_VERBOSE);
                // av_log_set_level(AV_LOG_DEBUG);
                TextScreen_ClearScreen();
             } else {
                av_log_set_level(AV_LOG_QUIET);
                TextScreen_ClearScreen();
            }
        }
        */
        if (ch == 'p') Do_Clipboard_Copy(0);
        if (ch == 'P') Do_Clipboard_Copy(1);
        if (ch == ' ') {
            gPause = !gPause;
            if (gPause) {
                //savepts = (int64_t)GetTickCount() * 1000L - gStartTime;
                gPauseTime = (int64_t)GetTickCount() * 1000L - gStartTime;
                //SDL_PauseAudio(1);
            } else {
                //gStartTime = (int64_t)GetTickCount() * 1000L - savepts;
                gStartTime = (int64_t)GetTickCount() * 1000L - gPauseTime;
                //SDL_PauseAudio(0);
            }
        }
        if (ch == ',') {
            AudioWave_PreviousSpectrumBase();
        }
        if (ch == '.') {
            AudioWave_NextSpectrumBase();
        }
        
        //for (i = 0; i < sizeof(savech); i++) savech[i] = 0;
        for (i = 0; i < 4; i++) savech[i] = 0;
        ch = 0;
    }
}

void Do_Resize_Check(void)
{
    TextScreenSetting screen;
    int console_width, console_height;
    
    Get_ConsoleSize(&console_width, &console_height);
    TextScreen_GetSetting(&screen);
    
    if ( ((screen.width != console_width - 4) ||
        (screen.height != console_height - 3)) &&
        (console_width - 4 > 0) && (console_height - 3 > 0) ) {
        
        Framebuffer *buf;
        TextScreenBitmap *bitmap, *newbitmap;
        int  listnum;
        int  i;
        char strbuf[32];
        
        screen.width = console_width - 4;     // console width  - 4
        screen.height = console_height - 3;   // console height - 3
        if (screen.width < 1) screen.width = 1;
        if (screen.height < 1) screen.height = 1;
        
        TextScreen_Init(&screen);
        TextScreen_ClearScreen();
        
        TextScreen_FreeBitmap(gBitmap);
        gBitmap = TextScreen_CreateBitmap(screen.width, screen.height);
        
        SDL_LockAudio();
        MUTEX_LOCK(gMutexBitmapWave);
        TextScreen_FreeBitmap(gBitmapWave);
        gBitmapWave = TextScreen_CreateBitmap(screen.width, screen.height);
        MUTEX_UNLOCK(gMutexBitmapWave);
        SDL_UnlockAudio();
        
        if (gBitmapList) {
            TextScreen_FreeBitmap(gBitmapList);
            gBitmapList = NULL;
        }
        /*
        if (gBitmapLastVideo) {
            //TextScreen_ResizeBitmap(gBitmapLastVideo, screen.width, screen.height);
            TextScreen_CropBitmap(gBitmapLastVideo, 0, 0, screen.width, screen.height);
        }
        */
        
        listnum = Framebuffer_ListNum(FRAMEBUFFER_TYPE_VIDEO);
        for (i = 0; i < listnum; i++) {
            buf = Framebuffer_Get(FRAMEBUFFER_TYPE_VIDEO);
            bitmap = (TextScreenBitmap *)buf->data;
            newbitmap = TextScreen_CreateBitmap(screen.width, screen.height);
            TextScreen_CopyBitmap(newbitmap, bitmap, 0, 0);
            TextScreen_FreeBitmap(bitmap);
            buf->data = (void *)newbitmap;
            Framebuffer_Put(buf);
        }
        if (video_stream_index != -1) {
            snprintf(strbuf, sizeof(strbuf), "scale=%d:%d", gBitmap->width, gBitmap->height);
            if (VideoStream_InitFilters(strbuf) < 0 )
                    exit_proc();
        }
    }
}

void Do_DrawInfo(TextScreenBitmap *bitmap)
{
    TextScreenSetting screen;
    int console_width, console_height;
    int y;
    int fps;
    char strbuf[MAX_PATH];
    char timestr[32];
    int64_t duration;
    int audio_codec_id, video_codec_id;
    PlaylistData  *ppd;
    //int len;
    
    if (!gShowInfo) return;
    
    Get_ConsoleSize(&console_width, &console_height);
    TextScreen_GetSetting(&screen);
    
    y = 0;
    
    ppd = Playlist_GetData(gAudioCurrentPlaynum);
    Get_FilenameFromFullpath(strbuf, MAX_PATH - 1, ppd->filename);
    //Get_FilenameFromFullpath(strbuf, MAX_PATH - 1, gFilename);
    if (strbuf[0])
        TextScreen_DrawText(bitmap, 0, y++, strbuf);
    /*
    // get name part
    len = strlen(gFilename);
    if (len && (len < MAX_PATH - 1)) {
        i = len - 1;
        while ((gFilename[i] != '\\') && (gFilename[i] != '/') && (i >= 0)) i--;
        snprintf(strbuf, len - i, "%s", (gFilename + i + 1));
        TextScreen_DrawText(bitmap, 0, y++, strbuf);
    }
    */
    //TextScreen_DrawText(bitmap, 0, y++, gFilename);
    
    if (gShowInfo < 2) return;
    
    snprintf(strbuf, sizeof(strbuf), "Console Size: %d x %d (%d x %d) ",
                    console_width, console_height,
                    screen.width, screen.height);
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    
    /*
    duration = 0;
    audio_codec_id = 0;
    video_codec_id = 0;
    */
    
    audio_codec_id = ppd->audio_codec_id;
    video_codec_id = ppd->video_codec_id;
    duration = ppd->duration;
    /*
    if (audio_stream_index != -1) {
        audio_codec_id = adec_ctx->codec_id;
        duration = afmt_ctx->duration;
    }
    if (video_stream_index != -1) {
        video_codec_id = dec_ctx->codec_id;
        duration = fmt_ctx->duration;
    }
    */
    if (duration == AV_NOPTS_VALUE) {
        snprintf(timestr, sizeof(timestr), "%s", " --:--:--.--- ");
    } else {
        int sign, hour, min, sec, dec;
        
        sec = duration / 1000;
        sign = (sec >= 0);
        sec = sign ? sec : -sec;
        dec = sec % 1000;
        sec = sec / 1000;
        hour = sec / 3600;
        min = (sec / 60) % 60;
        sec = sec % 60;
        snprintf(timestr, sizeof(timestr),"%c%02d:%02d:%02d.%03d", sign ? ' ' : '-', hour, min, sec, dec);
    }
    snprintf(strbuf, sizeof(strbuf), "Duration:%s ", timestr);
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    snprintf(strbuf, sizeof(strbuf), "Codec: %s/%s ", avcodec_get_name(video_codec_id), avcodec_get_name(audio_codec_id));
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
   
    if (video_stream_index != -1) {
        int num, den;
        //den = fmt_ctx->streams[video_stream_index]->avg_frame_rate.den;
        //num = fmt_ctx->streams[video_stream_index]->avg_frame_rate.num;
        den = ppd->video_avg_frame_rate_den;
        num = ppd->video_avg_frame_rate_num;
        if (den) {
            fps = (int)((int64_t)num * 100LL / (int64_t)den);
            snprintf(strbuf, sizeof(strbuf), "FPS(ave): %d.%02d ", fps / 100, fps % 100);
            TextScreen_DrawText(bitmap, 0, y++, strbuf);
        } else {
            snprintf(strbuf, sizeof(strbuf), "FPS(ave): n/a ");
            TextScreen_DrawText(bitmap, 0, y++, strbuf);
        }
    }
    snprintf(strbuf, sizeof(strbuf), "Audio Buffer: %2d ", Framebuffer_ListNum(FRAMEBUFFER_TYPE_AUDIO));
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    snprintf(strbuf, sizeof(strbuf), "Video Buffer: %2d ", Framebuffer_ListNum(FRAMEBUFFER_TYPE_VIDEO));
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    snprintf(strbuf, sizeof(strbuf), "Player Version: %s(%d), Build: %s %s ", VER_FILEVERSION_STR, (int)TEXTMOVIE_TEXTMOVIE_VERSION, __DATE__, __TIME__);
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    snprintf(strbuf, sizeof(strbuf), "by Coffey (c)2015-2016 ");
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    //snprintf(strbuf, sizeof(strbuf), "use library ffmpeg2.5(libav*), SDL-1.2.15(libSDL*) ");
    //TextScreen_DrawText(bitmap, 0, y++, strbuf);
    snprintf(strbuf, sizeof(strbuf), "[q]quit  [i]show info  [space]pause [v]peak level/playback pos (indicator) ");
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    snprintf(strbuf, sizeof(strbuf), "[w]wave view  [e]switch visual effect (wave view) ");
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    snprintf(strbuf, sizeof(strbuf), "[p]print screen (to clipboard)  [P]print screen (negative image) ");
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    snprintf(strbuf, sizeof(strbuf), "[up/down]volume  [left/right]seek  [h][Home]seek head ");
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    snprintf(strbuf, sizeof(strbuf), "[l]playlist  [b][Page Up]previous  [n][Page Down]next ");
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    snprintf(strbuf, sizeof(strbuf), "[s]shuffle list (playlist view)  [S]sort list (playlist view) ");
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    snprintf(strbuf, sizeof(strbuf), "[,][.]change base key (musical spectrum view) ");
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    /*
    snprintf(strbuf, sizeof(strbuf), "[G]decode debug mode (toggle) ");
    TextScreen_DrawText(bitmap, 0, y++, strbuf);
    */
}

void Do_DrawStatus(void)
{
    int ad, vd;
    int64_t pts;
    int hour, min, sec, dec, sign;
    int i, level;
    char strbuf[128], strbuf2[64];
    PlaylistData  *ppd;
    int64_t duration, start_time;
    
    ppd = Playlist_GetData(gAudioCurrentPlaynum);
    duration = ppd->duration;
    start_time = ppd->start_time;
    
    if (!gPause) {
        pts = (int64_t)GetTickCount() * 1000 - gStartTime;
    } else {
        pts = gPauseTime;
    }
    
    if (gBarMode == 1) {
        if (duration < 1000000) duration = 1000000;
        level = (pts - start_time) * 33 / duration;
        if (level < 0) level = 0;
        if (level > 32) level = 32;
        for (i = 0; i < level; i++) strbuf2[i] = '@';
        for (i = level; i < 32; i++) strbuf2[i] = ' ';
        strbuf2[32] = 0;
    } else {
        level = gAudioLevel / 1024;
        if (level < 0) level = 0;
        if (level > 32) level = 32;
        for (i = 0; i < level; i++) strbuf2[i] = '#';
        for (i = level; i < 32; i++) strbuf2[i] = ' ';
        strbuf2[32] = 0;
    }
    
    ad = (int)(gADiff/1000);
    //ad = (int)(gCallDiff);
    if (ad > 999) ad = 999;
    if (ad < -999) ad = -999;
    vd = (int)(gVDiff/1000);
    if (vd > 999) vd = 999;
    if (vd < -999) vd = -999;
    /*
    if (!gPause) {
        pts = (int64_t)GetTickCount() * 1000 - gStartTime;
    } else {
        pts = gPauseTime;
    }
    */
    sec = pts / 1000;
    sign = (sec >= 0);
    sec = sign ? sec : -sec;
    dec = sec % 1000;
    sec = sec / 1000;
    hour = sec / 3600;
    min = (sec / 60) % 60;
    sec = sec % 60;
    snprintf(strbuf, sizeof(strbuf),"  %c%02d:%02d:%02d.%03d  V:%3d%c D:% 4d/%3dms%c [%s]  ", 
            sign ? ' ' : '-', hour, min, sec, dec, gVolume, gAudioClip ? '@' : ' ',
            ad, vd, gFrameDrop ? '*': ' ', strbuf2);
    strbuf[78]=0;
    printf("%s", strbuf);
    
    // gAudioLevel = gAudioLevel * 7 / 8;    // (to make level meter slow release)
    gAudioClip = (gAudioClip > 0) ? gAudioClip - 1 : 0;  // (decrement clip indicator count)
    gFrameDrop = 0;
}

void AudioWave_Entry(void)
{
    TextScreenBitmap *waveBitmap = NULL;
    int  currentWaveType = 0;
    
    if (!waveBitmap) {
        MUTEX_LOCK(gMutexBitmapWave);
        waveBitmap = TextScreen_DupBitmap(gBitmapWave);
        MUTEX_UNLOCK(gMutexBitmapWave);
        if (!waveBitmap) return;
    }
    
    while(!gQuitFlag) {
        Framebuffer *abuf;
        int     stream16len;
        int16_t *stream16buf;
        
        if (currentWaveType != AudioWave_CurrentWaveType()) {
            currentWaveType = AudioWave_CurrentWaveType();
            TextScreen_ClearBitmap(waveBitmap);
        }
        
        while (Framebuffer_ListNum(FRAMEBUFFER_TYPE_AUDIOWAVE) > 1) {
            abuf = Framebuffer_Get(FRAMEBUFFER_TYPE_AUDIOWAVE);
            Framebuffer_Free(abuf);
        }
        
        abuf = Framebuffer_Get(FRAMEBUFFER_TYPE_AUDIOWAVE);
        if (abuf) {
            stream16len = abuf->size / 2;
            stream16buf = (int16_t *)abuf->data;
            AudioWave_Draw(waveBitmap, stream16buf, stream16len);
            Framebuffer_Free(abuf);
            
            MUTEX_LOCK(gMutexBitmapWave);
            if ((gBitmapWave->width != waveBitmap->width) || (gBitmapWave->height != waveBitmap->height)) {
                TextScreen_FreeBitmap(waveBitmap);
                waveBitmap = TextScreen_DupBitmap(gBitmapWave);
                if (!waveBitmap) {
                    MUTEX_UNLOCK(gMutexBitmapWave);
                    break;
                }
            }
            TextScreen_CopyBitmap(gBitmapWave, waveBitmap, 0, 0);
            MUTEX_UNLOCK(gMutexBitmapWave);
        }
        
        Sleep(10);
    }
    if (waveBitmap) TextScreen_FreeBitmap(waveBitmap);
}

int main(int argc, char *argv[])
{
    HANDLE stdinh, stdouth;
    TextScreenSetting screen;
    SDL_AudioSpec desired, obtained;
    int console_width, console_height;
//    char strbuf[256];
//    int ret;
    int64_t pts = 0;
    //PlaylistData *ppd;
    // MediaInfo minfo;
    
    if (argc < 2) {
        printf("Usage: %s <media file name>\n", TEXTMOVIE_TEXTMOVIE_NAME);
        printf("       or drag and drop media file into %s\n", TEXTMOVIE_TEXTMOVIE_NAME);
        printf("       press [i] key twice when playing for more help.\n");
        printf("\n");
        printf("Press any key to exit.\n");
        _getch();
        exit(1);
    }
    
    if (argv[0]) {
        char strbuf[MAX_PATH];
        char strbuf2[MAX_PATH];
        
        Get_DirNameFromFullpath(strbuf, sizeof(strbuf) , argv[0]);
        snprintf(strbuf2, sizeof(strbuf), "%s%s", strbuf, TEXTMOVIE_TEXTMOVIE_INITFILE_NAME);
        ReadInitFile(strbuf2);
        
        gSampleRate = gInitSampleRate;
        AudioWave_SetSampleRate(gSampleRate);
    }
    
    {  // set codec debug mode
        int i;
        char filename[MAX_PATH];
        
        gDebugDecode |= TEXTMOVIE_TEXTMOVIE_DEBUG_DECODE;
        for ( i = 1 ; i < argc; i++ ) {
            Get_FilenameFromFullpath(filename, MAX_PATH - 1, argv[i]);
            if (!strcmp(filename, "TEXTMOVIE_DEBUG")) {  // debug mode
                gDebugDecode |= 1;
            }
        }
    }
    
    // init framebuffer
    if (Framebuffer_Init()) {
        printf("Can not initialize framebuffer\n");
        exit(1);
    }
    
    // set terminate callback routine (for press ctrl+c, press close button of console window, user logout ...)
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)MyConsoleCtrlHandler, TRUE);
    
    // check Windows console in/out
    stdinh = GetStdHandle(STD_INPUT_HANDLE);
    stdouth = GetStdHandle(STD_OUTPUT_HANDLE);
    if ((!stdinh) || (!stdouth)) exit(1);
    // FlushConsoleInputBuffer(stdinh);
    
    // get console size
    Get_ConsoleSize(&console_width, &console_height);
    
    // Initialize TextScreen. (calculate text screen size from current console window size)
    TextScreen_GetSettingDefault(&screen);
    screen.renderingMethod = TEXTSCREEN_RENDERING_METHOD_FAST;
    // screen.renderingMethod = TEXTSCREEN_RENDERING_METHOD_WINCONSOLE;
    
    screen.width  = console_width - 4;   // console width  - 4
    screen.height = console_height - 3;  // console height - 3
    if (screen.width < 1) screen.width = 1;
    if (screen.height < 1) screen.height = 1;
    TextScreen_Init(&screen);
    gBitmap = TextScreen_CreateBitmap(screen.width, screen.height);  // main plane
    gBitmapWave = TextScreen_DupBitmap(gBitmap);  // audio wave plane
    
    
    // libav initialize
    
    av_register_all();
    avfilter_register_all();
    
    frame = av_frame_alloc();
    filter_frame = av_frame_alloc();
    aframe = av_frame_alloc();
    afilter_frame = av_frame_alloc();
    
    if (gDebugDecode) {
        av_log_set_level(AV_LOG_VERBOSE);
    } else {
        av_log_set_level(AV_LOG_QUIET);
    }
    
    // Initialize SDL
    // SDL_putenv("SDL_VIDEODRIVER=dummy");
    SDL_putenv("SDL_AUDIODRIVER=dsound");
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);
    
    // SDL audio setting
    desired.freq = gSampleRate;
    desired.format = AUDIO_S16LSB;
    desired.channels = 2;
    desired.samples = 2048;
    desired.callback = AudioStream_SDLCallback;
    desired.userdata = NULL;
    if (SDL_OpenAudio(&desired, &obtained) < 0) {
        printf("Can not create SDL audio: %s\n", SDL_GetError());
        exit(1);
    }
    
    // thread initialize
    if (!MUTEX_CREATE(gMutexBitmapWave)) {
        printf("Can not create mutex for AudioWave\n");
        exit(1);
    }
    if (pthread_create(&gAudioWaveTid, NULL,(void *)AudioWave_Entry, (void *)NULL)) {
        printf("Can not create AudioWave Thread\n");
        exit(1);
    }
    
    // ===== now! all initialize is successful =====
    
    
    // set start time (master clock), audio start and clear screen
    //gStartTime = (int64_t)GetTickCount() * 1000;
    //SDL_PauseAudio(0);
    if (!gDebugDecode) {
        TextScreen_ClearScreen();
    }
    
    {  // input file check
        int i;
        PlaylistData *ppd;
        MediaInfo    minfo;
        int count;
        wchar_t  **argv_w;
        int      argc_w;
        
        argv_w = CommandLineToArgvW(GetCommandLineW(), &argc_w);
        if (!argv) exit_proc();
        
        Playlist_ClearList();
        count = 0;
        for ( i = 1 ; i < argc; i++ ) {
            if (isFolder(argv_w[i])) {
                count = Playlist_Make(argv_w[i], count);
            } else {
                CheckOptionFilename(argv_w[i]);
                if (isMediaFile(argv_w[i], &minfo)) {
                    ppd = Playlist_CreateData();
                    snwprintf(ppd->filename_w, MAX_PATH - 1, L"%s", argv_w[i]);
                    ppd->filename_w[MAX_PATH - 1] = 0;
                    snprintf(ppd->filename, MAX_PATH - 1, "%s", argv[i]);
                    ppd->filename[MAX_PATH - 1] = 0;
                    CopyMediaInfo2PlaylistData(ppd, &minfo);
                    if (Playlist_AddData(ppd) == -1) {
                        Playlist_FreeData(ppd);
                    } else {
                        count++;
                    }
                }
            }
        }
        if (count) {
            if (gOptionShuffle) {
                int numlist;
                numlist = Playlist_GetNumData();
                Playlist_SetCurrentPlay(numlist - 1);
                Playlist_Shuffle();
                Playlist_SetCurrentPlay(0);
                Playlist_Shuffle();
            }
            Playlist_SetCurrentPlay(0);
            ppd = Playlist_GetData(0);
            Stream_Restart(ppd->filename_w, 0);
            // snprintf(gFilename, MAX_PATH - 1, "%s", ppd->filename);
            // gFilename[MAX_PATH - 1] = 0;
        } else {
            printf("No playable file.\n");
            Sleep(500);
            exit_proc();
        }
        
        LocalFree(argv_w);
    }
    
    // thread exection state 'system sleep' = off, 'display power off' = off
    if (1) {
        EXECUTION_STATE esFlags;
        //esFlags = ES_SYSTEM_REQUIRED | ES_CONTINUOUS;
        esFlags = ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED | ES_CONTINUOUS;
        SetThreadExecutionState(esFlags);
    }
    
    // TEST: change timer resolution
    /*
    if (0) {
        timeBeginPeriod(1);
    }
    */
    
    // loop for read stream and rendering
    while (1) {
        Framebuffer *fbuf;
        TextScreenBitmap *bitmap;
        PlaylistData *ppd;
        int skip;
        //int cuemargin;
        
        // quit check
        if (gQuitFlag) break;
        
        /*
        cuemargin = 16;
        ppd = Playlist_GetData(Playlist_GetCurrentPlay());
        if (ppd) {
            if ((ppd->audio_codec_id == AV_CODEC_ID_WMALOSSLESS) ||
                (ppd->audio_codec_id == AV_CODEC_ID_WMAV1) ||
                (ppd->audio_codec_id == AV_CODEC_ID_WMAV2) ||
                (ppd->audio_codec_id == AV_CODEC_ID_WMAVOICE) ||
                (ppd->audio_codec_id == AV_CODEC_ID_WMAPRO)) {
                cuemargin = 8;
            }
        }
        */
        // no more presentation then loop end and quit (playlist: play next)
        if (gReadDoneAudio && gReadDoneVideo && 
                    (Framebuffer_ListNum(FRAMEBUFFER_TYPE_AUDIO) < 16) &&
                    !Framebuffer_ListNum(FRAMEBUFFER_TYPE_VIDEO) ) {
            if (Playlist_GetCurrentPlay() >= 0) {
                if( Framebuffer_ListNum(FRAMEBUFFER_TYPE_AUDIO) ) {
                    Playlist_SetNextCurrentPlay();
                    ppd = Playlist_GetData(Playlist_GetCurrentPlay());
                    if (!ppd->video || (ppd->video && isVideoStillPicture(ppd->video_codec_id))) {
                        Stream_Restart(ppd->filename_w, 1);  // try audio gapless
                    } else {
                        Playlist_SetPreviousCurrentPlay();
                    }
                } else {
                    Playlist_SetNextCurrentPlay();
                    ppd = Playlist_GetData(Playlist_GetCurrentPlay());
                    Stream_Restart(ppd->filename_w, 0);
                }
            } else {
                if( !Framebuffer_ListNum(FRAMEBUFFER_TYPE_AUDIO) ) {
                    break;
                }
            }
        }
        
        // read audio and video
        {  // experimental 20150228  audio read 3times of video one.
            int i;
            for (i = 0; i < 3; i++ ) {
                if (!gReadDoneAudio && !gPause) {
                    if (AudioStream_ReadAndBuffer() < 0) {
                        gReadDoneAudio = 1;
                    }
                }
            }
        }
        if (!gReadDoneVideo && !gPause) {
            if (VideoStream_ReadAndBuffer() < 0) {
                gReadDoneVideo = 1;
            }
        }
        gSeeked = 0;
        
        /*
        {  // draw wave
            Framebuffer *abuf;
            int     stream16len;
            int16_t *stream16buf;
            
            while (Framebuffer_ListNum(FRAMEBUFFER_TYPE_AUDIOWAVE) > 1) {
                abuf = Framebuffer_Get(FRAMEBUFFER_TYPE_AUDIOWAVE);
                Framebuffer_Free(abuf);
            }
            
            abuf = Framebuffer_Get(FRAMEBUFFER_TYPE_AUDIOWAVE);
            if (abuf) {
                stream16len = abuf->size / 2;
                stream16buf = (int16_t *)abuf->data;
                AudioWave_Draw(gBitmapWave, stream16buf, stream16len);
                Framebuffer_Free(abuf);
            }
        }
        */
        
        // video (character) rendering
        //if (!gPause && !gDebugDecode) {
        if (1 && !gDebugDecode) {
            if (Framebuffer_ListNum(FRAMEBUFFER_TYPE_VIDEO) && !gPause) {
                pts = Framebuffer_GetPts(FRAMEBUFFER_TYPE_VIDEO);
                
                if ((gReadDoneAudio || isFramebuffer_Full(FRAMEBUFFER_TYPE_AUDIO)) && (gReadDoneVideo || isFramebuffer_Full(FRAMEBUFFER_TYPE_VIDEO))) {
                    int64_t stime;
                    stime = pts - ((int64_t)GetTickCount() * 1000 - gStartTime) - (10*1000);
                    if (stime > 100000) stime = 100000;
                    if (stime > 0) Sleep(stime / 1000);
                }
                
                if (pts < ((int64_t)GetTickCount() * 1000 - gStartTime)) {
                    gVDiff = ((int64_t)GetTickCount() * 1000 - gStartTime) - pts;
                    
                    skip = 0;
                    if ((video_stream_index != -1) && 1) {  // frame skip (experimental 20150228)
                        int num, den, fps, dur;
                        den = fmt_ctx->streams[video_stream_index]->avg_frame_rate.den;
                        num = fmt_ctx->streams[video_stream_index]->avg_frame_rate.num;
                        if (den) {
                            fps = num / den;
                            if (fps >= 12) {
                                dur = 1000000 / fps;
                                if (gVDiff > dur * 3) {  // 3 frames late then skip next picture
                                    while ((Framebuffer_ListNum(FRAMEBUFFER_TYPE_VIDEO) > 0) && 
                                            (Framebuffer_GetPts(FRAMEBUFFER_TYPE_VIDEO) < ((int64_t)GetTickCount() * 1000 - gStartTime))) {
                                        fbuf = Framebuffer_Get(FRAMEBUFFER_TYPE_VIDEO);
                                        if (fbuf) {
                                            TextScreen_FreeBitmap((TextScreenBitmap *)fbuf->data);
                                            Framebuffer_Free(fbuf);
                                        }
                                    }
                                    /*
                                    fbuf = Framebuffer_Get(FRAMEBUFFER_TYPE_VIDEO);
                                    if (fbuf) {
                                         TextScreen_FreeBitmap((TextScreenBitmap *)fbuf->data);
                                         Framebuffer_Free(fbuf);
                                    }
                                    */
                                    //printf("Skipped ");
                                    gFrameDrop = 1;
                                    skip = 1;
                                }
                            }
                        }
                    }
                    
                    if (!skip) {
                        fbuf = Framebuffer_Get(FRAMEBUFFER_TYPE_VIDEO);
                        if (fbuf) {
                            if (gBitmapClip) {
                                TextScreen_FreeBitmap(gBitmapClip);
                                gBitmapClip = NULL;
                            }
                            if (gBitmapLastVideo) {
                                TextScreen_FreeBitmap(gBitmapLastVideo);
                                gBitmapLastVideo = NULL;
                            }
                            bitmap = (TextScreenBitmap *)fbuf->data;
                            gBitmapLastVideo = TextScreen_DupBitmap(bitmap);
                            
                            if (gShowPlaylist) {
                                if (!gBitmapList) gBitmapList = TextScreen_DupBitmap(gBitmap);
                                TextScreen_ClearBitmap(gBitmap);
                                Do_DrawPlaylist(gBitmap);
                                if (TextScreen_CompareBitmap(gBitmap, gBitmapList, 0, 0)) {
                                    TextScreen_ShowBitmap(gBitmap, 0, 0);
                                    if (gBitmapList) TextScreen_FreeBitmap(gBitmapList);
                                    gBitmapList = TextScreen_DupBitmap(gBitmap);
                                } else {
                                    TextScreen_SetCursorPos(0, 0);
                                }
                                gBitmapClip = TextScreen_DupBitmap(gBitmap);
                            } else {
                                if (gShowWave) {
                                    {
                                        MUTEX_LOCK(gMutexBitmapWave);
                                        TextScreen_CopyBitmap(bitmap, gBitmapWave, 0, 0);
                                        MUTEX_UNLOCK(gMutexBitmapWave);
                                        Do_DrawInfo(bitmap);
                                        TextScreen_ShowBitmap(bitmap, 0, 0);
                                        gBitmapClip = bitmap;
                                    }
                                } else {
                                    Do_DrawInfo(bitmap);
                                    TextScreen_ShowBitmap(bitmap, 0, 0);
                                    gBitmapClip = bitmap;
                                }
                            }
                            Framebuffer_Free(fbuf);
                            TextScreen_SetCursorPos(0, gBitmap->height + screen.topMargin);
                            Do_DrawStatus();
                        }
                        //Do_DrawStatus();
                    }
                }
            } else {
                
                if ((gReadDoneAudio || isFramebuffer_Full(FRAMEBUFFER_TYPE_AUDIO)) && 
                    (gReadDoneVideo || isFramebuffer_Full(FRAMEBUFFER_TYPE_VIDEO)) && !gPause) {
                    int64_t stime;
                    
                    stime = pts - ((int64_t)GetTickCount() * 1000 - gStartTime) - (0*1000);
                    if ((stime > 100000) || (stime < -100000)) {
                        pts = ((int64_t)GetTickCount() * 1000 - gStartTime);
                    }
                    if (stime > 100000) stime = 100000;
                    if (stime > 0) Sleep(stime / 1000);
                }
                
                if ((pts < ((int64_t)GetTickCount() * 1000 - gStartTime)) || gPause) {
                    if (!gPause) {
                        gVDiff = ((int64_t)GetTickCount() * 1000 - gStartTime) - pts;
                    } else {
                        gVDiff = 0;
                    }
                    if (gBitmapClip) {
                        TextScreen_FreeBitmap(gBitmapClip);
                        gBitmapClip = NULL;
                    }
                    if (gShowPlaylist) {
                        if (!gBitmapList) gBitmapList = TextScreen_DupBitmap(gBitmap);
                        TextScreen_ClearBitmap(gBitmap);
                        Do_DrawPlaylist(gBitmap);
                        if (TextScreen_CompareBitmap(gBitmap, gBitmapList, 0, 0)) {
                            TextScreen_ShowBitmap(gBitmap, 0, 0);
                            if (gBitmapList) TextScreen_FreeBitmap(gBitmapList);
                            gBitmapList = TextScreen_DupBitmap(gBitmap);
                        } else {
                            TextScreen_SetCursorPos(0, 0);
                        }
                        gBitmapClip = TextScreen_DupBitmap(gBitmap);
                    } else {
                        if (gShowWave || (video_stream_index == -1)) {
                            {
                                MUTEX_LOCK(gMutexBitmapWave);
                                TextScreen_CopyBitmap(gBitmap, gBitmapWave, 0, 0);
                                MUTEX_UNLOCK(gMutexBitmapWave);
                                Do_DrawInfo(gBitmap);
                                TextScreen_ShowBitmap(gBitmap, 0, 0);
                                gBitmapClip = TextScreen_DupBitmap(gBitmap);
                            }
                        } else {
                            
                            if (gBitmapLastVideo) {
                                TextScreen_ClearBitmap(gBitmap);
                                TextScreen_CopyBitmap(gBitmap, gBitmapLastVideo, 0, 0);
                            } else {
                                TextScreen_ClearBitmap(gBitmap);
                            }
                            
                            Do_DrawInfo(gBitmap);
                            TextScreen_ShowBitmap(gBitmap, 0, 0);
                            gBitmapClip = TextScreen_DupBitmap(gBitmap);
                        }
                    }
                    TextScreen_SetCursorPos(0, gBitmap->height + screen.topMargin);
                    Do_DrawStatus();
                    pts = (int64_t)GetTickCount() * 1000 - gStartTime + 40000;
                }
            }
        }
        
        Do_Resize_Check();
        Do_Keyboard_Check();
        if (gPause) {
            Sleep(40);
        } else {
            Sleep(0);
        }
        // SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED | ES_CONTINUOUS);
    }
    
    exit_proc();
    exit(0);
}

