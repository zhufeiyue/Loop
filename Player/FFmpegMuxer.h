#pragma once
#include "FFmpegDemuxer.h"

class FFmpegMuxer
{
public:
	struct MuxerStream
	{
		AVStream* pStream = nullptr;
		AVCodec* pCodec = nullptr;
		AVCodecContext* pCodecContext = nullptr;

		int sampleSize = 10000;

		~MuxerStream()
		{
			avcodec_close(pCodecContext);
			avcodec_free_context(&pCodecContext);
		}

		void SupportedPixFormat(std::vector<AVPixelFormat>& v)
		{
			v.clear();
			if (pCodec && pCodec->pix_fmts)
			{
				//auto p1 = pCodec->profiles;
				//while (p1 != NULL && p1->profile != FF_PROFILE_UNKNOWN)
				//{
				//	std::cout << p1->name << '-' << p1->profile << std::endl;
				//	++p1;
				//}

				auto p = pCodec->pix_fmts;
				while (*p != -1)
				{
					v.push_back(*p);
					++p;
				}
			}
		}

		void SupportedSampleFormat(std::vector<AVSampleFormat>& v)
		{
			v.clear();
			if(pCodec && pCodec->sample_fmts)
			{
				auto p = pCodec->sample_fmts;
				while (*p != -1)
				{
					v.push_back(*p);
					++p;
				}
			}
		}

		void SupportedSampleRate(std::vector<int>& v)
		{
			v.clear();
			if (pCodec && pCodec->supported_samplerates)
			{
				auto p = pCodec->supported_samplerates;
				while (*p != 0)
				{
					v.push_back(*p);
					++p;
				}
			}
		}

		void SupportedChannelLayout(std::vector<uint64_t>& v)
		{
			v.clear();
			if (pCodec && pCodec->channel_layouts)
			{
				auto p = pCodec->channel_layouts;
				while (*p != 0)
				{
					v.push_back(*p);
					++p;
				}
			}
		}
	};

	FFmpegMuxer();
	virtual ~FFmpegMuxer();
	virtual int Init(std::string);
	virtual int Destroy();

	virtual int DoRead(uint8_t* buf, int buf_size);
	virtual int DoWrite(uint8_t* buf, int buf_size);
	virtual int64_t DoSeek(int64_t offset, int whence);

protected:
	virtual AVIOContext* CreateCustomIO();

	virtual MuxerStream* CreateVideoMuxerStream();
	virtual int OpenVideoStream(MuxerStream*);
	virtual MuxerStream* CreateAudioMuxerStream();
	virtual int OpenAudioStream(MuxerStream*);

protected:
	AVFormatContext* m_pFormatContext = nullptr;
	AVIOContext* m_pCustomIO = nullptr; 
	MuxerStream* m_pVStream = nullptr;
	MuxerStream* m_pAStream = nullptr;
};

class FFmpegMuxerImage : public FFmpegMuxer
{
public:
	int InitMuxerImage(std::string, int width, int height, AVPixelFormat fmt, double frameRate = 1.0);
	int DestroyMuxerImage();
	int EncodeImage(AVFrame* pFrame);

protected:
	MuxerStream* CreateAudioMuxerStream() override { return nullptr; };
	MuxerStream* CreateVideoMuxerStream() override { return FFmpegMuxer::CreateVideoMuxerStream(); }
	int OpenVideoStream(MuxerStream*) override;

protected:
	int m_iWidth = 1280;
	int m_iHeight = 720;
	double m_dFrameRate = 1.0;
	int64_t m_iFrameCount = 0;
	AVPixelFormat m_rawVideoDataFormat = AV_PIX_FMT_NONE;
	std::unique_ptr<FFmpegImageScale> m_pScale;
};

class FFmpegMuxerAudio : public FFmpegMuxer
{
public:
	int InitMuxerAudio(std::string, int sampleRate, uint64_t channelLayout, AVSampleFormat);
	int DestroyMuxerAudio();
	int EncodeAudioSample(AVFrame*);

protected:
	MuxerStream* CreateVideoMuxerStream() override { return nullptr; }
	MuxerStream* CreateAudioMuxerStream() override { return FFmpegMuxer::CreateAudioMuxerStream(); };
	int OpenAudioStream(MuxerStream*) override;

protected:
	int m_iSampleRate = 48000;
	uint64_t m_iChannelLayout = 3;
	AVSampleFormat m_rawAudioDataFormat = AV_SAMPLE_FMT_NONE;
	std::unique_ptr<FFmpegAudioConvert> m_pConvert;
	int64_t m_iSampleCount = 0;
};