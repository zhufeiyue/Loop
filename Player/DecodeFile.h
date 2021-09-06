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
	int Seek(int64_t) override;
	int GetNextFrame(FrameHolderPtr&, int) override;

protected:
	int DecodeVideoFrame();

private:
	std::thread m_threadWork;
	std::unique_ptr<Eventloop> m_pEventLoop;
	std::unique_ptr<FFmpegDecode> m_pDecoder;
	std::atomic<int> m_iCachedFrameCount = 0;
	bool m_bDecoding = false;
	bool m_bDecodeError = false;
	FramePool m_cachedVideoFrame;
	FramePool m_blankVideoFrame;
};