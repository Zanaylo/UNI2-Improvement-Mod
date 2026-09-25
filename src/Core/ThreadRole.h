#pragma once

namespace ThreadRole
{
	enum Role
	{
		Role_Unknown,
		Role_Game,
		Role_Render,
		Role_Loader,
		Role_Worker,
		Role_COUNT
	};

	void Mark(Role role);
	Role Current();

	const char* Name(Role role);

	struct Site
	{
		const char* where;
		bool reported;
	};

	void Expect(Role role, Site& site);
}

#define EXPECT_THREAD(role)                                                  \
	do                                                                       \
	{                                                                        \
		static ThreadRole::Site threadSite = { __FUNCTION__, false };        \
		ThreadRole::Expect(role, threadSite);                                \
	} while (false)
