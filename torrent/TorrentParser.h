#pragma once

#include <iostream>
#include <sstream>
#include "BCode.h"

//https://libtorrent.org/udp_tracker_protocol.html
//https://wiki.theory.org/index.php/BitTorrentSpecification

struct BCodeInfoHash : public BCode_d
{
	BCodeInfoHash();
	void CreateInfoHash(const std::string&, const char*, int) override;

	int8_t m_infoHash[20];
	bool m_bInfoHash;
};

class TorrentParser
{
public:
	struct DownloadFileInfo
	{
		int64_t size;
		std::string path;
	};

	int ParseFromFile(std::string);
	void ClearParse();

	const BCodeInfoHash& GetDictionary() const;
	std::string GetInfoHash() const;
	std::vector<std::string> GetTrackerURLs() const;
	std::vector<DownloadFileInfo> GetFileInfo() const;
	int64_t GetFileSize() const;
	int32_t GetPieceSize() const;
	int32_t GetPieceNumber() const;

protected:
	BCodeInfoHash m_bcode;
};

class TorrentFile
{
public:
	int LoadFromFile(std::string s);

protected:
	TorrentParser m_torrentFileParser;
	std::vector<std::string> m_vTrackers;
};