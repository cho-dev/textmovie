/*
    audiowave.c , part of textmovie (play movie with console. for Windows)
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

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "audiowave.h"

// FT_COSINETABLE_LENGTH must be power of 2
#define FT_COSINETABLE_LENGTH 2048

static int gAudioWaveType = 0;
static int gAudioWaveSpectrumBase = 3;  // use in AudioWave_SpectrumTone()
static int gAudioWaveSampleRate = 48000;

void AudioWave_SetSampleRate(int freq) {
    gAudioWaveSampleRate = freq;
}

int AudioWave_CurrentWaveType(void) {
    return gAudioWaveType;
}

void AudioWave_NextWaveType(void) {
    gAudioWaveType++;
    if (gAudioWaveType >= NUMBER_OF_AUDIOWAVE_TYPE)
        gAudioWaveType = 0;
}

void AudioWave_PreviousWaveType(void) {
    gAudioWaveType--;
    if (gAudioWaveType < 0)
        gAudioWaveType = NUMBER_OF_AUDIOWAVE_TYPE - 1;
}

void AudioWave_SetWaveType(int waveType) {
    gAudioWaveType = waveType;
    if (gAudioWaveType < 0) gAudioWaveType = 0;
    if (gAudioWaveType >= NUMBER_OF_AUDIOWAVE_TYPE) gAudioWaveType = NUMBER_OF_AUDIOWAVE_TYPE - 1;
}

void AudioWave_NextSpectrumBase(void) {
    if (gAudioWaveType == AUDIOWAVE_SPECTRUM_TONE) {
	    gAudioWaveSpectrumBase++;
	    if (gAudioWaveSpectrumBase > 6)
	        gAudioWaveSpectrumBase = 6;
	}
}

void AudioWave_PreviousSpectrumBase(void) {
    if (gAudioWaveType == AUDIOWAVE_SPECTRUM_TONE) {
	    gAudioWaveSpectrumBase--;
	    if (gAudioWaveSpectrumBase < 1)
	        gAudioWaveSpectrumBase = 1;
	}
}

void AudioWave_SetSpectrumBase(int base) {
    if (base > 6) base = 6;
    if (base < 1) base = 1;
    gAudioWaveSpectrumBase = base;
}

void AudioWave_Wave(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len);
void AudioWave_Circle(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len);
void AudioWave_ScrollPeak(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len);
void AudioWave_ScrollRms(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len);
void AudioWave_Spectrum(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len);
void AudioWave_SpectrumTone(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len);


void AudioWave_Draw(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len)
{
    switch (gAudioWaveType) {
        case AUDIOWAVE_WAVE:
            AudioWave_Wave(wavebitmap, stream16buf, stream16len);
            break;
        case AUDIOWAVE_CIRCLE:
            AudioWave_Circle(wavebitmap, stream16buf, stream16len);
            break;
        case AUDIOWAVE_SCROLL_PEAK:
            AudioWave_ScrollPeak(wavebitmap, stream16buf, stream16len);
            break;
        case AUDIOWAVE_SCROLL_RMS:
            AudioWave_ScrollRms(wavebitmap, stream16buf, stream16len);
            break;
        case AUDIOWAVE_SPECTRUM:
            AudioWave_Spectrum(wavebitmap, stream16buf, stream16len);
            break;
        case AUDIOWAVE_SPECTRUM_TONE:
            AudioWave_SpectrumTone(wavebitmap, stream16buf, stream16len);
            break;
        default:
            break;
    }
}

void AudioWave_Wave(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len)
{
    // show LR waveform
    int32_t sdat32;
    int x, y, yoffset;
    int lpos, rpos;
    int count, maxdrawcount;
    TextScreenBitmap *bitmap;
    
    bitmap = TextScreen_CreateBitmap(wavebitmap->width, wavebitmap->height);
    if (!bitmap) return;
    
    lpos = bitmap->height * 1 / 4;   // Left  draw offset
    rpos = bitmap->height * 3 / 4;   // Right draw offset
    
    maxdrawcount = (stream16len > (bitmap->width * 2)) ? bitmap->width * 2 : stream16len;
    
    for (count = 0; count < maxdrawcount; count++) {
        sdat32 = (int32_t)stream16buf[count];   // int16_t to int32_t to prevent overflow
        x = count / 2;
        y = sdat32 * bitmap->height / 65536 / 2;
        if (count % 2) {
            yoffset = rpos;
        } else {
            yoffset = lpos;
        }
        if (x)
            TextScreen_DrawLine(bitmap, x, yoffset - y, x, yoffset + y, '#');
        if ((sdat32 < 512) && (sdat32 > -512))
            TextScreen_PutCell(bitmap, x, yoffset, '-');
    }
    TextScreen_PutCell(bitmap, 0, lpos, 'L');
    TextScreen_PutCell(bitmap, 0, rpos, 'R');
    
    TextScreen_CopyBitmap(wavebitmap, bitmap, 0, 0);
	TextScreen_FreeBitmap(bitmap);
}

void AudioWave_Circle(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len)
{
    // show LR circle
    int32_t sdat32;
    int lpeak, rpeak;
    int lpos, rpos, ypos;
    int count;
    int lr, rr;
    TextScreenBitmap *bitmap;
    
    bitmap = TextScreen_CreateBitmap(wavebitmap->width, wavebitmap->height);
    if (!bitmap) return;
    
    lpos = bitmap->width * 1 / 4;   // Left  draw x offset
    rpos = bitmap->width * 3 / 4;   // Right draw x offset
    ypos = bitmap->height / 2;      // draw y offset
    
    rpeak = 0;
    lpeak = 0;
    for (count = 0; count < stream16len; count++) {
        sdat32 = (int32_t)stream16buf[count];   // int16_t to int32_t to prevent overflow
        sdat32 = (sdat32 > 0) ? sdat32 : -sdat32;
        if (count % 2) {
            if (sdat32 > rpeak) rpeak = sdat32;
        } else {
            if (sdat32 > lpeak) lpeak = sdat32;
        }
    }
    lr = bitmap->width * lpeak / 32768 / 8;
    rr = bitmap->width * rpeak / 32768 / 8;
    TextScreen_DrawCircle(bitmap, lpos, ypos, lr, '.');
    TextScreen_DrawCircle(bitmap, rpos, ypos, rr, '.');
    lr -= 2;
    rr -= 2;
    if (lr > 0)
        TextScreen_DrawCircle(bitmap, lpos, ypos, lr, ' ');
    if (rr > 0)
        TextScreen_DrawCircle(bitmap, rpos, ypos, rr, ' ');
    TextScreen_PutCell(bitmap, lpos, ypos, 'L');
    TextScreen_PutCell(bitmap, rpos, ypos, 'R');
    
    TextScreen_CopyBitmap(wavebitmap, bitmap, 0, 0);
	TextScreen_FreeBitmap(bitmap);
}

void AudioWave_ScrollPeak(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len)
{
	// show LR peak level scroll
    int32_t sdat32;
    int lpos, rpos;
    static int lpeak, rpeak;
    int count;
    static int packetcount = 0;
    int interval = 5;
    TextScreenBitmap *bitmap;
    
    bitmap = TextScreen_CreateBitmap(wavebitmap->width, wavebitmap->height);
    if (!bitmap) return;
    
    if (packetcount % interval == 0) {
	    lpeak = 0;
	    rpeak = 0;
	}
    for (count = 0; count < stream16len; count++) {
        sdat32 = (int32_t)stream16buf[count];   // int16_t to int32_t to prevent overflow
        sdat32 = (sdat32 > 0) ? sdat32 : -sdat32;
        if (count % 2) {
            if (sdat32 > rpeak) rpeak = sdat32;
        } else {
            if (sdat32 > lpeak) lpeak = sdat32;
        }
    }
    
    if (packetcount % interval == (interval - 1)) {
        lpos = bitmap->height / 2 - 1;   // Left  draw x offset
	    rpos = bitmap->height - 1;       // Right draw x offset
	    
        TextScreen_CopyBitmap(bitmap, wavebitmap, -1, 0);
        TextScreen_DrawLine(bitmap, bitmap->width - 1, 0, bitmap->width - 1, bitmap->height - 1, ' ');
        
        {
            char strbuf[32];
            int ldb, rdb;
            
            // dB(V) = 20log10(peak x)
            // ldb,rdb = dB * 10    ex. -235 -> -23.5dB
            // +0.1 = to prevent log(0) error
            ldb = (int)( log10( ((double)lpeak + 0.1) / 32768 ) * 200 );
            rdb = (int)( log10( ((double)rpeak + 0.1) / 32768 ) * 200 );
            
            if (ldb < -1000) ldb = -1000;
            if (rdb < -1000) rdb = -1000;
            snprintf(strbuf, sizeof(strbuf), " -%d.%01ddB ", (-ldb)/10, (-ldb) % 10);
            TextScreen_DrawText(bitmap, 8, lpos, strbuf);
            snprintf(strbuf, sizeof(strbuf), " -%d.%01ddB ", (-rdb)/10, (-rdb) % 10);
            TextScreen_DrawText(bitmap, 8, rpos, strbuf);
        }
        
	    TextScreen_DrawLine(bitmap, bitmap->width - 1, lpos, bitmap->width - 1, lpos - bitmap->height * lpeak / 2 / 32768, '#');
	    TextScreen_DrawLine(bitmap, bitmap->width - 1, rpos, bitmap->width - 1, rpos - bitmap->height * rpeak / 2 / 32768, '#');
	    TextScreen_DrawText(bitmap, 0, lpos, "[L peak]");
	    TextScreen_DrawText(bitmap, 0, rpos, "[R peak]");
    } else {
        TextScreen_CopyBitmap(bitmap, wavebitmap, 0, 0);
    }
    packetcount++;
    
    TextScreen_CopyBitmap(wavebitmap, bitmap, 0, 0);
	TextScreen_FreeBitmap(bitmap);
}

void AudioWave_ScrollRms(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len)
{
	// show LR power level scroll
    int32_t sdat32;
    static int64_t rsum, lsum, sumcount;
    int rpowave, lpowave;
    int lpos, rpos;
    int count;
    static int packetcount = 0;
    int interval = 5;
    TextScreenBitmap *bitmap;
    
    bitmap = TextScreen_CreateBitmap(wavebitmap->width, wavebitmap->height);
    if (!bitmap) return;
    
    if (packetcount % interval == 0) {
	    lsum = 0;
	    rsum = 0;
	    sumcount = 0;
	}
    for (count = 0; count < stream16len; count++) {
        sdat32 = (int32_t)stream16buf[count];   // int16_t to int32_t to prevent overflow
        sdat32 = (sdat32 > 0) ? sdat32 : -sdat32;
        if (count % 2) {
            rsum += (sdat32 * sdat32);
        } else {
            lsum += (sdat32 * sdat32);
        }
        sumcount++;
    }
    
    if (packetcount % interval == (interval - 1)) {
        lpos = bitmap->height / 2 - 1;   // Left  draw x offset
	    rpos = bitmap->height - 1;       // Right draw x offset
	    
        TextScreen_CopyBitmap(bitmap, wavebitmap, -1, 0);
        TextScreen_DrawLine(bitmap, bitmap->width - 1, 0, bitmap->width - 1, bitmap->height - 1, ' ');
        
        
        // calculate mean power of sound
        // rms = 10log10(mean square x)
        // lpowave,rpowave = dB * 10    ex. -235 -> -23.5dB
        // +0.1 = to prevent log(0) error
        // 12.34223 = ( log10(2~15*2~15) + log10(sample=2048) )
        lpowave = (int)( (log10( ((double)lsum + 0.1) / (double)interval ) -12.34223) * 100 );
        rpowave = (int)( (log10( ((double)rsum + 0.1) / (double)interval ) -12.34223) * 100 );
        
        if (lpowave < -1000) lpowave = -1000;
        if (rpowave < -1000) rpowave = -1000;
        
        {
            char strbuf[32];
            snprintf(strbuf, sizeof(strbuf), " -%d.%01ddB ", (-lpowave)/10, (-lpowave) % 10);
            TextScreen_DrawText(bitmap, 7, lpos, strbuf);
            snprintf(strbuf, sizeof(strbuf), " -%d.%01ddB ", (-rpowave)/10, (-rpowave) % 10);
            TextScreen_DrawText(bitmap, 7, rpos, strbuf);
        }
        
        // make offset to draw (show -5dB to -40dB)
        lpowave += 400;
        rpowave += 400;
        if (lpowave < 0) lpowave = 0;
        if (lpowave > 350) lpowave = 350;
        if (rpowave < 0) rpowave = 0;
        if (rpowave > 350) rpowave = 350;
        
	    TextScreen_DrawLine(bitmap, bitmap->width - 1, lpos, bitmap->width - 1, lpos - bitmap->height * lpowave / 2 / 350, '#');
	    TextScreen_DrawLine(bitmap, bitmap->width - 1, rpos, bitmap->width - 1, rpos - bitmap->height * rpowave / 2 / 350, '#');
	    TextScreen_DrawText(bitmap, 0, lpos, "[L rms]");
	    TextScreen_DrawText(bitmap, 0, rpos, "[R rms]");
    } else {
        TextScreen_CopyBitmap(bitmap, wavebitmap, 0, 0);
    }
    packetcount++;
    
    TextScreen_CopyBitmap(wavebitmap, bitmap, 0, 0);
	TextScreen_FreeBitmap(bitmap);
}

void AudioWave_Spectrum(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len)
{
	// spectrum
    int32_t sdat32l, sdat32r;
    int64_t lpf[38], rpf[38], lpfd[38], rpfd[38];
	static int64_t costable[FT_COSINETABLE_LENGTH] = {0};
	int64_t freq, samplerate, count;
	int i, n, lpos, rpos;
	double freqd;
	int64_t   costablelen, costablemask, costablelen4;
	int64_t   countmaxa[38], countmax;
    TextScreenBitmap *bitmap;
    
    bitmap = TextScreen_CreateBitmap(wavebitmap->width, wavebitmap->height);
    if (!bitmap) return;
	
	
	samplerate = gAudioWaveSampleRate;
    
    costablelen = FT_COSINETABLE_LENGTH;
    costablemask = costablelen - 1;
    costablelen4 = costablelen / 4;
    
	if (!costable[10]) {  // make cosine table
	    double t;
	    int    i;
	    for (i = 0; i < costablelen; i++) {
	        t = 3.1415926 * 2 * i / costablelen;
	        costable[i] = (int64_t)(-cos(t) * 32768);
	    }
	}
	
	// a[sample]*sin[(sample*f*costlen/samplerate)%costlen] if sintable is 0-255
	// ( sample * f / sample * costlen ) % costlen
	// 1:  f / sample * costlen
	
	n = 17;  // 63,88,125,176,250,353,500,707,1000,1414,2000,2828,4000,5656,8000,11313,16000
	freqd = 62.5;
	freq = 63;
	//n = 37;  // 44,52,63,74,88,105,125,148,176,210,250,297,353,420,500,594,707,840,1000,
	         //1189,1414,1681,2000,2378,2828,3363,4000,4756,5656,6727,8000,9513,11313,13454,16000,19027,22627
	//freqd = 44.194174;
	//freq = 44;
	
	//freq = (int64_t)freqd;
	for (i = 0; i < n; i++ ) {
	    int64_t k1, k2, k3;
	    int  sample;
	    
	    lpf[i] = 0;
	    rpf[i] = 0;
	    lpfd[i] = 0;
	    rpfd[i] = 0;
	    k1 = (freq * costablelen * (1 << 16)) / samplerate;
	    k2 = 0;
	    countmax = samplerate * 10 / freq;
	    if (countmax > (stream16len / 2)) {
	        countmax = (stream16len / 2);
	    }
	    countmaxa[i] = countmax;
	    if ((freq * 2) > gAudioWaveSampleRate) {
	        continue;
	    }
	    
	    //for (count = 0; count < stream16len; count+=2) {
	    for (count = 0; count < (countmax * 2); count+=2) {
	        sample = (count >> 1);
	        sdat32l = (int32_t)stream16buf[count];
	        sdat32r = (int32_t)stream16buf[count+1];
	        //sdat32l = (sdat32l * (costable[sample] + 32768)) >> 16;  // hanning
	        //sdat32r = (sdat32r * (costable[sample] + 32768)) >> 16;  // hanning
	        sdat32l = (sdat32l * (costable[costablelen * sample / countmax] + 32768)) >> 16;  // hanning
	        sdat32r = (sdat32r * (costable[costablelen * sample / countmax] + 32768)) >> 16;  // hanning
	        k3 = k2 >> 16 ;
	        lpf[i] += sdat32l * costable[k3 & costablemask];  // max (1<<15)*(1<<15)=(1<<30)
	        lpfd[i] += sdat32l * costable[(costablelen4 + k3) & costablemask];
	        rpf[i] += sdat32r * costable[k3 & costablemask];
	        rpfd[i] += sdat32r * costable[(costablelen4 + k3) & costablemask];
	        k2 += k1;
	    }
	    //freqd = freqd * 1.189207115;  // 1/4 oct
	    freqd = freqd * 1.4142135624;  // 1/2 oct
	    freq = (int64_t)freqd;
	}
    
    lpos = 1;
    rpos = bitmap->width / 2 + 1;
    
    for (i = 0; i < n; i++) {
        int linelen;
        int64_t mag;
        int w;
        
        //lpf[i] = lpf[i] >> (11 + 14);  // 11 = samples (1 << 11), 14 = max 65536 (1 << 16) = (1 << 30) / (1 << 14)
        //lpfd[i] = lpfd[i] >> (11 + 14);
        if (countmaxa[i] == 0) countmaxa[i] = 1;
        lpf[i]  = (lpf[i] >> 14) / countmaxa[i];  // 11 = samples (1 << 11), 14 = max 65536 (1 << 16) = (1 << 30) / (1 << 14)
        lpfd[i] = (lpfd[i] >> 14) / countmaxa[i];
        mag = sqrt(lpf[i]*lpf[i] + lpfd[i]*lpfd[i]);
        // y axis = square root scale
        linelen = sqrt(mag) * bitmap->height / 64;  // sqr(max 65536) = max 256,  64=max scale = 1/4
        if (linelen > bitmap->height) linelen = bitmap->height;
        for (w = (bitmap->width / 2 - 2)*i/n; w < (bitmap->width / 2 - 2)*(i+1)/n; w++)
            TextScreen_DrawLine(bitmap, lpos+w, bitmap->height - 1, lpos+w, bitmap->height - 1 - linelen, '#');
        
        //rpf[i] = rpf[i] >> (11 + 14);
        //rpfd[i] = rpfd[i] >> (11 + 14);
        rpf[i]  = (rpf[i] >> 14) / countmaxa[i];
        rpfd[i] = (rpfd[i] >> 14) / countmaxa[i];
        mag = sqrt(rpf[i]*rpf[i] + rpfd[i]*rpfd[i]);
        // y axis = square root scale
        linelen = sqrt(mag) * bitmap->height / 64;
        if (linelen > bitmap->height) linelen = bitmap->height;
        for (w = (bitmap->width / 2 - 2)*i/n; w < (bitmap->width / 2 - 2)*(i+1)/n; w++)
            TextScreen_DrawLine(bitmap, rpos+w, bitmap->height - 1, rpos+w, bitmap->height - 1 - linelen, '#');
    }
    
    TextScreen_DrawText(bitmap, lpos, bitmap->height - 1, "L [sqrt/log 63Hz-1kHz-16kHz]");
    TextScreen_DrawText(bitmap, rpos, bitmap->height - 1, "R [sqrt/log 63Hz-1kHz-16kHz]");
    
    TextScreen_CopyBitmap(wavebitmap, bitmap, 0, 0);
	TextScreen_FreeBitmap(bitmap);
}

void AudioWave_SpectrumTone(TextScreenBitmap *wavebitmap, const int16_t *stream16buf, int stream16len)
{
	// doremi
    int32_t sdat32l, sdat32r;
    int64_t lpf[128], lpfd[128];
    //int64_t rpf[60], rpfd[60];
	static int64_t costable[FT_COSINETABLE_LENGTH] = {0};
	static int64_t freq[128] = {0};
	char *notename[12] = {"C.","C#","D.","D#","E.","F.","F#","G.","G#","A.","A#","B."};
	int64_t samplerate, count;
	int i, n;
	double freqd;
	int64_t   costablelen, costablemask, costablelen4;
	int64_t   countmaxa[128], countmax;
    TextScreenBitmap *bitmap;
    
    bitmap = TextScreen_CreateBitmap(wavebitmap->width, wavebitmap->height);
    if (!bitmap) return;
    
    
	samplerate = gAudioWaveSampleRate;
    
    costablelen = FT_COSINETABLE_LENGTH;
    costablemask = costablelen - 1;
    costablelen4 = costablelen / 4;
    
	if (!costable[10]) {  // make cosine table
	    double t;
	    int    i;
	    for (i = 0; i < costablelen; i++) {
	        t = 3.1415926 * 2 * i / costablelen;
	        costable[i] = (int64_t)(-cos(t) * 32768);
	    }
	}
    
	//n = 37;
	n = (bitmap->width - 2) / 2;
	if (n >= 128) n = 128;
	
	// make note freq
	if (!freq[0] || 1) {
	    freqd = 32.70319566;
	    for (i = 0; i < gAudioWaveSpectrumBase; i++) {
	        freqd = freqd * 2;
	    }
	    //freqd = 130.812783;  // base = C2
	    //freqd = 261.625566;  // base = C3
	    //freqd = 523.251131;  // base = C4
		for (i = 0; i < n; i++ ) {
		    if ((int)freqd >= (samplerate / 2)) {
		        n = i;
		        break;
		    } 
		    freq[i] = (int64_t)freqd;
		    freqd = freqd * 1.059463094359295;
		}
	}
	
	for (i = 0; i < n; i++ ) {
	    int64_t k1, k2, k3;
	    int  sample;
	    
	    lpf[i] = 0;
	    //rpf[i] = 0;
	    lpfd[i] = 0;
	    //rpfd[i] = 0;
	    k1 = (freq[i] * costablelen * (1 << 16)) / samplerate;
	    k2 = 0;
	    countmax = samplerate * 24 / freq[i];
	    if (countmax > (stream16len / 2)) countmax = (stream16len / 2);
	    countmaxa[i] = countmax;
	    //for (count = 0; count < stream16len; count+=2) {
	    for (count = 0; count < (countmax * 2); count+=2) {
	        sample = (count >> 1);
	        sdat32l = (int32_t)stream16buf[count];
	        sdat32r = (int32_t)stream16buf[count+1];
	        sdat32l = (sdat32l + sdat32r) / 2;
	        //sdat32l = (sdat32l * (costable[sample] + 32768)) >> 16;  // hanning
	        //sdat32r = (sdat32r * (costable[sample] + 32768)) >> 16;  // hanning
	        sdat32l = (sdat32l * (costable[costablelen * sample / countmax] + 32768)) >> 16;  // hanning
	        //sdat32l = (sdat32l * (32768)) >> 16;  // test (square)
	        //sdat32r = (sdat32r * (costable[costablelen * sample / countmax] + 32768)) >> 16;  // hanning
	        k3 = k2 >> 16 ;
	        lpf[i] += sdat32l * costable[k3 & costablemask];  // max (1<<15)*(1<<15)=(1<<30)
	        lpfd[i] += sdat32l * costable[(costablelen4 + k3) & costablemask];
	        //rpf[i] += sdat32r * costable[k3 & costablemask];
	        //rpfd[i] += sdat32r * costable[(costablelen4 + k3) & costablemask];
	        k2 += k1;
	    }
	}
    
    for (i = 0; i < n; i++) {
        int linelen;
        int64_t mag, sum;
        int w, wpos;
        int j;
        
        sum = 1;
        for (j = 0; j < 1; j++) {
            //lpf[i+j*12] = lpf[i+j*12] >> (11 + 14);
            //lpfd[i+j*12] = lpfd[i+j*12] >> (11 + 14);
            if (countmaxa[i] == 0) countmaxa[i] = 1;
            lpf[i]  = (lpf[i] >> 14) / countmaxa[i];  // 11 = samples (1 << 11), 14 = max 65536 (1 << 16) = (1 << 30) / (1 << 14)
            lpfd[i] = (lpfd[i] >> 14) / countmaxa[i];
            mag = sqrt(lpf[i+j*12]*lpf[i+j*12] + lpfd[i+j*12]*lpfd[i+j*12]);
            sum += mag;
            
            //rpf[i+j*12] = rpf[i+j*12] >> (11 + 14);
            //rpfd[i+j*12] = rpfd[i+j*12] >> (11 + 14);
            //mag = sqrt(rpf[i+j*12]*rpf[i+j*12] + rpfd[i+j*12]*rpfd[i+j*12]);
            //sum += mag;
        }
        linelen = sqrt(mag) * bitmap->height / 64;
        if (linelen > bitmap->height) linelen = bitmap->height;
        for (w = 0; w < 2; w++) {
            // wpos = (bitmap->width - 2) * i/n + w;
            wpos = i * 2 + w;
            TextScreen_DrawLine(bitmap, 1+wpos, bitmap->height - 1, 1+wpos, bitmap->height - 1 - linelen, notename[i%12][w]);
        }
    }
    {
        char strbuf[32];
        int  j = 0;
        while (j * 12 < n) {
            if (j) {
	            snprintf(strbuf, sizeof(strbuf), "C%d", gAudioWaveSpectrumBase + j);
	        } else {
	            snprintf(strbuf, sizeof(strbuf), "C%d(%dHz)", gAudioWaveSpectrumBase + j, (int)freq[j * 12]);
	        }
	        TextScreen_DrawText(bitmap, 1 + (j * 24), bitmap->height - 1, strbuf);
	        j++;
	    }
    }
    
    TextScreen_CopyBitmap(wavebitmap, bitmap, 0, 0);
	TextScreen_FreeBitmap(bitmap);
}

