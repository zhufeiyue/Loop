#include "Dic.h"

void testDic()
{
	if (true)
	{
		Dic::DicHelper dh(100.4);
		auto ret0 = dh.to<int32_t>(101);
		auto ret1 = dh.to<int64_t>(102);
		auto ret2 = dh.to<float>(103.1f);
		auto ret3 = dh.to<double>(104.2);
		auto ret4 = dh.to<QString>("empty");
	}

	if (true)
	{
		Dic::DicHelper dh(100.4f);
		auto ret0 = dh.to<int32_t>(101);
		auto ret1 = dh.to<int64_t>(102);
		auto ret2 = dh.to<float>(103.1f);
		auto ret3 = dh.to<double>(104.2);
		auto ret4 = dh.to<QString>("empty");
	}

	if (true)
	{
		Dic::DicHelper dh(100);
		auto ret0 = dh.to<int32_t>(101);
		auto ret1 = dh.to<int64_t>(102);
		auto ret2 = dh.to<float>(103.1f);
		auto ret3 = dh.to<double>(104.2);
		auto ret4 = dh.to<QString>("empty");
	}

	if (true)
	{
		Dic::DicHelper dh("value");
		auto ret0 = dh.to<int32_t>(101);
		auto ret1 = dh.to<int64_t>(102);
		auto ret2 = dh.to<float>(103.1f);
		auto ret3 = dh.to<double>(104.2);
		auto ret4 = dh.to<QString>("empty");
	}

	if (true)
	{
		Dic dic;
		dic.insert("int32", 100);
		dic.insert("int64", (int64_t)101);
		dic.insert("float", 102.3f);
		dic.insert("double", 103.4);
		dic.insert("string", QString("sss"));

		auto iter = dic.find("int32");
		auto n1 = iter->second.to<int32_t>(0);
		iter = dic.find("int64");
		auto n2 = iter->second.to<int64_t>(0);
		iter = dic.find("float");
		auto n3 = iter->second.to<float>(0);
		iter = dic.find("double");
		auto n4 = iter->second.to<double>(0);
		iter = dic.find("string");
		auto n5 = iter->second.to<QString>(0);

		dic.insert("haha", "2345");
		auto n6 = dic.get<int32_t>("haha");
		auto n7 = dic.get<int64_t>("haha");
		auto n8 = dic.get<float>("haha");
		auto n9 = dic.get<QString>("haha");
		auto n10 = dic.get<std::string>("haha");
	}
}