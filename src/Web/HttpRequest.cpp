#include "Web/HttpRequest.h"

#include "Core/Text/Language.h"
#include "Core/info.h"

#include <Windows.h>
#include <wininet.h>

#include <cstdio>

#pragma comment(lib, "wininet.lib")

namespace {

constexpr DWORD kTimeoutMs = 20000;
constexpr DWORD kReadChunk = 16384;
constexpr size_t kMaxAnswer = 2u * 1024u * 1024u;
constexpr DWORD kUrlPartChars = 2048;
constexpr DWORD kRequestFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI |
	INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_PRAGMA_NOCACHE;

class Handle
{
public:
	explicit Handle(HINTERNET handle = nullptr) : m_handle(handle) {}
	~Handle()
	{
		if (m_handle != nullptr)
			InternetCloseHandle(m_handle);
	}

	Handle(const Handle&) = delete;
	Handle& operator=(const Handle&) = delete;

	HINTERNET Get() const { return m_handle; }
	explicit operator bool() const { return m_handle != nullptr; }

private:
	HINTERNET m_handle;
};

struct Target
{
	bool secure = false;
	INTERNET_PORT port = 0;
	std::string host;
	std::string path;
};

std::string Describe(const char* english)
{
	const DWORD code = GetLastError();

	return TrFormat("%s (Windows error %lu)", Tr(english), code);
}

bool Crack(const std::string& url, Target& out)
{
	char host[kUrlPartChars] = {};
	char path[kUrlPartChars] = {};
	char extra[kUrlPartChars] = {};

	URL_COMPONENTSA parts = {};
	parts.dwStructSize = sizeof(parts);
	parts.lpszHostName = host;
	parts.dwHostNameLength = kUrlPartChars;
	parts.lpszUrlPath = path;
	parts.dwUrlPathLength = kUrlPartChars;
	parts.lpszExtraInfo = extra;
	parts.dwExtraInfoLength = kUrlPartChars;

	if (!InternetCrackUrlA(url.c_str(), static_cast<DWORD>(url.size()), 0, &parts))
		return false;

	out.secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
	out.port = parts.nPort;
	out.host = host;
	out.path = std::string(path) + extra;
	return !out.host.empty();
}

std::string JoinHeaders(const std::vector<HttpRequest::Header>& headers)
{
	std::string joined;

	for (const HttpRequest::Header& header : headers)
		joined += header.name + ": " + header.value + "\r\n";

	return joined;
}

void SetTimeouts(HINTERNET session)
{
	DWORD timeout = kTimeoutMs;

	InternetSetOptionA(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
	InternetSetOptionA(session, INTERNET_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
	InternetSetOptionA(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
}

int ReadStatus(HINTERNET request)
{
	DWORD status = 0;
	DWORD size = sizeof(status);

	if (!HttpQueryInfoA(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &size, nullptr))
		return 0;

	return static_cast<int>(status);
}

bool ReadBody(HINTERNET request, std::string& out)
{
	char buffer[kReadChunk];
	DWORD read = 0;

	while (InternetReadFile(request, buffer, kReadChunk, &read) && read != 0)
	{
		if (out.size() + read > kMaxAnswer)
			return false;

		out.append(buffer, read);
	}

	return true;
}

}

bool HttpRequest::Send(const Request& request, Response& out, std::string& outError)
{
	out = Response();
	outError.clear();

	Target target;

	if (!Crack(request.url, target))
	{
		outError = Tr("the address could not be read");
		return false;
	}

	const Handle session(InternetOpenA(UNI2_IM_USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0));

	if (!session)
	{
		outError = Describe("no internet session");
		return false;
	}

	SetTimeouts(session.Get());

	const Handle connection(InternetConnectA(session.Get(), target.host.c_str(), target.port, nullptr, nullptr,
		INTERNET_SERVICE_HTTP, 0, 0));

	if (!connection)
	{
		outError = Describe("the server could not be reached");
		return false;
	}

	const DWORD flags = kRequestFlags | (target.secure ? INTERNET_FLAG_SECURE : 0u);
	const Handle handle(HttpOpenRequestA(connection.Get(), request.method.c_str(), target.path.c_str(), nullptr,
		nullptr, nullptr, flags, 0));

	if (!handle)
	{
		outError = Describe("the request could not be opened");
		return false;
	}

	const std::string headers = JoinHeaders(request.headers);
	void* const body = request.body.empty() ? nullptr : const_cast<char*>(request.body.data());

	if (!HttpSendRequestA(handle.Get(), headers.c_str(), static_cast<DWORD>(headers.size()), body,
		static_cast<DWORD>(request.body.size())))
	{
		outError = Describe("the request could not be sent");
		return false;
	}

	out.status = ReadStatus(handle.Get());

	if (!ReadBody(handle.Get(), out.body))
	{
		outError = Tr("the answer could not be read");
		return false;
	}

	return true;
}
