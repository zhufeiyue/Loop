#include "Sqlite3Wrap.h"
#include "Log.h"
#include <filesystem>

#ifdef _MSC_VER
#include <Windows.h>
#endif

KVStore::KVStore(std::string strFile)
{
	auto path = std::filesystem::path(strFile);
	auto bExist = std::filesystem::exists(path);
	int res = SQLITE_ERROR;

	res = sqlite3_open(strFile.c_str(), &m_db);
	if (SQLITE_OK != res)
	{
		LOG() << "sqlite3_open fail " << sqlite3_errmsg(m_db);
		sqlite3_close(m_db);
		m_db = nullptr;
		return;
	}

	const char* kTurnOffSynchronous = "PRAGMA synchronous = OFF;";
	sqlite3_exec(m_db, kTurnOffSynchronous, NULL, NULL, NULL);

	std::stringstream ss;
	ss << "create table kv("
		<< "key TEXT PRIMARY KEY NOT NULL,"
		<< "value TEXT NOT NULL);";
	res = sqlite3_exec(m_db, ss.str().c_str(), nullptr, nullptr, nullptr);
	if (SQLITE_OK != res)
	{
		LOG() << "create table fail " << sqlite3_errmsg(m_db);
	}

	ss.str("");
	ss << "insert into kv(key, value) values(?,?);";
	sqlite3_prepare_v2(m_db, ss.str().c_str(), (int)ss.str().length(), &m_insert_stmt, nullptr);

	ss.str("");
	ss << "delete from kv where key =?;";
	sqlite3_prepare_v2(m_db, ss.str().c_str(), (int)ss.str().length(), &m_delete_stmt, nullptr);

	ss.str("");
	ss << "select value from kv where key =?";
	sqlite3_prepare_v2(m_db, ss.str().c_str(), (int)ss.str().length(), &m_select_stmt, nullptr);
}

KVStore::~KVStore()
{
	if (m_insert_stmt)
	{
		sqlite3_finalize(m_insert_stmt);
	}
	if (m_delete_stmt)
	{
		sqlite3_finalize(m_delete_stmt);
	}
	if (m_select_stmt)
	{
		sqlite3_finalize(m_select_stmt);
	}

	if (m_db)
	{
		sqlite3_close(m_db);
		m_db = nullptr;
	}
}

int KVStore::DeleteKeyStartWith(const std::string& strKeyStartWith)
{
	std::stringstream ss;
	ss << "delete from kv where key LIKE '" << strKeyStartWith << "%';";
	auto res = sqlite3_exec(m_db, ss.str().c_str(), nullptr, nullptr, nullptr);
	if (res != SQLITE_OK)
	{
		LOG() << sqlite3_errmsg(m_db);
		return CodeNo;
	}

	return CodeOK;
}

int KVStore::Delete(const std::string& strKey)
{
	if (!m_delete_stmt)
	{
		return CodeNo;
	}

	int res = SQLITE_OK;
	res = sqlite3_bind_text(m_delete_stmt, 1, strKey.c_str(), (int)strKey.length(), nullptr);
	res = sqlite3_step(m_delete_stmt);
	sqlite3_reset(m_delete_stmt);

	if (res != SQLITE_DONE)
	{
		LOG() << sqlite3_errmsg(m_db);
		return CodeNo;
	}

	return CodeOK;
}

int KVStore::Put(const std::string& strKey, const std::string& strValue, bool bUpdateIfExistKey)
{
	if (!m_insert_stmt)
	{
		return CodeNo;
	}

	int res = SQLITE_OK;
	res = sqlite3_bind_text(m_insert_stmt, 1, strKey.c_str(), (int)strKey.length(), nullptr);
	res = sqlite3_bind_text(m_insert_stmt, 2, strValue.c_str(), (int)strValue.length(), nullptr);
	res = sqlite3_step(m_insert_stmt);
	sqlite3_reset(m_insert_stmt);

	if (res == SQLITE_CONSTRAINT && bUpdateIfExistKey)
	{
		std::stringstream ss;
		ss << "update kv set value = '" << strValue << "' where key = '" << strKey << "';";
		res = sqlite3_exec(m_db, ss.str().c_str(), nullptr, nullptr, nullptr);
		if (res == SQLITE_OK)
		{
			return CodeOK;
		}
	}

	if (res != SQLITE_DONE)
	{
		LOG() << sqlite3_errmsg(m_db);
		return CodeNo;
	}

	return CodeOK;
}

static int Getcallback(void* para, int nCount, char** pValue, char** pName) 
{
	std::string* pStrValue = static_cast<std::string*>(para);

	if (pStrValue)
	{
		pStrValue->operator= (pValue[0]);
	}

	// abort
	return 1;
}

std::string KVStore::Get(const std::string& strKey)
{
	std::string strValue;
	if (!m_db)
	{
		return strValue;
	}

	int res = SQLITE_OK;
	res = sqlite3_bind_text(m_select_stmt, 1, strKey.c_str(), (int)strKey.length(), nullptr);
	res = sqlite3_step(m_select_stmt);
	if (res == SQLITE_ROW)
	{
		strValue = (char*)sqlite3_column_text(m_select_stmt, 0);
	}
	sqlite3_reset(m_select_stmt);

	return strValue;
}