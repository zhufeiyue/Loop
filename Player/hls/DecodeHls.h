#pragma once
#include "IDecoder.h"
#include "HLSPlaylist.h"

class DecodeHls : public IDecoder
{
public:
	int InitDecoder(std::string, Callback) override;
	int DestroyDecoder(Callback) override;
	int Seek(int64_t, int64_t, Callback) override;
	int GetNextFrame(FrameHolderPtr&, int) override;
	bool EnableVideo(bool) override;
	bool EnableAudio(bool) override;
};