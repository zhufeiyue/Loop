#include "RenderOpenAL.h"

OpenALDevice::OpenALDevice(std::string strDeviceName)
{
	m_pDevice = alcOpenDevice(NULL);
	if (!m_pDevice)
	{
		alGetError();
	}
}

OpenALDevice::~OpenALDevice()
{
}