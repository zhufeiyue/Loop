#include "WavDemuxer.h"

WavDemuxerFile::WavDemuxerFile(std::string fileName)
{
	m_fileIn.open(fileName, std::fstream::in | std::fstream::binary);
}

int WavDemuxerFile::ReadFormat()
{
	uint32_t  got = 0, n = 0;
	uint8_t buf[256] = { 0 };

	if (CodeOK != Seek(0))
	{
		return CodeNo;
	}

	// RIFF
	if (CodeOK != Read(buf, 8, got) || got != 8)
	{
		return CodeNo;
	}
	if (0 != strncmp((char*)buf, "RIFF", 4))
	{
		return CodeNo;
	}
	m_iRIFFChunkSize = *(uint32_t*)(buf + 4);

	// WAVE fmt
	if (CodeOK != Read(buf, 12, got) || got != 12)
	{
		return CodeNo;
	}
	if (0 != strncmp((char*)buf, "WAVE", 4) ||
		0 != strncmp((char*)buf + 4, "fmt ", 4))
	{
		return CodeNo;
	}
	m_iFmtChunkSize = *(uint32_t*)(buf + 8); // 16 18 or 40
	if (CodeOK != Read(buf, m_iFmtChunkSize, got) || got != m_iFmtChunkSize)
	{
		return CodeNo;
	}
	m_format.Format = *(WAVEFORMATEX*)buf;
	if (m_iFmtChunkSize == 40)
	{
		m_format.Samples.wValidBitsPerSample = *(uint16_t*)(buf + 18); // 18 is sizeof(WAVEFORMATEX)
		m_format.dwChannelMask = *(uint32_t*)(buf + 20);
		m_format.SubFormat = *(GUID*)(buf + 24);
	}

	bool gotDataChunk = false;
	int tryCount = 0;
	do
	{
		if (CodeOK != Read(buf, 8, got) || got != 8)
		{
			return CodeNo;
		}
		if (strncmp((char*)buf, "data", 4) == 0)
		{
			gotDataChunk = true;
			m_iDataChunkSize = *(uint32_t*)(buf + 4);
			m_iSampleCount = m_iDataChunkSize / m_format.Format.nBlockAlign;
			return CodeOK;
		}

		n = *(uint32_t*)(buf + 4);
		if (CodeOK != Seek(n, std::fstream::cur))
		{
			return CodeNo;
		}

	} while (!gotDataChunk && ++tryCount <= 3);

	return CodeNo;
}

int WavDemuxerFile::ReadSample(uint8_t* buf, uint32_t frameWant, uint32_t& frameGot)
{
	if (m_format.Format.nBlockAlign == 0)
	{
		return CodeNo;
	}

	auto res = Read(buf, frameWant * m_format.Format.nBlockAlign, frameGot);
	frameGot /= m_format.Format.nBlockAlign;
	return res;
}

int WavDemuxerFile::Seek(uint32_t offset, int type)
{
	if (!m_fileIn.is_open() || m_fileIn.bad())
	{
		return CodeNo;
	}

	m_fileIn.seekg(offset, type);
	return CodeOK;
}

int WavDemuxerFile::Read(uint8_t* pBuf, uint32_t want, uint32_t& got)
{
	got = 0;
	if (!m_fileIn.is_open() || m_fileIn.bad() || m_fileIn.eof())
	{
		return CodeNo;
	}

	m_fileIn.read((char*)pBuf, want);
	if (m_fileIn.fail())
	{
		return CodeNo;
	}

	got = (uint32_t)m_fileIn.gcount();

	return CodeOK;
}