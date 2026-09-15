#pragma once

namespace BattleDataReload
{
	bool IsSupported();

	bool CanRunNow();

	bool Run();

	int VectorRecords();

	const char* StatusText();
}
