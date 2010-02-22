#ifndef _FMUSIC_XAUDIO2_H_
#define _FMUSIC_XAUDIO2_H_

#include "FModXAudio2Cfg.h"

#if USE_XAUDIO2_ENGINE

#ifndef _WIN32_WINNT			// 指定要求的最低平台是 Windows Vista。
#define _WIN32_WINNT 0x0600     // 将此值更改为相应的值，以适用于 Windows 的其他版本。
#endif
#include <XAudio2.h>

//= VARIABLE EXTERNS ==========================================================================
#ifdef __cplusplus
extern "C" 
{
#endif

	IXAudio2SourceVoice*	FSOUND_XAudio2_SourceVoiceHandle;

	UINT			FMUSIC_XAudio2_SourceVoice_Create(IXAudio2SourceVoice** out_pSourceVoice, WAVEFORMATEX* in_pcmwf);
	UINT			FMUSIC_XAudio2_SourceVoice_Destroy(IXAudio2SourceVoice** in_pSourceVoice);
	UINT			FMUSIC_XAudio2_SourceVoice_Start(IXAudio2SourceVoice* in_pSourceVoice);
	UINT			FMUSIC_XAudio2_SourceVoice_Stop(IXAudio2SourceVoice* in_pSourceVoice);
	UINT			FMUSIC_XAudio2_SourceVoice_Submit_FirstTime(IXAudio2SourceVoice* in_pSourceVoice, unsigned int uBufLength, BYTE* in_Buf);
	UINT			FMUSIC_XAudio2_SourceVoice_Submit_Again(IXAudio2SourceVoice* in_pSourceVoice);
#if USE_XAUDIO2_LOOP
	BOOL			FMUSIC_XAudio2_SourceVoice_GetSamplesPlayed(IXAudio2SourceVoice* in_pSourceVoice, UINT64* out_SamplePlayed);
#else
	BOOL			FMUSIC_XAudio2_SourceVoice_CheckBufferEndEvent(IXAudio2SourceVoice* in_pSourceVoice, UINT64* out_SamplePlayed);
#endif

#ifdef __cplusplus
}
#endif

#endif  //#if USE_XAUDIO2_ENGINE

#endif //_FMUSIC_XAUDIO2_H_