#pragma once
#include <cmath>
#include <algorithm>
#include <map>
#include <vector>
#include <string>

struct BCode
{
	enum type
	{
		none,
		string,
		interger,
		list,
		dictionary
	};

	virtual ~BCode();
	virtual type GetType() const;
	virtual int Parse(const char*, size_t );
};

struct BCode_s: public BCode
{
	type GetType() const override;
	int Parse(const char*, size_t) override;
	std::string m_str;
};

struct BCode_i : public BCode
{
	BCode_i();
	type GetType() const override;
	int Parse(const char*, size_t) override;
	long long m_i;
};

struct BCode_l : public BCode
{
	~BCode_l();
	type GetType() const override;
	int Parse(const char*, size_t) override;
	void Clear();
	std::vector<BCode*> m_list;
};

struct BCode_d : public BCode
{
	~BCode_d();
	type GetType() const override;
	int Parse(const char*, size_t) override;
	void Clear();
	bool Contain(const std::string& )const;
	bool Contain(const std::string&, BCode::type) const;
	const BCode* GetValue(const std::string& k) const;
	const BCode* GetValue(const std::string& k, BCode::type t) const;
	virtual void CreateInfoHash(const std::string&, const char*, int);
private:
	std::multimap<std::string, BCode*> m_dic;
};
