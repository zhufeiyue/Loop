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
	virtual int ConfigureRender(RenderInfo) = 0;
	virtual int UpdataFrame(FrameHolderPtr data) = 0;
};