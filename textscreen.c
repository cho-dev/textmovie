/*
    textscreen.c (textscreen library C version)
    Copyright (C) 2015  by Coffey

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


    VERSION 20151030
    
    Windows     : Win2K or later
    Non Windows : console support ANSI escape sequence
*/

#ifdef _WIN32
#include <windows.h>
#else
// for use nanosleep()
// #define _POSIX_C_SOURCE 199309L
// #define _POSIX_C_SOURCE 199506L
// for use snprintf()
#define _POSIX_C_SOURCE 200112L
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "textscreen.h"

// default settings
// default console size of most platform is usually 80x25 or 80x24
// this setting is best for it
#define SCREEN_DEFAULT_SIZE_X             76
#define SCREEN_DEFAULT_SIZE_Y             22
#define SCREEN_DEFAULT_TOP_MARGIN         1
#define SCREEN_DEFAULT_LEFT_MARGIN        2
#define SCREEN_DEFAULT_SAR                2
#define SCREEN_DEFAULT_SPACE_CHAR         ' '
#ifdef _WIN32
// #define SCREEN_DEFAULT_RENDERING_METHOD   TEXTSCREEN_RENDERING_METHOD_WINCONSOLE
#define SCREEN_DEFAULT_RENDERING_METHOD   TEXTSCREEN_RENDERING_METHOD_FAST
#else
#define SCREEN_DEFAULT_RENDERING_METHOD   TEXTSCREEN_RENDERING_METHOD_FAST
#endif

// translate table
// <0x20(control code), 0x7f(del), 0xfd<  ===> space(' ')
static unsigned char gTranslateTable[256] = {
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x20,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0x20, 0x20, 0x20 };

static TextScreenSetting gSetting = {0};

void TextScreen_Init(TextScreenSetting *usersetting)
{
    if (!usersetting) {
        TextScreen_GetSettingDefault(&gSetting);
    } else {
        gSetting = *usersetting;
    }
}

int TextScreen_ClearScreen(void)
{
#ifdef _WIN32
    HANDLE stdh;
    COORD  coord;
    CONSOLE_SCREEN_BUFFER_INFO info;
    DWORD bufsize;
    DWORD len;
    
    stdh = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!stdh) return -1;
    GetConsoleScreenBufferInfo(stdh, &info);
    
    // fill console buffer to space
    bufsize = info.dwSize.X * info.dwSize.Y;
    coord.X = 0;
    coord.Y = 0;
    FillConsoleOutputCharacter(stdh, ' ', bufsize, coord, &len);
    
    // set cursor position to (0,0)
    coord.X = 0;
    coord.Y = 0;
    SetConsoleCursorPosition(stdh ,coord);
#else
    printf("\x1b""c");
//  printf("\x1b[2J");
    printf("\x1b[1;1H");
#endif
    return 0;
}

// TextScreen_GetConsoleSize() is currently Windows only
int TextScreen_GetConsoleSize(int *width, int *height)
{
#ifdef _WIN32
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
        *width  = info.srWindow.Right-info.srWindow.Left + 1;
        *height = info.srWindow.Bottom-info.srWindow.Top + 1;
    } else {
        *width  = 80;
        *height = 25;
        return -1;
    }
    return 0;
#else
    struct winsize ws;
    if (ioctl(0, TIOCGWINSZ, &ws) != -1) {
        *width  = ws.ws_col;
        *height = ws.ws_row;
    } else {
        *width  = 80;
        *height = 25;
        return -1;
    }
    return 0;
#endif
}

int TextScreen_SetCursorPos(int x, int y)
{
#ifdef _WIN32
    HANDLE stdouth;
    COORD  coord;
    CONSOLE_SCREEN_BUFFER_INFO info;
    int    width, height;
    int    ret;
    
    stdouth = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!stdouth) return -1;
    
    ret = GetConsoleScreenBufferInfo(stdouth, &info);
    if (ret) {
        width  = info.srWindow.Right-info.srWindow.Left + 1;
        height = info.srWindow.Bottom-info.srWindow.Top + 1;
    } else {
        return -1;
    }
    
    if (x < 0) x = 0;
    if (x >= width) x = width - 1;
    if (y < 0) y = 0;
    if (y >= height) y = height - 1;
    
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(stdouth ,coord);
#else
    char strbuf[32];
    
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > 32767) x = 32767;
    if (y > 32767) y = 32767;
    snprintf(strbuf, sizeof(strbuf), "\x1b[%d;%dH", y+1, x+1);
    printf("%s", strbuf);   // set cursor position "\x1b[yy;xxH"
#endif
    return 0;
}

int TextScreen_SetCursorVisible(int visible)
{
#ifdef _WIN32
    HANDLE stdouth;
    CONSOLE_CURSOR_INFO cursorinfo;  // DWORD dwSize, BOOL bVisible
    
    stdouth = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!stdouth) return -1;
    
    GetConsoleCursorInfo(stdouth, &cursorinfo);
    cursorinfo.bVisible = (visible != 0);
    SetConsoleCursorInfo(stdouth, &cursorinfo);
    return 0;
#else
    if (visible) {
        printf("\x1b[?25h");  // show cursor
    } else {
        printf("\x1b[?25l");  // hide cursor
    }
    return 0;
#endif
}

void TextScreen_Wait(unsigned int ms)
{
#ifdef _WIN32
    /*
    DWORD tick, start_tick;
    
    start_tick = GetTickCount();
    do {
        tick = GetTickCount();
        if (tick < start_tick) {  // if GetTickCount() was overflow
            ms = ms - (0xffffffffUL - start_tick + 1);
            start_tick = tick;
        }
    } while(tick - start_tick < ms);
    */
    Sleep(ms);
#else
    struct timespec t;
    
    t.tv_sec  = ms / 1000;
    t.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&t, NULL);
#endif
}

unsigned int TextScreen_GetTickCount(void)
{
#ifdef _WIN32
    DWORD tick;
    
    tick = GetTickCount();
    return tick;
#else
    struct timeval tv;
    
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
#endif
}

void TextScreen_SetSpaceChar(char ch)
{
    gSetting.space = ch;
}

void TextScreen_GetSetting(TextScreenSetting *setting)
{
    *setting = gSetting;
}

void TextScreen_GetSettingDefault(TextScreenSetting *setting)
{
    int width, height;
    
    TextScreen_GetConsoleSize(&width, &height);
    
    setting->space      = SCREEN_DEFAULT_SPACE_CHAR;
    // setting->width      = SCREEN_DEFAULT_SIZE_X;
    // setting->height     = SCREEN_DEFAULT_SIZE_Y;
    setting->width      = width  - (SCREEN_DEFAULT_LEFT_MARGIN * 2);
    setting->height     = height - (SCREEN_DEFAULT_TOP_MARGIN * 2);
    setting->topMargin  = SCREEN_DEFAULT_TOP_MARGIN;
    setting->leftMargin = SCREEN_DEFAULT_LEFT_MARGIN;
    setting->sar        = SCREEN_DEFAULT_SAR;
    setting->renderingMethod = SCREEN_DEFAULT_RENDERING_METHOD;
    setting->translate  = (char *)gTranslateTable;
}

void TextScreen_DrawCircle(const TextScreenBitmap *bitmap, int x, int y, int r, char ch)
{
    int xmin, xmax, ymin, ymax;
    int xc, yc;
    
    if (!bitmap) return;
    xmin = x - r * gSetting.sar;
    xmax = x + r * gSetting.sar;
    ymin = y - r;
    ymax = y + r;
    
    if (xmin < 0) xmin = 0;
    if (xmax > bitmap->width) xmax = bitmap->width;
    if (ymin < 0) ymin = 0;
    if (ymax > bitmap->height) ymax = bitmap->height;

    for (yc = ymin; yc < ymax; yc++) {
        for (xc = xmin; xc < xmax; xc++) {
            if ((y - yc)*(y - yc) + (x - xc)*(x - xc) / (gSetting.sar * gSetting.sar) < r*r)
                TextScreen_PutCell(bitmap, xc, yc, ch);
        }
    }
}

void TextScreen_DrawFillRect(const TextScreenBitmap *bitmap, int x, int y, int w, int h, char ch)
{
    int xmin, xmax, ymin, ymax;
    int xc, yc;
    
    if (!bitmap) return;
    xmin = x;
    xmax = x + w;
    ymin = y;
    ymax = y + h;
    
    if (xmin < 0) xmin = 0;
    if (xmax > bitmap->width) xmax = bitmap->width;
    if (ymin < 0) ymin = 0;
    if (ymax > bitmap->height) ymax = bitmap->height;

    for (yc = ymin; yc < ymax; yc++) {
        for (xc = xmin; xc < xmax; xc++) {
            TextScreen_PutCell(bitmap, xc, yc, ch);
        }
    }
}

void TextScreen_DrawBorderRect(const TextScreenBitmap *bitmap, int x, int y, int w, int h, char ch, int clean)
{
    int xc, yc;
    
    if (!bitmap) return;
    for (xc = x; xc < x + w; xc++) {
        TextScreen_PutCell(bitmap, xc, y        , ch);
        TextScreen_PutCell(bitmap, xc, y + h - 1, ch);
    }
    for (yc = y; yc < y + h; yc++) {
        TextScreen_PutCell(bitmap, x        , yc, ch);
        TextScreen_PutCell(bitmap, x + w - 1, yc, ch);
    }
    
    if (clean) {
        TextScreen_DrawFillRect(bitmap, x+1, y+1, w-2, h-2, gSetting.space); 
    }
}

void TextScreen_DrawLine(const TextScreenBitmap *bitmap, int x1, int y1, int x2, int y2, char ch)
{
    int xd, yd;
    int xda, yda;
    int xn, yn;
    int x, y;
    
    xd = x2 - x1;
    yd = y2 - y1;
    xda = (xd >= 0) ? xd : -xd;
    yda = (yd >= 0) ? yd : -yd;
    
    if (!bitmap) return;
    if (!xd) {
        if (yd >= 0) {
            for (y = y1; y <= y2; y++)
                TextScreen_PutCell(bitmap, x1, y, ch);
        } else {
            for (y = y1; y >= y2; y--)
                TextScreen_PutCell(bitmap, x1, y, ch);
        }
        return;
    }
    if (!yd) {
        if (xd >= 0) {
            for (x = x1; x <= x2; x++)
                TextScreen_PutCell(bitmap, x, y1, ch);
        } else {
            for (x = x1; x >= x2; x--)
                TextScreen_PutCell(bitmap, x, y1, ch);
        }
        return;
    }
    
    if (xda >= yda) {
       if ((xd >= 0) && (yd >= 0)) {
           for (x = x1; x <= x2; x++) {
               yn = y1 + ((yda*(x-x1)*256)/(xda) +128)/256;
               TextScreen_PutCell(bitmap, x, yn, ch);
           }
       }
       if ((xd >= 0) && (yd < 0)) {
           for (x = x1; x <= x2; x++) {
               yn = y1 - ((yda*(x-x1)*256)/(xda) +128)/256;
               TextScreen_PutCell(bitmap, x, yn, ch);
           }
       }
       if ((xd < 0) && (yd >= 0)) {
           for (x = x1; x >= x2; x--) {
               yn = y1 + ((yda*(x1-x)*256)/(xda) +128)/256;
               TextScreen_PutCell(bitmap, x, yn, ch);
           }
       }
       if ((xd < 0) && (yd < 0)) {
           for (x = x1; x >= x2; x--) {
               yn = y1 - ((yda*(x1-x)*256)/(xda) +128)/256;
               TextScreen_PutCell(bitmap, x, yn, ch);
           }
       }
    } else {
       if ((xd >= 0) && (yd >= 0)) {
           for (y = y1; y <= y2; y++) {
               xn = x1 + ((xda*(y-y1)*256)/(yda) +128)/256;
               TextScreen_PutCell(bitmap, xn, y, ch);
           }
       }
       if ((xd < 0) && (yd >= 0)) {
           for (y = y1; y <= y2; y++) {
               xn = x1 - ((xda*(y-y1)*256)/(yda) +128)/256;
               TextScreen_PutCell(bitmap, xn, y, ch);
           }
       }
       if ((xd >= 0) && (yd < 0)) {
           for (y = y1; y >= y2; y--) {
               xn = x1 + ((xda*(y1-y)*256)/(yda) +128)/256;
               TextScreen_PutCell(bitmap, xn, y, ch);
           }
       }
       if ((xd < 0) && (yd < 0)) {
           for (y = y1; y >= y2; y--) {
               xn = x1 - ((xda*(y1-y)*256)/(yda) +128)/256;
               TextScreen_PutCell(bitmap, xn, y, ch);
           }
       }
    }
}

void TextScreen_DrawText(const TextScreenBitmap *bitmap, int x, int y, const char *str)
{
    if (!bitmap) return;
    while (*str != '\0') {
        TextScreen_PutCell(bitmap, x, y, *str);
        x++;
        str++;
    }
}

char TextScreen_GetCell(const TextScreenBitmap *bitmap, int x, int y)
{
    if (!bitmap) return 0;
    if ((x >= 0) && (x < bitmap->width) && (y >= 0) && (y < bitmap->height))
        return *(bitmap->data + y * bitmap->width + x);
    else
        return 0;
}

void TextScreen_PutCell(const TextScreenBitmap *bitmap, int x, int y, char ch)
{
    if (!bitmap) return;
    if ((x >= 0) && (x < bitmap->width) && (y >= 0) && (y < bitmap->height))
        *(bitmap->data + y * bitmap->width + x) = ch;
}

void TextScreen_ClearCell(const TextScreenBitmap *bitmap, int x, int y)
{
    if (!bitmap) return;
    if ((x >= 0) && (x < bitmap->width) && (y >= 0) && (y < bitmap->height))
        *(bitmap->data + y * bitmap->width + x) = gSetting.space;
}

TextScreenBitmap *TextScreen_CreateBitmap(int width, int height)
{
    TextScreenBitmap *bitmap;
    char *data;
    
    if ((width < 1) || (width > TEXTSCREEN_MAXSIZE) || (height < 1) || (height > TEXTSCREEN_MAXSIZE)) {
        return NULL;
    }
    bitmap = (TextScreenBitmap *)malloc(sizeof(TextScreenBitmap));
    data   = (char *)malloc(width * height);
    if (!bitmap || !data) {
        if (bitmap) free(bitmap);
        if (data)   free(data);
        return NULL;
    }
    
    bitmap->width  = width;
    bitmap->height = height;
    bitmap->exdata = NULL;
    bitmap->data   = data;
    TextScreen_ClearBitmap(bitmap);
    
    return bitmap;
}

void TextScreen_FreeBitmap(TextScreenBitmap *bitmap)
{
    if (bitmap) {
        if (bitmap->data)
            free(bitmap->data);
        free(bitmap);
    }
}

void TextScreen_CopyBitmap(const TextScreenBitmap *dstmap, const TextScreenBitmap *srcmap, int dx, int dy)
{
    int x, y;
    
    if (!srcmap || !dstmap) return;
    
    for (y = 0; y < srcmap->height; y++) {
        for (x = 0; x < srcmap->width; x++) {
            TextScreen_PutCell(dstmap, x + dx, y + dy, TextScreen_GetCell(srcmap, x, y));
        }
    }
}

TextScreenBitmap *TextScreen_DupBitmap(const TextScreenBitmap *bitmap)
{
    TextScreenBitmap *newmap;
    
    if (!bitmap) return NULL;
    
    newmap = TextScreen_CreateBitmap(bitmap->width, bitmap->height);
    if (newmap) {
        TextScreen_CopyBitmap(newmap, bitmap, 0, 0);
    }
    return newmap;
}

void TextScreen_OverlayBitmap(const TextScreenBitmap *dstmap, const TextScreenBitmap *srcmap, int dx, int dy)
{
    char ch;
    int x, y;
    
    if (!srcmap || !dstmap) return;
    
    for (y = 0; y < srcmap->height; y++) {
        for (x = 0; x < srcmap->width; x++) {
            ch = TextScreen_GetCell(srcmap, x, y);
            if (ch != gSetting.space)
                TextScreen_PutCell(dstmap, x + dx, y + dy, ch);
        }
    }
}

int TextScreen_CropBitmap(TextScreenBitmap *bitmap, int x, int y, int width, int height)
{
    char *data, *olddata;
    int  oldwidth, oldheight;
    int  xc, yc;
    char ch;
    
    if (!bitmap) return -1;
    if ((width < 1) || (width > TEXTSCREEN_MAXSIZE) || (height < 1) || (height > TEXTSCREEN_MAXSIZE)) {
        return -1;
    }
    data   = (char *)malloc(width * height);
    if (!data) return -1;
    
    oldwidth  = bitmap->width;
    oldheight = bitmap->height;
    olddata   = bitmap->data;
    
    bitmap->width  = width;
    bitmap->height = height;
    bitmap->data   = data;
    
    TextScreen_ClearBitmap(bitmap);
    
    for (yc = 0; yc < height; yc++) {
        for (xc = 0; xc < width; xc++) {
            if (((xc + x) >= 0) && ((yc + y) >= 0) && ((xc + x) < oldwidth) && ((yc + y) < oldheight)) {
                ch = *(olddata + ((yc + y) * oldwidth + (xc + x)));
                *(data + (yc * width + xc)) = ch;
            }
        }
    }
    
    free(olddata);
    
    return 0;
}

int TextScreen_ResizeBitmap(TextScreenBitmap *bitmap, int width, int height)
{
    char *data, *olddata;
    int  oldwidth, oldheight;
    int  xc, yc;
    char ch;
    
    if (!bitmap) return -1;
    if ((width < 1) || (width > TEXTSCREEN_MAXSIZE) || (height < 1) || (height > TEXTSCREEN_MAXSIZE)) {
        return -1;
    }
    data   = (char *)malloc(width * height);
    if (!data) return -1;
    
    oldwidth  = bitmap->width;
    oldheight = bitmap->height;
    olddata   = bitmap->data;
    
    bitmap->width  = width;
    bitmap->height = height;
    bitmap->data   = data;
    
    TextScreen_ClearBitmap(bitmap);
    // scaling with nearest neighbor
    for (yc = 0; yc < height; yc++) {
        for (xc = 0; xc < width; xc++) {
            ch = *(olddata + ((oldheight * yc / height) * oldwidth) + (oldwidth * xc / width));
            *(data + (yc * width + xc)) = ch;
        }
    }
    
    free(olddata);
    
    return 0;
}

int TextScreen_CompareBitmap(const TextScreenBitmap *dstmap, const TextScreenBitmap *srcmap, int dx, int dy)
{
    int  x, y;
    char chsrc, chdst;
    
    if (!srcmap || !dstmap) return -1;
    
    for (y = 0; y < srcmap->height; y++) {
        for (x = 0; x < srcmap->width; x++) {
            chsrc = TextScreen_GetCell(srcmap, x, y);
            chdst = TextScreen_GetCell(dstmap, x + dx, y + dy);
            if (chsrc > chdst) return 1;
            if (chsrc < chdst) return -1;
        }
    }
    return 0;
}

void TextScreen_ClearBitmap(const TextScreenBitmap *bitmap)
{
    if (!bitmap) return;
    TextScreen_DrawFillRect(bitmap, 0, 0, bitmap->width, bitmap->height, gSetting.space);
}

int TextScreen_ShowBitmap(const TextScreenBitmap *bitmap, int dx, int dy)
{
    char *buf;
    char ch;
    int  index;
    int  i, x, y;
    
    if (!gSetting.width || !gSetting.height)
        TextScreen_Init(NULL);
    
    if (!bitmap) return 0;
    buf = NULL;
    switch (gSetting.renderingMethod) {  // Create buffer
        case TEXTSCREEN_RENDERING_METHOD_WINCONSOLE:
        case TEXTSCREEN_RENDERING_METHOD_FAST:
            buf = (char *)malloc( (gSetting.topMargin * 2) + 
                    (gSetting.width+gSetting.leftMargin+2) * gSetting.height + 4 );
            if (!buf) return -1;
            break;
        case TEXTSCREEN_RENDERING_METHOD_NORMAL:
            buf = (char *)malloc(gSetting.width+gSetting.leftMargin+2);
            if (!buf) return -1;
            break;
        case TEXTSCREEN_RENDERING_METHOD_SLOW:
        default:
            break;
    }
    
#ifdef _WIN32
    do {  // set cursor position to 0,0 (top-left corner)
        HANDLE stdh;
        COORD  coord;
        stdh = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdh) {
            coord.X = 0;
            coord.Y = 0;
            SetConsoleCursorPosition(stdh ,coord);
        } else {
            free(buf);
            return -1;
        }
    } while(0);
#else
    // printf("\x1b[?25l");    // hide cursor
    printf("\x1b[1;1H");   // set cursor position to 1,1 (top-left corner)
#endif

    if (gSetting.renderingMethod == TEXTSCREEN_RENDERING_METHOD_NORMAL) {
        
        for (i = 0; i < gSetting.topMargin; i++)
            printf("\n");
        
        for (y = 0; y < gSetting.height; y++) {
            index = 0;
            for (i = 0; i < gSetting.leftMargin; i++) {
                buf[index++] = ' ';
            }
            for (x = 0; x < gSetting.width; x++) {
                ch = TextScreen_GetCell(bitmap, x + dx, y + dy);
                buf[index++] = gSetting.translate[(unsigned char)ch];
            }
            buf[index++] = 0;
            printf("%s\n", buf);
        }
    }
    
    if (gSetting.renderingMethod == TEXTSCREEN_RENDERING_METHOD_SLOW) {
        
        for (i = 0; i < gSetting.topMargin; i++)
            printf("\n");
        
        for (y = 0; y < gSetting.height; y++) {
            index = 0;
            for (i = 0; i < gSetting.leftMargin; i++) {
                fputc(' ', stdout);
            }
            for (x = 0; x < gSetting.width; x++) {
                ch = TextScreen_GetCell(bitmap, x + dx, y + dy);
                fputc(gSetting.translate[(unsigned char)ch], stdout);
            }
            printf("\n");
        }
    }
    
    if (gSetting.renderingMethod == TEXTSCREEN_RENDERING_METHOD_FAST) {
        index = 0;
        for (i = 0; i < gSetting.topMargin; i++) {
#ifdef _WIN32
            // buf[index++] = 0x0d;
            buf[index++] = 0x0a;
#else
            buf[index++] = 0x0a;
#endif
        }
        for (y = 0; y < gSetting.height; y++) {
            for (i = 0; i < gSetting.leftMargin; i++) {
                buf[index++] = ' ';
            }
            for (x = 0; x < gSetting.width; x++) {
                ch = TextScreen_GetCell(bitmap, x + dx, y + dy);
                buf[index++] = gSetting.translate[(unsigned char)ch];
            }
#ifdef _WIN32
            // buf[index++] = 0x0d;
            buf[index++] = 0x0a;
#else
            buf[index++] = 0x0a;
#endif
        }
        buf[index++] = 0;
        printf("%s", buf);
    }
    
    if (gSetting.renderingMethod == TEXTSCREEN_RENDERING_METHOD_WINCONSOLE) { // Windows only
#ifdef _WIN32
        HANDLE stdh;
        DWORD  wlen;
        
        index = 0;
        for (i = 0; i < gSetting.topMargin; i++) {
            // buf[index++] = 0x0d;
            buf[index++] = 0x0a;
        }
        for (y = 0; y < gSetting.height; y++) {
            for (i = 0; i < gSetting.leftMargin; i++) {
                buf[index++] = ' ';
            }
            for (x = 0; x < gSetting.width; x++) {
                ch = TextScreen_GetCell(bitmap, x + dx, y + dy);
                buf[index++] = gSetting.translate[(unsigned char)ch];
            }
            // buf[index++] = 0x0d;
            buf[index++] = 0x0a;
        }
        stdh = GetStdHandle(STD_OUTPUT_HANDLE);
        if (stdh) {
            WriteConsole(stdh, buf, index, &wlen, NULL);
        }
#endif
    }
    
#ifdef _WIN32
#else
    // printf("\x1b[?25h");  // show cursor
#endif
    
    if (buf) free(buf);
    return 0;
}

