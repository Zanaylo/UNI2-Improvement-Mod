#include "Network/Steam/SteamWatch.h"

#include "Network/NetLog.h"
#include "Network/RoomRoster.h"

#include <Windows.h>

#include <cstdint>
#include <cstring>

namespace {

class CallbackBase
{
public:
	virtual void Run(void* param) = 0;
	virtual void Run(void* param, bool ioFailure, uint64_t call) = 0;
	virtual int GetCallbackSizeBytes() = 0;

protected:
	uint8_t m_flags = 0;
	int m_callback = 0;
};

using Handler = void(*)(const uint8_t* data);

class Listener final : public CallbackBase
{
public:
	Listener(int callback, int size, Handler handler)
		: m_size(size), m_handler(handler)
	{
		m_callback = callback;
	}

	void Run(void* param) override
	{
		if (param != nullptr)
			m_handler(static_cast<const uint8_t*>(param));
	}

	void Run(void* param, bool, uint64_t) override
	{
		Run(param);
	}

	int GetCallbackSizeBytes() override
	{
		return m_size;
	}

	int Id() const
	{
		return m_callback;
	}

private:
	int m_size;
	Handler m_handler;
};

using RegisterCallback_t = void(__cdecl*)(CallbackBase*, int);

uint64_t U64(const uint8_t* data, int offset)
{
	uint64_t value = 0;
	memcpy(&value, data + offset, sizeof(value));

	return value;
}

int32_t I32(const uint8_t* data, int offset)
{
	int32_t value = 0;
	memcpy(&value, data + offset, sizeof(value));

	return value;
}

void Text(const uint8_t* data, int offset, int size, char* out, int outSize)
{
	const int copy = size < outSize - 1 ? size : outSize - 1;
	memcpy(out, data + offset, static_cast<size_t>(copy));
	out[copy] = 0;
}

void OnServersConnected(const uint8_t*)
{
	NetLog::Write("steam: connected to Steam servers");
}

void OnServerConnectFailure(const uint8_t* data)
{
	NetLog::Write("steam: could not reach Steam servers, result %d, still retrying %d", I32(data, 0), data[4]);
}

void OnServersDisconnected(const uint8_t* data)
{
	NetLog::Write("steam: lost the Steam servers, result %d", I32(data, 0));
}

void OnIpcFailure(const uint8_t* data)
{
	NetLog::Write("steam: IPC failure, type %u", data[0]);
}

void OnJoinRequested(const uint8_t* data)
{
	NetLog::Write("steam: join requested for lobby %llu by friend %llu", static_cast<unsigned long long>(U64(data, 0)),
		static_cast<unsigned long long>(U64(data, 8)));
}

void OnLobbyEnter(const uint8_t* data)
{
	NetLog::Write("steam: entered lobby %llu, response %d, locked %u", static_cast<unsigned long long>(U64(data, 0)),
		I32(data, 16), data[12]);
}

void OnLobbyDataUpdate(const uint8_t* data)
{
	const uint64_t lobby = U64(data, 0);
	const uint64_t member = U64(data, 8);

	if (lobby == member)
	{
		NetLog::Write("steam: lobby %llu data changed, success %u", static_cast<unsigned long long>(lobby), data[16]);
		return;
	}

	NetLog::Write("steam: lobby %llu member %llu data changed, success %u", static_cast<unsigned long long>(lobby),
		static_cast<unsigned long long>(member), data[16]);
}

void OnLobbyChatUpdate(const uint8_t* data)
{
	const uint64_t user = U64(data, 8);
	const int flags = I32(data, 24);

	char described[96] = {};
	RoomRoster::DescribeFlags(flags, described, sizeof(described));
	RoomRoster::Observe(user, flags);

	NetLog::Write("steam: lobby %llu member %llu %s (0x%02x), changed by %llu", static_cast<unsigned long long>(U64(data, 0)),
		static_cast<unsigned long long>(user), described, flags, static_cast<unsigned long long>(U64(data, 16)));
}

void OnLobbyChatMsg(const uint8_t* data)
{
	NetLog::Write("steam: lobby %llu chat entry %d type %u from %llu", static_cast<unsigned long long>(U64(data, 0)),
		I32(data, 20), data[16], static_cast<unsigned long long>(U64(data, 8)));
}

void OnP2PRequest(const uint8_t* data)
{
	NetLog::Write("steam: p2p session request from %llu", static_cast<unsigned long long>(U64(data, 0)));
}

void OnP2PFail(const uint8_t* data)
{
	NetLog::Write("steam: p2p session with %llu FAILED, error %u", static_cast<unsigned long long>(U64(data, 0)), data[8]);
}

void OnConnectionStatus(const uint8_t* data)
{
	char debug[129] = {};
	Text(data, 192, 128, debug, sizeof(debug));

	NetLog::Write("steam: connection %u with %llu state %d -> %d, end reason %d %s", static_cast<unsigned>(I32(data, 0)),
		static_cast<unsigned long long>(U64(data, 16)), I32(data, 704), I32(data, 184), I32(data, 188), debug);
}

void OnMessagesRequest(const uint8_t* data)
{
	NetLog::Write("steam: messages session request from %llu", static_cast<unsigned long long>(U64(data, 8)));
}

void OnMessagesFailed(const uint8_t* data)
{
	char debug[129] = {};
	Text(data, 184, 128, debug, sizeof(debug));

	NetLog::Write("steam: messages session with %llu FAILED, state %d, end reason %d %s",
		static_cast<unsigned long long>(U64(data, 8)), I32(data, 176), I32(data, 180), debug);
}

void OnRelayStatus(const uint8_t* data)
{
	char debug[129] = {};
	Text(data, 16, 128, debug, sizeof(debug));

	NetLog::Write("steam: relay network %d, config %d, any relay %d, measuring %d %s", I32(data, 0), I32(data, 8),
		I32(data, 12), I32(data, 4), debug);
}

Listener g_listeners[] = {
	Listener(101, 1, &OnServersConnected),
	Listener(102, 8, &OnServerConnectFailure),
	Listener(103, 4, &OnServersDisconnected),
	Listener(117, 1, &OnIpcFailure),
	Listener(333, 16, &OnJoinRequested),
	Listener(504, 24, &OnLobbyEnter),
	Listener(505, 24, &OnLobbyDataUpdate),
	Listener(506, 32, &OnLobbyChatUpdate),
	Listener(507, 24, &OnLobbyChatMsg),
	Listener(1202, 8, &OnP2PRequest),
	Listener(1203, 16, &OnP2PFail),
	Listener(1221, 712, &OnConnectionStatus),
	Listener(1251, 136, &OnMessagesRequest),
	Listener(1252, 696, &OnMessagesFailed),
	Listener(1281, 272, &OnRelayStatus),
};

int g_registered = 0;
bool g_tried = false;

}

bool SteamWatch::Register()
{
	if (g_tried)
		return g_registered > 0;

	const HMODULE steam = GetModuleHandleA("steam_api.dll");

	if (steam == nullptr)
		return false;

	g_tried = true;

	const RegisterCallback_t registerCallback =
		reinterpret_cast<RegisterCallback_t>(GetProcAddress(steam, "SteamAPI_RegisterCallback"));

	if (registerCallback == nullptr)
	{
		NetLog::Write("steam: SteamAPI_RegisterCallback is missing, Steam events are not logged");
		return false;
	}

	for (Listener& listener : g_listeners)
	{
		__try
		{
			registerCallback(&listener, listener.Id());
			++g_registered;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			NetLog::Write("steam: registering callback %d faulted", listener.Id());
		}
	}

	NetLog::Write("steam: watching %d Steam event kind(s) as a listener, nothing hooked", g_registered);
	return g_registered > 0;
}

bool SteamWatch::IsRegistered()
{
	return g_registered > 0;
}

int SteamWatch::Registered()
{
	return g_registered;
}
