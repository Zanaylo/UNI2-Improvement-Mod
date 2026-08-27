#include "Core/CrashContext.h"

#include "Core/logger.h"

#include <Windows.h>

namespace {

constexpr int kMaxWriters = 8;

struct Entry
{
	const char* name;
	CrashContext::Writer writer;
};

Entry g_writers[kMaxWriters] = {};
int g_count = 0;

}

void CrashContext::Register(const char* name, Writer writer)
{
	if (name == nullptr || writer == nullptr || g_count >= kMaxWriters)
		return;

	g_writers[g_count].name = name;
	g_writers[g_count].writer = writer;
	++g_count;
}

void CrashContext::WriteAll()
{
	for (int i = 0; i < g_count; ++i)
	{
		LOG_SECTION(g_writers[i].name);

		__try
		{
			g_writers[i].writer();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			LOG("  (this writer faulted and was skipped)");
		}
	}
}
