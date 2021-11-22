#pragma once
#include <map>
#include <string>
#include <variant>

class Dictionary
{
public:
	class DictionaryHelper
	{
	public:
		//explicit DictionaryHelper(const std::string& v) : data(v) {}
		//explicit DictionaryHelper(const int32_t& v) : data(v) {}
		//explicit DictionaryHelper(const int64_t& v) : data(v) {}
		//explicit DictionaryHelper(const float& v) : data(v) {}
		//explicit DictionaryHelper(const double& v) : data(v) {}

		template <typename T>
		explicit DictionaryHelper(const T& v) : data(v) {}

		template<typename T>
		T to(T defaultValue = T()) const
		{
			try
			{
				return std::get<T>(data);
			}
			catch (std::bad_variant_access const&)
			{
				return std::move(defaultValue);
			}
		}

	private:
		std::variant<std::string, int32_t, int64_t, float, double> data;
	};

	typedef typename std::map<std::string, DictionaryHelper> Data;
	typedef typename Data::iterator Iterator;
	typedef typename Data::const_iterator ConstIterator;
	typedef typename std::pair<Iterator, bool> InsRetVal;

	template<typename T>
	InsRetVal insert(const std::string& key, const T& value)
	{
		return m_data.insert(std::make_pair(key, DictionaryHelper(value)));
	}

	Iterator begin() { return m_data.begin(); }
	Iterator end() { return m_data.end(); }
	ConstIterator begin() const { return m_data.begin(); }
	ConstIterator end() const { return m_data.end(); }

	Iterator find(const std::string& key) { return m_data.find(key); }
	ConstIterator find(const std::string& key) const  { return m_data.find(key); }

	Iterator erase(const std::string& key)
	{
		auto iter = find(key);
		if (iter != m_data.end())
		{
			iter = m_data.erase(iter);
		}
		return iter;
	}

	bool contain(const std::string& key) const
	{
		return find(key) != m_data.end();
	}

	template<typename T>
	bool contain_key_value(const std::string& key, const T& value)const
	{
		auto iter = find(key);
		if (iter != m_data.end())
		{
			if (iter->second.to<T>() == value)
			{
				return true;
			}
		}

		return false;
	}

private:
	Data m_data;
};