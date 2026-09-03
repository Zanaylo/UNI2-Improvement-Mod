#pragma once

#include "Game/GameArchive.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace VoiceMap
{
	class Reader
	{
	public:
		virtual ~Reader() = default;

		virtual bool SubFolders(const std::string& parent, std::vector<std::string>& out) = 0;
		virtual bool List(const std::string& folder, std::vector<std::string>& out) = 0;
		virtual bool Read(const std::string& folder, const std::string& file,
			std::vector<uint8_t>& out) = 0;
	};

	class LooseReader : public Reader
	{
	public:
		explicit LooseReader(const std::string& root);

		bool SubFolders(const std::string& parent, std::vector<std::string>& out) override;
		bool List(const std::string& folder, std::vector<std::string>& out) override;
		bool Read(const std::string& folder, const std::string& file,
			std::vector<uint8_t>& out) override;

	private:
		std::string m_root;
	};

	class ArchiveReader : public Reader
	{
	public:
		bool Open(const std::string& folder);

		bool SubFolders(const std::string& parent, std::vector<std::string>& out) override;
		bool List(const std::string& folder, std::vector<std::string>& out) override;
		bool Read(const std::string& folder, const std::string& file,
			std::vector<uint8_t>& out) override;

	private:
		GameArchive m_archive;
		std::vector<std::string> m_folders;
	};

	struct Copy
	{
		std::string sourceFolder;
		std::string sourceFile;
		std::string target;
	};

	std::unique_ptr<Reader> Open(const std::string& root);

	std::string TagOf(Reader& ours, int chara);

	void Build(Reader& ours, Reader& theirs, int chara, const std::string& tag,
		std::vector<Copy>& out);
}
