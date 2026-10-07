#include "Network/UploadLedger.h"

bool UploadLedger::Adopt(uintptr_t request, UploadKind kind, uint32_t now)
{
	if (request == 0 || m_count >= kCapacity || Holds(request))
		return false;

	m_entries[m_count++] = { request, kind, now };
	return true;
}

int UploadLedger::Collect(IUploadRequests& requests, uint32_t now, Outcome* out, int capacity)
{
	int written = 0;
	int index = 0;

	while (index < m_count && written < capacity)
	{
		const Entry entry = m_entries[index];
		const uint32_t elapsed = now - entry.adoptedAt;

		if (requests.IsFinished(entry.request))
		{
			out[written++] = { entry.kind, requests.Result(entry.request), elapsed, false };
			requests.Release(entry.request);
			RemoveAt(index);
			continue;
		}

		if (elapsed >= kAbandonMs)
		{
			out[written++] = { entry.kind, 0, elapsed, true };
			RemoveAt(index);
			continue;
		}

		++index;
	}

	return written;
}

int UploadLedger::Pending() const
{
	return m_count;
}

bool UploadLedger::IsPending(UploadKind kind) const
{
	for (int i = 0; i < m_count; ++i)
	{
		if (m_entries[i].kind == kind)
			return true;
	}

	return false;
}

bool UploadLedger::Holds(uintptr_t request) const
{
	for (int i = 0; i < m_count; ++i)
	{
		if (m_entries[i].request == request)
			return true;
	}

	return false;
}

void UploadLedger::RemoveAt(int index)
{
	for (int i = index + 1; i < m_count; ++i)
		m_entries[i - 1] = m_entries[i];

	--m_count;
}
