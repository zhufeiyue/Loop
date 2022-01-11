#include <clocale>
#include <iostream>

#include <common/EventLoop.h>
#include <common/AsioSocket.h>
#include <common/Sqlite3Wrap.h>
#include <torrent/TorrentParser.h>

int main(int arc, char* argv[])
{
	LOG() << std::setlocale(LC_ALL, "");
	Eventloop loop;

	KVStore* s = new KVStore("kv.db");

	auto n1 = std::chrono::steady_clock::now();
	s->Put("a", "b");
	s->Put("a", "123");

	for (int i = 0; i < 500; ++i)
	{
		s->Put(std::to_string(i), std::to_string(10000 - i) + "asdderferer");
	}
	auto value = s->Get("10");
	value = s->Get("999");
	value = s->Get("888");
	s->Delete("100");
	s->Delete("101");
	s->Delete("102");
	s->Delete("888");
	s->Put("888", "testPut########################################\n### ## #########################$$$$$$$$$$$$$$$$$$$$$$$$$$$");
	s->DeleteKeyStartWith("2");
	auto n2 = std::chrono::steady_clock::now();
	auto n = std::chrono::duration_cast<std::chrono::milliseconds>(n2 - n1).count();

	delete s;
	return 0;

	TorrentFile torrentFile;
	if (0 > torrentFile.LoadFromFile("d:/temp/2.torrent"))
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
			//LOG() <<"interval " << temp << "ms";

			if (count++ > 10000)
			{
				return -1;
			}

			return (int)CodeOK;
		}, 40, true);

	auto pHttp = std::make_shared<HttpClient>(loop);
	pHttp->Get("httP://tsing-i.com/");
	pHttp->Get("https://www.baidu.com/");

	loop.Run();

	return 0;
}