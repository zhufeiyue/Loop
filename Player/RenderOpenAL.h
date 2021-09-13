#pragma once

#include <string>
#include "al.h"
#include "alc.h"


class OpenALDevice
{
public:
	OpenALDevice(std::string);
	~OpenALDevice();

private:
	ALCdevice* m_pDevice = NULL;
	ALCcontext* m_pContext = NULL;
};