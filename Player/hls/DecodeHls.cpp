#include "DecodeHls.h"

int DecodeHls::InitDecoder(std::string, Callback)
{
	return CodeOK;
}

int DecodeHls::DestroyDecoder(Callback)
{
	return CodeOK;
}

int DecodeHls::Seek(int64_t, int64_t, Callback)
{
	return CodeOK;
}

int DecodeHls::GetNextFrame(FrameHolderPtr&, int)
{
	return CodeOK;
}

bool DecodeHls::EnableVideo(bool)
{
	return CodeOK;
}

bool DecodeHls::EnableAudio(bool)
{
	return CodeOK;
}