#include "Web/Http.h"

#include "Core/info.h"
#include "Core/logger.h"

#include <Windows.h>
#include <wincrypt.h>
#include <wininet.h>

#include <cstdio>
#include <cstring>
#include <vector>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "advapi32.lib")

namespace {

constexpr DWORD kReadChunk = 32768;
constexpr DWORD kOpenFlags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
	INTERNET_FLAG_NO_UI | INTERNET_FLAG_PRAGMA_NOCACHE;
constexpr size_t kMaxText = 4u * 1024u * 1024u;

std::wstring Widen(const std::string& value)
{
	if (value.empty())
		return std::wstring();

	const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);

	if (length <= 1)
		return std::wstring();

	std::wstring wide(static_cast<size_t>(length - 1), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &wide[0], length);
	return wide;
}

std::string Describe(const char* prefix, DWORD code)
{
	char text[256] = {};

	FormatMessageA(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS, GetModuleHandleA("wininet.dll"), code, 0, text,
		sizeof(text), nullptr);

	std::string message = prefix;

	if (text[0] != '\0')
	{
		message += ": ";
		message += text;
	}
	else
	{
		char number[32] = {};
		sprintf_s(number, ": Windows error %lu", code);
		message += number;
	}

	while (!message.empty() && (message.back() == '\r' || message.back() == '\n' ||
		message.back() == '.' || message.back() == ' '))
	{
		message.pop_back();
	}

	return message;
}

bool IsSecure(const std::string& url)
{
	return _strnicmp(url.c_str(), "https://", 8) == 0;
}

class Sink
{
public:
	bool Open(const std::string& path);
	bool Write(const void* data, DWORD size);
	void Close();

private:
	HANDLE m_file = INVALID_HANDLE_VALUE;
};

bool Sink::Open(const std::string& path)
{
	m_file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, nullptr);

	return m_file != INVALID_HANDLE_VALUE;
}

bool Sink::Write(const void* data, DWORD size)
{
	DWORD written = 0;

	return WriteFile(m_file, data, size, &written, nullptr) != FALSE && written == size;
}

void Sink::Close()
{
	if (m_file == INVALID_HANDLE_VALUE)
		return;

	CloseHandle(m_file);
	m_file = INVALID_HANDLE_VALUE;
}

class Session
{
public:
	~Session();

	bool Open(const std::string& url, std::string& outError);
	bool Read(void* buffer, DWORD capacity, DWORD& outRead);

	uint64_t ContentLength() const { return m_length; }

private:
	void ReadHeaders();

	HINTERNET m_session = nullptr;
	HINTERNET m_request = nullptr;
	uint64_t m_length = 0;
	int m_status = 0;
};

Session::~Session()
{
	if (m_request != nullptr)
		InternetCloseHandle(m_request);

	if (m_session != nullptr)
		InternetCloseHandle(m_session);
}

void Session::ReadHeaders()
{
	DWORD status = 0;
	DWORD size = sizeof(status);

	if (HttpQueryInfoW(m_request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &status, &size,
		nullptr))
	{
		m_status = static_cast<int>(status);
	}

	wchar_t length[64] = {};
	size = sizeof(length);

	if (HttpQueryInfoW(m_request, HTTP_QUERY_CONTENT_LENGTH, length, &size, nullptr))
		m_length = _wcstoui64(length, nullptr, 10);
}

bool Session::Open(const std::string& url, std::string& outError)
{
	m_session = InternetOpenW(Widen(UNI2_IM_USER_AGENT).c_str(), INTERNET_OPEN_TYPE_PRECONFIG,
		nullptr, nullptr, 0);

	if (m_session == nullptr)
	{
		outError = Describe("no internet session", GetLastError());
		return false;
	}

	const DWORD flags = kOpenFlags | (IsSecure(url) ? INTERNET_FLAG_SECURE : 0u);
	const wchar_t* const headers = L"Accept: */*\r\n";

	m_request = InternetOpenUrlW(m_session, Widen(url).c_str(), headers,
		static_cast<DWORD>(wcslen(headers)), flags, 0);

	if (m_request == nullptr)
	{
		outError = Describe("the address could not be opened", GetLastError());
		return false;
	}

	ReadHeaders();

	if (m_status >= 400)
	{
		char text[96] = {};
		sprintf_s(text, "the server answered %d", m_status);
		outError = text;
		return false;
	}

	return true;
}

bool Session::Read(void* buffer, DWORD capacity, DWORD& outRead)
{
	outRead = 0;

	return InternetReadFile(m_request, buffer, capacity, &outRead) != FALSE;
}

}

std::string Http::HostOf(const std::string& url)
{
	size_t start = url.find("://");

	start = start == std::string::npos ? 0 : start + 3;

	const size_t end = url.find_first_of("/?#", start);

	return url.substr(start, end == std::string::npos ? std::string::npos : end - start);
}

bool Http::GetText(const std::string& url, std::string& out, std::string& outError)
{
	out.clear();
	outError.clear();

	Session session;

	if (!session.Open(url, outError))
		return false;

	std::vector<char> buffer(kReadChunk);
	DWORD read = 0;

	while (session.Read(buffer.data(), kReadChunk, read) && read != 0)
	{
		if (out.size() + read > kMaxText)
		{
			outError = "the answer is too large";
			return false;
		}

		out.append(buffer.data(), read);
	}

	if (out.empty())
	{
		outError = "the answer was empty";
		return false;
	}

	return true;
}

bool Http::Download(const std::string& url, const std::string& path, Progress* progress,
	std::string& outError)
{
	outError.clear();

	Session session;

	if (!session.Open(url, outError))
		return false;

	Sink sink;

	if (!sink.Open(path))
	{
		outError = Describe("the download file could not be created", GetLastError());
		return false;
	}

	std::vector<char> buffer(kReadChunk);
	const uint64_t total = session.ContentLength();
	uint64_t received = 0;
	DWORD read = 0;

	while (session.Read(buffer.data(), kReadChunk, read) && read != 0)
	{
		if (!sink.Write(buffer.data(), read))
		{
			sink.Close();
			DeleteFileA(path.c_str());
			outError = Describe("the download could not be written to disk", GetLastError());
			return false;
		}

		received += read;

		if (progress != nullptr && !progress->OnProgress(received, total))
		{
			sink.Close();
			DeleteFileA(path.c_str());
			outError = "cancelled";
			return false;
		}
	}

	sink.Close();

	if (received == 0)
	{
		DeleteFileA(path.c_str());
		outError = "nothing came back";
		return false;
	}

	if (total != 0 && received != total)
	{
		DeleteFileA(path.c_str());
		outError = "the download stopped early - antivirus, a firewall or a proxy may have cut it";
		return false;
	}

	LOG("Http: %llu byte(s) from %s", static_cast<unsigned long long>(received),
		HostOf(url).c_str());

	return true;
}

bool Http::Sha256OfFile(const std::string& path, std::string& outHex, std::string& outError)
{
	outHex.clear();
	outError.clear();

	const HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		outError = "the file could not be opened";
		return false;
	}

	HCRYPTPROV provider = 0;
	HCRYPTHASH hash = 0;

	if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) ||
		!CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash))
	{
		CloseHandle(file);

		if (provider != 0)
			CryptReleaseContext(provider, 0);

		outError = "SHA-256 is not available";
		return false;
	}

	std::vector<BYTE> buffer(kReadChunk);
	DWORD read = 0;
	bool ok = true;

	while (ok && ReadFile(file, buffer.data(), kReadChunk, &read, nullptr) != FALSE && read != 0)
		ok = CryptHashData(hash, buffer.data(), read, 0) != FALSE;

	BYTE digest[32] = {};
	DWORD digestSize = sizeof(digest);

	ok = ok && CryptGetHashParam(hash, HP_HASHVAL, digest, &digestSize, 0) != FALSE;

	CryptDestroyHash(hash);
	CryptReleaseContext(provider, 0);
	CloseHandle(file);

	if (!ok)
	{
		outError = "the file could not be hashed";
		return false;
	}

	char text[sizeof(digest) * 2 + 1] = {};

	for (DWORD i = 0; i < digestSize; ++i)
		sprintf_s(text + i * 2, sizeof(text) - i * 2, "%02x", digest[i]);

	outHex = text;
	return true;
}
