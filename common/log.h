#pragma once
#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

void SaveTemp(const std::string&, const std::string&);
int InitLogger(const char*);

class LogPrintHelper
{
public:
	LogPrintHelper();
	~LogPrintHelper();
	std::stringstream& Log();
protected:
	std::stringstream m_ss;
};

#ifdef LOG
#error redefine LOG
#else
#define LOG LogPrintHelper().Log
#endif

enum ErrorCode
{
	CodeInvalidParam = -2,
	CodeNo = -1,
	CodeOK = 0,
	CodeAgain = 1,
	CodeRejection
};