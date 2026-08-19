#include "Training/DummyScript.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/GameOffsets.h"
#include "Game/GameState.h"
#include "Game/MemoryMap.h"
#include "Palette/PaletteManager.h"

#include <cstdio>
#include <string>
#include <cstring>

namespace {

DummyScript::Frame g_frames[DummyScript::kMaxFrames] = {};
int g_count = 0;

char g_status[128] = "nothing written yet";

uint8_t g_heldLever = 0;
uint8_t g_heldButtons = 0;

bool Emit(uint8_t lever, uint8_t buttons, int frames)
{
	for (int i = 0; i < frames; ++i)
	{
		if (g_count >= DummyScript::kMaxFrames)
			return false;

		g_frames[g_count].lever = lever;
		g_frames[g_count].buttons = buttons;
		++g_count;
	}

	return true;
}

bool ButtonBit(char c, uint8_t& out)
{
	switch (c)
	{
	case 'A': case 'a': out = 1; return true;
	case 'B': case 'b': out = 2; return true;
	case 'C': case 'c': out = 4; return true;
	case 'D': case 'd': out = 8; return true;
	default: return false;
	}
}

}

bool DummyScript::Parse(const char* text, char* outError, int errorSize)
{
	g_count = 0;
	g_heldLever = 0;
	g_heldButtons = 0;

	if (text == nullptr)
		return false;

	int line = 1;
	const char* p = text;

	while (*p != '\0')
	{

		while (*p == ' ' || *p == '\t' || *p == '\r')
			++p;

		if (*p == '\n')
		{
			++p;
			++line;
			continue;
		}

		if (*p == '\0')
			break;

		const char* start = p;
		while (*p != '\0' && *p != '\n' && *p != '\r')
			++p;

		char token[64] = {};
		const int length = static_cast<int>(p - start) < 63 ? static_cast<int>(p - start) : 63;
		memcpy(token, start, length);

		int end = length;
		while (end > 0 && (token[end - 1] == ' ' || token[end - 1] == '\t'))
			token[--end] = '\0';

		bool ok = false;

		if (end == 0)
		{
			ok = true;
		}
		else if (token[0] == 'W' || token[0] == 'w')
		{

			const int frames = atoi(token + 1);
			ok = frames > 0 && Emit(g_heldLever, g_heldButtons, frames);
		}
		else if (token[0] == '[')
		{

			uint8_t bit = 0;
			if (token[1] >= '0' && token[1] <= '9')
			{
				g_heldLever = static_cast<uint8_t>(token[1] - '0');
				ok = true;
			}
			else if (ButtonBit(token[1], bit))
			{
				g_heldButtons |= bit;
				ok = true;
			}
		}
		else if (token[0] == ']')
		{

			uint8_t bit = 0;
			if (token[1] >= '0' && token[1] <= '9')
			{
				g_heldLever = 0;
				ok = true;
			}
			else if (ButtonBit(token[1], bit))
			{
				g_heldButtons &= static_cast<uint8_t>(~bit);
				ok = true;
			}
		}
		else
		{

			int digits = 0;
			while (token[digits] >= '0' && token[digits] <= '9')
				++digits;

			uint8_t buttons = 0;
			bool buttonsOk = true;
			for (int i = digits; i < end; ++i)
			{
				uint8_t bit = 0;
				if (!ButtonBit(token[i], bit))
				{
					buttonsOk = false;
					break;
				}

				buttons = static_cast<uint8_t>(buttons | bit);
			}

			if (buttonsOk && (digits > 0 || buttons != 0))
			{
				ok = true;

				if (digits == 0)
				{
					ok = Emit(g_heldLever, static_cast<uint8_t>(g_heldButtons | buttons), 1);
				}
				else
				{
					for (int i = 0; i < digits && ok; ++i)
					{
						const uint8_t lever = static_cast<uint8_t>(token[i] - '0');
						const bool last = i == digits - 1;
						const uint8_t pressed = last
							? static_cast<uint8_t>(g_heldButtons | buttons) : g_heldButtons;

						ok = Emit(lever, pressed, 1);
					}
				}
			}
		}

		if (!ok)
		{
			if (outError != nullptr && errorSize > 0)
				sprintf_s(outError, errorSize, "line %d: cannot read \"%s\"", line, token);

			g_count = 0;
			return false;
		}
	}

	if (outError != nullptr && errorSize > 0)
		outError[0] = '\0';

	return g_count > 0;
}

int DummyScript::GetFrameCount()
{
	return g_count;
}

const DummyScript::Frame* DummyScript::GetFrames()
{
	return g_frames;
}

bool DummyScript::WriteToSlot(int slot)
{
	if (g_count <= 0)
	{
		sprintf_s(g_status, "nothing to write - the script is empty");
		return false;
	}

	if (slot < 0 || slot >= GameOffsets::kRecorderSlotCount)
	{
		sprintf_s(g_status, "slot %d is out of range", slot);
		return false;
	}

	if (!GameState::AllowsTrainingTools())
	{
		sprintf_s(g_status, "not in a match");
		return false;
	}

	const uintptr_t container = RvaToAddress(GameOffsets::kRecorderContainer);
	const uintptr_t record = container + GameOffsets::kRecorderTakeTable +
		static_cast<uintptr_t>(slot) * GameOffsets::kRecorderTakeStride;

	const uint32_t start = static_cast<uint32_t>(slot) * GameOffsets::kRecorderSlotSpan;

	if (g_count > GameOffsets::kRecorderSlotFrames)
	{
		sprintf_s(g_status, "script is %d frames, a slot holds %d", g_count,
			GameOffsets::kRecorderSlotFrames);
		return false;
	}

	const uint32_t bytes = static_cast<uint32_t>(g_count) * 4u;
	const uintptr_t data = container + GameOffsets::kRecorderDataBase + start;

	for (int i = 0; i < g_count; ++i)
	{

		const uint32_t side = (static_cast<uint32_t>(g_frames[i].lever) << 8) | g_frames[i].buttons;
		const uint32_t both = (side << 16) | side;

		if (!TryWriteUnaligned(reinterpret_cast<void*>(data + static_cast<uintptr_t>(i) * 4), both))
		{
			sprintf_s(g_status, "the write failed %d frames in", i);
			return false;
		}
	}

	const bool wrote =
		TryWriteDword(reinterpret_cast<void*>(record + GameOffsets::kRecorderTakeStart), start) &&
		TryWriteDword(reinterpret_cast<void*>(record + GameOffsets::kRecorderTakeLength), bytes);

	if (!wrote)
	{
		sprintf_s(g_status, "the take record could not be written");
		return false;
	}

	TryWriteDword(reinterpret_cast<void*>(RvaToAddress(GameOffsets::kRecorderSlotActive) +
		static_cast<uintptr_t>(slot) * 4), 1);

	TryWriteDword(reinterpret_cast<void*>(RvaToAddress(GameOffsets::kRecorderObject) +
		GameOffsets::kRecorderLength), static_cast<uint32_t>(g_count));

	sprintf_s(g_status, "wrote %d frames to slot %d", g_count, slot + 1);
	LOG("dummy script: %s, span 0x%06x..0x%06x", g_status, start, start + bytes);
	return true;
}

const char* DummyScript::GetStatus()
{
	return g_status;
}

namespace {

struct LibraryEntry
{
	char label[96];
	char name[64];
	char path[MAX_PATH];
	int chara;
};

LibraryEntry g_library[DummyScript::kMaxLibrary] = {};
int g_libraryCount = 0;

std::string CharaFolder(int chara)
{
	if (chara < 0)
		return std::string();

	return GetModScriptPath(PaletteManager::GetCharaName(chara));
}

void AddFolder(int chara)
{
	const std::string folder = CharaFolder(chara);
	if (folder.empty())
		return;

	WIN32_FIND_DATAA found = {};
	HANDLE search = FindFirstFileA((folder + "\\*.txt").c_str(), &found);
	if (search == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			continue;

		if (g_libraryCount >= DummyScript::kMaxLibrary)
			break;

		LibraryEntry& entry = g_library[g_libraryCount];

		std::string name(found.cFileName);
		const size_t dot = name.find_last_of('.');
		if (dot != std::string::npos)
			name = name.substr(0, dot);

		entry.chara = chara;
		strncpy_s(entry.name, name.c_str(), _TRUNCATE);
		sprintf_s(entry.label, "%s / %s", PaletteManager::GetCharaName(chara), name.c_str());
		sprintf_s(entry.path, "%s\\%s", folder.c_str(), found.cFileName);

		++g_libraryCount;
	}
	while (FindNextFileA(search, &found));

	FindClose(search);
}

}

namespace {

int g_scannedP1 = -2;
int g_scannedP2 = -2;

void Rescan(int p1, int p2)
{
	g_libraryCount = 0;
	g_scannedP1 = p1;
	g_scannedP2 = p2;

	AddFolder(p2);

	if (p1 != p2)
		AddFolder(p1);
}

}

namespace {

bool IsNeutral(uint32_t word)
{
	return (word & 0xffff) == 0;
}

uint32_t FrameWord(uintptr_t container, uint32_t start, int index)
{
	uint32_t word = 0;
	const uintptr_t at = container + GameOffsets::kRecorderDataBase + start +
		static_cast<uintptr_t>(index) * 4;

	if (!TryReadUnaligned(reinterpret_cast<const void*>(at), word))
		return 0;

	return word;
}

int CountNonNeutral(uintptr_t container, uint32_t start, int frames)
{
	if (start >= GameOffsets::kRecorderDataSize || frames <= 0)
		return 0;

	int count = 0;
	for (int i = 0; i < frames; ++i)
	{
		if (!IsNeutral(FrameWord(container, start, i)))
			++count;
	}

	return count;
}

int MeasureExtent(uintptr_t container, uint32_t start, int frames)
{
	if (start >= GameOffsets::kRecorderDataSize)
		return 0;

	int last = -1;
	for (int i = 0; i < frames; ++i)
	{
		if (!IsNeutral(FrameWord(container, start, i)))
			last = i;
	}

	return last + 1;
}

}

void DummyScript::RefreshLibrary()
{
	const int p1 = PaletteManager::GetCharaNumber(0);
	const int p2 = PaletteManager::GetCharaNumber(1);

	if (p1 == g_scannedP1 && p2 == g_scannedP2)
		return;

	Rescan(p1, p2);
}

int DummyScript::GetLibraryCount()
{
	return g_libraryCount;
}

const char* DummyScript::GetLibraryLabel(int index)
{
	if (index < 0 || index >= g_libraryCount)
		return "";

	return g_library[index].label;
}

int DummyScript::GetLibraryChara(int index)
{
	if (index < 0 || index >= g_libraryCount)
		return -1;

	return g_library[index].chara;
}

const char* DummyScript::GetLibraryName(int index)
{
	if (index < 0 || index >= g_libraryCount)
		return "";

	return g_library[index].name;
}

bool DummyScript::DeleteFromLibrary(int index)
{
	if (index < 0 || index >= g_libraryCount)
		return false;

	if (DeleteFileA(g_library[index].path) == 0)
	{
		sprintf_s(g_status, "could not delete %s", g_library[index].label);
		return false;
	}

	sprintf_s(g_status, "deleted %s", g_library[index].label);
	Rescan(g_scannedP1, g_scannedP2);
	return true;
}

bool DummyScript::LoadFromLibrary(int index, char* outText, int size)
{
	if (index < 0 || index >= g_libraryCount || outText == nullptr || size <= 0)
		return false;

	FILE* file = nullptr;
	if (fopen_s(&file, g_library[index].path, "r") != 0 || file == nullptr)
	{
		sprintf_s(g_status, "could not read %s", g_library[index].label);
		return false;
	}

	const size_t read = fread(outText, 1, static_cast<size_t>(size) - 1, file);
	outText[read] = '\0';
	fclose(file);

	sprintf_s(g_status, "loaded %s", g_library[index].label);
	return true;
}

bool DummyScript::Save(int player, const char* name, const char* text)
{
	if (name == nullptr || name[0] == '\0' || text == nullptr)
	{
		sprintf_s(g_status, "give the script a name first");
		return false;
	}

	const int chara = PaletteManager::GetCharaNumber(player);
	const std::string folder = CharaFolder(chara);

	if (folder.empty())
	{
		sprintf_s(g_status, "no character on screen to save it under");
		return false;
	}

	CreateDirectoryA(folder.c_str(), nullptr);

	const std::string path = folder + "\\" + name + ".txt";

	FILE* file = nullptr;
	if (fopen_s(&file, path.c_str(), "w") != 0 || file == nullptr)
	{
		sprintf_s(g_status, "could not write %s.txt", name);
		return false;
	}

	fputs(text, file);
	fclose(file);

	sprintf_s(g_status, "saved %s / %s", PaletteManager::GetCharaName(chara), name);
	Rescan(PaletteManager::GetCharaNumber(0), PaletteManager::GetCharaNumber(1));
	return true;
}

bool DummyScript::ReadFromSlot(int slot, char* outText, int size)
{
	if (outText == nullptr || size <= 0)
		return false;

	if (slot < 0 || slot >= GameOffsets::kRecorderSlotCount)
	{
		sprintf_s(g_status, "slot %d is out of range", slot);
		return false;
	}

	if (!GameState::AllowsTrainingTools())
	{
		sprintf_s(g_status, "not in a match");
		return false;
	}

	const uintptr_t container = RvaToAddress(GameOffsets::kRecorderContainer);
	const uintptr_t record = container + GameOffsets::kRecorderTakeTable +
		static_cast<uintptr_t>(slot) * GameOffsets::kRecorderTakeStride;

	uint32_t start = 0;
	uint32_t bytes = 0;

	if (!TryReadDword(reinterpret_cast<const void*>(record + GameOffsets::kRecorderTakeStart), start) ||
		!TryReadDword(reinterpret_cast<const void*>(record + GameOffsets::kRecorderTakeLength), bytes))
	{
		sprintf_s(g_status, "could not read slot %d", slot + 1);
		return false;
	}

	const char* how = "the slot's own record";

	int frames = static_cast<int>(bytes / 4u);
	uint32_t position = start;

	const bool recordSane = bytes > 0 && bytes <= GameOffsets::kRecorderDataSize &&
		start < GameOffsets::kRecorderDataSize &&
		start + bytes <= GameOffsets::kRecorderDataSize;

	if (!recordSane || CountNonNeutral(container, start, frames) == 0)
	{
		const uint32_t mine = static_cast<uint32_t>(slot) * GameOffsets::kRecorderSlotSpan;
		const int span = GameOffsets::kRecorderSlotFrames;
		const int used = MeasureExtent(container, mine, span);

		if (used > 0)
		{
			position = mine;
			frames = recordSane && frames <= span ? frames : used;
			how = "the mod's own region";
		}
		else if (!recordSane)
		{
			sprintf_s(g_status, "slot %d has no take - start 0x%x length %u", slot + 1, start, bytes);
			LOG("dummy script: %s", g_status);
			return false;
		}
	}

	if (frames <= 0)
	{
		sprintf_s(g_status, "slot %d is empty", slot + 1);
		return false;
	}

	LOG("dummy script: slot %d record start 0x%x length %u, reading %d frames at 0x%x from %s",
		slot + 1, start, bytes, frames, position, how);

	const uintptr_t data = container + GameOffsets::kRecorderDataBase + position;

	int written = 0;
	uint8_t heldLever = 0;
	uint8_t heldButtons = 0;

	int runLever = -1;
	int runButtons = 0;
	int runLength = 0;

	auto flush = [&]()
	{
		if (runLength <= 0 || written >= size - 32)
			return;

		if (runLever != heldLever)
		{
			if (heldLever != 0)
				written += sprintf_s(outText + written, size - written, "]%d[\n", heldLever);
			if (runLever != 0)
				written += sprintf_s(outText + written, size - written, "[%d]\n", runLever);

			heldLever = static_cast<uint8_t>(runLever);
		}

		for (int bit = 0; bit < 4; ++bit)
		{
			const uint8_t mask = static_cast<uint8_t>(1u << bit);
			const char name = "ABCD"[bit];

			if ((runButtons & mask) && !(heldButtons & mask))
				written += sprintf_s(outText + written, size - written, "[%c]\n", name);
			else if (!(runButtons & mask) && (heldButtons & mask))
				written += sprintf_s(outText + written, size - written, "]%c[\n", name);
		}

		heldButtons = static_cast<uint8_t>(runButtons);
		written += sprintf_s(outText + written, size - written, "W%d\n", runLength);
	};

	bool useHigh = false;
	{
		int low = 0;
		int high = 0;

		for (int i = 0; i < frames; ++i)
		{
			uint32_t word = 0;
			if (!TryReadUnaligned(reinterpret_cast<const void*>(data + static_cast<uintptr_t>(i) * 4),
				word))
				break;

			if ((word & 0xffffu) != 0)
				++low;
			if ((word >> 16) != 0)
				++high;
		}

		useHigh = high > low;
	}

	for (int i = 0; i < frames && written < size - 64; ++i)
	{
		uint32_t word = 0;
		if (!TryReadUnaligned(reinterpret_cast<const void*>(data + static_cast<uintptr_t>(i) * 4), word))
			break;

		if (useHigh)
			word >>= 16;

		const int lever = static_cast<int>((word >> 8) & 0xff);
		const int buttons = static_cast<int>(word & 0x0f);

		if (lever == runLever && buttons == runButtons)
		{
			++runLength;
			continue;
		}

		flush();
		runLever = lever;
		runButtons = buttons;
		runLength = 1;
	}

	flush();

	if (heldLever != 0 && written < size - 8)
		written += sprintf_s(outText + written, size - written, "]%d[\n", heldLever);

	for (int bit = 0; bit < 4 && written < size - 8; ++bit)
	{
		if (heldButtons & (1u << bit))
			written += sprintf_s(outText + written, size - written, "]%c[\n", "ABCD"[bit]);
	}

	outText[written < size ? written : size - 1] = '\0';

	sprintf_s(g_status, "read %d frames out of slot %d", frames, slot + 1);
	return true;
}

namespace {

uintptr_t RecorderField(uintptr_t offset)
{
	const uintptr_t recorder = RvaToAddress(GameOffsets::kRecorderObject);
	return recorder == 0 ? 0 : recorder + offset;
}

bool SetRecorderField(uintptr_t offset, uint32_t value)
{
	const uintptr_t at = RecorderField(offset);
	return at != 0 && TryWriteDword(reinterpret_cast<void*>(at), value);
}

uint32_t GetRecorderField(uintptr_t offset)
{
	uint32_t value = 0;
	const uintptr_t at = RecorderField(offset);

	if (at == 0 || !TryReadDword(reinterpret_cast<const void*>(at), value))
		return 0;

	return value;
}

void ClearInputHold()
{
	for (int i = 0; i < 2; ++i)
	{
		uint8_t* const chara = static_cast<uint8_t*>(MemoryMap::GetCharaSlot(i));
		if (chara == nullptr)
			continue;

		uint32_t word = 0;
		uint8_t* const at = chara + GameOffsets::kPlayerDataInputHold;

		if (TryReadUnaligned(at, word))
			TryWriteUnaligned(at, word & 0xffffff00u);
	}
}

}

bool DummyScript::Play(int slot)
{
	if (slot < 0 || slot >= GameOffsets::kRecorderSlotCount)
	{
		sprintf_s(g_status, "slot %d is out of range", slot);
		return false;
	}

	if (!GameState::AllowsTrainingTools())
	{
		sprintf_s(g_status, "not in a match");
		return false;
	}

	const uintptr_t container = RvaToAddress(GameOffsets::kRecorderContainer);
	const uintptr_t record = container + GameOffsets::kRecorderTakeTable +
		static_cast<uintptr_t>(slot) * GameOffsets::kRecorderTakeStride;

	uint32_t bytes = 0;
	if (!TryReadDword(reinterpret_cast<const void*>(record + GameOffsets::kRecorderTakeLength), bytes) ||
		bytes < 4)
	{
		sprintf_s(g_status, "slot %d has nothing to play", slot + 1);
		return false;
	}

	const uint32_t frames = bytes / 4u;

	ClearInputHold();

	bool ok = SetRecorderField(GameOffsets::kRecorderMode, GameOffsets::kRecorderModePlayback);
	ok = ok && SetRecorderField(GameOffsets::kRecorderSlot, static_cast<uint32_t>(slot));
	ok = ok && SetRecorderField(GameOffsets::kRecorderCursor, 0);
	ok = ok && SetRecorderField(GameOffsets::kRecorderStarted, 1);
	ok = ok && SetRecorderField(GameOffsets::kRecorderTake, static_cast<uint32_t>(record));
	ok = ok && SetRecorderField(GameOffsets::kRecorderLength, frames);
	ok = ok && SetRecorderField(GameOffsets::kRecorderCapacity, frames);
	ok = ok && SetRecorderField(GameOffsets::kRecorderLiveInput, 0);
	ok = ok && SetRecorderField(GameOffsets::kRecorderOverran, 0);
	ok = ok && SetRecorderField(GameOffsets::kRecorderRunning, 1);

	if (!ok)
	{
		sprintf_s(g_status, "could not start slot %d", slot + 1);
		return false;
	}

	sprintf_s(g_status, "playing slot %d, %u frames", slot + 1, frames);
	LOG("dummy script: %s, take 0x%08x", g_status, static_cast<unsigned>(record));
	return true;
}

void DummyScript::Stop()
{
	if (GetRecorderField(GameOffsets::kRecorderMode) != GameOffsets::kRecorderModePlayback)
		return;

	SetRecorderField(GameOffsets::kRecorderRunning, 0);
	SetRecorderField(GameOffsets::kRecorderMode, GameOffsets::kRecorderModeIdle);
	SetRecorderField(GameOffsets::kRecorderLiveInput, 0);

	sprintf_s(g_status, "stopped");
}

bool DummyScript::IsPlaying()
{
	if (GetRecorderField(GameOffsets::kRecorderMode) != GameOffsets::kRecorderModePlayback)
		return false;

	if (GetRecorderField(GameOffsets::kRecorderRunning) == 0)
		return false;

	return GetRecorderField(GameOffsets::kRecorderCursor) <
		GetRecorderField(GameOffsets::kRecorderLength);
}

int DummyScript::GetPlaybackFrame()
{
	return static_cast<int>(GetRecorderField(GameOffsets::kRecorderCursor));
}
