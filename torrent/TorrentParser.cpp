#include "TorrentParser.h"
#include <fstream>
#include <iomanip>
#include <memory>
#include <boost/uuid/detail/sha1.hpp>
#include <boost/algorithm/string.hpp>
#include <common/Log.h>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static std::string utf8_to_local(const std::string& strUtf8)
{
	std::string res;
	char buf[2048];
	wchar_t wbuf[1024];

	if (strUtf8.empty())
	{
		return res;
	}

	// utf8 -> utf16
	auto n = MultiByteToWideChar(CP_UTF8, 0, strUtf8.c_str(), (int)strUtf8.length(), wbuf, 1024);
	if (n != 0)
	{
		// utf16 -> local
		n = WideCharToMultiByte(CP_ACP, 0, wbuf, n, buf, 2048, nullptr, nullptr);
		if (n != 0)
		{
			res = std::string(buf, n);
		}
	}

	return res;
}
#else

static std::string utf8_to_local(const std::string& strUtf8) { return strUtf8; }

#endif

BCodeInfoHash::BCodeInfoHash()
{
	m_bInfoHash = false;
}

void BCodeInfoHash::CreateInfoHash(const std::string& key, const char* data, int len)
{
	if (key != "info")
	{
		return;
	}
	if (!data || len < 10)
	{
		return;
	}

	unsigned int res[5] = { 0 };
	boost::uuids::detail::sha1 sha;
	sha.process_bytes(data, len);
	sha.get_digest(res);

	for (int i = 0; i < 5; ++i)
	{
		auto p = (int8_t*)&res[i];
		m_infoHash[i * 4] = p[3];
		m_infoHash[i * 4 + 1] = p[2];
		m_infoHash[i * 4 + 2] = p[1];
		m_infoHash[i * 4 + 3] = p[0];
	}
	m_bInfoHash = true;
}


int TorrentParser::ParseFromFile(std::string strFile)
{
	int res(-1);
	std::ifstream fileIn;
	std::streampos fileSize = 0;

	fileIn.open(strFile, std::ifstream::in | std::ifstream::binary);
	if (!fileIn.is_open())
	{
		return res;
	}

	fileIn.seekg(0, std::ifstream::end);
	fileSize = fileIn.tellg();
	fileIn.seekg(0, std::ifstream::beg);
	std::unique_ptr<char[]> pData(new char[(int)fileSize + 10]);
	if (!pData)
	{
		return res;
	}

	fileIn.read(pData.get(), (int)fileSize);
	auto readed = fileIn.gcount();
	if (readed != fileSize)
	{
		return res;
	}
	if (0 > m_bcode.Parse(pData.get(), (int)fileSize))
	{
		return res;
	}
	res = 1;
	return res;
}

void TorrentParser::ClearParse()
{
	m_bcode.Clear();
}

const BCodeInfoHash& TorrentParser::GetDictionary()const
{
	return m_bcode;
}

std::string TorrentParser::GetInfoHash()const
{
	if (m_bcode.m_bInfoHash)
	{
		return std::string((char*)m_bcode.m_infoHash, 20);
	}
	else
	{
		return "";
	}
}

std::vector<std::string> TorrentParser::GetTrackerURLs() const
{
	std::vector<std::string> urls;
	auto & dic = GetDictionary();
	auto announce = static_cast<const BCode_s*>(dic.GetValue("announce", BCode::string));
	if (announce)
	{
		urls.emplace_back(announce->m_str);
	}

	auto announce_list = static_cast<const BCode_l*>(dic.GetValue("announce-list", BCode::list));
	if (announce_list)
	{
		for (auto iter = announce_list->m_list.begin(); iter != announce_list->m_list.end(); ++iter)
		{
			auto item = static_cast<const BCode_l*>(*iter);
			if (item && item->m_list.size() > 0 && item->m_list[0]->GetType() == BCode::string)
			{
				urls.emplace_back(static_cast<const BCode_s*>(item->m_list[0])->m_str);
			}
		}
	}

	return urls;
}

int64_t TorrentParser::GetTotalSize()const
{
	auto fileInfos = GetFileInfo();
	int64_t res = 0;

	for (auto i : fileInfos)
	{
		res += i.size;
	}

	return res;
}

std::vector<TorrentParser::DownloadFileInfo> TorrentParser::GetFileInfo() const
{
	std::vector<TorrentParser::DownloadFileInfo> items;
	auto& dic = GetDictionary();

	if (!dic.Contain("info", BCode::dictionary))
	{
		return items;
	}
	auto info = static_cast<const BCode_d*>(dic.GetValue("info"));

	/*
	piece length
	pieces
	private

	1. single file
	name
	length
	md5sum

	2. multiple file
	name
	files
		length
		md5sum
		path
	*/

	if (info->Contain("length", BCode::interger))
	{
		DownloadFileInfo fileInfo;
		auto length = static_cast<const BCode_i*>(info->GetValue("length", BCode::interger));
		auto name = static_cast<const BCode_s*>(info->GetValue("name", BCode::string));

		if (length)
		{
			fileInfo.size = length->m_i;
		}
		if (name)
		{
			fileInfo.path = utf8_to_local(name->m_str);
		}

		items.push_back(fileInfo);
	}
	else if (info->Contain("files", BCode::list))
	{
		auto name = static_cast<const BCode_s*>(info->GetValue("name", BCode::string));
		std::string strFolderName;
		if (name)
		{
			strFolderName = name->m_str;
		}

		auto files = static_cast<const BCode_l*>(info->GetValue("files", BCode::list));
		for (auto iter = files->m_list.begin();
			iter != files->m_list.end();
			++iter)
		{
			auto item = *iter;
			if (item->GetType() == BCode::dictionary)
			{
				auto item_dic = static_cast<const BCode_d*>(item);
				if (item_dic->Contain("length", BCode::interger))
				{
					auto length = static_cast<const BCode_i*>(item_dic->GetValue("length", BCode::interger));
					auto path = static_cast<const BCode_l*>(item_dic->GetValue("path", BCode::list));

					std::string strPath = strFolderName;
					for (auto iter = path->m_list.begin(); iter != path->m_list.end(); ++iter)
					{
						strPath += "/";
						auto part = static_cast<BCode_s*>(*iter);
						if (part)
						{
							strPath += part->m_str;
						}
					}

					DownloadFileInfo fileInfo;
					fileInfo.size = length->m_i;
					fileInfo.path = utf8_to_local(strPath);

					items.push_back(fileInfo);
				}
			}
			else
			{}
		}
	}

	return items;
}

int32_t TorrentParser::GetPieceSize()const
{

	auto &dic = GetDictionary();
	if (!dic.Contain("info", BCode::dictionary))
	{
		return 0;
	}
	auto info = static_cast<const BCode_d*>(dic.GetValue("info"));

	if (info->Contain("piece length", BCode::interger))
	{
		auto pl = static_cast<const BCode_i*>(info->GetValue("piece length"));
		return (int32_t)pl->m_i;
	}
	else
	{
		return 0;
	}
}

int32_t TorrentParser::GetPieceNumber()const
{
	auto &dic = GetDictionary();
	if (!dic.Contain("info", BCode::dictionary))
	{
		return 0;
	}
	auto info = static_cast<const BCode_d*>(dic.GetValue("info"));

	if (info->Contain("pieces", BCode::string))
	{
		auto p = static_cast<const BCode_s*>(info->GetValue("pieces"));
		auto pieces_size = p->m_str.length();

		return int32_t(pieces_size) / 20;
	}
	else
	{
		return 0;
	}
}


int TorrentFile::LoadFromFile(std::string s)
{
	m_torrentFileParser.ClearParse();

	int res = m_torrentFileParser.ParseFromFile(s);
	if (res < 0)
	{
		LOG() << "parse " << s << " error ";
		return res;
	}

	auto fileInfos = m_torrentFileParser.GetFileInfo();
	auto pieceNum = m_torrentFileParser.GetPieceNumber();
	auto pieceSize = m_torrentFileParser.GetPieceSize();
	auto trackerURLs = m_torrentFileParser.GetTrackerURLs();
	auto infoHash = m_torrentFileParser.GetInfoHash();

	if (trackerURLs.empty() ||
		infoHash.length() != 20)
	{
		return -1;
	}

	trackerURLs.erase(
		std::unique(trackerURLs.begin(), trackerURLs.end()),
		trackerURLs.end());

	// 计算每个文件的位置
	int32_t pieceNo = 0;
	int32_t fileNo = 0;
	int64_t fileLeft = fileInfos[0].size;;
	int64_t offset = 0;
	int64_t pieceLeft = pieceSize;
	int64_t n;
	while (pieceNo < pieceNum)
	{
		n = (std::min)(fileLeft, pieceLeft);

		fileLeft -= n;
		pieceLeft -= n;
		offset += n;

		if (fileLeft == 0)
		{
			fileInfos[fileNo].iEndPieceIndex = pieceNo;
			fileInfos[fileNo].iEndPieceOffset = (int32_t)offset;

			fileNo += 1;
			if (fileNo >= fileInfos.size())
			{
				break;
			}
			fileLeft = fileInfos[fileNo].size;

			if (offset < pieceSize)
			{
				fileInfos[fileNo].iStartPieceIndex = pieceNo;
				fileInfos[fileNo].iStartPieceOffset = (int32_t)offset;
			}
			else
			{
				fileInfos[fileNo].iStartPieceIndex = pieceNo + 1;
				fileInfos[fileNo].iStartPieceOffset = 0;
			}
		}

		if (pieceLeft == 0)
		{
			pieceNo += 1;
			offset = 0;
			pieceLeft = pieceSize;
		}
	}

	for (auto iter = trackerURLs.begin(); iter != trackerURLs.end(); ++iter)
	{
		if (boost::istarts_with(*iter, "udp"))
		{
			auto pTracker = std::make_shared<UdpTracker>();
			//pTracker->Init(*iter);
			//pTracker->Init("udp://tracker.opentrackr.org:1337/announce");
			break;
		}
	}

	return 0;
}