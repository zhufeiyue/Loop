#pragma once
#include <string>

bool ParseUrl(std::string& url,
	std::string& scheme,
	std::string& host,
	std::string& path,
	int& port);