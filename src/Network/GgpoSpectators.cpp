#include "Network/GgpoSpectators.h"

#include "Core/utils.h"
#include "Game/GameOffsets.h"

#include <Windows.h>

#include <algorithm>

namespace {

typedef int(__thiscall* AddSpectatorFn)(void*, const char*, uint32_t, uint32_t, uint32_t);

constexpr const char* kLoopback = "127.0.0.1";

uintptr_t EndpointAt(uintptr_t session, int index)
{
	return session + GameOffsets::kGgpoSpectatorEndpoints + static_cast<uintptr_t>(index) * GameOffsets::kGgpoEndpointStride;
}

uint32_t ReadField(uintptr_t address)
{
	uint32_t value = 0;
	TryReadDword(reinterpret_cast<const void*>(address), value);

	return value;
}

uint64_t EndpointSteamId(uintptr_t endpoint)
{
	const uint64_t high = ReadField(endpoint + GameOffsets::kGgpoEndpointSteamHigh);

	return (high << 32) | ReadField(endpoint + GameOffsets::kGgpoEndpointSteamLow);
}

int Find(uintptr_t session, uint64_t steamId)
{
	const int count = GgpoSpectators::Count(session);

	for (int i = 0; i < count; ++i)
	{
		if (EndpointSteamId(EndpointAt(session, i)) == steamId)
			return i;
	}

	return -1;
}

int CallAdd(uintptr_t session, uint64_t steamId)
{
	const AddSpectatorFn add = reinterpret_cast<AddSpectatorFn>(RvaToAddress(GameOffsets::kFnAddSpectatorEndpoint));

	__try
	{
		return add(reinterpret_cast<void*>(session), kLoopback, GameOffsets::kGgpoLocalPort,
			static_cast<uint32_t>(steamId), static_cast<uint32_t>(steamId >> 32));
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return GgpoSpectators::kAddFaulted;
	}
}

}

uintptr_t GgpoSpectators::PeerSession()
{
	const uint32_t session = ReadField(RvaToAddress(GameOffsets::kGgpoSession));

	if (session == 0)
		return 0;

	return ReadField(session) == RvaToAddress(GameOffsets::kGgpoBackendVTable) ? session : 0;
}

bool GgpoSpectators::Synchronizing(uintptr_t session)
{
	uint8_t flag = 0;

	return session != 0 &&
		TryReadMemory(&flag, reinterpret_cast<const void*>(session + GameOffsets::kGgpoSynchronizing), sizeof(flag)) &&
		flag != 0;
}

int GgpoSpectators::Count(uintptr_t session)
{
	if (session == 0)
		return 0;

	const uint32_t count = ReadField(session + GameOffsets::kGgpoSpectatorCount);

	return count > static_cast<uint32_t>(GameOffsets::kGgpoMostSpectators) ? 0 : static_cast<int>(count);
}

int GgpoSpectators::IndexOf(uintptr_t session, uint64_t steamId)
{
	return session != 0 && steamId != 0 ? Find(session, steamId) : -1;
}

bool GgpoSpectators::Holds(uintptr_t session, uint64_t steamId)
{
	return IndexOf(session, steamId) >= 0;
}

int GgpoSpectators::Links(uintptr_t session, Link* out, int most)
{
	const int count = (std::min)(Count(session), most);

	for (int i = 0; i < count; ++i)
	{
		const uintptr_t endpoint = EndpointAt(session, i);

		out[i].id = EndpointSteamId(endpoint);
		out[i].state = ReadField(endpoint + GameOffsets::kGgpoEndpointState);
		out[i].pending = static_cast<int>(ReadField(endpoint + GameOffsets::kGgpoEndpointPendingOutput));
	}

	return count;
}

int GgpoSpectators::Add(uintptr_t session, uint64_t steamId, uint32_t syncTimeoutMs)
{
	if (session == 0 || steamId == 0)
		return kAddInvalid;

	if (Find(session, steamId) >= 0)
		return kAddAlreadyThere;

	const int index = Count(session);
	const int result = CallAdd(session, steamId);

	if (result != kAddOk)
		return result;

	TryWriteDword(reinterpret_cast<void*>(EndpointAt(session, index) + GameOffsets::kGgpoEndpointSyncTimeout),
		syncTimeoutMs);

	return kAddOk;
}

bool GgpoSpectators::Drop(uintptr_t session, uint64_t steamId)
{
	const int index = IndexOf(session, steamId);

	if (index < 0)
		return false;

	const uintptr_t endpoint = EndpointAt(session, index);

	return TryWriteDword(reinterpret_cast<void*>(endpoint + GameOffsets::kGgpoEndpointShutdownAt), 0) &&
		TryWriteDword(reinterpret_cast<void*>(endpoint + GameOffsets::kGgpoEndpointState),
			GameOffsets::kGgpoStateDisconnected);
}
