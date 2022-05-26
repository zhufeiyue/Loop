#pragma once
#include <common/BufPool.h>
#include <common/Dic.h>
#include "IDecoder.h"

class IRender
{
public:
	typedef typename Dictionary RenderInfo;

public:
	virtual ~IRender() {}
	virtual int ConfigureRender(RenderInfo)      = 0;
	virtual int UpdataFrame(FrameHolderPtr data) = 0;

	virtual int Start() = 0;
	virtual int Stop()  = 0;
	virtual int Pause(bool) = 0;
	virtual int Reset()                 = 0;
	virtual int GetRenderTime(int64_t&) = 0;
};

class IAudioRender : public IRender
{
public:
	virtual int GetVolume(int&) = 0;
	virtual int SetVolume(int) = 0;
	virtual int Flush() = 0;
	virtual int GetUseableDuration(int32_t&) = 0;
};