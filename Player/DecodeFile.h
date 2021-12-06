#pragma once

#include <atomic>
#include <mutex>
#include <queue>
#include "IDecoder.h"

class Eventloop;
class FFmpegDecode;
class DecodeFile : public IDecoder
{
public:
	DecodeFile();
	~DecodeFile();

	int InitDecoder(std::string, Callback) override;
	int DestroyDecoder(Callback) override;
	int Seek(int64_t, int64_t, Callback) override;
	int GetNextFrame(FrameHolderPtr&, int) override;
	bool EnableVideo(bool) override;
	bool EnableAudio(bool) override;
protected:
	int GetNextVideoFrmae(FrameHolderPtr&);
	int DecodeVideoFrame();

	int GetNextAudioFrame(FrameHolderPtr&);
	int DecodeAudioFrame(int&);

private:
	std::thread                   m_threadWork[2];
	std::unique_ptr<Eventloop>    m_pEventLoop;
	std::unique_ptr<FFmpegDecode> m_pDecoder;

	std::atomic_int  m_iCachedFrameCount = 0;
	int       m_iMaxCacheVideoFrameCount = 0;
	bool      m_bVideoDecoding = false;
	bool      m_bVideoDecodeError = false;
	FramePool m_cachedVideoFrame;
	FramePool m_blankVideoFrame;

	std::atomic_int m_iCachedSampleCount = 0;
	int       m_iMaxCacheAudioFrameCount = 0;
	bool      m_bAudioDecoding = false;
	bool      m_bAudioDecodeError = false;
	FramePool m_cacheAudioFrame;
	FramePool m_blankAudioFrame;

	int  m_iAuioRate = 48000;
	bool m_bSeeking = false;
};