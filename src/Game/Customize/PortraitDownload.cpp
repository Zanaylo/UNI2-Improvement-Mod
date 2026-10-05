#include "Game/Customize/PortraitDownload.h"

#include "Core/BackgroundJob.h"
#include "Core/Config/Settings.h"
#include "Core/Formats/ZipArchive.h"
#include "Core/logger.h"
#include "Core/utils.h"
#include "Game/Customize/PortraitCatalog.h"
#include "Game/Customize/PortraitLibrary.h"
#include "Web/Http.h"
#include "Web/ShareLink.h"

#include <Windows.h>

#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace {

constexpr const char* kDefaultPackUrl =
	"https://drive.google.com/file/d/1EQdjMJCgG4x8s3ruSlzxa2_1eTELfhXV/view?usp=sharing";
constexpr const char* kSection = "Portraits";
constexpr const char* kPackKey = "PackUrl";
constexpr const char* kPackFile = "UNI2-IM-Portraits.zip";
constexpr size_t kUrlBytes = 1024;
constexpr uint8_t kZipMagic[] = { 'P', 'K', 3, 4 };
constexpr uint64_t kMegabyte = 1024 * 1024;
constexpr int kFullProgress = 100;

BackgroundJob g_job("PortraitDownload");
std::mutex g_fetchLock;

class PackProgress : public Http::Progress
{
public:
	bool OnProgress(uint64_t received, uint64_t total) override
	{
		char status[128] = {};
		sprintf_s(status, "downloading the portrait pack (%llu of %llu MB)",
			static_cast<unsigned long long>(received / kMegabyte), static_cast<unsigned long long>(total / kMegabyte));
		g_job.SetStatus(status);
		g_job.SetProgress(total == 0 ? 0 : static_cast<int>(received * kFullProgress / total));
		return true;
	}
};

std::string ReadPackUrl()
{
	char url[kUrlBytes] = {};
	GetPrivateProfileStringA(kSection, kPackKey, kDefaultPackUrl, url, sizeof(url), Settings::GetIniPath().c_str());
	return ShareLink::Direct(url);
}

const std::string& PackUrl()
{
	static const std::string url = ReadPackUrl();
	return url;
}

bool AnyMissing()
{
	for (const PortraitCatalog::Art* art : PortraitCatalog::FromThePack())
	{
		if (!PortraitLibrary::IsReady(*art))
			return true;
	}

	return false;
}

bool IsZip(const std::string& path)
{
	std::vector<uint8_t> bytes;

	return ReadWholeFile(path, bytes, sizeof(kZipMagic)) && memcmp(bytes.data(), kZipMagic, sizeof(kZipMagic)) == 0;
}

bool Unpack(const std::string& zip, std::string& outError)
{
	if (!IsZip(zip))
	{
		outError = "the pack link did not send the pack";
		return false;
	}

	int files = 0;
	char status[192] = {};

	if (!ZipArchive::Extract(zip, PortraitLibrary::Folder(), files, status, sizeof(status)))
	{
		outError = status;
		return false;
	}

	LOG("PortraitDownload: %d art(s) unpacked", files);
	return true;
}

bool FetchPack(Http::Progress* progress, std::string& outError)
{
	std::lock_guard<std::mutex> hold(g_fetchLock);

	if (!AnyMissing())
		return true;

	const std::string& url = PackUrl();

	if (url.empty())
	{
		outError = "no link to the portrait pack is set";
		return false;
	}

	const std::string zip = GetModDownloadPath(kPackFile);
	CreateDirectoryTree(zip.substr(0, zip.find_last_of('\\')));

	const bool unpacked = Http::Download(url, zip, progress, outError) && Unpack(zip, outError);
	DeleteFileA(zip.c_str());

	if (!unpacked)
		LOG("PortraitDownload: the pack failed: %s", outError.c_str());

	return unpacked;
}

bool FetchAll()
{
	PackProgress progress;
	std::string error;

	if (!FetchPack(&progress, error))
	{
		g_job.SetStatus(("the download failed: " + error).c_str());
		return false;
	}

	g_job.SetStatus("every portrait is downloaded");
	g_job.SetProgress(kFullProgress);
	return true;
}

}

bool PortraitDownload::HasLink()
{
	return !PackUrl().empty();
}

bool PortraitDownload::Fetch(std::string& outError)
{
	return FetchPack(nullptr, outError);
}

bool PortraitDownload::BeginAll()
{
	if (g_job.IsBusy())
		return false;

	g_job.SetStatus("looking for missing art...");
	return g_job.Start([]() { return FetchAll(); });
}

void PortraitDownload::Update()
{
	g_job.ConsumeFinished();
}

bool PortraitDownload::IsBusy()
{
	return g_job.IsBusy();
}

int PortraitDownload::Progress()
{
	return g_job.Progress();
}

std::string PortraitDownload::StatusText()
{
	return g_job.Status();
}
