#include "Palette/PaletteTrace.h"

#include "Palette/PaletteTexture.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {

constexpr int kEntries = 512;
constexpr int kTextLength = 112;

struct Entry
{
	int frame;
	char text[kTextLength];
};

Entry g_entries[kEntries] = {};
std::atomic<int> g_written{ 0 };

int Slot(int index)
{
	const int written = g_written.load(std::memory_order_relaxed);
	if (written <= kEntries)
		return index;

	return (written + index) % kEntries;
}

}

void PaletteTrace::Reset()
{
	g_written.store(0, std::memory_order_relaxed);
	memset(g_entries, 0, sizeof(g_entries));
}

void PaletteTrace::Note(const char* format, ...)
{
	const int written = g_written.fetch_add(1, std::memory_order_relaxed);

	Entry& entry = g_entries[written % kEntries];
	entry.frame = PaletteTexture::GetFrameSerial();

	va_list args;
	va_start(args, format);
	vsnprintf_s(entry.text, sizeof(entry.text), _TRUNCATE, format, args);
	va_end(args);
}

int PaletteTrace::GetCount()
{
	const int written = g_written.load(std::memory_order_relaxed);

	return written < kEntries ? written : kEntries;
}

int PaletteTrace::GetDropped()
{
	const int written = g_written.load(std::memory_order_relaxed);

	return written > kEntries ? written - kEntries : 0;
}

int PaletteTrace::GetFrame(int index)
{
	if (index < 0 || index >= GetCount())
		return -1;

	return g_entries[Slot(index)].frame;
}

const char* PaletteTrace::GetText(int index)
{
	if (index < 0 || index >= GetCount())
		return "";

	return g_entries[Slot(index)].text;
}
