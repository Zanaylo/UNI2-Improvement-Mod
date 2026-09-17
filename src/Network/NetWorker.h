#pragma once

namespace NetWorker
{
	void Start();
	void Stop();

	bool IsRunning();
	double SlowestJobMs();
}
