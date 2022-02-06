#pragma once

#include <sstream>
#include <vector>
#include <common/Dic.h>

class M3U8Parser
{
public:
	M3U8Parser(const std::string&);
	~M3U8Parser();

	bool IsValid() const;
	bool IsMain() const;
	bool IsLive() const;
	bool IsVod() const;

	int GetSubM3U8Info(std::vector<Dictionary>&);
	int GetSegmentInfo(std::vector<Dictionary>&);

protected:
	std::vector<std::string> m_vecLines;
};