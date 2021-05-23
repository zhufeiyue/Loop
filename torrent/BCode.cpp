#include "BCode.h"

BCode::~BCode()
{
}

BCode::type BCode::GetType()const
{
	return BCode::none;
}

int BCode::Parse(const char*, size_t)
{
	return -1;
}

BCode::type BCode_s::GetType()const
{
	return BCode::string;
}

int BCode_s::Parse(const char* s, size_t size)
{
	if (!s || size < 3 || s[0] == '0')
		return -1;
	auto pPos = strchr(s, ':');
	if (!pPos || pPos == s)
		return -1;
	if (!std::all_of(s, pPos, [](char ch) {return isdigit(ch); }))
		return -1;
	auto len = atoi(s);
	if (len < 1)
		return -1;
	if (pPos - s + 1 + len > size)
		return -1;
	m_str = std::string(pPos + 1, len);

	return pPos - s + 1 + len;
}

BCode_i::BCode_i():m_i(0)
{
}

BCode::type BCode_i::GetType()const
{
	return BCode::interger;
}

int BCode_i::Parse(const char* s, size_t size)
{
	if (!s || size < 3 || s[0] != 'i')
		return -1;
	auto pPos = strchr(s, 'e');
	if (!pPos)
		return -1;
	if (!std::any_of((s[1] == '-' ? s + 2 : s + 1), pPos, [](char ch) {return isdigit(ch); }))
		return -1;
	std::string temp(s + 1, pPos - s - 1);
	if (temp[0] == '0' && temp.length() != 1) // 03 is illeage
		return -1;
	m_i = std::stoll(temp, NULL, 10);

	return pPos - s + 1;
}

BCode_l::~BCode_l()
{
	Clear();
}

void BCode_l::Clear()
{
	for (auto iter = m_list.begin(); iter != m_list.end(); ++iter)
	{
		if (*iter)
			delete *iter;
	}
	m_list.clear();
}

BCode::type BCode_l::GetType()const
{
	return BCode::list;
}

int BCode_l::Parse(const char* s, size_t size)
{
	if (!s || size < 2 || s[0] != 'l')
		return -1;
	int listsize = 1;
	int itemsize(0);
	int tempsize = size - 1;
	auto pTemp = s + 1;
	BCode* pBCodeItem = NULL;
	while (true)
	{
		if (isdigit(pTemp[0]))
			pBCodeItem = new BCode_s();
		else if (pTemp[0] == 'i')
			pBCodeItem = new BCode_i();
		else if (pTemp[0] == 'l')
			pBCodeItem = new BCode_l();
		else if (pTemp[0] == 'd')
			pBCodeItem = new BCode_d();
		else if (pTemp[0] == 'e')
		{
			listsize += 1;
			break;
		}
		else
			return -1;

		itemsize = pBCodeItem->Parse(pTemp, tempsize);
		if (itemsize < 0)
		{
			delete pBCodeItem;
			return -1;
		}
		
		tempsize -= itemsize;
		if (tempsize <= 0)
			return -1;
		m_list.push_back(pBCodeItem);
		pTemp += itemsize;
		listsize += itemsize;
	}

	return listsize;
}

BCode_d::~BCode_d()
{
	Clear();
}

void BCode_d::Clear()
{
	for (auto iter = m_dic.begin(); iter != m_dic.end(); ++iter)
	{
		if (iter->second)
			delete iter->second;
	}
	m_dic.clear();
}

bool BCode_d::Contain(const std::string& k)const
{
	return GetValue(k) != nullptr;
}

bool BCode_d::Contain(const std::string& k, BCode::type type)const 
{
	return GetValue(k, type) != nullptr;
}

const BCode* BCode_d::GetValue(const std::string& k)const
{
	auto iter = m_dic.find(k);
	if (iter == m_dic.end())
	{
		return nullptr;
	}

	return iter->second;
}

const BCode* BCode_d::GetValue(const std::string& k, BCode::type t) const
{
	auto p = GetValue(k);
	if (p && p->GetType() == t)
	{
		return p;
	}
	else
	{
		return nullptr;
	}
}

void BCode_d::CreateInfoHash(const std::string&, const char*, int)
{
}

BCode::type BCode_d::GetType()const
{
	return BCode::dictionary;
}

int BCode_d::Parse(const char*s, size_t  size)
{
	if (!s || size < 2 || s[0] != 'd')
		return -1;
	int dicsize = 1;
	int tempsize = size - 1;
	auto pTemp = s + 1;
	BCode_s* pItemKey(NULL);
	BCode  * pItemValue(NULL);
	int itemkey_size(0), itemvalue_size(0);
	while (true)
	{
		if (pTemp[0] == 'e')
		{
			dicsize += 1;
			break;
		}

		if (!isdigit(pTemp[0]))
			return -1;
		pItemKey = new BCode_s();
		itemkey_size = pItemKey->Parse(pTemp, tempsize);
		if (itemkey_size < 0)
		{
			delete pItemKey;
			return -1;
		}
		tempsize -= itemkey_size;
		pTemp += itemkey_size;

		if (isdigit(pTemp[0]))
			pItemValue = new BCode_s();
		else if (pTemp[0] == 'i')
			pItemValue = new BCode_i();
		else if (pTemp[0] == 'l')
			pItemValue = new BCode_l();
		else if (pTemp[0] == 'd')
			pItemValue = new BCode_d();
		else 
		{
			delete pItemKey;
			return -1;
		}
		itemvalue_size = pItemValue->Parse(pTemp, tempsize);
		if (itemkey_size < 0)
		{
			delete pItemKey;
			delete pItemValue;
			return -1;
		}

		if (pItemKey->m_str == "info")
			CreateInfoHash(pItemKey->m_str, pTemp, itemvalue_size);

		tempsize -= itemvalue_size;
		pTemp += itemvalue_size;
		dicsize = dicsize + itemkey_size + itemvalue_size;
		m_dic.insert(std::make_pair(pItemKey->m_str, pItemValue));
		delete pItemKey;

		if (tempsize <= 0)
		{
			return -1;
		}
	}

	return dicsize;
}
