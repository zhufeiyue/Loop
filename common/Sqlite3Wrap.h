#pragma once
#include <iostream>
#include <string>
#include "sqlite3/sqlite3.h"

class KVStore
{
public:
	KVStore(std::string);
	~KVStore();

	int DeleteKeyStartWith(const std::string& strKeyStartWith);
	int Delete(const std::string& strKey);
	int Put(const std::string& strKey, const std::string& strValue, bool bUpdateIfExist = true);
	std::string Get(const std::string& strKey);

protected:
	sqlite3* m_db = nullptr;
	sqlite3_stmt* m_insert_stmt = nullptr;
	sqlite3_stmt* m_delete_stmt = nullptr;
	sqlite3_stmt* m_select_stmt = nullptr;
};