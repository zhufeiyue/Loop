#include <iostream>

#include "common/EventLoop.h"
#include "common/AsioSocket.h"
#include "torrent/TorrentParser.h"

int main(int arc, char* argv[])
{
	Eventloop loop;

	LOG() << std::setlocale(LC_ALL, "");

	TorrentFile torrentFile;
	if (0 > torrentFile.LoadFromFile("d:/temp/1.torrent"))
	{
		return 0;
	}

	int count = 0;
	auto last = std::chrono::steady_clock::now();
	loop.AsioQueue().ScheduleTimer([&]()
		{
			auto now = std::chrono::steady_clock::now();
			auto temp = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
			last = now;
			LOG() <<"interval " << temp << "ms";
			//Sleep(10);

			if (count++ > 10000)
			{
				return -1;
			}

			return (int)CodeOK;
		}, 40, true);

	//auto pHttp = std::make_shared<HttpClient>(loop);
	//pHttp->Get("httP://tsing-i.com/");
	//pHttp->Get("https://www.baidu.com/");

	loop.Run();

	return 0;
}