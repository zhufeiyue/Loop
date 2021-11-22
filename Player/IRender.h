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
	virtual int Pause() = 0;
	virtual int Stop()  = 0;
	virtual int Seek(int64_t)           = 0;
	virtual int GetRenderTime(int64_t&) = 0;
};