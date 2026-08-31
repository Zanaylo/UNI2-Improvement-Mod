#pragma once

#include <string>
#include <unordered_map>

class FileIndexNaming
{
public:
	virtual ~FileIndexNaming() = default;

	virtual bool Folder(const std::string& path, const std::string& relative,
		std::string& renamed);

	virtual bool File(const std::string& folder, const std::string& name, std::string& key);
};

class FileIndex
{
public:
	using Map = std::unordered_map<std::string, std::string>;

	static std::string Key(const char* path, size_t length);
	static std::string Key(const std::string& path);
	static std::string Join(const std::string& folder, const std::string& name);

	void Walk(const std::string& folder);
	void Walk(const std::string& folder, FileIndexNaming& naming);

	void Add(const std::string& key, const std::string& path);

	const std::string* Find(const std::string& key) const;
	bool Has(const std::string& key) const;

	const Map& Entries() const;
	int Count() const;

	void Swap(FileIndex& other);
	void Clear();

private:
	void WalkInto(const std::string& folder, const std::string& relative, FileIndexNaming& naming);

	Map m_entries;
};
