#include "FFmpegFilter.h"
#include <common/Log.h>

void testFilter()
{
	AVRational timeBase;
	timeBase.den = 48000;
	timeBase.num = 1;
	//auto pAudioFilter = new FilterAudio(AV_SAMPLE_FMT_FLTP, timeBase, 3, 48000);
	//auto pVolumeFilter = new FilterVolume(AV_SAMPLE_FMT_FLTP, timeBase, 3, 48000, 0.9);
	auto pAudioSpeed = new Filter_atempo(AV_SAMPLE_FMT_FLTP, timeBase, 3, 48000, 2.0);
}

FilterAudio::FilterAudio(AVSampleFormat audioFormat,
	AVRational timeBase,
	int64_t channel_layout,
	int rate)
{
	m_pGraph = avfilter_graph_alloc();
	if (!m_pGraph)
	{
		LOG() << "avfilter_graph_alloc return NULL";
		return;
	}

	m_pBufferFilter = avfilter_get_by_name("abuffer");
	m_pBufferSinkFilter = avfilter_get_by_name("abuffersink");
	m_pFormatFilter = avfilter_get_by_name("aformat");
	if (!m_pBufferFilter ||
		!m_pBufferSinkFilter ||
		!m_pFormatFilter)
	{
		LOG() << "avfilter_get_by_name return NULL";
		return;
	}

	m_pBufferContext = avfilter_graph_alloc_filter(m_pGraph, m_pBufferFilter, "src");
	m_pBufferSinkContext = avfilter_graph_alloc_filter(m_pGraph, m_pBufferSinkFilter, "sink");
	m_pFormatContext = avfilter_graph_alloc_filter(m_pGraph, m_pFormatFilter, "format");
	if (!m_pBufferContext ||
		!m_pBufferSinkContext||
		!m_pFormatContext)
	{
		LOG() << "avfilter_graph_alloc_filter return NULL";
		return;
	}

	char buf[1024] = { 0 };
	char bufChannelLayout[128] = { 0 };
	int result = 0;
	av_get_channel_layout_string(bufChannelLayout, sizeof(bufChannelLayout), 0, channel_layout);
	av_opt_set(m_pBufferContext, "channel_layout", bufChannelLayout, AV_OPT_SEARCH_CHILDREN);
	av_opt_set(m_pBufferContext, "sample_fmt", av_get_sample_fmt_name(audioFormat), AV_OPT_SEARCH_CHILDREN);
	av_opt_set_q(m_pBufferContext, "time_base", timeBase, AV_OPT_SEARCH_CHILDREN);
	av_opt_set_int(m_pBufferContext, "sample_rate", rate, AV_OPT_SEARCH_CHILDREN);
	result = avfilter_init_str(m_pBufferContext, nullptr);
	if (result < 0)
	{
		PrintFFmpegError(result, "avfilter_init_str abuffer");
		return;
	}

	snprintf(buf, sizeof(buf),
		"sample_fmts=%s:sample_rates=%d:channel_layouts=%s",
		av_get_sample_fmt_name(audioFormat), 
		rate,
		bufChannelLayout);
	result = avfilter_init_str(m_pFormatContext, buf);
	if (result < 0)
	{
		PrintFFmpegError(result, "avfilter_init_str aformat");
		return;
	}

	result = avfilter_init_str(m_pBufferSinkContext, nullptr);
	if (result < 0)
	{
		PrintFFmpegError(result, "avfilter_init_str abuffersink");
		return;
	}
}

FilterAudio::~FilterAudio()
{
	if (m_pBufferContext)
	{
		avfilter_free(m_pBufferContext);
		m_pBufferContext = nullptr;
	}
	if (m_pFormatContext)
	{
		avfilter_free(m_pFormatContext);
		m_pFormatContext = nullptr;
	}
	if (m_pBufferSinkContext)
	{
		avfilter_free(m_pBufferSinkContext);
		m_pBufferSinkContext = nullptr;
	}

	if (m_pGraph)
	{
		avfilter_graph_free(&m_pGraph);
	}
}

int FilterAudio::Process(AVFrame* pFrame)
{
	AV_BUFFERSRC_FLAG_KEEP_REF;

	/*
	todo
	从av_buffersink_get_frame中得到的frame，其pts值与原始输入（av_buffersrc_add_frame中传入）的值不一致

	使用函数av_buffersrc_add_frame_flags ，flag参数指定为AV_BUFFERSRC_FLAG_PUSH，是否可以立即得到输出？
	*/
	if (sizeof(void*) == 8)
	{
		pFrame->opaque = (void*)pFrame->pts;
	}
	else if(sizeof(void*) == 4)
	{
		pFrame->pkt_dts = pFrame->pts;
	}
	else
	{
		assert(0);
	}

	int result = 0;
	result = av_buffersrc_add_frame(m_pBufferContext, pFrame);
	if (result < 0)
	{
		PrintFFmpegError(result, "av_buffersrc_add_frame");
		return CodeNo;
	}

	result = av_buffersink_get_frame(m_pBufferSinkContext, pFrame);
	if (result < 0)
	{
		if (result == AVERROR(EAGAIN))
		{
			return CodeAgain;
		}
		PrintFFmpegError(result, "av_buffersink_get_frame");
		return CodeNo;
	}
	else
	{
		//LOG() << (int64_t)pFrame->opaque << " "<<  pFrame->pts << " " << pFrame->nb_samples;

		if (sizeof(void*) == 8)
		{
			pFrame->pts = (int64_t)pFrame->opaque;
		}
		else
		{
			pFrame->pts = pFrame->pkt_dts;
		}
	}

	return CodeOK;
}


FilterVolume::FilterVolume(AVSampleFormat audioFormat,
	AVRational timeBase,
	int64_t channel_layout,
	int rate,
	double volume):
	FilterAudio(audioFormat, timeBase, channel_layout, rate)
{
	if (!m_pGraph || !m_pBufferContext || !m_pBufferSinkContext)
	{
		return;
	}

	m_pVolumeFilter = avfilter_get_by_name("volume");
	if (!m_pVolumeFilter)
	{
		LOG() << "avfilter_get_by_name volume return NULL";
		return;
	}

	m_pVolumeContext = avfilter_graph_alloc_filter(m_pGraph, m_pVolumeFilter, "volume");
	if (!m_pVolumeContext)
	{
		LOG() << "avfilter_graph_alloc_filter volume return NULL";
		return;
	}

	std::stringstream ss;
	int result = 0;
	AVDictionary* options_dict = nullptr;
	ss << volume;
	av_dict_set(&options_dict, "volume", ss.str().c_str(), 0);
	result = avfilter_init_dict(m_pVolumeContext, &options_dict);
	av_dict_free(&options_dict);
	if (result < 0) 
	{
		PrintFFmpegError(result, "avfilter_init_dict volume");
		return;
	}

	result = avfilter_link(m_pBufferContext, 0, m_pVolumeContext, 0);
	if (result < 0)
	{
		PrintFFmpegError(result, "avfilter_link buffer->volume");
		return;
	}

	result = avfilter_link(m_pVolumeContext, 0, m_pFormatContext, 0);
	if (result < 0)
	{
		PrintFFmpegError(result, "avfilter_link volume->format");
		return;
	}

	result = avfilter_link(m_pFormatContext, 0, m_pBufferSinkContext, 0);
	if (result < 0)
	{
		PrintFFmpegError(result, "avfilter_link format->buffersink");
		return;
	}

	result = avfilter_graph_config(m_pGraph, nullptr);
	if (result < 0)
	{
		PrintFFmpegError(result, "avfilter_graph_config");
		return;
	}
}

FilterVolume::~FilterVolume()
{
	if (m_pVolumeContext)
	{
		avfilter_free(m_pVolumeContext);
		m_pVolumeContext = nullptr;
	}
}


Filter_atempo::Filter_atempo(AVSampleFormat audioFormat,
	AVRational timeBase,
	int64_t channel_layout,
	int rate,
	double atempo):
	FilterAudio(audioFormat, timeBase, channel_layout, rate)
{
	if (!m_pGraph || !m_pBufferContext || !m_pBufferSinkContext)
	{
		return;
	}

	m_pAtempoFilter = avfilter_get_by_name("atempo");
	if (!m_pAtempoFilter)
	{
		LOG() << "avfilter_get_by_name atempo return NULL";
		return;
	}

	m_pAtempoContext = avfilter_graph_alloc_filter(m_pGraph, m_pAtempoFilter, "audiospeed");
	if (!m_pAtempoContext)
	{
		LOG() << "avfilter_graph_alloc_filter atempo return NULL";
		return;
	}
	int result = 0;
	std::stringstream ss;
	ss << "tempo=" << atempo;

	result = avfilter_init_str(m_pAtempoContext, ss.str().c_str());
	if (result < 0)
	{
		PrintFFmpegError(result, "avfilter_init_str");
		return;
	}

	result = avfilter_link(m_pBufferContext, 0, m_pAtempoContext, 0);
	if (result < 0)
	{
		PrintFFmpegError(result, "avfilter_link buffer->atempo");
		return;
	}
	result = avfilter_link(m_pAtempoContext, 0, m_pFormatContext, 0);
	if (result < 0)
	{
		PrintFFmpegError(result, "avfilter_link atempo->format");
		return;
	}
	result = avfilter_link(m_pFormatContext, 0, m_pBufferSinkContext, 0);
	if (result < 0)
	{
		PrintFFmpegError(result, "avfilter_link format->buffersink");
		return;
	}
	result = avfilter_graph_config(m_pGraph, nullptr);
	if (result < 0)
	{
		PrintFFmpegError(result, "avfilter_graph_config");
		return;
	}
}

Filter_atempo::~Filter_atempo()
{
	if (m_pAtempoContext)
	{
		avfilter_free(m_pAtempoContext);
		m_pAtempoContext = nullptr;
	}
}