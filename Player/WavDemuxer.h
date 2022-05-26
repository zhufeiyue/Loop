#pragma once

#include <common/Log.h>

#ifdef _WIN32
#include <Windows.h>
#include <mmreg.h>
#else
#error "not windows"
#endif

class WavDemuxerFile
{
public:
	WavDemuxerFile(std::string fileName);

	const WAVEFORMATEXTENSIBLE& AudioFormat() const
	{
		return m_format;
	}

	int ReadFormat();
	int ReadSample(uint8_t* buf, uint32_t frameWant, uint32_t& frameGot);

protected:
	int Seek(uint32_t offset, int type = std::fstream::beg);
	int Read(uint8_t* pBuf, uint32_t want, uint32_t& got);

protected:
	WAVEFORMATEXTENSIBLE m_format = { 0 };
	uint32_t m_iRIFFChunkSize = 0;
	uint32_t m_iFmtChunkSize = 0;
	uint32_t m_iDataChunkSize = 0;
	uint32_t m_iSampleCount = 0;
	std::fstream m_fileIn;
};