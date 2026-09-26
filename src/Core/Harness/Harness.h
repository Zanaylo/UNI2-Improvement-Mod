#pragma once

namespace Harness
{
	constexpr const char* kEnvironmentVariable = "UNI2_IM_HARNESS";
	constexpr const char* kPipeName = "\\\\.\\pipe\\uni2-im-harness";

	bool IsActive();

	void InstallEarly();
	void Start();
}
