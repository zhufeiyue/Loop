#pragma once

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avstring.h>
#include <libavutil/imgutils.h>
}

#include <cassert>
#include <iostream>
#include <iomanip>
#include <string>
#include <queue>
#include <map>
#include <mutex>

void PrintFFmpegError(int code);

class FFmpegDemuxer
{
public:
	FFmpegDemuxer(std::string);
	virtual ~FFmpegDemuxer();
	friend int InterruptCB(void*);

	int DemuxVideo(AVPacket& );
	int DemuxAudio(AVPacket& );

	bool ContainVideo() const;
	bool ContainAudio() const;

	double GetFrameRate();
	std::pair<int, int> GetFrameSize();
	AVPixelFormat GetFrameFormat();

	AVSampleFormat GetSampleFormat();
	int GetSampleRate();
	int GetSampleChannel();
	int64_t GetChannelLayout();
	int64_t GetDuration();

	AVRational GetVideoTimebase(int);
	AVRational GetAudioTimebase(int);

	std::map<std::string, std::string> Parse_extradata(int);

protected:
	AVFormatContext* m_pFormatContext = nullptr;
	int m_iInterrupt = 0;
	int m_iVideoIndex = AVERROR_STREAM_NOT_FOUND;
	int m_iAudioIndex = AVERROR_STREAM_NOT_FOUND;
	AVCodecID m_vCodecID = AV_CODEC_ID_NONE;
	AVCodecID m_aCodecID = AV_CODEC_ID_NONE;
	AVPixelFormat m_pixFormat = AV_PIX_FMT_NONE;
	AVSampleFormat m_sampleFormat = AV_SAMPLE_FMT_NONE;

	std::queue<AVPacket> m_vPackets;
	std::queue<AVPacket> m_aPackets;
	std::mutex           m_demuxeLock;

	/*
	* Annex-B // rtsp
	* AVCC    // mp4 flv mkv
	*/
	bool m_bAnnexb = true;
	AVBSFContext* m_bsfMp4ToAnnexb = nullptr;
};

class FFmpegDecode : public FFmpegDemuxer
{
public:
	FFmpegDecode(std::string);
	~FFmpegDecode();

	virtual int CreateDecoder();
	virtual int DecodeVideo(AVPacket&);
	virtual int DecodeAudio(AVPacket&);
	int DecodeVideoFrame();
	int DecodeAudioFrame();

	AVFrame* GetVFrame() { return m_pVFrame; }
	AVFrame* GetAFrame() { return m_pAFrame; }
	virtual bool IsSupportHW() { return false; }
protected:
	AVCodecContext* m_pVCodecContext = nullptr;
	AVCodecContext* m_pACodecContext = nullptr;
	AVCodec* m_pVCodec = nullptr;
	AVCodec* m_pACodec = nullptr;
	AVFrame* m_pVFrame = nullptr;
	AVFrame* m_pAFrame = nullptr;
};

class FFmpegHWDecode : public FFmpegDecode
{
public:
	friend enum AVPixelFormat get_hw_format(AVCodecContext*, const enum AVPixelFormat*);

	FFmpegHWDecode(std::string);
	~FFmpegHWDecode();
	int CreateDecoder();
	int DecodeVideo(AVPacket&);
	bool IsSupportHW() { return m_bIsSupportHW; }

protected:
	enum AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;
	AVBufferRef* hw_device_ctx = NULL;
	AVFrame* m_hwVFrame = NULL;
	bool m_bIsSupportHW = false;
};

class FFmpegImageScale
{
public:
	FFmpegImageScale();
	~FFmpegImageScale();

	int Configure(int srcW, int srcH, AVPixelFormat srcFmt,
		int dstW, int dstH, AVPixelFormat dstFmt);
	int Convert(const uint8_t* const src[], const int srcStride[]);
	int Convert(const uint8_t* const src[], const int srcStride[], uint8_t* const dst[], const int dstStride[]);
	AVFrame* Frame()
	{
		return m_pFrame;
	}

protected:
	int m_iSrcW = 0;
	int m_iSrcH = 0;
	SwsContext* m_pVSws = NULL;
	AVFrame* m_pFrame = NULL;
};

class FFmpegAudioConvert
{
public:
	FFmpegAudioConvert();
	~FFmpegAudioConvert();

	int Configure(int srcSampleRate, uint64_t srcLayout, AVSampleFormat srcFmt,
		int dstSampleRate, uint64_t dstLayout, AVSampleFormat dstFmt,
		int dstSampleCount);
	int Convert(const uint8_t**, int);
	AVFrame* Frame()
	{
		return m_pFrame;
	}

protected:
	SwrContext* m_pASwr = NULL;
	AVFrame* m_pFrame = NULL;
};