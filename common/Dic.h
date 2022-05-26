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
		explicit DictionaryHelper(const T& v) : data(v) 
		{
		}
		template <typename T>
		explicit DictionaryHelper(T&& v) : data(std::forward<T>(v)) 
		{
		}

		template<typename T>
		T to(T defaultValue = T()) const
		{
			try
			{
				return std::get<T>(data);
			}
			catch (std::bad_variant_access const&)
			{
				return defaultValue;
			}
		}

		template <typename T>
		const T& toRef() const
		{
			return std::get<T>(data);
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
	template<typename T>
	InsRetVal insert(const std::string& key, T&& value)
	{
		return m_data.insert(std::make_pair(key, DictionaryHelper(std::forward<T>(value))));
	}

	template<typename T>
	T get(const std::string key) const
	{
		auto iter = m_data.find(key);
		if (iter != m_data.end()) 
		{
			return iter->second.to<T>();
		}
		else
		{
			return T();
		}
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

	void clear()
	{
		m_data.clear();
	}
private:
	Data m_data;
};