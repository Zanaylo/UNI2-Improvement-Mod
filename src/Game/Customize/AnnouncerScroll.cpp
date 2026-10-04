#include "Game/Customize/AnnouncerScroll.h"

#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Customize/AnnouncerList.h"
#include "Hooks/GameHook.h"
#include "Hooks/HookManager.h"

#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace {

constexpr const char* kMenuClass = ".?AVCCustomizeMenuMenuCharacter@@";
constexpr int kUpdateSlot = 2;
constexpr size_t kGridCount = 0x924;
constexpr size_t kScrollY = 0x954;
constexpr size_t kWindowTop = 0x974;
constexpr size_t kWindowBottom = 0x97c;
constexpr size_t kSearchBytes = 0x5000;
constexpr size_t kAnimeY = 0x48;
constexpr size_t kNameBytes = 0x18;
constexpr size_t kNameSize = 0x10;
constexpr size_t kNameCapacity = 0x14;
constexpr uint32_t kInlineCapacity = 15;
constexpr int kColumns = 11;
constexpr int kShownRows = 3;
constexpr int kMostObjects = 1024;
constexpr const char* kIconPrefix = "CharaIcon";

using Update_t = int(__fastcall*)(void*);

GameHook<Update_t> g_updateHook("AnnouncerMenuUpdate");

struct Icons
{
	const uint8_t* menu = nullptr;
	const uint8_t* container = nullptr;
	uintptr_t objects = 0;
	std::unordered_map<uintptr_t, int> baseY;
	bool logged = false;
};

Icons g_icons;

uint32_t Dword(const uint8_t* at)
{
	uint32_t value = 0;
	TryReadDword(at, value);
	return value;
}

bool NameStarts(uintptr_t name, const char* prefix)
{
	uint32_t size = 0;
	uint32_t capacity = 0;
	const uint8_t* const at = reinterpret_cast<const uint8_t*>(name);

	if (!TryReadDword(at + kNameSize, size) || !TryReadDword(at + kNameCapacity, capacity))
		return false;

	const size_t length = strlen(prefix);
	const uint8_t* text = at;

	if (capacity > kInlineCapacity)
		text = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(Dword(at)));

	char read[16] = {};

	return size >= length && length < sizeof(read) && TryReadMemory(read, text, length) && memcmp(read, prefix, length) == 0;
}

bool IsIconContainer(const uint8_t* at)
{
	const uint32_t objectsBegin = Dword(at + 4);
	const uint32_t objectsEnd = Dword(at + 8);
	const uint32_t namesBegin = Dword(at + 0x10);
	const uint32_t namesEnd = Dword(at + 0x14);

	if (objectsBegin == 0 || objectsEnd <= objectsBegin || namesEnd <= namesBegin)
		return false;

	const uint32_t count = (objectsEnd - objectsBegin) / 4;

	return count <= kMostObjects && (namesEnd - namesBegin) == count * kNameBytes && NameStarts(namesBegin, kIconPrefix);
}

const uint8_t* FindContainer(const uint8_t* menu)
{
	for (size_t offset = 0; offset < kSearchBytes; offset += 4)
	{
		if (IsIconContainer(menu + offset))
			return menu + offset;
	}

	return nullptr;
}

int GridRows(const uint8_t* menu)
{
	const int count = static_cast<int>(Dword(menu + kGridCount));

	return count > 0 ? (count - 1) / kColumns + 1 : 0;
}

void Refresh(const uint8_t* menu)
{
	if (g_icons.menu == menu && g_icons.container != nullptr && g_icons.objects == Dword(g_icons.container + 4))
		return;

	g_icons = Icons();
	g_icons.menu = menu;
	g_icons.container = FindContainer(menu);
	g_icons.objects = g_icons.container != nullptr ? Dword(g_icons.container + 4) : 0;
}

void Place(const uint8_t* menu)
{
	const int scroll = static_cast<int>(Dword(menu + kScrollY));
	const uint32_t namesBegin = Dword(g_icons.container + 0x10);
	const uint32_t count = (Dword(g_icons.container + 8) - g_icons.objects) / 4;

	for (uint32_t i = 0; i < count; ++i)
	{
		if (!NameStarts(namesBegin + i * kNameBytes, kIconPrefix))
			continue;

		uint8_t* const anime = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(Dword(
			reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(g_icons.objects)) + i * 4)));

		if (anime == nullptr)
			continue;

		const auto known = g_icons.baseY.emplace(reinterpret_cast<uintptr_t>(anime), static_cast<int>(Dword(anime + kAnimeY)));
		TryWriteDword(anime + kAnimeY, static_cast<uint32_t>(AnnouncerList::ShownY(known.first->second, scroll)));
	}
}

void Scroll(void* self)
{
	uint8_t* const menu = static_cast<uint8_t*>(self);

	if (menu == nullptr || GridRows(menu) <= kShownRows)
		return;

	TryWriteDword(menu + kWindowTop, 0);
	TryWriteDword(menu + kWindowBottom, static_cast<uint32_t>(AnnouncerList::WindowBottom()));

	Refresh(menu);

	if (g_icons.container == nullptr)
		return;

	if (!g_icons.logged)
	{
		LOG("AnnouncerScroll: %d row(s), the icons scroll with the cursor", GridRows(menu));
		g_icons.logged = true;
	}

	Place(menu);
}

int __fastcall HookedUpdate(void* self)
{
	const int result = g_updateHook.Original()(self);
	Scroll(self);
	return result;
}

}

bool AnnouncerScroll::Initialize()
{
	const uintptr_t vtable = HookManager::FindRttiVTable(kMenuClass);
	uint32_t update = 0;

	if (vtable == 0 || !TryReadDword(reinterpret_cast<const void*>(vtable + kUpdateSlot * 4), update)
		|| !IsAddressInGameModule(update))
	{
		LOG("AnnouncerScroll: the announcer menu was not found, its grid stays as the game draws it");
		return false;
	}

	return g_updateHook.Install(reinterpret_cast<void*>(static_cast<uintptr_t>(update)), &HookedUpdate);
}
