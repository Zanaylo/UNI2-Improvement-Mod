#include "Game/Engine/CodeSignatures.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Hooks/HookManager.h"

#include <Windows.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct Signature
{
	uintptr_t rva;
	const char* name;
	const char* pattern;
};

const Signature kSignatures[] = {
#include "Game/Engine/CodeSignatures.inc"
};

constexpr int kCount = static_cast<int>(sizeof(kSignatures) / sizeof(kSignatures[0]));
constexpr int kMaxPatternBytes = 128;

struct Parsed
{
	uint8_t bytes[kMaxPatternBytes];
	bool known[kMaxPatternBytes];
	int length;
};

uintptr_t g_addresses[kCount] = {};
bool g_matched[kCount] = {};
std::atomic<bool> g_ready{ false };
int g_resolved = 0;
char g_status[192] = "not checked yet";

bool Parse(const char* pattern, Parsed& out)
{
	out.length = 0;

	for (const char* at = pattern; *at != '\0' && out.length < kMaxPatternBytes; )
	{
		if (*at == ' ')
		{
			++at;
			continue;
		}

		const bool wildcard = at[0] == '?';
		out.known[out.length] = !wildcard;
		out.bytes[out.length] = wildcard ? 0 : static_cast<uint8_t>(strtoul(at, nullptr, 16));
		++out.length;
		at += 2;
	}

	return out.length > 0;
}

bool MatchesAt(const uint8_t* code, const Parsed& parsed)
{
	for (int i = 0; i < parsed.length; ++i)
	{
		if (parsed.known[i] && code[i] != parsed.bytes[i])
			return false;
	}

	return true;
}

bool Verify(uintptr_t address, const Parsed& parsed)
{
	uint8_t code[kMaxPatternBytes] = {};

	return address != 0 && TryReadMemory(code, reinterpret_cast<const void*>(address), parsed.length) &&
		MatchesAt(code, parsed);
}

uintptr_t Search(const Parsed& parsed)
{
	uintptr_t start = 0;
	size_t size = 0;

	if (!HookManager::GetSectionBounds(".text", start, size) || size < static_cast<size_t>(parsed.length))
		return 0;

	const uint8_t* const code = reinterpret_cast<const uint8_t*>(start);
	uintptr_t found = 0;

	for (size_t i = 0; i + parsed.length <= size; ++i)
	{
		if (!MatchesAt(code + i, parsed))
			continue;

		if (found != 0)
			return 0;

		found = start + i;
	}

	return found;
}

uintptr_t ResolveOne(const Signature& signature, bool measured, bool& matched)
{
	Parsed parsed = {};
	matched = false;

	if (!Parse(signature.pattern, parsed))
		return 0;

	if (measured)
	{
		const uintptr_t address = RvaToAddress(signature.rva);
		matched = Verify(address, parsed);
		return matched ? address : 0;
	}

	const uintptr_t found = Search(parsed);
	matched = found != 0;
	return found;
}

int IndexOf(uintptr_t rva)
{
	int low = 0;
	int high = kCount - 1;

	while (low <= high)
	{
		const int middle = (low + high) / 2;

		if (kSignatures[middle].rva == rva)
			return middle;

		if (kSignatures[middle].rva < rva)
			low = middle + 1;
		else
			high = middle - 1;
	}

	for (int i = 0; i < kCount; ++i)
	{
		if (kSignatures[i].rva == rva)
			return i;
	}

	return -1;
}

}

void CodeSignatures::Initialize()
{
	if (g_ready.load())
		return;

	const bool measured = IsMeasuredGameBuild();
	int mismatched = 0;

	for (int i = 0; i < kCount; ++i)
	{
		g_addresses[i] = ResolveOne(kSignatures[i], measured, g_matched[i]);

		if (g_matched[i])
		{
			++g_resolved;
			continue;
		}

		++mismatched;
		LOG("CodeSignatures: %s at rva 0x%x %s", kSignatures[i].name, static_cast<unsigned>(kSignatures[i].rva),
			measured ? "does not match its signature, so it will not be called or hooked"
			: "was not found on this build");
	}

	sprintf_s(g_status, "%d of %d game functions %s", g_resolved, kCount,
		measured ? "verified against their signatures" : "found by signature on an unmeasured build");

	LOG("CodeSignatures: %s%s", g_status, mismatched == 0 ? "" : ", see the lines above");
	g_ready.store(true);
}

uintptr_t CodeSignatures::Address(uintptr_t rva)
{
	const int index = g_ready.load() ? IndexOf(rva) : -1;

	if (index < 0)
		return RvaToAddress(rva);

	return g_addresses[index];
}

int CodeSignatures::Count()
{
	return kCount;
}

int CodeSignatures::Resolved()
{
	return g_resolved;
}

const char* CodeSignatures::StatusText()
{
	return g_status;
}

bool CodeSignatures::Get(int index, Info& out)
{
	if (index < 0 || index >= kCount)
		return false;

	out = { kSignatures[index].name, kSignatures[index].rva, g_addresses[index], g_matched[index] };
	return true;
}
