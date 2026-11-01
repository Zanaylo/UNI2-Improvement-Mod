#include "Core/Boot/CrashReport.h"

#include "Core/Log/LogFormat.h"
#include "Core/Log/LogRing.h"
#include "Core/info.h"
#include "Core/logger.h"
#include "Core/utils.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr size_t kTailLines = 300;
constexpr const char* kDumpExtension = ".dmp";
constexpr const char* kReportExtension = ".txt";

std::string ReportPath(const char* dumpPath)
{
	std::string path(dumpPath);
	const size_t dot = path.rfind(kDumpExtension);

	if (dot != std::string::npos)
		path.erase(dot);

	return path + kReportExtension;
}

void WriteHeader(FILE* file, const char* kind, const char* fault)
{
	fprintf(file, "%s %s stopped the game (%s)\r\n", UNI2_IM_NAME, UNI2_IM_VERSION, kind);
	fprintf(file, "Game build stamp 0x%08x, the mod supports %s\r\n", GetGameBuildStamp(), UNI2_IM_SUPPORTED_GAME_VERSION);
	fprintf(file, "%s\r\n\r\n", fault);

	const std::string logFile = GetLogFilePath();

	if (IsLoggerOpen() && !logFile.empty())
		fprintf(file, "The full log is %s\r\n", logFile.c_str());
	else
		fprintf(file, "Logging was off, so only warnings, errors and the crash context are below.\r\n");
}

}

void CrashReport::Write(const char* dumpPath, const char* kind, const char* fault)
{
	std::vector<LogEntry> tail;
	const bool read = GetLogHistory().TryCopyTail(kTailLines, tail);

	FILE* file = nullptr;

	if (fopen_s(&file, ReportPath(dumpPath).c_str(), "wb") != 0 || file == nullptr)
		return;

	WriteHeader(file, kind, fault);

	if (!read)
		fprintf(file, "\r\nThe log was busy at the moment of the crash and could not be read.\r\n");
	else
		fprintf(file, "\r\nThe last %zu log line(s):\r\n%s", tail.size(), LogFormat::Lines(tail).c_str());

	fclose(file);
}
