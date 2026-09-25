#include "Network/Spectate/Spectate.h"

#include "Network/ModChannel.h"
#include "Network/Spectate/SpectateHost.h"
#include "Network/Spectate/SpectateViewer.h"
#include "Network/Spectate/SpectateWire.h"

#include <cstring>

namespace {

bool ForHost(uint8_t type)
{
	return type == SpectateWire::Type_Join || type == SpectateWire::Type_Armed || type == SpectateWire::Type_Leave ||
		type == SpectateWire::Type_Ack;
}

void Receive(const uint8_t* data, int size, uint64_t from)
{
	if (from == 0 || size < static_cast<int>(sizeof(SpectateWire::Message)))
		return;

	SpectateWire::Message message = {};
	memcpy(&message, data, sizeof(message));

	if (message.header.version != SpectateWire::kVersion)
		return;

	if (ForHost(message.type))
	{
		SpectateHost::Receive(message.type, data, size, from);
		return;
	}

	SpectateViewer::Receive(message.type, message.detail, data, size, from);
}

}

void Spectate::Initialize()
{
	ModChannel::Register(ModChannel::kKindSpectate, &Receive);

	SpectateHost::Initialize();
	SpectateViewer::Initialize();
}

void Spectate::Update()
{
	SpectateHost::Update();
	SpectateViewer::Update();
}
