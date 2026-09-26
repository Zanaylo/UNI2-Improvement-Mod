#include "Game/Menus/MenuInput.h"

#include "Core/utils.h"
#include "Game/Engine/GameOffsets.h"
#include "Game/Engine/CodeSignatures.h"

#include <cstdint>

namespace {

typedef int(__fastcall* ButtonFn)(void* self, void* unused, int repeat);

struct RawInput
{
	uint8_t bytes[GameOffsets::kPadInputSize];
};

bool Fetch(int player, RawInput& raw)
{
	const uintptr_t fetch = CodeSignatures::Address(GameOffsets::kFnFetchMenuInput);

	if (!IsAddressInGameModule(fetch))
		return false;

	uint8_t* const destination = raw.bytes;

	__asm
	{
		mov ecx, destination
		mov edx, player
		push 0
		mov eax, fetch
		call eax
		add esp, 4
	}

	return true;
}

bool FetchShared(RawInput& raw)
{
	const uintptr_t fetch = CodeSignatures::Address(GameOffsets::kFnFetchSharedMenuInput);

	if (!IsAddressInGameModule(fetch))
		return false;

	reinterpret_cast<void(__fastcall*)(void*)>(fetch)(raw.bytes);
	return true;
}

bool Asks(uintptr_t rva, RawInput& raw)
{
	const ButtonFn check = reinterpret_cast<ButtonFn>(CodeSignatures::Address(rva));

	if (!IsAddressInGameModule(reinterpret_cast<uintptr_t>(check)))
		return false;

	return check(raw.bytes, nullptr, 1) != 0;
}

bool Triggered(const RawInput& raw, int button)
{
	return (raw.bytes[GameOffsets::kMenuInputButtons + button] & GameOffsets::kMenuInputTriggered) != 0;
}

int Lever(const RawInput& raw)
{
	const int lever = raw.bytes[GameOffsets::kMenuInputLever];

	if (lever == MenuInput::kLeverUp || lever == MenuInput::kLeverDown ||
		lever == MenuInput::kLeverLeft || lever == MenuInput::kLeverRight)
		return lever;

	return MenuInput::kLeverNone;
}

void Decode(RawInput& raw, MenuInput::State& out)
{
	out.lever = Lever(raw);
	out.confirm = Asks(GameOffsets::kFnMenuInputConfirm, raw);
	out.cancel = Asks(GameOffsets::kFnMenuInputCancel, raw);
	out.openMenu = Triggered(raw, GameOffsets::kMenuButtonOpenMenu);
	out.nextPage = Triggered(raw, GameOffsets::kMenuButtonNextPage);
	out.previousPage = Triggered(raw, GameOffsets::kMenuButtonPreviousPage);
}

}

bool MenuInput::Read(int player, State& out)
{
	out = State();

	RawInput raw = {};

	if (!Fetch(player, raw))
		return false;

	Decode(raw, out);
	return true;
}

bool MenuInput::ReadShared(State& out)
{
	out = State();

	RawInput raw = {};

	if (!FetchShared(raw))
		return false;

	Decode(raw, out);
	return true;
}
