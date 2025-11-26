/*
 * Copyright (C) 2005 to 2013 by Jonathan Duddington
 * email: jonsd@users.sourceforge.net
 * Copyright (C) 2015-2017 Reece H. Dunn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see: <http://www.gnu.org/licenses/>.
 */

#ifndef ESPEAK_NG_SPEAK_LIB_H
#define ESPEAK_NG_SPEAK_LIB_H

#ifdef __cplusplus
extern "C" {
#endif

#define ESPEAK_API_REVISION  12

/****************************************************************************/
/* ESPEAK_EVENT                                                             */
/****************************************************************************/

typedef enum {
    espeakEVENT_LIST_TERMINATED = 0,
    espeakEVENT_WORD = 1,
    espeakEVENT_SENTENCE = 2,
    espeakEVENT_MARK = 3,
    espeakEVENT_PLAY = 4,
    espeakEVENT_END = 5,
    espeakEVENT_MSG_TERMINATED = 6,
    espeakEVENT_PHONEME = 7,
    espeakEVENT_SAMPLERATE = 8
} espeak_EVENT_TYPE;

typedef struct {
    espeak_EVENT_TYPE type;
    unsigned int unique_identifier;
    int text_position;
    int length;
    int audio_position;
    int sample;
    void *user_data;
    union {
        int number;
        const char *name;
        char string[8];
    } id;
} espeak_EVENT;

/****************************************************************************/
/* ESPEAK_POSITION                                                          */
/****************************************************************************/

typedef enum {
    POS_CHARACTER = 1,
    POS_WORD = 2,
    POS_SENTENCE = 3
} espeak_POSITION_TYPE;

/****************************************************************************/
/* ESPEAK_AUDIO_OUTPUT                                                      */
/****************************************************************************/

typedef enum {
    AUDIO_OUTPUT_PLAYBACK = 0,
    AUDIO_OUTPUT_RETRIEVAL = 1,
    AUDIO_OUTPUT_SYNCHRONOUS = 2,
    AUDIO_OUTPUT_SYNCH_PLAYBACK = 3
} espeak_AUDIO_OUTPUT;

/****************************************************************************/
/* ESPEAK_ERROR                                                             */
/****************************************************************************/

typedef enum {
    EE_OK = 0,
    EE_INTERNAL_ERROR = -1,
    EE_BUFFER_FULL = 1,
    EE_NOT_FOUND = 2
} espeak_ERROR;

/****************************************************************************/
/* ESPEAK_PARAMETER                                                         */
/****************************************************************************/

typedef enum {
    espeakSILENCE = 0,
    espeakRATE = 1,
    espeakVOLUME = 2,
    espeakPITCH = 3,
    espeakRANGE = 4,
    espeakPUNCTUATION = 5,
    espeakCAPITALS = 6,
    espeakWORDGAP = 7,
    espeakOPTIONS = 8,
    espeakINTONATION = 9,
    espeakRESERVED1 = 10,
    espeakRESERVED2 = 11,
    espeakEMPHASIS = 12,
    espeakLINELENGTH = 13,
    espeakVOICETYPE = 14,
    N_SPEECH_PARAM = 15
} espeak_PARAMETER;

/****************************************************************************/
/* ESPEAK_PUNCT_TYPE                                                        */
/****************************************************************************/

typedef enum {
    espeakPUNCT_NONE = 0,
    espeakPUNCT_ALL = 1,
    espeakPUNCT_SOME = 2
} espeak_PUNCT_TYPE;

/****************************************************************************/
/* ESPEAK_VOICE                                                             */
/****************************************************************************/

typedef struct {
    const char *name;
    const char *languages;
    const char *identifier;
    unsigned char gender;
    unsigned char age;
    unsigned char variant;
    unsigned char xx1;
    int score;
    void *spare;
} espeak_VOICE;

/****************************************************************************/
/* ESPEAK FLAGS                                                             */
/****************************************************************************/

#define espeakCHARS_AUTO     0
#define espeakCHARS_UTF8     1
#define espeakCHARS_8BIT     2
#define espeakCHARS_WCHAR    3
#define espeakCHARS_16BIT    4

#define espeakSSML          0x10
#define espeakPHONEMES      0x100
#define espeakENDPAUSE      0x1000
#define espeakKEEP_NAMEDATA 0x2000

/****************************************************************************/
/* FUNCTIONS                                                                */
/****************************************************************************/

typedef int (t_espeak_callback)(short *wav, int numsamples, espeak_EVENT *events);

#ifdef _WIN32
#define ESPEAK_API __declspec(dllimport)
#else
#define ESPEAK_API
#endif

ESPEAK_API int espeak_Initialize(espeak_AUDIO_OUTPUT output, int buflength, const char *path, int options);

ESPEAK_API void espeak_SetSynthCallback(t_espeak_callback *SynthCallback);

ESPEAK_API espeak_ERROR espeak_SetParameter(espeak_PARAMETER parameter, int value, int relative);

ESPEAK_API int espeak_GetParameter(espeak_PARAMETER parameter, int current);

ESPEAK_API espeak_ERROR espeak_SetVoiceByName(const char *name);

ESPEAK_API espeak_ERROR espeak_SetVoiceByProperties(espeak_VOICE *voice_spec);

ESPEAK_API espeak_VOICE *espeak_GetCurrentVoice(void);

ESPEAK_API espeak_ERROR espeak_Synth(const void *text, size_t size,
    unsigned int position, espeak_POSITION_TYPE position_type,
    unsigned int end_position, unsigned int flags,
    unsigned int *unique_identifier, void *user_data);

ESPEAK_API espeak_ERROR espeak_Synth_Mark(const void *text, size_t size,
    const char *index_mark, unsigned int end_position,
    unsigned int flags, unsigned int *unique_identifier, void *user_data);

ESPEAK_API espeak_ERROR espeak_Key(const char *key_name);

ESPEAK_API espeak_ERROR espeak_Char(wchar_t character);

ESPEAK_API espeak_ERROR espeak_Cancel(void);

ESPEAK_API int espeak_IsPlaying(void);

ESPEAK_API espeak_ERROR espeak_Synchronize(void);

ESPEAK_API espeak_ERROR espeak_Terminate(void);

ESPEAK_API const char *espeak_Info(const char **path_data);

ESPEAK_API const espeak_VOICE **espeak_ListVoices(espeak_VOICE *voice_spec);

#ifdef __cplusplus
}
#endif

#endif // ESPEAK_NG_SPEAK_LIB_H
