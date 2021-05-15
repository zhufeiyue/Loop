#include <iostream>

#include "common/EventLoop.h"
#include "common/AsioSocket.h"
#include "torrent/TorrentParser.h"

int main(int arc, char* argv[])
{
	Eventloop loop;

	TorrentFile torrentFile;
	if (0 > torrentFile.LoadFromFile("d:/1.torrent"))
	{
		return 0;
	}

	//loop.AsioQueue().ScheduleTimer([]()
	//	{
	//		LOG() << "on timer";
	//		return CodeOK;
	//	}, 200, true);

	//auto pHttp = std::make_shared<HttpClient>(loop);
	//pHttp->Get("http://tsing-i.com/");
	//pHttp->Get("https://www.baidu.com/");

	loop.Run();

	return 0;
}