#include "../minifmod.h"
#include "FModXAudio2.h"

#if USE_XAUDIO2_ENGINE

IXAudio2* g_pXAudio2 = NULL;

unsigned int g_uBufLength;					// 存放第一次递交时的声音缓冲长度（字节数）
BYTE*        g_pBuf;						// 存放第一次递交时的声音缓冲

#if !(USE_XAUDIO2_LOOP)
HANDLE       g_hBufferEndEvent;				// 当前音频缓冲段落播放完毕时发生的事件(可用WaitForSingleObject进行阻塞等待的事件句柄)
HANDLE       g_hStreamEndEvent;				// 音频流全部播放完毕时发生的事件
BOOL         g_BufferEndEventOccor;			// 当前音频缓冲段落是否播放完毕(用于非阻塞式判断)

#define      MAX_BUFFER_COUNT 3				// 最多准备多少个缓冲
DWORD        g_CurrBufferId;				// 存放多重缓冲中下一次提交时需要填充的缓冲编号
BYTE*        g_pBufCopy[MAX_BUFFER_COUNT];	// 多重缓冲数组

class CMySourceVoiceCallback : public IXAudio2VoiceCallback
{
private:
	static CMySourceVoiceCallback* m_Singleton;
	CMySourceVoiceCallback(){g_hBufferEndEvent = CreateEvent( NULL, FALSE, FALSE, NULL );g_hStreamEndEvent = CreateEvent( NULL, FALSE, FALSE, NULL );g_BufferEndEventOccor = FALSE;}
	~CMySourceVoiceCallback(){ CloseHandle( g_hBufferEndEvent );CloseHandle( g_hStreamEndEvent );if (m_Singleton){delete m_Singleton;} }
public:
	static CMySourceVoiceCallback* GetSingleton(){if (!m_Singleton){m_Singleton = new CMySourceVoiceCallback();} return CMySourceVoiceCallback::m_Singleton; }

	// Called when the voice has just finished playing a contiguous audio stream.
	void __stdcall OnStreamEnd() { SetEvent( g_hStreamEndEvent ); }

	// Unused methods are stubs
	void __stdcall OnVoiceProcessingPassEnd() { }
	void __stdcall OnVoiceProcessingPassStart(UINT32 SamplesRequired) {    }

	// Called when current buffer processing is end
	void __stdcall OnBufferEnd(void * pBufferContext)    {
		SetEvent(g_hBufferEndEvent);
		g_BufferEndEventOccor = TRUE;
	}
	void __stdcall OnBufferStart(void * pBufferContext) {    }
	void __stdcall OnLoopEnd(void * pBufferContext) {    }
	void __stdcall OnVoiceError(void * pBufferContext, HRESULT Error) { }
};

CMySourceVoiceCallback* CMySourceVoiceCallback::m_Singleton = NULL;

#endif //#if !(USE_XAUDIO2_LOOP)



int FMUSIC_XAudio2_Init()
{
	// 创建XAudio2实例
#ifndef _XBOX
	if (S_FALSE == CoInitializeEx(NULL, COINIT_MULTITHREADED))
		return -1;
#endif
	HRESULT hr;
	if ( FAILED(hr = XAudio2Create( &g_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR ) ) )
		return hr;

	// 创建MasteringVoice
	IXAudio2MasteringVoice* pMasterVoice = NULL;
	if ( FAILED(hr = g_pXAudio2->CreateMasteringVoice( &pMasterVoice, XAUDIO2_DEFAULT_CHANNELS,
		XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, NULL ) ) )
		return hr;

	// 初始化SourceVoice句柄
	FSOUND_XAudio2_SourceVoiceHandle = NULL;
	return 0;
}

UINT FMUSIC_XAudio2_SourceVoice_Create(IXAudio2SourceVoice** out_ppSourceVoice, WAVEFORMATEX* in_pcmwf)
{
	// 创建Source Voice
	HRESULT hr;
#if USE_XAUDIO2_LOOP
	IXAudio2VoiceCallback* pVoiceCallback = NULL;
#else
	IXAudio2VoiceCallback* pVoiceCallback = CMySourceVoiceCallback::GetSingleton();
#endif
	if( FAILED(hr = g_pXAudio2->CreateSourceVoice( out_ppSourceVoice, in_pcmwf, 0, XAUDIO2_DEFAULT_FREQ_RATIO, pVoiceCallback, NULL, NULL ) ) )
	{
		return 1;
	}
	return 0;
}

UINT FMUSIC_XAudio2_SourceVoice_Destroy(IXAudio2SourceVoice** out_ppSourceVoice)
{
	(*out_ppSourceVoice)->DestroyVoice();
	out_ppSourceVoice = NULL;
	return 0;
}

UINT FMUSIC_XAudio2_SourceVoice_Start(IXAudio2SourceVoice* in_pSourceVoice)
{
	in_pSourceVoice->Start(0,NULL);

#if !(USE_XAUDIO2_LOOP)
	g_CurrBufferId = 0;
	for (int i = 0;i<MAX_BUFFER_COUNT;i++)
	{
		if (g_pBufCopy[i])
		{
			delete[] g_pBufCopy[i];
		}
	}
#endif
	return 0;
}

UINT FMUSIC_XAudio2_SourceVoice_Stop(IXAudio2SourceVoice* in_pSourceVoice)
{
	in_pSourceVoice->Stop();

#if !(USE_XAUDIO2_LOOP)
	g_CurrBufferId = 0;
	for (int i = 0;i<MAX_BUFFER_COUNT;i++)
	{
		if (g_pBufCopy[i])
		{
			delete[] g_pBufCopy[i];
		}
	}
#endif
	return 0;
}

UINT FMUSIC_XAudio2_SourceVoice_Submit_Again(IXAudio2SourceVoice* in_pSourceVoice)
{
#if USE_XAUDIO2_LOOP
	// Submit buffer
	XAUDIO2_BUFFER buf = {0};
	buf.AudioBytes = g_uBufLength;
	buf.pAudioData = g_pBuf;
	buf.LoopBegin = 0;
	buf.LoopLength = g_uBufLength/2;
	buf.LoopCount = XAUDIO2_LOOP_INFINITE;
	in_pSourceVoice->SubmitSourceBuffer( &buf );
#else
	// Make a copy of sound buffer(release previously allocated memory)
	if (g_pBufCopy[g_CurrBufferId])
	{
		delete[] g_pBufCopy[g_CurrBufferId];
	}
	g_pBufCopy[g_CurrBufferId] = new BYTE[g_uBufLength];
	memcpy(g_pBufCopy[g_CurrBufferId],g_pBuf,g_uBufLength);

	// Submit buffer
	XAUDIO2_BUFFER buf = {0};
	buf.AudioBytes = g_uBufLength;
	buf.pAudioData = g_pBufCopy[g_CurrBufferId];
	in_pSourceVoice->SubmitSourceBuffer( &buf );

	// Increase buffer id
	g_CurrBufferId++;
	if (g_CurrBufferId >= MAX_BUFFER_COUNT)
	{
		g_CurrBufferId = 0;
	}
#endif

	return 0;
}

UINT FMUSIC_XAudio2_SourceVoice_Submit_FirstTime(IXAudio2SourceVoice* in_pSourceVoice, unsigned int in_uBufLength, BYTE* in_pBuf)
{
	// Store buffer length and buffer address to global vars
	g_uBufLength = in_uBufLength;
	g_pBuf = in_pBuf;

	return FMUSIC_XAudio2_SourceVoice_Submit_Again(in_pSourceVoice);
}

// private,non api function,used by FMUSIC_XAudio2_WaitBufferEndEvent and FMUSIC_XAudio2_SourceVoice_GetSamplesPlayed
BOOL _GetSamplesPlayed(XAUDIO2_VOICE_STATE* in_pVoiceState, UINT64* out_SamplePlayed)
{
	*out_SamplePlayed = 0;
	if (in_pVoiceState)
	{
		*out_SamplePlayed = in_pVoiceState->SamplesPlayed;
		return TRUE;
	}
	return FALSE;
}

#if USE_XAUDIO2_LOOP
BOOL FMUSIC_XAudio2_SourceVoice_GetSamplesPlayed(IXAudio2SourceVoice* in_pSourceVoice, UINT64* out_SamplePlayed)
{
	XAUDIO2_VOICE_STATE state;
	in_pSourceVoice->GetState( &state );
	_GetSamplesPlayed(&state, out_SamplePlayed);

	return TRUE;
}

#else
BOOL FMUSIC_XAudio2_SourceVoice_CheckBufferEndEvent(IXAudio2SourceVoice* in_pSourceVoice, UINT64* out_SamplePlayed)
{
	XAUDIO2_VOICE_STATE state;
	in_pSourceVoice->GetState( &state );
	_GetSamplesPlayed(&state, out_SamplePlayed);

	if(state.BuffersQueued >= (MAX_BUFFER_COUNT-1))
	{
// 		WaitForSingleObject( g_hBufferEndEvent, INFINITE );
		if (g_BufferEndEventOccor == TRUE)
		{
			g_BufferEndEventOccor = FALSE;
			return TRUE;
		}
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}
#endif //#if USE_XAUDIO2_LOOP

#endif //#if USE_XAUDIO2_ENGINE