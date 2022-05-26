#pragma once

#include <sstream>
#include <vector>
#include <common/Dic.h>

//https://datatracker.ietf.org/doc/html/rfc8216
class M3U8Parser
{
public:
	M3U8Parser(const std::string&);
	~M3U8Parser();

	bool IsValid() const;
	bool IsMaster() const;
	int64_t     GetSequenceNumber() const;
	int64_t     GetTargetDuration() const;
	std::string GetType() const;

	int GetVariantInfo(std::vector<Dictionary>&);
	int GetSegmentInfo(std::vector<Dictionary>&);

protected:
	std::vector<std::string> m_vecLines;
};

int ParseM3U8(std::string, Dictionary&, std::vector<Dictionary>&);