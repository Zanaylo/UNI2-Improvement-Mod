// Mod paths, game module resolution and the SEH guarded memory reads everything else goes through.
// TryRead*/TryWrite* refuse unaligned addresses; TryRead/WriteUnaligned exist for packed game buffers.

#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>

void SetModModuleHandle(HMODULE hModule);
HMODULE GetModModuleHandle();

std::string GetModDirectory();
std::string GetModFilePath(const std::string& fileName);

std::string GetModRootPath(const std::string& fileName = std::string());
std::string GetModAssetPath(const std::string& fileName = std::string());
std::string GetModPalettePath(const std::string& fileName = std::string());
std::string GetModDownloadPath(const std::string& fileName = std::string());
std::string GetModLogPath(const std::string& fileName = std::string());
std::string GetModScriptPath(const std::string& fileName = std::string());

bool CreateModDirectories();

bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out, size_t minimumSize = 0);

uintptr_t GetGameBaseAddress();
size_t GetGameModuleSize();
uintptr_t RvaToAddress(uintptr_t rva);
bool IsAddressInGameModule(uintptr_t address);
bool IsReadableMemory(const void* address, size_t size);

bool TryReadMemory(void* destination, const void* source, size_t size);
bool TryWriteMemory(void* destination, const void* source, size_t size);
bool TryReadDword(const void* source, uint32_t& outValue);

bool TryWriteDword(void* address, uint32_t value);

bool TryReadUnaligned(const void* source, uint32_t& outValue);
bool TryWriteUnaligned(void* address, uint32_t value);

std::string GetSystemDirectoryPath();

bool IsHotkeyPressed(int virtualKey);

bool IsHotkeyHeld(int virtualKey);

bool IsHotkeyRepeating(int virtualKey, unsigned delayMs, unsigned intervalMs);
