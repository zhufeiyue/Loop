#include "ParseUrl.h"
#include <algorithm>
#include <regex>

bool ParseUrl(const std::string& url,
	std::string& scheme,
	std::string& host,
	std::string& path,
	int& port)
{
	bool res(false);
	// https://stackoverflow.com/questions/5620235/cpp-regular-expression-to-validate-url
	// https://tools.ietf.org/html/rfc3986#page-50
	std::regex r(
		R"(^(([^:\/?#]+):)?(//([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)",
		std::regex::extended
	);
	std::smatch rmath;

	//	scheme = $2
	//	authority = $4
	//	path = $5
	//	query = $7
	//	fragment = $9

	if (std::regex_match(url, rmath, r))
	{
		scheme = rmath[2];
		if (scheme.empty())
		{
			goto End;
		}
		std::transform(scheme.begin(), scheme.end(), scheme.begin(), tolower);

		host = rmath[4];
		if (host.empty())
		{
			goto End;
		}
		auto pos = host.find(':');
		if (pos != std::string::npos)
		{
			port = atoi(host.c_str() + pos + 1);
			host = host.substr(0, pos);
		}
		else
		{
			if (scheme == "http")
				port = 80;
			else if (scheme == "https")
				port = 443;
			else
				goto End;
		}

		path = std::string(rmath[5]);
		if (path.empty())
		{
			path = '/';
		}
		else
		{
			auto query = std::string(rmath[7]);
			if (!query.empty())
				path = path + '?' + query;

			//auto fragment = std::string(rmath[9]);
			//if (!fragment.empty())
			//	path = path + '#' + fragment;
		}

		res = true;
	}
End:
	return res;
}
