#include "FFmpegMuxer.h"

static int ReadIO(void* opaque, uint8_t* buf, int buf_size)
{
	auto pThis = static_cast<FFmpegMuxer*>(opaque);
	return pThis->DoRead(buf, buf_size);
}

static int WriteIO(void* opaque, uint8_t* buf, int buf_size)
{
	auto pThis = static_cast<FFmpegMuxer*>(opaque);
	return pThis->DoWrite(buf, buf_size);
}

static int64_t SeekIO(void* opaque, int64_t offset, int whence)
{
	auto pThis = static_cast<FFmpegMuxer*>(opaque);
	return pThis->DoSeek(offset, whence);
}

FFmpegMuxer::FFmpegMuxer()
{
}

FFmpegMuxer::~FFmpegMuxer()
{
}

int FFmpegMuxer::Init(std::string nameOrFormat)
{
	int n = 0;
	bool bToFile = false;

	if (!m_pFormatContext)
	{
		if (nameOrFormat.find('.') != std::string::npos)
		{
			n = avformat_alloc_output_context2(&m_pFormatContext, NULL, NULL, nameOrFormat.c_str());
			bToFile = true;
		}
		else
		{
			n = avformat_alloc_output_context2(&m_pFormatContext, NULL, nameOrFormat.c_str(), NULL);
		}

		if (n < 0)
		{
			char buf[64] = { 0 };
			av_make_error_string(buf, sizeof(buf), n);
			std::cout << buf << std::endl;
			return  n;
		}

		if (!bToFile)
		{
			m_pFormatContext->pb = CreateCustomIO();
			m_pCustomIO = m_pFormatContext->pb;
		}
		else
		{
			if (!(m_pFormatContext->oformat->flags & AVFMT_NOFILE))
			{
				n = avio_open(&m_pFormatContext->pb, nameOrFormat.c_str(), AVIO_FLAG_WRITE);
				if (n < 0)
				{
					return n;
				}
			}
		}
	}

	//m_pFormatContext->oformat->video_codec = AV_CODEC_ID_VP9;
	//m_pFormatContext->oformat->video_codec = AV_CODEC_ID_HEVC;
	m_pVStream = CreateVideoMuxerStream();
	if (m_pVStream)
	{
		OpenVideoStream(m_pVStream);
	}
	m_pAStream = CreateAudioMuxerStream();
	if (m_pAStream)
	{
		OpenAudioStream(m_pAStream);
	}

	n = avformat_write_header(m_pFormatContext, NULL);

	return n;
}

int FFmpegMuxer::Destroy()
{
	if (m_pFormatContext)
	{
		av_write_trailer(m_pFormatContext);
	}

	if (m_pVStream)
	{
		delete m_pVStream;
		m_pVStream = NULL;
	}

	if (m_pAStream)
	{
		delete m_pAStream;
		m_pAStream = NULL;
	}

	if (m_pCustomIO)
	{
		av_free(m_pCustomIO->buffer);
		avio_context_free(&m_pCustomIO);
		m_pCustomIO = NULL;
	}
	else
	{
		avio_closep(&m_pFormatContext->pb);
	}

	if (m_pFormatContext)
	{
		avformat_free_context(m_pFormatContext);
		m_pFormatContext = NULL;
	}
	return 0;
}

AVIOContext* FFmpegMuxer::CreateCustomIO()
{
	auto pBuf = av_malloc(4096);
	int bufSize = 4096;
	AVIOContext* pIO = nullptr;
	pIO = avio_alloc_context((unsigned char*)pBuf, bufSize, 1, this, ReadIO, WriteIO, SeekIO);
	if (!pIO)
	{
		av_free(pBuf);
		return NULL;
	}
	return pIO;
}

FFmpegMuxer::MuxerStream* FFmpegMuxer::CreateVideoMuxerStream()
{
	MuxerStream* ms = nullptr;

	if (!m_pFormatContext || !m_pFormatContext->oformat
		|| m_pFormatContext->oformat->video_codec == AV_CODEC_ID_NONE)
	{
		return nullptr;
	}

	ms = new MuxerStream();
	ms->pCodec = avcodec_find_encoder(m_pFormatContext->oformat->video_codec);
	ms->pStream = avformat_new_stream(m_pFormatContext, NULL);
	ms->pStream->id = m_pFormatContext->nb_streams - 1;
	ms->pCodecContext = avcodec_alloc_context3(ms->pCodec);
	//ms->pCodecContext->codec_type = AVMEDIA_TYPE_VIDEO;
	//ms->pCodecContext->codec_id = m_pFormatContext->oformat->video_codec;
	ms->pCodecContext->bit_rate = 2000000;
	ms->pCodecContext->width = 1280;
	ms->pCodecContext->height = 720;
	ms->pCodecContext->pix_fmt = *ms->pCodec->pix_fmts;
	ms->pCodecContext->time_base = ms->pStream->time_base = av_make_q(1, 25);
	ms->pCodecContext->gop_size = 25;

	if (m_pFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
	{
		ms->pCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}

	return ms;
}

int FFmpegMuxer::OpenVideoStream(MuxerStream* ms)
{
	if (!ms || !ms->pCodec || !ms->pCodecContext)
	{
		return -1;
	}

	AVDictionary* opt = NULL;
	char buf[64] = { 0 };

	av_dict_set(&opt, "crf", "23", 0);
	av_dict_set(&opt, "tune", "zerolatency", 0);
	av_dict_set(&opt, "preset", "ultrafast", 0);
	av_dict_set(&opt, "profile", "baseline", 0);

	int n = avcodec_open2(ms->pCodecContext, ms->pCodec, &opt);
	if (n < 0)
	{
		av_make_error_string(buf, sizeof(buf), n);
		std::cout << buf << std::endl;
		return n;
	}

	n = avcodec_parameters_from_context(ms->pStream->codecpar, ms->pCodecContext);
	if (n < 0)
	{
		av_make_error_string(buf, sizeof(buf), n);
		std::cout << buf << std::endl;
		return n;
	}

	return n;
}

FFmpegMuxer::MuxerStream* FFmpegMuxer::CreateAudioMuxerStream()
{
	MuxerStream* ms = NULL;

	if (!m_pFormatContext || !m_pFormatContext->oformat
		|| m_pFormatContext->oformat->audio_codec == AV_CODEC_ID_NONE)
	{
		return NULL;
	}

	ms = new MuxerStream();
	ms->pCodec = avcodec_find_encoder(m_pFormatContext->oformat->audio_codec);
	ms->pStream = avformat_new_stream(m_pFormatContext, NULL);
	ms->pStream->id = m_pFormatContext->nb_streams - 1;
	ms->pCodecContext = avcodec_alloc_context3(ms->pCodec);
	ms->pCodecContext->bit_rate = 96000;
	ms->pCodecContext->sample_rate = 44100;
	ms->pCodecContext->channel_layout = AV_CH_LAYOUT_STEREO;
	ms->pCodecContext->channels = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
	ms->pCodecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
	ms->pCodecContext->time_base = av_make_q(1, 44100);
	ms->pStream->time_base = av_make_q(1, 44100);

	if (m_pFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
	{
		ms->pCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	}
	return ms;
}

int FFmpegMuxer::OpenAudioStream(MuxerStream* ms)
{
	if (!ms || !ms->pCodec || !ms->pCodecContext)
	{
		return -1;
	}

	char buf[64] = { 0 };
	int n = avcodec_open2(ms->pCodecContext, ms->pCodec, NULL);
	if (n < 0)
	{
		av_make_error_string(buf, sizeof(buf), n);
		std::cout << buf << std::endl;
		return n;
	}

	if (ms->pCodec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
	{
		ms->sampleSize = 10000;
	}
	else
	{
		ms->sampleSize = ms->pCodecContext->frame_size;
	}

	n = avcodec_parameters_from_context(ms->pStream->codecpar, ms->pCodecContext);
	if (n < 0)
	{
		av_make_error_string(buf, sizeof(buf), n);
		std::cout << buf << std::endl;
		return n;
	}

	return n;
}

 int FFmpegMuxer::DoRead( uint8_t* buf, int buf_size)
{
	 return buf_size;
}

 int FFmpegMuxer::DoWrite( uint8_t* buf, int buf_size)
{
	 return buf_size;
}

 int64_t FFmpegMuxer::DoSeek(int64_t offset, int whence)
{
	 return 0;
}


 int FFmpegMuxerImage::InitMuxerImage(std::string s, int width, int height, AVPixelFormat pf, double frameRate)
 {
	 m_iWidth = width;
	 m_iHeight = height;
	 m_rawVideoDataFormat = pf;
	 m_dFrameRate = frameRate;

	 return Init(s);
 }

 int FFmpegMuxerImage::DestroyMuxerImage()
 {
	 AVPacket packet;
	 int n = 0;

	 if (m_pVStream && m_pVStream->pCodecContext)
	 {
		 av_init_packet(&packet);
		 while ((n = avcodec_receive_packet(m_pVStream->pCodecContext, &packet)) == 0)
		 {
			 n = av_interleaved_write_frame(m_pFormatContext, &packet);
			 av_packet_unref(&packet);
			 if (n != 0)
			 {
				 break;
			 }
		 }
	 }

	 return FFmpegMuxer::Destroy();
 }

 int FFmpegMuxerImage::EncodeImage(AVFrame* pFrame)
 {
	 if (!pFrame || !m_pVStream || !m_pVStream->pCodecContext)
	 {
		 return -1;
	 }

	 int n(0);
	 char buf[64];
	 AVFrame* pInputFrame = pFrame;
	 
	 AVPacket packet = { 0 };
	 AVPixelFormat takeFormat = m_pVStream->pCodecContext->pix_fmt;

	 if (takeFormat == AV_PIX_FMT_YUVJ420P)
	 {
		 takeFormat = AV_PIX_FMT_YUV420P;
	 }
	 if (pFrame->format != takeFormat 
		 || pFrame ->width != m_iWidth
		 || pFrame->height != m_iHeight)
	 {
		 if (!m_pScale)
		 {
			 m_pScale.reset(new FFmpegImageScale());
		 }

		 if (0 != m_pScale->Configure(pFrame->width,
			 pFrame->height,
			 (AVPixelFormat)pFrame->format,
			 m_iWidth,
			 m_iHeight,
			 takeFormat))
		 {
			 return -1;
		 }

		 n = m_pScale->Convert(pFrame->data, pFrame->linesize);
		 if (n < 0)
		 {
			 return n;
		 }

		 pInputFrame = m_pScale->Frame();
	 }

	 pInputFrame->pts = 1000 * m_iFrameCount++;

	 //av_dump_format(m_pFormatContext, 0, "test", 1);
	 n = avcodec_send_frame(m_pVStream->pCodecContext, pInputFrame);
	 if (n < 0)
	 {
		 av_strerror(n, buf, sizeof(buf));
		 std::cout << buf << std::endl;
		 goto End;
	 }

	 av_init_packet(&packet);
	 n = avcodec_receive_packet(m_pVStream->pCodecContext, &packet);
	 if (n < 0 || !packet.buf)
	 {
		 if (n == AVERROR(EAGAIN))
		 {
			 return 0;
		 }
		 av_strerror(n, buf, sizeof(buf));
		 std::cout << buf << std::endl;
		 goto End;
	 }
	 av_packet_rescale_ts(&packet, m_pVStream->pCodecContext->time_base, m_pVStream->pStream->time_base);
	 packet.stream_index = m_pVStream->pStream->index;

	 //n = av_write_frame(m_pFormatContext, &packet);
	 n = av_interleaved_write_frame(m_pFormatContext, &packet);
	 av_packet_unref(&packet);

 End:
	 return n;
 }

 int FFmpegMuxerImage::OpenVideoStream(MuxerStream* ms)
 {
	 if (AV_PIX_FMT_NONE == m_rawVideoDataFormat)
	 {
		 return -1;
	 }

	 AVPixelFormat takeFmt(AV_PIX_FMT_NONE);
	 std::vector<AVPixelFormat> v;
	 m_pVStream->SupportedPixFormat(v);
	 if (v.empty())
	 {
		 return -1;
	 }

	 auto iter = std::find(v.begin(), v.end(), m_rawVideoDataFormat);
	 if (iter != v.end())
	 {
		 takeFmt = m_rawVideoDataFormat;
	 }
	 //else if ((iter = std::find(v.begin(), v.end(), AV_PIX_FMT_YUV420P)) != v.end())
	 //{
		// takeFmt = AV_PIX_FMT_YUV420P;
	 //}
	 else
	 {
		 takeFmt = *m_pVStream->pCodec->pix_fmts;
	 }

	 m_pVStream->pCodecContext->pix_fmt = takeFmt;
	 m_pVStream->pCodecContext->width = m_iWidth;
	 m_pVStream->pCodecContext->height = m_iHeight;
	 m_pVStream->pCodecContext->time_base = m_pVStream->pStream->time_base = av_make_q(1, std::lround(m_dFrameRate * 1000));
	 m_pVStream->pCodecContext->framerate = av_make_q(std::lround(m_dFrameRate * 1000), 1000);
	 m_pVStream->pCodecContext->gop_size = std::lround(m_dFrameRate);

	 return FFmpegMuxer::OpenVideoStream(ms);
 }


 int FFmpegMuxerAudio::InitMuxerAudio(std::string file, int sampleRate, uint64_t channelLayout, AVSampleFormat sf)
 {
	 AV_CH_LAYOUT_STEREO;

	 m_iSampleRate = sampleRate;
	 m_iChannelLayout = channelLayout;
	 m_rawAudioDataFormat = sf;

	 int n = FFmpegMuxer::Init(file);
	 if (n < 0)
	 {
		 return n;
	 }

	 return n;
 }

 int FFmpegMuxerAudio::DestroyMuxerAudio()
 {
	 return FFmpegMuxer::Destroy();
 }

 int FFmpegMuxerAudio::EncodeAudioSample(AVFrame* pFrame)
 {
	 char buf[64];
	 int n = 0;
	 int outcount = m_pConvert->Convert((const uint8_t**)pFrame->extended_data,
		 pFrame->nb_samples);

	 if (outcount <= 0)
	 {
		 return 0;
	 }

	 AVPacket packet;
	 av_init_packet(&packet);

	 m_pConvert->Frame()->pts = m_iSampleCount;

	 n = avcodec_send_frame(m_pAStream->pCodecContext, m_pConvert->Frame());
	 m_iSampleCount += outcount;
	 if (n < 0)
	 {
		 av_strerror(n, buf, sizeof(buf));
		 std::cout << buf << std::endl;
		 return n;
	 }

	 n = avcodec_receive_packet(m_pAStream->pCodecContext, &packet);
	 if (n < 0)
	 {
		 if (n == AVERROR(EAGAIN))
		 {
			 return 0;
		 }
		 av_strerror(n, buf, sizeof(buf));
		 std::cout << buf << std::endl;

		 return n;
	 }

	 packet.stream_index = m_pAStream->pStream->index;
	 n = av_interleaved_write_frame(m_pFormatContext, &packet);
	 av_packet_unref(&packet);

	 return 0;
 }
 
 int FFmpegMuxerAudio::OpenAudioStream(MuxerStream* ms)
 {
	 int n = -1;
	 if (!ms || m_rawAudioDataFormat == AV_SAMPLE_FMT_NONE)
	 {
		 return -1;
	 }

	 std::vector <AVSampleFormat> formats;
	 std::vector <int> rates;
	 std::vector<uint64_t> layouts;

	 ms->SupportedSampleFormat(formats);
	 ms->SupportedSampleRate(rates);
	 ms->SupportedChannelLayout(layouts);


	 auto iter = std::find(formats.begin(), formats.end(), m_rawAudioDataFormat);
	 if (iter != formats.end())
	 {
		 ms->pCodecContext->sample_fmt = m_rawAudioDataFormat;
	 }
	 else
	 {
		 if (!formats.empty())
			 ms->pCodecContext->sample_fmt = formats[0];
	 }

	 auto iterRate = std::find(rates.begin(), rates.end(), m_iSampleRate);
	 if (iterRate != rates.end())
	 {
		 ms->pCodecContext->sample_rate = m_iSampleRate;
	 }
	 else
	 {
		 if (!rates.empty())
			 ms->pCodecContext->sample_rate = rates[0];
		 else
			 ms->pCodecContext->sample_rate = m_iSampleRate;
	 }

	 auto iterLayout = std::find(layouts.begin(), layouts.end(), m_iChannelLayout);
	 if (iterLayout != layouts.end())
	 {
		 ms->pCodecContext->channel_layout = m_iChannelLayout;
	 }
	 else
	 {
		 if (!layouts.empty())
			 ms->pCodecContext->channel_layout = layouts[0];
		 else
			 ms->pCodecContext->channel_layout = m_iChannelLayout;
	 }
	 ms->pCodecContext->channels = av_get_channel_layout_nb_channels(ms->pCodecContext->channel_layout);
	 ms->pCodecContext->time_base = av_make_q(1, ms->pCodecContext->sample_rate);
	 ms->pStream->time_base = av_make_q(1, ms->pCodecContext->sample_rate);

	 n = FFmpegMuxer::OpenAudioStream(ms);

	 if (true || ms->pCodecContext->sample_fmt != m_rawAudioDataFormat
		 || ms->pCodecContext->sample_rate != m_iSampleRate
		 || ms->pCodecContext->channel_layout != m_iChannelLayout)
	 {
		 m_pConvert.reset(new FFmpegAudioConvert());
		 if (m_pConvert->Configure(m_iSampleRate, m_iChannelLayout, m_rawAudioDataFormat,
			 ms->pCodecContext->sample_rate,
			 ms->pCodecContext->channel_layout,
			 ms->pCodecContext->sample_fmt,
			 m_pAStream->sampleSize) < 0)
		 {
			 return -1;
		 }
	 }

	 return n;
 }