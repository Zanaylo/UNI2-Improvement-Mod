#include "Core/TimeStamp.h"

#include <Windows.h>

#include <cstdio>

namespace {

std::string Format(const SYSTEMTIME& time, bool withClock)
{
	char stamp[32] = {};

	if (withClock)
	{
		sprintf_s(stamp, "%04d%02d%02d_%02d%02d%02d",
			time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond);
		return stamp;
	}

	sprintf_s(stamp, "%04d%02d%02d", time.wYear, time.wMonth, time.wDay);
	return stamp;
}

SYSTEMTIME Now()
{
	SYSTEMTIME time = {};
	GetLocalTime(&time);
	return time;
}

}

std::string LocalDateStamp()
{
	return Format(Now(), false);
}

std::string LocalTimeStamp()
{
	return Format(Now(), true);
}
