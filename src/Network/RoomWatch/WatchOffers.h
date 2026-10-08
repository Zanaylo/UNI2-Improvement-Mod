#pragma once

#include <cstdint>

class WatchOffers
{
public:
	static constexpr int kMostHosts = 16;
	static constexpr uint32_t kFreshMs = 6000;

	void Note(uint64_t host, uint32_t now);
	void Forget(uint64_t host);
	bool AnyFresh(uint32_t now) const;

private:
	struct Offer
	{
		uint64_t host;
		uint32_t at;
	};

	Offer* SlotFor(uint64_t host, uint32_t now);

	Offer m_offers[kMostHosts] = {};
};
