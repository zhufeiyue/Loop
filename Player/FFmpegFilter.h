#pragma once

#include "FFmpegDemuxer.h"

void testFilter();

class FilterAudio
{
public:
	FilterAudio(AVSampleFormat, AVRational, int64_t, int);
	virtual ~FilterAudio();
	int Process(AVFrame*);

protected:
	const AVFilter* m_pBufferFilter = nullptr;
	const AVFilter* m_pBufferSinkFilter = nullptr;
	const AVFilter* m_pFormatFilter = nullptr;
	AVFilterContext* m_pBufferContext = nullptr;
	AVFilterContext* m_pBufferSinkContext = nullptr;
	AVFilterContext* m_pFormatContext = nullptr;
	AVFilterGraph* m_pGraph = nullptr;
};

class FilterVolume : public FilterAudio
{
public:
	FilterVolume(AVSampleFormat, AVRational, int64_t, int, double volume);
	~FilterVolume();

protected:
	const AVFilter* m_pVolumeFilter = nullptr;
	AVFilterContext* m_pVolumeContext = nullptr;
};

class Filter_atempo : public FilterAudio
{
public:
	Filter_atempo(AVSampleFormat, AVRational, int64_t, int, double atempo);
	~Filter_atempo();

protected:
	const AVFilter* m_pAtempoFilter = nullptr;
	AVFilterContext* m_pAtempoContext = nullptr;
};