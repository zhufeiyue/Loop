#pragma once
#include <string>

#include <QString>
#include <QVariant>

class Dic
{
public:
	class DicHelper
	{
	public:
		template <typename T>
		explicit DicHelper(const T& v) : data(v)
		{
		}
		DicHelper(const std::string& v) : data(QString::fromStdString(v))
		{
		}

		template<typename T>
		T to(T defaultValue = T()) const
		{
			return defaultValue;
		}

		template<> int32_t to<int32_t>(int32_t v)const {
			return data.type() == QMetaType::Int ? data.toInt() : v;
		}
		template<> int64_t to<int64_t>(int64_t v)const {
			return data.type() == QMetaType::LongLong ? data.toLongLong() : v;
		}
		template<> float to<float>(float v)const {
			return data.type() == QMetaType::Float ? data.toFloat() : v;
		}
		template<> double to<double>(double v)const {
			return data.type() == QMetaType::Double ? data.toDouble() : v;
		}
		template<> QString to<QString>(QString v)const {
			return data.type() == QMetaType::QString ? data.toString() : v;
		}
		template<> std::string to<std::string>(std::string v)const {
			return data.type() == QMetaType::QString ? data.toString().toStdString() : v;
		}
		template<> QByteArray to<QByteArray>(QByteArray)const {
			return data.toByteArray();
		}

	private:
		QVariant data;
	};

	typedef typename std::map<std::string, DicHelper> Data;
	typedef typename Data::iterator Iterator;
	typedef typename Data::const_iterator ConstIterator;
	typedef typename std::pair<Iterator, bool> InsRetVal;

	template<typename T>
	InsRetVal insert(const std::string& key, const T& value)
	{
		return m_data.insert(std::make_pair(key, DicHelper(value)));
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
	ConstIterator find(const std::string& key) const { return m_data.find(key); }

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