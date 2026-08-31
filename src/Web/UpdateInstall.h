#pragma once

#include "Web/Job.h"

#include <string>

namespace UpdateInstall
{
	struct Snapshot
	{
		Web::Job::Status job;
		bool busy;
		bool staged;
	};

	bool IsBusy();
	bool IsStaged();

	void Start();
	void Cancel();

	void OnFrame();

	void Read(Snapshot& out);
}
