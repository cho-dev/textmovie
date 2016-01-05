/*
    textscreen.h (textscreen library C version)
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

#ifndef TEXTSCREEN_TEXTSCREEN_H
#define TEXTSCREEN_TEXTSCREEN_H

#define TEXTSCREEN_TEXTSCREEN_VERSION 20151030

// max bitmap width and height
#define TEXTSCREEN_MAXSIZE 32768

// rendering method (output method)
enum TextScreenRenderingMethod {
    TEXTSCREEN_RENDERING_METHOD_FAST,        // fast speed
    TEXTSCREEN_RENDERING_METHOD_NORMAL,      // normal speed. good quality
    TEXTSCREEN_RENDERING_METHOD_SLOW,        // output character 1 by 1 (use fputc)
    TEXTSCREEN_RENDERING_METHOD_WINCONSOLE,  // use Windows console api (very fast, use WriteConsole())
    TEXTSCREEN_RENDERING_METHOD_NB           // number of method
};

typedef struct TextScreenSetting {
    // character code for space: use for ClearBitmap, except character for OverlayBitmap, ...
    char space;
    // screen width
    int  width;
    // screen height
    int  height;
    // screen top margin
    int  topMargin;
    // screen left margin
    int  leftMargin;
    // sample aspect ratio (use for DrawCircle)
    int  sar;
    // rendering method
    int  renderingMethod;
    // translate table
    char *translate;
} TextScreenSetting;

typedef struct TextScreenBitmap {
    // bitmap width
    int width;
    // bitmap height
    int height;
    // extra user data  Note: This handle will not free automatically by TextScreen_FreeBitmap(). so manage by user.
    void *exdata;
    // bitmap data handle (size = width x height). Create by TextScreen_CreateBitmap()
    char *data;
} TextScreenBitmap;

// initialize TextScreen library (NULL: use default setting)
void TextScreen_Init(TextScreenSetting *usersetting);
// clear console
int TextScreen_ClearScreen(void);
// get console size,  return  0: successful 1: error(could not get, width and height is set to default)
int TextScreen_GetConsoleSize(int *width, int *height);
// set cursor position (x, y),  (left, top) = (0, 0)
int TextScreen_SetCursorPos(int x, int y);
// set visible/hide cursor  0:hide  1:visible
int TextScreen_SetCursorVisible(int visible);
// wait for ms millisecond
void TextScreen_Wait(unsigned int ms);
// get tick count (msec)
unsigned int TextScreen_GetTickCount(void);
// set space character to ch
void TextScreen_SetSpaceChar(char ch);
// get copy of current settings
void TextScreen_GetSetting(TextScreenSetting *setting);
// get copy of default settings
void TextScreen_GetSettingDefault(TextScreenSetting *setting);

// ******** bitmap draw tools ********
// bitmap handle=bitmap; center position=(x,y);  radius=r;  draw character=ch
void TextScreen_DrawCircle(const TextScreenBitmap *bitmap, int x, int y, int r, char ch);
// bitmap handle=bitmap; left=x; top=y; width=w; height=h; draw character=ch
void TextScreen_DrawFillRect(const TextScreenBitmap *bitmap, int x, int y, int w, int h, char ch);
// bitmap handle=bitmap; left=x; top=y; width=w; height=h; draw character=ch; clean flag=clean(0:none,1:fill space)
void TextScreen_DrawBorderRect(const TextScreenBitmap *bitmap, int x, int y, int w, int h, char ch, int clean);
// bitmap handle=bitmap; draw position(x1,y1) to (x2,y2); draw character=ch
void TextScreen_DrawLine(const TextScreenBitmap *bitmap, int x1, int y1, int x2, int y2, char ch);
// bitmap handle=bitmap; draw position(x,y); text string=str
void TextScreen_DrawText(const TextScreenBitmap *bitmap, int x, int y, const char *str);
// bitmap handle=bitmap; get character at (x,y)
char TextScreen_GetCell(const TextScreenBitmap *bitmap, int x, int y);
// bitmap handle=bitmap; put character ch to (x,y)
void TextScreen_PutCell(const TextScreenBitmap *bitmap, int x, int y, char ch);
// bitmap handle=bitmap; clear (x,y)
void TextScreen_ClearCell(const TextScreenBitmap *bitmap, int x, int y);

// ******** bitmap tools ********
// create bitmap handle (width x height)
TextScreenBitmap *TextScreen_CreateBitmap(int width, int height);
// free bitmap handle
void TextScreen_FreeBitmap(TextScreenBitmap *bitmap);
// copy srcmap to dstmap(dx, dy) (not create new bitmap)
void TextScreen_CopyBitmap(const TextScreenBitmap *dstmap, const TextScreenBitmap *srcmap, int dx, int dy);
// duplicate bitmap handle (create new bitmap and copy)   Note: member 'exdata' will not duplicate cause unknown its size
TextScreenBitmap *TextScreen_DupBitmap(const TextScreenBitmap *bitmap);
// copy srcmap to dstmap(dx, dy) except null character
void TextScreen_OverlayBitmap(const TextScreenBitmap *dstmap, const TextScreenBitmap *srcmap, int dx, int dy);
// crop bitmap; position(x, y)  size=(w x h),  return 0:successful  -1:failed
int TextScreen_CropBitmap(TextScreenBitmap *bitmap, int x, int y, int width, int height);
// resize bitmap; size=(w x h),  return 0:successful  -1:failed
int TextScreen_ResizeBitmap(TextScreenBitmap *bitmap, int width, int height);
// compare srcmap and dstmap(dx, dy),  return 0:same   1,-1:different
int TextScreen_CompareBitmap(const TextScreenBitmap *dstmap, const TextScreenBitmap *srcmap, int dx, int dy);
// clear bitmap (fill space character)
void TextScreen_ClearBitmap(const TextScreenBitmap *bitmap);
// show bitmap to console. position of console(0,0) = bitmap(dx,dy),  return 0:successful  -1:error
int TextScreen_ShowBitmap(const TextScreenBitmap *bitmap, int dx, int dy);

#endif

/* simple usage of this library ----------------------------------------

// build command (sample.c is this sample, sample.exe is executable)
// gcc sample.c textscreen.c -o sample.exe

#include "textscreen.h"

int main(void)
{
    TextScreenSetting setting;                // setting struct values
    TextScreenBitmap  *bitmap;                // bitmap pointer
    
    TextScreen_GetSettingDefault(&setting);   // get default settings
    TextScreen_Init(&setting);                // initialize with 'setting'
    bitmap = TextScreen_CreateBitmap(setting.width, setting.height);  // create bitmap same size of setting
    TextScreen_ClearScreen();                 // clear console text
    
    TextScreen_DrawFillRect(bitmap, 6, 6, 20, 10, '#');  // draw rectangle left,top(6,6) width 20, height 10
    TextScreen_PutCell(bitmap, 6, 16, '@');              // put one char to position(6,16)
    TextScreen_DrawText(bitmap, 8, 16, "Rectangle !");   // draw text at (8,16)
    
    TextScreen_ShowBitmap(bitmap, 0, 0);      // show bitmap to console
    TextScreen_Wait(500);                     // wait 500ms
    TextScreen_ShowBitmap(bitmap, 2, 2);      // show bitmap to console. base position = (2,2) of bitmap
    TextScreen_Wait(500);                     // wait 500ms
    TextScreen_ShowBitmap(bitmap, 4, 4);      // show bitmap to console. base position = (4,4) of bitmap
    TextScreen_Wait(500);                     // wait 500ms
    
    TextScreen_FreeBitmap(bitmap);            // free bitmap
}
*/

