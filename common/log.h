#pragma once
#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

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
	CodeNo = -1,
	CodeOK = 0
};