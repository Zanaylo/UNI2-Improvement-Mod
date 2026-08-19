// The game builds its five full screen render targets through d3dx9_42!D3DXCreateTexture, so this
// is how the mod reports what they were actually created at rather than what it asked for.

#pragma once

namespace D3DX9Hooks
{
	bool Install();
	bool IsInstalled();
}
