#include "Web/ShareLink.h"

namespace {

constexpr const char* kDriveHost = "drive.google.com";
constexpr const char* kFilePath = "/file/d/";
constexpr const char* kIdQuery = "id=";
constexpr const char* kPathEnd = "/?&#";
constexpr const char* kQueryEnd = "&#";
constexpr const char* kDirectFront = "https://drive.usercontent.google.com/download?id=";
constexpr const char* kDirectBack = "&export=download&confirm=t";

std::string Between(const std::string& url, const std::string& marker, const char* ends)
{
	const size_t start = url.find(marker);

	if (start == std::string::npos)
		return std::string();

	const size_t from = start + marker.size();
	const size_t to = url.find_first_of(ends, from);
	return url.substr(from, to == std::string::npos ? std::string::npos : to - from);
}

std::string DriveId(const std::string& url)
{
	const std::string fromPath = Between(url, kFilePath, kPathEnd);
	return fromPath.empty() ? Between(url, kIdQuery, kQueryEnd) : fromPath;
}

}

std::string ShareLink::Direct(const std::string& url)
{
	if (url.find(kDriveHost) == std::string::npos)
		return url;

	const std::string id = DriveId(url);
	return id.empty() ? url : kDirectFront + id + kDirectBack;
}
