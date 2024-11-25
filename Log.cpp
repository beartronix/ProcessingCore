/*
  This file is part of the DSP-Crowd project
  https://www.dsp-crowd.com

  Author(s):
      - Johannes Natter, office@dsp-crowd.com

  File created on 19.03.2021

  Copyright (C) 2021-now Authors and www.dsp-crowd.com

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include <iostream>
#include <time.h>
#include <stdarg.h>
#if CONFIG_PROC_HAVE_DRIVERS
#include <mutex>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

#if CONFIG_PROC_HAVE_CHRONO
#include <chrono>
using namespace chrono;
#else
#include "time.hpp" // ??? my own implementation... otherwise need to include "stm32h5xx.h" or similar sh*t
#endif

#include "Processing.h"

typedef void (*LogEntryCreatedFct)(
			const int severity,
			const char *filename,
			const char *function,
			const int line,
			const int16_t code,
			const char *msg,
			const size_t len);

static LogEntryCreatedFct pFctLogEntryCreated = NULL;

#if CONFIG_PROC_HAVE_DRIVERS
static mutex mtxPrint;
#endif

#if CONFIG_PROC_HAVE_CHRONO
static system_clock::time_point tOld;
#endif

const string red("\033[0;31m");
const string yellow("\033[0;33m");
const string green("\033[0;32m");
const string cyan("\033[0;36m");
const string reset("\033[0m");

const size_t cLogEntryBufferSize = 1024;
static int levelLog = 2;

void levelLogSet(int lvl)
{
	levelLog = lvl;
}

void pFctLogEntryCreatedSet(LogEntryCreatedFct pFct)
{
	pFctLogEntryCreated = pFct;
}

const char* severityToStr(const int severity)
{
	switch (severity)
	{
	case 1:
		return "ERR";
	case 2:
		return "WRN";
	case 3:
		return "INF";
	default:
		break;
	}
	return "DBG";
}

int16_t logEntryCreate(const int severity, const char *filename, const char *function, const int line, const int16_t code, const char *msg, ...)
{
#if CONFIG_PROC_HAVE_DRIVERS
	lock_guard<mutex> lock(mtxPrint);
#endif
	(void)filename;

	char *pBuf = new (nothrow) char[cLogEntryBufferSize];
	if (!pBuf)
		return code;

	char *pStart = pBuf;
	char *pEnd = pStart + cLogEntryBufferSize - 1;

	*pStart = 0;
	*pEnd = 0;

	va_list args;

#if CONFIG_PROC_HAVE_CHRONO
	system_clock::time_point t = system_clock::now();
	duration<long, nano> tDiff = t - tOld;
	double tDiffSec = tDiff.count() / 10e9;
	time_t tt_t = system_clock::to_time_t(t);
	tOld = t;
#else
	time_t tt_t = getRtcTime(); // from time.hpp...
#endif
	tm bt {};
	char timeBuf[32];

#ifdef _WIN32
	::localtime_s(&bt, &tt_t);
#else
	::localtime_r(&tt_t, &bt);
#endif
	strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &bt);

	// "%03d"
	pStart += snprintf(pStart, pEnd - pStart, "%s.%03d L%4d %s  %-24s ", timeBuf, (int)(millis()%1000), line, severityToStr(severity), function);

	va_start(args, msg);
	pStart += vsnprintf(pStart, pEnd - pStart, msg, args);
	va_end(args);

	// Creating log entry
	if (severity <= levelLog)
	{
#ifdef _WIN32
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

		if (severity == 1)
		{
			SetConsoleTextAttribute(hConsole, 4);
			cerr << pBuf << "\r\n" << flush;
		}
		else
		if (severity == 2)
		{
			SetConsoleTextAttribute(hConsole, 6);
			cerr << pBuf << "\r\n" << flush;
		}
		else
			cout << pBuf << "\r\n" << flush;

		SetConsoleTextAttribute(hConsole, 7);
#else

#if 1
		if (severity == 1)
			cerr << red;
		else
		if (severity == 2)
			cerr << yellow;
		else
		if (severity == 3)
			cout << green;
		else
			cout << cyan;
		cout << pBuf << reset << "\r\n" << flush;
#else
			cout << pBuf << reset << "\r\n" << flush;
#endif
#endif
	}

	if (pFctLogEntryCreated)
		pFctLogEntryCreated(severity, filename, function, line, code, pBuf, pStart - pBuf);

	delete[] pBuf;

	return code;
}

