#include "Network/RoomWatch/WatchOffers.h"

void WatchOffers::Note(uint64_t host, uint32_t now)
{
	if (host == 0)
		return;

	Offer* const slot = SlotFor(host, now);
	slot->host = host;
	slot->at = now;
}

void WatchOffers::Forget(uint64_t host)
{
	for (Offer& offer : m_offers)
	{
		if (offer.host == host)
			offer = {};
	}
}

bool WatchOffers::AnyFresh(uint32_t now) const
{
	for (const Offer& offer : m_offers)
	{
		if (offer.host != 0 && now - offer.at < kFreshMs)
			return true;
	}

	return false;
}

WatchOffers::Offer* WatchOffers::SlotFor(uint64_t host, uint32_t now)
{
	Offer* oldest = &m_offers[0];

	for (Offer& offer : m_offers)
	{
		if (offer.host == host || offer.host == 0)
			return &offer;

		if (now - offer.at > now - oldest->at)
			oldest = &offer;
	}

	return oldest;
}
