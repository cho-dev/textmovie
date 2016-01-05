/*
    audiowave.h , part of textmovie (play movie with console. for Windows)
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

#ifndef AUDIOWAVE_AUDIOWAVE_H
#define AUDIOWAVE_AUDIOWAVE_H

#include "textscreen.h"

enum AudioWaveType {
    AUDIOWAVE_WAVE,
    AUDIOWAVE_CIRCLE,
    AUDIOWAVE_SCROLL_PEAK,
    AUDIOWAVE_SCROLL_RMS,
    AUDIOWAVE_SPECTRUM,
    AUDIOWAVE_SPECTRUM_TONE,
    NUMBER_OF_AUDIOWAVE_TYPE,
};

void AudioWave_Draw(TextScreenBitmap *bitmap, const int16_t *stream16buf, int stream16len);
int  AudioWave_CurrentWaveType(void);
void AudioWave_NextWaveType(void);
void AudioWave_PreviousWaveType(void);
void AudioWave_SetWaveType(int waveType);
void AudioWave_NextSpectrumBase(void);
void AudioWave_PreviousSpectrumBase(void);
void AudioWave_SetSpectrumBase(int base);

#endif
