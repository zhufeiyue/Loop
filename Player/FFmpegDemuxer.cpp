#include "FFmpegDemuxer.h"
#include <common/Log.h>

static int InterruptCB(void* para)
{
	//return 0 不中断， 非0中断阻塞的操作
	auto p = (FFmpegDemuxer*)(para);
	if (p)
	{
		return p->m_iInterrupt;
	}
	return 0;
}

void PrintFFmpegError(int code)
{
	char buf[64] = { 0 };
	av_make_error_string(buf, sizeof(buf), code);
	LOG() << buf;
}

FFmpegDemuxer::FFmpegDemuxer(std::string strMediaAddress)
{
	m_pFormatContext = avformat_alloc_context();
	m_pFormatContext->interrupt_callback.callback = InterruptCB;
	m_pFormatContext->interrupt_callback.opaque = this;

	AVDictionary* opts = NULL;
	int n = 0;

	if (av_stristart(strMediaAddress.c_str(), "rtsp", NULL)||
		av_stristart(strMediaAddress.c_str(), "rtmp", NULL))
	{
		av_dict_set(&opts, "stimeout", "10000000", 0); // 10s timeout
		av_dict_set(&opts, "buffer_size", "4096000", 0);
		av_dict_set(&opts, "rtsp_transport", "tcp", 0);

		// 默认5000000字节，减小这个值会加快流打开速度，但是也会导致兼容性变差，频繁的打开流失败
		m_pFormatContext->probesize = 1024 * 1024 * 2;
		// 默认0，ffmpeg内部会根据不同的容器格式选择一个时间，常用的是5秒（5*AV_TIME_BASE）
		m_pFormatContext->max_analyze_duration = 1 * AV_TIME_BASE; 
	}

	n = avformat_open_input(&m_pFormatContext, strMediaAddress.c_str(), NULL, &opts);
	if (n != 0)
	{
		PrintFFmpegError(n);
		LOG() << strMediaAddress;
		return;
	}

	n = avformat_find_stream_info(m_pFormatContext, NULL);
	if (n < 0)
	{
		PrintFFmpegError(n);
		return;
	}

	if (!strcmp(m_pFormatContext->iformat->long_name, "QuickTime / MOV")
		|| !strcmp(m_pFormatContext->iformat->long_name, "FLV (Flash Video)")
		|| !strcmp(m_pFormatContext->iformat->long_name, "Matroska / WebM"))
	{
		m_bAnnexb = false;
	}
	
	m_iAudioIndex = av_find_best_stream(m_pFormatContext, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	m_iVideoIndex = av_find_best_stream(m_pFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (m_iVideoIndex >= 0)
	{
		auto codecpar = m_pFormatContext->streams[m_iVideoIndex]->codecpar;
		m_vCodecID = codecpar->codec_id;
		m_pixFormat = (AVPixelFormat)codecpar->format;

		//for (int i = 0; i < codecpar->extradata_size; ++i)
		//{
		//	std::cout << std::hex << (int)codecpar->extradata[i] << ' ';
		//}
		//std::cout << std::endl;

		const AVBitStreamFilter* bsf = NULL;
 		if (!m_bAnnexb)
		{
			//if (m_vCodecID == AV_CODEC_ID_H264)
			//{
			//	bsf = av_bsf_get_by_name("h264_mp4toannexb");
			//}
			//else if (m_vCodecID == AV_CODEC_ID_HEVC)
			//{
			//	bsf = av_bsf_get_by_name("hevc_mp4toannexb");
			//}
		}
		if (bsf)
		{
			av_bsf_alloc(bsf, &m_bsfMp4ToAnnexb);
			avcodec_parameters_copy(m_bsfMp4ToAnnexb->par_in, m_pFormatContext->streams[m_iVideoIndex]->codecpar);
			av_bsf_init(m_bsfMp4ToAnnexb);
		}
	}

	if (m_iAudioIndex >= 0)
	{
		auto pStream = m_pFormatContext->streams[m_iAudioIndex];
		auto codecpar = pStream->codecpar;
		m_aCodecID = codecpar->codec_id;
		m_sampleFormat = (AVSampleFormat)codecpar->format;
		codecpar->frame_size;

		//for (int i = 0; i < codecpar->extradata_size; ++i)
		//{
		//	std::cout << std::hex << (int)codecpar->extradata[i] << ' ';
		//}
		//std::cout << std::endl;
	}
}

FFmpegDemuxer::~FFmpegDemuxer()
{
	m_iInterrupt = 0;

	while (!m_vPackets.empty())
	{
		av_packet_unref(&m_vPackets.front());
		m_vPackets.pop();
	}
	while (!m_aPackets.empty())
	{
		av_packet_unref(&m_aPackets.front());
		m_aPackets.pop();
	}

	if (m_bsfMp4ToAnnexb)
	{
		av_bsf_free(&m_bsfMp4ToAnnexb);
	}
	if (m_pFormatContext)
	{
		avformat_close_input(&m_pFormatContext);
	}
}

int FFmpegDemuxer::DemuxVideo(AVPacket& got)
{
	std::lock_guard<std::mutex> guard(m_demuxeLock);

	if (!m_pFormatContext || m_iVideoIndex < 0)
	{
		return -1;
	}
	if (got.data)
	{
		av_packet_unref(&got);
	}

	int n = 0;
	AVPacket packet;
	av_init_packet(&packet);
	packet.data = NULL;
	packet.size = 0;

	if (!m_vPackets.empty())
	{
		packet = m_vPackets.front();
		m_vPackets.pop();
	}
	else
	{
	again:
		n = av_read_frame(m_pFormatContext, &packet);
		if (n < 0)
		{
			PrintFFmpegError(n);
			return n;
		}

		if (packet.stream_index != m_iVideoIndex)
		{
			if (packet.stream_index == m_iAudioIndex)
			{
				m_aPackets.push(packet);
			}
			else
			{
				av_packet_unref(&packet);
			}
			goto again;
		}
	}


	if(m_bsfMp4ToAnnexb)
	{
		AVPacket filterPacket;
		av_init_packet(&filterPacket);
		filterPacket.data = NULL;
		filterPacket.size = 0;

		n = av_bsf_send_packet(m_bsfMp4ToAnnexb, &packet);
		av_packet_unref(&packet);
		if (0 == n)
		{
			n = av_bsf_receive_packet(m_bsfMp4ToAnnexb, &filterPacket);
			got = filterPacket;
		}
		else
		{
			return n;
		}
	}
	else
	{
		got = packet;
	}

	return 0;
}

int FFmpegDemuxer::DemuxAudio(AVPacket& got)
{
	std::lock_guard<std::mutex> guard(m_demuxeLock);

	if (!m_pFormatContext || m_iAudioIndex < 0)
	{
		return -1;
	}
	if (got.data)
	{
		av_packet_unref(&got);
	}

	int n = 0;

	if (!m_aPackets.empty())
	{
		got = m_aPackets.front();
		m_aPackets.pop();
	}
	else
	{
	again:
		n = av_read_frame(m_pFormatContext, &got);
		if (n < 0)
		{
			PrintFFmpegError(n);
			return n;
		}

		if (got.stream_index != m_iAudioIndex)
		{
			if (got.stream_index == m_iVideoIndex)
			{
				m_vPackets.push(got);
			}
			else
			{
				av_packet_unref(&got);
			}
			goto again;
		}
	}

	return n;
}

bool FFmpegDemuxer::ContainVideo() const
{
	return m_iVideoIndex >= 0;
}

bool FFmpegDemuxer::ContainAudio() const
{
	return m_iAudioIndex >= 0;
}

double FFmpegDemuxer::GetFrameRate()
{
	if (m_iVideoIndex >= 0)
	{
		auto pStream = m_pFormatContext->streams[m_iVideoIndex];
		auto fr = av_guess_frame_rate(m_pFormatContext, pStream, NULL);
		return av_q2d(fr);
	}
	return 0;
}

std::pair<int, int> FFmpegDemuxer::GetFrameSize()
{
	if (m_iVideoIndex >= 0)
	{
		auto pStream = m_pFormatContext->streams[m_iVideoIndex];
		auto w = pStream->codecpar->width;
		auto h = pStream->codecpar->height;
		auto aspect_ratio = av_q2d(pStream->codecpar->sample_aspect_ratio);

		//if (aspect_ratio <= 0.0)
		//	aspect_ratio = 1.0;
		//aspect_ratio *= (float)w / (float)h;
		//w = lrint(h * aspect_ratio) & ~1;
		//if (w > pStream->codecpar->width) {
		//	w = pStream->codecpar->width;
		//	h = lrint(pStream->codecpar->height / aspect_ratio) & ~1;
		//}

		return std::make_pair(w, h);
	}
	return std::make_pair(0, 0);
}

AVPixelFormat FFmpegDemuxer::GetFrameFormat()
{
	if (m_iVideoIndex >= 0)
	{
		auto pStream = m_pFormatContext->streams[m_iVideoIndex];
		return (AVPixelFormat)pStream->codecpar->format;
	}

	return AV_PIX_FMT_NONE;
}

AVSampleFormat FFmpegDemuxer::GetSampleFormat()
{
	if (m_iAudioIndex >= 0)
	{
		auto pStream = m_pFormatContext->streams[m_iAudioIndex];
		return (AVSampleFormat)pStream->codecpar->format;
	}

	return AV_SAMPLE_FMT_NONE;
}

int FFmpegDemuxer::GetSampleRate()
{
	if (m_iAudioIndex >= 0)
	{
		auto pStream = m_pFormatContext->streams[m_iAudioIndex];
		return pStream->codecpar->sample_rate;
	}

	return 0;
}

int FFmpegDemuxer::GetSampleChannel()
{
	if (m_iAudioIndex >= 0)
	{
		auto pStream = m_pFormatContext->streams[m_iAudioIndex];
		return pStream->codecpar->channels;
	}

	return 0;
}

int64_t FFmpegDemuxer::GetChannelLayout()
{
	if (m_iAudioIndex >= 0)
	{
		auto pStream = m_pFormatContext->streams[m_iAudioIndex];
		return pStream->codecpar->channel_layout;
	}

	return 0;
}

int64_t FFmpegDemuxer::GetDuration()
{
	if (m_pFormatContext)
	{
		return m_pFormatContext->duration / AV_TIME_BASE;
	}
	else
	{
		return 0;
	}
}

AVRational FFmpegDemuxer::GetVideoTimebase(int)
{
	AVRational tb;
	tb.den = 1;
	tb.num = 0;

	if (m_iVideoIndex >= 0)
	{
		tb = m_pFormatContext->streams[m_iVideoIndex]->time_base;
	}

	return tb;
}

AVRational FFmpegDemuxer::GetAudioTimebase(int)
{
	AVRational tb;
	tb.den = 1;
	tb.num = 0;

	if (m_iAudioIndex >= 0)
	{
		tb = m_pFormatContext->streams[m_iAudioIndex]->time_base;
	}

	return tb;
}

static int32_t Char2ToLittleInt(const uint8_t* temp)
{
	const unsigned char* p = (const uint8_t*)temp;
	return (p[0] << 8) | p[1];
}

std::map<std::string, std::string> FFmpegDemuxer::Parse_extradata(int what)
{
	std::map<std::string, std::string> items;

	int n = what == 0 ? m_iVideoIndex : m_iAudioIndex;

	if (n < 0)
	{
		goto End;
	}

	auto pStream = m_pFormatContext->streams[n];
	auto size = pStream->codecpar->extradata_size;
	auto data = pStream->codecpar->extradata;

	items["codecid"] = std::to_string(pStream->codecpar->codec_id);
	items["extradata"] = std::string((char*)data, size);
	//items["extradataSize"] = std::to_string(size);
	if (n == m_iVideoIndex)
	{
		auto n = GetFrameSize();
		items["rate"] = std::to_string(GetFrameRate());
		items["width"] = std::to_string(n.first);
		items["height"] = std::to_string(n.second);
	}

	if (pStream->codecpar->codec_id == AV_CODEC_ID_H264)
	{
		if (m_bAnnexb)
		{
			int lastStartCode = size;
			int startCodeSize = 0;
			for (int i = size - 1; i - 2 >= 0; )
			{
				if (data[i] == 1 && data[i - 1] == 0 && 
					((data[i - 2] == 0 && i-3>=0 && data[i - 3] == 0 && (startCodeSize = 4)) || (data[i - 2] == 0 && (startCodeSize = 3))))
				{
					auto type = data[i+1] & 0x1f;
					if (type == 7)
					{
						items["sps"] = std::string(data + i + 1, data + lastStartCode);
					}
					else if (type == 8)
					{
						items["pps"] = std::string(data + i + 1, data + lastStartCode);
					}

					i = i - startCodeSize;
					lastStartCode = i + 1;
				}
				else
				{
					i -= 1;
				}
			}
		}
		else
		{
			auto ptemp = data;
			int count(0);
			auto avc_seq_head_version = *(ptemp + (count++));
			auto avc_seq_head_profile = *(ptemp + (count++));
			auto avc_seq_head_profilecompatibility = *(ptemp + (count++));
			auto avc_seq_head_level = *(ptemp + (count++));
			auto avc_seq_head_NALUnitLength = *(ptemp + (count++)) & 3;
			avc_seq_head_NALUnitLength += 1;
			auto avc_seq_head_spsnum = *(ptemp + (count++)) & 0x1f;

			int spssize, ppssize;
			std::string strSPS, strPPS;

			spssize = Char2ToLittleInt(ptemp + count);
			count += 2;
			strSPS = std::string((const char*)ptemp + count, spssize);
			items["sps"] = strSPS;
			count += spssize;

			auto avc_seq_head_ppsnum = *(ptemp + count);
			count += 1;

			ppssize = Char2ToLittleInt(ptemp + count);
			count += 2;
			strPPS = std::string((const char*)ptemp + count, ppssize);
			items["pps"] = strPPS;
		}
	}
	else if (pStream->codecpar->codec_id == AV_CODEC_ID_HEVC)
	{
	}

End:
	return items;
}


FFmpegDecode::FFmpegDecode(std::string s):
	FFmpegDemuxer(s)
{
	m_pAFrame = av_frame_alloc();
	m_pVFrame = av_frame_alloc();
}

FFmpegDecode::~FFmpegDecode()
{
	av_frame_free(&m_pAFrame);
	av_frame_free(&m_pVFrame);

	if (m_pACodecContext)
	{
		avcodec_free_context(&m_pACodecContext);
	}
	if (m_pVCodecContext)
	{
		avcodec_free_context(&m_pVCodecContext);
	}
}

int FFmpegDecode::CreateDecoder()
{
	int n = 0;

	if (m_iVideoIndex >= 0)
	{
		m_pVCodec = avcodec_find_decoder(m_vCodecID);
		m_pVCodecContext = avcodec_alloc_context3(m_pVCodec);
		avcodec_parameters_to_context(m_pVCodecContext, m_pFormatContext->streams[m_iVideoIndex]->codecpar);
		m_pVCodecContext->time_base;
		m_pVCodecContext->framerate;
		m_pVCodecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
		m_pVCodecContext->thread_count = 0;
		n = avcodec_open2(m_pVCodecContext, m_pVCodec, NULL);
		if (n < 0)
		{
			PrintFFmpegError(n);
		}
	}

	if (m_iAudioIndex >= 0) 
	{
		m_pACodec = avcodec_find_decoder(m_aCodecID);
		m_pACodecContext = avcodec_alloc_context3(m_pACodec);
		avcodec_parameters_to_context(m_pACodecContext, m_pFormatContext->streams[m_iAudioIndex]->codecpar);
		n = avcodec_open2(m_pACodecContext, m_pACodec, NULL);
		if (n < 0)
		{
			PrintFFmpegError(n);
		}
	}

	return n;
}

int FFmpegDecode::DecodeVideo(AVPacket& packet)
{
	int n = -1;

	if (!m_pVCodecContext)
	{
		return -1;
	}

	if (packet.data)
	{
		n = avcodec_send_packet(m_pVCodecContext, &packet);
		av_packet_unref(&packet);
		if (n < 0)
		{
			PrintFFmpegError(n);
			// todo is this ok?
			if (n == AVERROR_INVALIDDATA)
			{
				n = AVERROR(EAGAIN);
			}
			return n;
		}
	}

	n = avcodec_receive_frame(m_pVCodecContext, m_pVFrame);
	if (n < 0)
	{
		PrintFFmpegError(n);
	}

	return n;
}

int FFmpegDecode::DecodeAudio(AVPacket& packet)
{
	int n = -1;

	if (!m_pACodecContext)
	{
		return -1;
	}

	if (packet.data)
	{
		n = avcodec_send_packet(m_pACodecContext, &packet);
		av_packet_unref(&packet);
		if (n < 0)
		{
			PrintFFmpegError(n);

			// todo is this ok?
			if (n == AVERROR_INVALIDDATA)
			{
				n = AVERROR(EAGAIN);
			}

			return n;
		}
	}

	n = avcodec_receive_frame(m_pACodecContext, m_pAFrame);
	if (n < 0)
	{
		PrintFFmpegError(n);
	}

	return n;
}

int FFmpegDecode::DecodeVideoFrame()
{
	AVPacket packet;
	av_init_packet(&packet);
	packet.data = NULL;
	packet.size = 0;
	int n = 0; 

read:
	n = DemuxVideo(packet);
	if (n == 0)
	{
	}
	else
	{
		return n;
	}

decode:
	n = DecodeVideo(packet);
	if (n == 0)
	{
		// todo image data
	}
	else if (n == AVERROR(EAGAIN))
	{
		goto read;
	}
	else
	{
		// handle error
	}

	return n;
}

int FFmpegDecode::DecodeAudioFrame()
{
	AVPacket packet;
	av_init_packet(&packet);
	packet.data = NULL;
	packet.size = 0;
	int n = 0;

read:
	n = DemuxAudio(packet);
	if (n == 0)
	{
	}
	else
	{
		return n;
	}

decode:
	n = DecodeAudio(packet);
	if (n == 0)
	{
		// todo audio data
	}
	else if (n == AVERROR(EAGAIN))
	{
		goto read;
	}
	else
	{
		// handle error
	}

	return n;
}


FFmpegHWDecode::FFmpegHWDecode(std::string s):
	FFmpegDecode(s)
{
	m_hwVFrame = av_frame_alloc();
}

FFmpegHWDecode::~FFmpegHWDecode()
{
	if (m_hwVFrame)
	{
		av_frame_free(&m_hwVFrame);
	}
	if (hw_device_ctx)
	{
		av_buffer_unref(&hw_device_ctx);
	}
}

static enum AVPixelFormat get_hw_format(AVCodecContext* ctx,
	const enum AVPixelFormat* pix_fmts)
{
	const enum AVPixelFormat* p = NULL;
	auto pThis = static_cast<FFmpegHWDecode*>(ctx->opaque);


	for (p = pix_fmts; *p != -1; p++)
	{
		if (*p == pThis->hw_pix_fmt)
		{
			return *p;
		}
	}

	return AV_PIX_FMT_NONE;
}

int FFmpegHWDecode::CreateDecoder()
{
	if (m_iVideoIndex >= 0)
	{
		m_pVCodec = avcodec_find_decoder(m_vCodecID);
		m_pVCodecContext = avcodec_alloc_context3(m_pVCodec);
		avcodec_parameters_to_context(m_pVCodecContext, m_pFormatContext->streams[m_iVideoIndex]->codecpar);

		std::vector<std::pair<AVHWDeviceType, AVPixelFormat>> supportHWType;
		const AVCodecHWConfig* config = NULL;
		for (int i = 0;; ++i)
		{
			config = avcodec_get_hw_config(m_pVCodec, i);
			if (config == NULL)
			{
				break;
			}

			if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
				&& config->device_type != AV_HWDEVICE_TYPE_NONE)
			{
				supportHWType.push_back(std::make_pair(config->device_type, config->pix_fmt));
			}
		}

		auto findDecodeTypeFunc = [=](AVHWDeviceType type) {
			AVDictionary* opts = NULL;
			int n = 0;

			auto iter = std::find_if(supportHWType.begin(), supportHWType.end(), [type](const std::pair<AVHWDeviceType, AVPixelFormat>& item)
				{
					return item.first == type;
				});
			if (iter == supportHWType.end())
			{
				return false;
			}

			n = av_hwdevice_ctx_create(&hw_device_ctx, type, NULL, NULL, 0);
			if (n < 0)
			{
				PrintFFmpegError(n);
				return false;
			}

			hw_pix_fmt = iter->second;
			m_pVCodecContext->opaque = this;
			m_pVCodecContext->get_format = get_hw_format;
			m_pVCodecContext->hw_device_ctx = av_buffer_ref(hw_device_ctx);

			n = avcodec_open2(m_pVCodecContext, m_pVCodec, &opts);
			if (n < 0)
			{
				PrintFFmpegError(n);

				av_buffer_unref(&hw_device_ctx);
				av_buffer_unref(&(m_pVCodecContext->hw_device_ctx));
				m_pVCodecContext->hw_device_ctx = NULL;
				m_pVCodecContext->get_format = NULL;
				return false;
			}

			return true;

		};

		if (findDecodeTypeFunc(AV_HWDEVICE_TYPE_CUDA) ||
			findDecodeTypeFunc(AV_HWDEVICE_TYPE_DXVA2) /*||
			findDecodeTypeFunc(AV_HWDEVICE_TYPE_D3D11VA)*/)
		{
			m_bIsSupportHW = true;
		}
		else
		{
			avcodec_free_context(&m_pVCodecContext);
			m_pVCodec = NULL;
			m_pVCodecContext = NULL;
			// 退回软解
			return  FFmpegDecode::CreateDecoder();
		}
	}

	if (m_iAudioIndex >= 0)
	{
		m_pACodec = avcodec_find_decoder(m_aCodecID);
		m_pACodecContext = avcodec_alloc_context3(m_pACodec);
		avcodec_parameters_to_context(m_pACodecContext, m_pFormatContext->streams[m_iAudioIndex]->codecpar);

		int n = avcodec_open2(m_pACodecContext, m_pACodec, NULL);
		if (n < 0)
		{
			PrintFFmpegError(n);
			return CodeNo;
		}
	}

	return 0;
}

int FFmpegHWDecode::DecodeVideo(AVPacket& packet)
{
	int n = -1;

	if (!hw_device_ctx)
	{
		return FFmpegDecode::DecodeVideo(packet);
	}

	if (!m_pVCodecContext)
	{
		return -1;
	}

	if (packet.data)
	{
		n = avcodec_send_packet(m_pVCodecContext, &packet);
		av_packet_unref(&packet);
		if (n < 0)
		{
			PrintFFmpegError(n);

			// todo is this ok? ignore this two errors
			if (n == AVERROR(EPERM) || n == AVERROR_INVALIDDATA)
			{
				n = AVERROR(EAGAIN);
			}
			return n;
		}
	}

	n = avcodec_receive_frame(m_pVCodecContext, m_hwVFrame);
	if (n < 0)
	{
		PrintFFmpegError(n);
	}
	else
	{
		//if (m_pVFrame && m_pVFrame->format != -1 && 
		//	(m_pVFrame->width != m_hwVFrame->width || m_pVFrame->height != m_hwVFrame->height))
		//{
		//	av_frame_free(&m_pVFrame);
		//	m_pVFrame = av_frame_alloc();
		//	LOG() << "should never here";
		//}

		n = av_hwframe_transfer_data(m_pVFrame, m_hwVFrame, 0);
		//AV_NOPTS_VALUE

		m_pVFrame->pts = m_hwVFrame->pts;
		m_pVFrame->pkt_pts = m_hwVFrame->pkt_pts;
		m_pVFrame->pkt_dts = m_hwVFrame->pkt_dts;
		m_pVFrame->key_frame = m_hwVFrame->key_frame;
		m_pVFrame->pict_type = m_hwVFrame->pict_type;
		m_pVFrame->sample_aspect_ratio = m_hwVFrame->sample_aspect_ratio;

		if (n < 0)
		{
			PrintFFmpegError(n);
		}
	}

	return n;
}


FFmpegImageScale::FFmpegImageScale()
{

}

FFmpegImageScale::~FFmpegImageScale()
{
	if (m_pFrame)
	{
		av_frame_free(&m_pFrame);
	}

	if (m_pVSws)
	{
		sws_freeContext(m_pVSws);
	}
}

int FFmpegImageScale::Configure(int srcW, int srcH, AVPixelFormat srcFmt,
	int dstW, int dstH, AVPixelFormat dstFmt)
{
	m_iSrcW = srcW;
	m_iSrcH = srcH;

	m_pVSws = sws_getCachedContext(m_pVSws, srcW, srcH, srcFmt,
		dstW, dstH, dstFmt,
		SWS_FAST_BILINEAR, NULL, NULL, NULL);
	if (!m_pVSws)
	{
		LOG() << "sws_getCachedContext return NULL";
		return -1;
	}

	if (m_pFrame)
	{
		av_frame_free(&m_pFrame);
	}

	m_pFrame = av_frame_alloc();
	m_pFrame->width = dstW;
	m_pFrame->height = dstH;
	m_pFrame->format = dstFmt;

	if (av_frame_get_buffer(m_pFrame, 0) < 0)
	{
		av_frame_free(&m_pFrame);
		LOG() << "av_frame_get_buffer error";
		return -1;
	}

	return 0;
}

int FFmpegImageScale::Convert(const uint8_t* const srcSlice[], const int srcStride[])
{
	if (!m_pVSws || !m_pFrame)
	{
		return -1;
	}

	return sws_scale(m_pVSws, srcSlice, srcStride, 0, m_iSrcH, m_pFrame->data, m_pFrame->linesize);
}

int FFmpegImageScale::Convert(const uint8_t* const srcSlice[], const int srcStride[], uint8_t* const dst[], const int dstStride[])
{
	if (!m_pVSws)
	{
		return -1;
	}

	return sws_scale(m_pVSws, srcSlice, srcStride, 0, m_iSrcH, dst, dstStride);
}


FFmpegAudioConvert::FFmpegAudioConvert()
{
}

FFmpegAudioConvert::~FFmpegAudioConvert()
{
	if (m_pFrame)
	{
		av_frame_free(&m_pFrame);
	}

	if (m_pASwr)
	{
		swr_free(&m_pASwr);
	}
}

int FFmpegAudioConvert::Configure(int srcSampleRate, uint64_t srcLayout, AVSampleFormat srcFmt,
	int dstSampleRate, uint64_t dstLayout, AVSampleFormat dstFmt,
	int dstSampleCount)
{
	m_pASwr = swr_alloc_set_opts(m_pASwr,
		dstLayout, dstFmt, dstSampleRate,
		srcLayout, srcFmt, srcSampleRate, 0, NULL);
	if (!m_pASwr)
	{
		return -1;
	}

	int n = swr_init(m_pASwr);
	if (n < 0)
	{
		swr_free(&m_pASwr);
		return n;
	}

	if (m_pFrame)
	{
		av_frame_free(&m_pFrame);
	}

	m_pFrame = av_frame_alloc();
	m_pFrame->format = dstFmt;
	m_pFrame->channel_layout = dstLayout;
	m_pFrame->sample_rate = dstSampleRate;
	m_pFrame->nb_samples = dstSampleCount;

	n = av_frame_get_buffer(m_pFrame, 0);
	m_pFrame->extended_data;

	return n;
}

int FFmpegAudioConvert::Convert(const uint8_t** ppInData, int incount)
{
	/*
	* 利用swr_convert的性质，转换、缓存，确切数量的音频帧
	*/
	if (!m_pASwr)
	{
		return -1;
	}

	int outcount = swr_get_out_samples(m_pASwr, incount);
	int n = swr_convert(m_pASwr, 
		m_pFrame->data,
		outcount >= m_pFrame->nb_samples ? m_pFrame->nb_samples : 0,
		ppInData, incount);

	return n;
}