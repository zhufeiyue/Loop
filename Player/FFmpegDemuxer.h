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
#include <libavutil/opt.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
}

#include <cassert>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <queue>
#include <map>
#include <mutex>

void PrintFFmpegError(int code, const char* strPrefix = nullptr);

class CustomIOProvider
{
public:
	virtual ~CustomIOProvider() {}
	virtual void    Reset() = 0;
	virtual int     Read(uint8_t* buf, int size) = 0;
	virtual int64_t Seek(int64_t offset, int whence) = 0;
};

class FileProvider : public CustomIOProvider
{
public:
	FileProvider(std::string);
	void    Reset();
	int     Read(uint8_t* buf, int size);
	int64_t Seek(int64_t offset, int whence);

private:
	std::ifstream m_file;
	int64_t       m_fileSize = 0;
};

class FFmpegDemuxer
{
public:
	FFmpegDemuxer(std::string, CustomIOProvider* );
	virtual ~FFmpegDemuxer();
	friend int InterruptCB(void*);
	virtual int Seek(int64_t, int64_t);

	int DemuxVideo(AVPacket& );
	int DemuxAudio(AVPacket& );

	bool ContainVideo() const;
	bool ContainAudio() const;
	bool EnableVideo(bool);
	bool EnableAudio(bool);

	double              GetFrameRate();
	std::pair<int, int> GetFrameSize();
	AVPixelFormat       GetFrameFormat();

	AVSampleFormat GetSampleFormat();
	int     GetSampleRate();
	int     GetSampleChannel();
	int64_t GetChannelLayout();
	int64_t GetDuration();

	AVRational GetVideoTimebase(int);
	AVRational GetAudioTimebase(int);

	int GetDemuxError();
	bool IsEOF();

	std::map<std::string, std::string> Parse_extradata(int);

protected:
	AVFormatContext* m_pFormatContext = nullptr;
	AVIOContext*     m_pIOContext = nullptr;
	int m_iInterrupt   = 0;
	int m_iDemuxError  = 0;
	int m_iVideoIndex    = AVERROR_STREAM_NOT_FOUND;
	int m_iAudioIndex    = AVERROR_STREAM_NOT_FOUND;
	bool m_bEnableVideo  = true;
	bool m_bEnableAudio  = true;
	AVCodecID m_vCodecID = AV_CODEC_ID_NONE;
	AVCodecID m_aCodecID = AV_CODEC_ID_NONE;
	AVPixelFormat m_pixFormat     = AV_PIX_FMT_NONE;
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
	FFmpegDecode(std::string, CustomIOProvider*);
	~FFmpegDecode();
	int Seek(int64_t, int64_t) override;

	virtual int CreateDecoder();
	virtual int DecodeVideo(AVPacket&);
	virtual int DecodeAudio(AVPacket&);
	int DecodeVideoFrame();
	int DecodeAudioFrame();
	int VideoDecodeError();
	int AudioDecodeError();

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

	int m_iDecodeAudioError = 0;
	int m_iDecodeVideoError = 0;
};

class FFmpegHWDecode : public FFmpegDecode
{
public:
	friend enum AVPixelFormat get_hw_format(AVCodecContext*, const enum AVPixelFormat*);

	FFmpegHWDecode(std::string, CustomIOProvider*);
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