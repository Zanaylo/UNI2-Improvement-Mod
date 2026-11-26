#include "ModLoader/LoaderUi.h"

#include "Core/Config/FeatureCatalog.h"
#include "Core/Formats/ZipArchive.h"
#include "Core/Text/TextList.h"
#include "Core/info.h"
#include "ModLoader/AddonStore.h"
#include "ModLoader/LoaderConfig.h"
#include "ModLoader/LoaderLocale.h"
#include "ModLoader/LoaderLog.h"
#include "ModLoader/PackStore.h"
#include "ModLoader/RmlFileInterface.h"
#include "ModLoader/RmlRenderGDI.h"

#include <RmlUi/Core.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi_Platform_Win32.h>

#include <ShObjIdl.h>
#include <commdlg.h>
#include <shellapi.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kClearColorBgra = 0xFF0B0E0Du;

enum class Tab
{
	Addons = 0,
	Store = 1,
	Packs = 2,
	Modules = 3,
	Log = 4
};

class LoaderSystemInterface final : public SystemInterface_Win32
{
public:
	bool LogMessage(Rml::Log::Type type, const Rml::String& message) override
	{
		if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_WARNING || type == Rml::Log::LT_ASSERT)
		{
			FILE* f = nullptr;
			if (fopen_s(&f, "UNI2ModLoader.log", "a") == 0 && f != nullptr)
			{
				fprintf(f, "[RmlUi %d] %s\n", static_cast<int>(type), message.c_str());
				fclose(f);
			}
		}

		return true;
	}
};

HWND g_window = nullptr;
std::unique_ptr<LoaderSystemInterface> g_system;
std::unique_ptr<RmlRenderGDI> g_render;
std::unique_ptr<RmlFileInterface> g_files;
std::unique_ptr<TextInputMethodEditor_Win32> g_ime;
Rml::Context* g_context = nullptr;
Rml::ElementDocument* g_doc = nullptr;

std::string g_gameDir;
Tab g_activeTab = Tab::Addons;
int g_selectedAddon = -1;
int g_selectedStore = -1;
bool g_updateOnStart = false;
bool g_gameAddonCheckOnStart = true;
size_t g_lastLogCount = 0;
std::string g_lastBusyStatus;
bool g_lastBusy = false;
bool g_lastUrlEmpty = true;

std::string ExeDir()
{
	char path[MAX_PATH] = {};
	if (GetModuleFileNameA(nullptr, path, MAX_PATH) == 0)
		return std::string();

	std::string full(path);
	const size_t slash = full.find_last_of("\\/");
	return slash == std::string::npos ? std::string() : full.substr(0, slash + 1);
}

std::string EscapeRml(const std::string& text)
{
	std::string out;
	out.reserve(text.size() + 16);

	for (char c : text)
	{
		switch (c)
		{
		case '&': out += "&amp;"; break;
		case '<': out += "&lt;"; break;
		case '>': out += "&gt;"; break;
		case '"': out += "&quot;"; break;
		default: out.push_back(c); break;
		}
	}

	return out;
}

bool PickFolderDialog(std::string& outPath)
{
	IFileOpenDialog* dialog = nullptr;

	if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog,
		reinterpret_cast<void**>(&dialog))))
	{
		return false;
	}

	DWORD options = 0;
	dialog->GetOptions(&options);
	dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

	bool picked = false;

	if (SUCCEEDED(dialog->Show(g_window)))
	{
		IShellItem* item = nullptr;

		if (SUCCEEDED(dialog->GetResult(&item)))
		{
			PWSTR wide = nullptr;

			if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wide)))
			{
				char narrow[MAX_PATH * 2] = {};
				WideCharToMultiByte(CP_ACP, 0, wide, -1, narrow, sizeof(narrow), nullptr, nullptr);
				outPath = narrow;
				CoTaskMemFree(wide);
				picked = !outPath.empty();
			}

			item->Release();
		}
	}

	dialog->Release();
	return picked;
}

bool PickZipDialog(std::string& outPath)
{
	char file[MAX_PATH * 2] = {};

	OPENFILENAMEA ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = g_window;
	ofn.lpstrFilter = "Zip archives (*.zip)\0*.zip\0All files (*.*)\0*.*\0";
	ofn.lpstrFile = file;
	ofn.nMaxFile = sizeof(file);
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

	if (!GetOpenFileNameA(&ofn))
		return false;

	outPath = file;
	return true;
}

void OpenInExplorer(const std::string& path)
{
	ShellExecuteA(g_window, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void LaunchGameViaSteam()
{
	const std::string uri = "steam://rungameid/" UNI2_IM_STEAM_APP_ID;
	ShellExecuteA(g_window, "open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	LoaderLog::Add(LoaderLocale::Text("msg_launch_steam"));
}

void LaunchGameDirect()
{
	const std::string exe = g_gameDir + UNI2_IM_GAME_EXE;
	ShellExecuteA(g_window, "open", exe.c_str(), nullptr, g_gameDir.c_str(), SW_SHOWNORMAL);
	LoaderLog::Add(exe);
}

bool InstallBundledProxyDll()
{
	const std::string source = ExeDir() + UNI2_IM_ENTRY_DLL;

	if (GetFileAttributesA(source.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		LoaderLog::Add("No dinput8.dll found next to UNI2ModLoader.exe.");
		return false;
	}

	const std::string dest = g_gameDir + UNI2_IM_ENTRY_DLL;

	if (!CopyFileA(source.c_str(), dest.c_str(), FALSE))
	{
		LoaderLog::Add("Could not copy dinput8.dll to the game folder.");
		return false;
	}

	LoaderLog::Add("dinput8.dll -> " + g_gameDir);
	return true;
}

void SetText(const char* id, const std::string& text)
{
	if (g_doc == nullptr)
		return;

	if (Rml::Element* const el = g_doc->GetElementById(id))
		el->SetInnerRML(EscapeRml(text));
}

void SetClass(const char* id, const char* cls, bool on)
{
	if (g_doc == nullptr)
		return;

	if (Rml::Element* const el = g_doc->GetElementById(id))
		el->SetClass(cls, on);
}

std::string ReadUrlInput()
{
	if (g_doc == nullptr)
		return std::string();

	if (auto* const input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(g_doc->GetElementById("url-input")))
		return input->GetValue();

	return std::string();
}

void SyncUrlPlaceholder()
{
	const bool empty = ReadUrlInput().empty();
	g_lastUrlEmpty = empty;
	SetClass("url-placeholder", "hidden", !empty);
}

void WriteUrlInput(const std::string& value)
{
	if (g_doc == nullptr)
		return;

	if (auto* const input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(g_doc->GetElementById("url-input")))
		input->SetValue(value);

	SyncUrlPlaceholder();
}

std::string BadgeHtml(const AddonManifest& manifest)
{
	std::string out;

	if (manifest.IsLua())
		out += "<span class=\"badge lua\">LUA</span>";

	if (manifest.IsNative())
		out += "<span class=\"badge dll\">DLL</span>";

	if (manifest.IsModule())
		out += "<span class=\"badge mod\">MODULE</span>";

	if (manifest.IsAssetPack())
		out += "<span class=\"badge assets\">ASSETS</span>";

	if (out.empty())
		out += "<span class=\"badge assets\">ADDON</span>";

	return out;
}

void ApplyStaticTranslations()
{
	if (g_doc == nullptr)
		return;

	SetText("btn-install-proxy", LoaderLocale::Text("btn_install_proxy"));
	SetText("btn-play-steam", LoaderLocale::Text("btn_play_steam"));
	SetText("btn-play-direct", LoaderLocale::Text("btn_play_direct"));
	SetText("label-game-folder", LoaderLocale::Text("label_game_folder"));
	SetText("btn-browse-game", LoaderLocale::Text("btn_browse"));
	SetText("btn-reload-ui", LoaderLocale::Text("btn_reload_ui"));
	SetText("nav-section-label", LoaderLocale::Text("nav_section"));
	SetText("nav-addons-label", LoaderLocale::Text("nav_addons"));
	SetText("nav-store-label", LoaderLocale::Text("nav_store"));
	SetText("nav-packs-label", LoaderLocale::Text("nav_packs"));
	SetText("nav-modules-label", LoaderLocale::Text("nav_modules"));
	SetText("nav-log-label", LoaderLocale::Text("nav_log"));
	SetText("drop-title", LoaderLocale::Text("drop_title"));
	SetText("drop-text", LoaderLocale::Text("drop_text"));
	SetText("addons-title", LoaderLocale::Text("addons_title"));
	SetText("addons-sub", LoaderLocale::Text("addons_sub"));
	SetText("btn-addon-zip", LoaderLocale::Text("btn_addon_zip"));
	SetText("btn-addon-folder", LoaderLocale::Text("btn_addon_folder"));
	SetText("btn-addon-open", LoaderLocale::Text("btn_open_folder"));
	SetText("btn-addon-update", LoaderLocale::Text("btn_update_linked"));
	SetText("btn-addon-refresh", LoaderLocale::Text("btn_refresh"));
	SetText("btn-save-link", LoaderLocale::Text("btn_save_github"));
	SetText("btn-install-url", LoaderLocale::Text("btn_download_install"));
	SetText("url-placeholder", LoaderLocale::Text("url_placeholder"));
	SyncUrlPlaceholder();
	SetText("store-title", LoaderLocale::Text("store_title"));
	SetText("store-sub", LoaderLocale::Text("store_sub"));
	SetText("btn-store-refresh", LoaderLocale::Text("btn_store_refresh"));
	SetText("btn-store-bundle", LoaderLocale::Text("btn_store_bundle"));
	SetText("btn-store-update-all", LoaderLocale::Text("btn_store_update_all"));
	SetText("packs-title", LoaderLocale::Text("packs_title"));
	SetText("packs-sub", LoaderLocale::Text("packs_sub"));
	SetText("btn-pack-zip", LoaderLocale::Text("btn_pack_zip"));
	SetText("btn-pack-folder", LoaderLocale::Text("btn_pack_folder"));
	SetText("btn-pack-open", LoaderLocale::Text("btn_pack_open"));
	SetText("btn-pack-refresh", LoaderLocale::Text("btn_refresh"));
	SetText("modules-title", LoaderLocale::Text("modules_title"));
	SetText("modules-sub", LoaderLocale::Text("modules_sub"));
	SetText("modules-core-title", LoaderLocale::Text("modules_core"));
	SetText("modules-opt-title", LoaderLocale::Text("modules_opt"));
	SetText("modules-opt-sub", LoaderLocale::Text("modules_opt_sub"));
	SetText("btn-reset-base", LoaderLocale::Text("btn_reset_base"));
	SetText("btn-open-basepals", LoaderLocale::Text("btn_open_basepals"));
	SetText("game-addon-check-label", LoaderLocale::Text("game_addon_check_label"));
	SetText("auto-update-label", LoaderLocale::Text("auto_update_label"));
	SetText("log-title", LoaderLocale::Text("log_title"));
	SetText("log-sub", LoaderLocale::Text("log_sub"));
	SetText("foot-ver", LoaderLocale::Text("foot_engine"));

	const LoaderLocale::Lang cur = LoaderLocale::Current();
	SetClass("lang-en", "active", cur == LoaderLocale::Lang::En);
	SetClass("lang-pt", "active", cur == LoaderLocale::Lang::PtBr);
	SetClass("lang-es", "active", cur == LoaderLocale::Lang::EsLatam);
}

void SyncHeader()
{
	if (g_doc == nullptr)
		return;

	SetText("brand-version", std::string(LoaderLocale::Text("brand_sub")) + "  v" UNI2_IM_VERSION);

	const bool valid = LoaderConfig::IsValidGameDir(g_gameDir);
	const bool hasDll = valid && LoaderConfig::HasEntryDll(g_gameDir);
	const bool busy = AddonStore::IsBusy();

	if (valid)
	{
		SetText("game-dir-text", g_gameDir);
		SetClass("game-dir-text", "missing", false);
	}
	else
	{
		SetText("game-dir-text", LoaderLocale::Text("game_missing"));
		SetClass("game-dir-text", "missing", true);
	}

	if (Rml::Element* const pill = g_doc->GetElementById("proxy-status"))
	{
		pill->SetClass("ok", valid && hasDll);
		pill->SetClass("warn", valid && !hasDll);
		pill->SetClass("err", !valid);
		const char* statusKey = !valid ? "status_no_game" : hasDll ? "status_ready" : "status_missing";
		pill->SetInnerRML(EscapeRml(LoaderLocale::Text(statusKey)));
	}

	SetClass("btn-install-proxy", "hidden", !valid || hasDll);
	SetClass("btn-play-steam", "disabled", !valid || busy);
	SetClass("btn-play-direct", "disabled", !valid || busy);
}

void SyncTabs()
{
	SetClass("nav-addons", "active", g_activeTab == Tab::Addons);
	SetClass("nav-store", "active", g_activeTab == Tab::Store);
	SetClass("nav-packs", "active", g_activeTab == Tab::Packs);
	SetClass("nav-modules", "active", g_activeTab == Tab::Modules);
	SetClass("nav-log", "active", g_activeTab == Tab::Log);

	SetClass("view-addons", "hidden", g_activeTab != Tab::Addons);
	SetClass("view-store", "hidden", g_activeTab != Tab::Store);
	SetClass("view-packs", "hidden", g_activeTab != Tab::Packs);
	SetClass("view-modules", "hidden", g_activeTab != Tab::Modules);
	SetClass("view-log", "hidden", g_activeTab != Tab::Log);
}

std::string ShortSha(const std::string& sha)
{
	return sha.size() > 16 ? (sha.substr(0, 16) + "...") : sha;
}

std::string ReleasePageForCatalogItem(const AddonCatalog::CatalogItem& item)
{
	std::string base = item.repo;
	if (base.empty())
		base = "https://github.com/Zanaylo/UNI2-Improvement-Mod";

	while (!base.empty() && base.back() == '/')
		base.pop_back();

	if (!item.tag.empty())
		return base + "/releases/tag/" + item.tag;

	return base + "/releases/latest";
}

void SyncAddonDetail()
{
	if (g_doc == nullptr)
		return;

	Rml::Element* const detail = g_doc->GetElementById("addon-detail");

	if (detail == nullptr)
		return;

	const auto& items = AddonStore::Items();

	if (g_selectedAddon < 0 || g_selectedAddon >= static_cast<int>(items.size()))
	{
		detail->SetInnerRML(
			"<div class=\"empty-state\">"
			"<div class=\"empty-title\">" + EscapeRml(LoaderLocale::Text("detail_empty_title")) + "</div>"
			"<div class=\"empty-text\">" + EscapeRml(LoaderLocale::Text("detail_empty_text")) + "</div>"
			"</div>");
		return;
	}

	const AddonStore::Entry& item = items[g_selectedAddon];
	const std::string updateVer = AddonStore::AvailableUpdateVersionFor(item.manifest.id);
	std::string rml;
	rml += "<div class=\"detail-title\">" + EscapeRml(item.manifest.name) + "</div>";
	rml += "<div class=\"detail-meta\">v" + EscapeRml(item.manifest.version.empty() ? "?" : item.manifest.version)
		+ "  \xC2\xB7  " + EscapeRml(LoaderLocale::Text("detail_by")) + " "
		+ EscapeRml(item.manifest.author.empty() ? LoaderLocale::Text("detail_unknown") : item.manifest.author) + "</div>";
	rml += "<div>" + BadgeHtml(item.manifest);
	if (!updateVer.empty())
		rml += "<span class=\"badge update\">" + EscapeRml(LoaderLocale::Text("badge_update")) + " v" + EscapeRml(updateVer) + "</span>";
	rml += "</div>";
	rml += "<div class=\"detail-divider\"/>";
	rml += "<div class=\"detail-desc\">" + EscapeRml(item.manifest.description.empty()
		? LoaderLocale::Text("detail_no_desc") : item.manifest.description) + "</div>";
	rml += "<div class=\"detail-divider\"/>";
	rml += "<div class=\"detail-kv\"><span class=\"k\">ID</span>" + EscapeRml(item.manifest.id) + "</div>";

	if (!item.sourceKind.empty())
	{
		const char* srcKey = TextList::SameNoCase(item.sourceKind, "release") ? "source_release" : "source_github";
		rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_source")) + "</span>" + EscapeRml(LoaderLocale::Text(srcKey)) + "</div>";
	}

	if (!item.manifest.entry.empty())
		rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_lua")) + "</span>" + EscapeRml(item.manifest.entry) + "</div>";

	if (!item.manifest.nativeDll.empty())
		rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_dll")) + "</span>" + EscapeRml(item.manifest.nativeDll) + "</div>";

	if (!item.manifest.modules.empty())
	{
		std::string mods;
		for (size_t i = 0; i < item.manifest.modules.size(); ++i)
		{
			if (i > 0)
				mods += ", ";
			mods += item.manifest.modules[i];
		}
		rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_modules")) + "</span>" + EscapeRml(mods) + "</div>";
	}

	if (!item.manifest.assets.empty())
		rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_assets")) + "</span>" + EscapeRml(item.manifest.assets) + "</div>";

	if (!item.githubUrl.empty())
		rml += "<div class=\"detail-kv\"><span class=\"k\">GITHUB</span>" + EscapeRml(item.githubUrl) + "</div>";

	if (!item.installedSha256.empty())
		rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_sha256")) + "</span>" + EscapeRml(ShortSha(item.installedSha256)) + "</div>";

	rml += "<div class=\"detail-actions\">";
	rml += "<div class=\"btn small\" data-act=\"addon-open-item\" data-idx=\"" + std::to_string(g_selectedAddon) + "\">" + EscapeRml(LoaderLocale::Text("btn_open_folder")) + "</div>";
	rml += "<div class=\"btn danger small\" data-act=\"addon-remove\" data-idx=\"" + std::to_string(g_selectedAddon) + "\">" + EscapeRml(LoaderLocale::Text("btn_remove")) + "</div>";
	rml += "</div>";

	detail->SetInnerRML(rml);
}

void SyncAddonsList()
{
	if (g_doc == nullptr)
		return;

	const auto& items = AddonStore::Items();
	SetText("count-addons", std::to_string(items.size()));

	Rml::Element* const list = g_doc->GetElementById("addons-list");

	if (list == nullptr)
		return;

	if (items.empty())
	{
		list->SetInnerRML(
			"<div class=\"empty-state\">"
			"<div class=\"empty-title\">" + EscapeRml(LoaderLocale::Text("addons_empty_title")) + "</div>"
			"<div class=\"empty-text\">" + EscapeRml(LoaderLocale::Text("addons_empty_text")) + "</div>"
			"</div>");
		SyncAddonDetail();
		return;
	}

	std::string rml;

	for (int i = 0; i < static_cast<int>(items.size()); ++i)
	{
		const AddonStore::Entry& item = items[i];
		const std::string updateVer = AddonStore::AvailableUpdateVersionFor(item.manifest.id);
		std::string cls = "item-card";
		if (i == g_selectedAddon)
			cls += " selected";
		if (!item.enabled)
			cls += " off";

		const std::string idx = std::to_string(i);
		rml += "<div class=\"" + cls + "\" data-act=\"addon-select\" data-idx=\"" + idx + "\">";
		rml += "<div class=\"switch" + std::string(item.enabled ? " on" : "") + "\" data-act=\"addon-toggle\" data-idx=\"" + idx + "\"><div class=\"switch-knob\"/></div>";
		rml += "<div class=\"item-main\">";
		rml += "<div class=\"item-top\"><span class=\"item-name\">" + EscapeRml(item.manifest.name) + "</span>" + BadgeHtml(item.manifest);
		if (!updateVer.empty())
			rml += "<span class=\"badge update\">" + EscapeRml(LoaderLocale::Text("badge_update")) + " v" + EscapeRml(updateVer) + "</span>";
		rml += "</div>";
		rml += "<div class=\"item-sub\">" + EscapeRml(item.manifest.description.empty() ? item.manifest.id : item.manifest.description) + "</div>";
		rml += "</div>";
		rml += "<div class=\"item-order\">";
		rml += "<div class=\"btn icon\" data-act=\"addon-up\" data-idx=\"" + idx + "\">UP</div>";
		rml += "<div class=\"btn icon\" data-act=\"addon-down\" data-idx=\"" + idx + "\">DN</div>";
		rml += "</div></div>";
	}

	list->SetInnerRML(rml);
	SyncAddonDetail();
}

void SyncStoreDetail(const std::vector<AddonCatalog::CatalogItem>& catalog)
{
	if (g_doc == nullptr)
		return;

	Rml::Element* const detail = g_doc->GetElementById("store-detail");

	if (detail == nullptr)
		return;

	if (g_selectedStore < 0 || g_selectedStore >= static_cast<int>(catalog.size()))
	{
		detail->SetInnerRML(
			"<div class=\"empty-state\">"
			"<div class=\"empty-title\">" + EscapeRml(LoaderLocale::Text("store_detail_empty_title")) + "</div>"
			"<div class=\"empty-text\">" + EscapeRml(LoaderLocale::Text("store_detail_empty_text")) + "</div>"
			"</div>");
		return;
	}

	const AddonCatalog::CatalogItem& item = catalog[g_selectedStore];
	const AddonStore::Entry* const installed = AddonStore::FindInstalledById(item.id);
	const bool hasUpdate = AddonStore::CatalogItemHasUpdate(item);
	const bool isOfficial = TextList::SameNoCase(item.source, "release");
	const bool valid = LoaderConfig::IsValidGameDir(g_gameDir);
	const bool busy = AddonStore::IsBusy();
	const bool canInstall = valid && !busy && item.resolved && !item.downloadUrl.empty();

	std::string rml;
	rml += "<div class=\"detail-head\">";
	if (!item.iconCachePath.empty())
		rml += "<img class=\"detail-icon\" src=\"abs:" + EscapeRml(item.iconCachePath) + "\"/>";
	rml += "<div>";
	rml += "<div class=\"detail-title\">" + EscapeRml(item.name) + "</div>";
	rml += "<div class=\"detail-meta\">" + EscapeRml(LoaderLocale::Text("detail_by")) + " "
		+ EscapeRml(item.author.empty() ? LoaderLocale::Text("detail_unknown") : item.author) + "</div>";
	rml += "</div></div>";

	rml += "<div>";
	rml += isOfficial
		? ("<span class=\"badge official\">" + EscapeRml(LoaderLocale::Text("badge_official")) + "</span>")
		: ("<span class=\"badge github\">" + EscapeRml(LoaderLocale::Text("badge_github")) + "</span>");
	if (!item.version.empty())
		rml += "<span class=\"badge ver\">v" + EscapeRml(item.version) + "</span>";
	if (hasUpdate)
		rml += "<span class=\"badge update\">" + EscapeRml(LoaderLocale::Text("badge_update")) + "</span>";
	else if (installed != nullptr)
		rml += "<span class=\"badge installed\">" + EscapeRml(LoaderLocale::Text("badge_installed")) + "</span>";
	rml += "</div>";

	rml += "<div class=\"detail-divider\"/>";
	rml += "<div class=\"detail-desc\">" + EscapeRml(item.description.empty()
		? LoaderLocale::Text("detail_no_desc") : item.description) + "</div>";
	rml += "<div class=\"detail-divider\"/>";

	rml += "<div class=\"detail-kv\"><span class=\"k\">ID</span>" + EscapeRml(item.id) + "</div>";
	rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_source")) + "</span>"
		+ EscapeRml(LoaderLocale::Text(isOfficial ? "source_release" : "source_github")) + "</div>";

	if (!item.version.empty() || !item.tag.empty())
	{
		const std::string verLabel = !item.version.empty() ? ("v" + item.version) : item.tag;
		rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_latest")) + "</span>"
			+ EscapeRml(verLabel) + "</div>";
	}

	if (installed != nullptr)
	{
		const std::string instVer = !installed->manifest.version.empty()
			? installed->manifest.version : installed->installedVersion;
		rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_installed")) + "</span>v"
			+ EscapeRml(instVer.empty() ? "?" : instVer) + "</div>";
	}

	if (!item.modules.empty())
		rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_modules")) + "</span>" + EscapeRml(item.modules) + "</div>";

	if (!item.assets.empty())
		rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_assets")) + "</span>" + EscapeRml(item.assets) + "</div>";

	if (!item.repo.empty())
		rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_repo")) + "</span>" + EscapeRml(item.repo) + "</div>";

	if (!item.sha256.empty())
		rml += "<div class=\"detail-kv\"><span class=\"k\">" + EscapeRml(LoaderLocale::Text("detail_sha256")) + "</span>" + EscapeRml(ShortSha(item.sha256)) + "</div>";

	if (!item.error.empty())
	{
		const bool is404 = item.error.find("404") != std::string::npos;
		rml += "<div class=\"detail-err\">" + EscapeRml(is404 ? LoaderLocale::Text("store_no_release") : item.error.c_str()) + "</div>";
	}
	else if (!item.resolved && item.downloadUrl.empty())
		rml += "<div class=\"detail-err\">" + EscapeRml(LoaderLocale::Text("store_no_release")) + "</div>";

	const char* btnKey = hasUpdate ? "btn_update" : (installed != nullptr ? "btn_reinstall" : "btn_install");
	const std::string btnCls = std::string("btn small ") + (hasUpdate ? "warn" : (installed != nullptr ? "" : "accent")) + (canInstall ? "" : " disabled");
	const std::string idx = std::to_string(g_selectedStore);

	rml += "<div class=\"detail-actions\">";
	rml += "<div class=\"" + btnCls + "\" data-act=\"store-install\" data-idx=\"" + idx + "\">" + EscapeRml(LoaderLocale::Text(btnKey)) + "</div>";
	rml += "<div class=\"btn small\" data-act=\"store-open-repo\" data-idx=\"" + idx + "\">" + EscapeRml(LoaderLocale::Text("btn_open_release")) + "</div>";
	rml += "</div>";

	detail->SetInnerRML(rml);
}

void SyncStoreList()
{
	if (g_doc == nullptr)
		return;

	const std::vector<AddonCatalog::CatalogItem> catalog = AddonStore::CatalogItems();
	const int updates = AddonStore::CountCatalogUpdates();
	SetText("count-store", updates > 0 ? (std::to_string(catalog.size()) + " (" + std::to_string(updates) + "!)") : std::to_string(catalog.size()));
	SetText("store-status-text", AddonStore::CatalogStatus());

	const bool valid = LoaderConfig::IsValidGameDir(g_gameDir);
	const bool busy = AddonStore::IsBusy();
	SetClass("btn-store-refresh", "disabled", busy);
	SetClass("btn-store-bundle", "disabled", !valid || busy);
	SetClass("btn-store-update-all", "disabled", !valid || busy);

	Rml::Element* const list = g_doc->GetElementById("store-list");

	if (list == nullptr)
		return;

	if (catalog.empty())
	{
		const char* titleKey = busy ? "store_loading_title" : "store_empty_title";
		const char* textKey = busy ? "store_loading_text" : "store_empty_text";
		list->SetInnerRML(
			"<div class=\"empty-state\">"
			"<div class=\"empty-title\">" + EscapeRml(LoaderLocale::Text(titleKey)) + "</div>"
			"<div class=\"empty-text\">" + EscapeRml(LoaderLocale::Text(textKey)) + "</div>"
			"</div>");
		SyncStoreDetail(catalog);
		return;
	}

	std::string rml;

	for (int i = 0; i < static_cast<int>(catalog.size()); ++i)
	{
		const AddonCatalog::CatalogItem& item = catalog[i];
		const AddonStore::Entry* const installed = AddonStore::FindInstalledById(item.id);
		const bool hasUpdate = AddonStore::CatalogItemHasUpdate(item);
		const bool isOfficial = TextList::SameNoCase(item.source, "release");
		const bool canInstall = valid && !busy && item.resolved && !item.downloadUrl.empty();

		std::string cls = "item-card";
		if (i == g_selectedStore)
			cls += " selected";

		const std::string idx = std::to_string(i);
		const char* btnKey = hasUpdate ? "btn_update" : (installed != nullptr ? "btn_reinstall" : "btn_install");
		const std::string btnCls = std::string("btn small ") + (hasUpdate ? "warn" : (installed != nullptr ? "" : "accent")) + (canInstall ? "" : " disabled");

		rml += "<div class=\"" + cls + "\" data-act=\"store-select\" data-idx=\"" + idx + "\">";
		if (!item.iconCachePath.empty())
			rml += "<img class=\"store-icon\" src=\"abs:" + EscapeRml(item.iconCachePath) + "\"/>";
		rml += "<div class=\"item-main\">";
		rml += "<div class=\"item-top\"><span class=\"item-name\">" + EscapeRml(item.name) + "</span>";
		rml += isOfficial
			? ("<span class=\"badge official\">" + EscapeRml(LoaderLocale::Text("badge_official")) + "</span>")
			: ("<span class=\"badge github\">" + EscapeRml(LoaderLocale::Text("badge_github")) + "</span>");
		if (!item.version.empty())
			rml += "<span class=\"badge ver\">v" + EscapeRml(item.version) + "</span>";
		if (hasUpdate)
			rml += "<span class=\"badge update\">" + EscapeRml(LoaderLocale::Text("badge_update")) + "</span>";
		else if (installed != nullptr)
			rml += "<span class=\"badge installed\">" + EscapeRml(LoaderLocale::Text("badge_installed")) + "</span>";
		rml += "</div>";
		rml += "<div class=\"item-sub\">" + EscapeRml(item.description.empty() ? item.repo : item.description) + "</div>";
		rml += "</div>";
		rml += "<div class=\"item-actions\">";
		rml += "<div class=\"" + btnCls + "\" data-act=\"store-install\" data-idx=\"" + idx + "\">" + EscapeRml(LoaderLocale::Text(btnKey)) + "</div>";
		rml += "</div></div>";
	}

	list->SetInnerRML(rml);
	SyncStoreDetail(catalog);
}

void SyncPacksList()
{
	if (g_doc == nullptr)
		return;

	const auto& packs = PackStore::Items();
	SetText("count-packs", std::to_string(packs.size()));

	Rml::Element* const list = g_doc->GetElementById("packs-list");

	if (list == nullptr)
		return;

	if (packs.empty())
	{
		list->SetInnerRML(
			"<div class=\"empty-state\">"
			"<div class=\"empty-title\">" + EscapeRml(LoaderLocale::Text("packs_empty_title")) + "</div>"
			"<div class=\"empty-text\">" + EscapeRml(LoaderLocale::Text("packs_empty_text")) + "</div>"
			"</div>");
		return;
	}

	std::string rml;

	for (int i = 0; i < static_cast<int>(packs.size()); ++i)
	{
		const PackStore::Pack& pack = packs[i];
		const std::string idx = std::to_string(i);
		const char* badgeClass = pack.isZip ? "lua" : "assets";
		const char* badgeText = pack.isZip ? "ZIP" : "MOD";
		rml += "<div class=\"item-card" + std::string(pack.enabled ? "" : " off") + "\">";
		rml += "<div class=\"switch" + std::string(pack.enabled ? " on" : "") + "\" data-act=\"pack-toggle\" data-idx=\"" + idx + "\"><div class=\"switch-knob\"/></div>";
		rml += "<div class=\"item-main\">";
		rml += "<div class=\"item-top\"><span class=\"item-name\">" + EscapeRml(pack.name) + "</span><span class=\"badge " + badgeClass + "\">" + badgeText + "</span></div>";
		rml += "<div class=\"item-sub\">" + EscapeRml(pack.folder) + "</div>";
		rml += "</div>";
		rml += "<div class=\"item-order\">";
		rml += "<div class=\"btn icon\" data-act=\"pack-up\" data-idx=\"" + idx + "\">UP</div>";
		rml += "<div class=\"btn icon\" data-act=\"pack-down\" data-idx=\"" + idx + "\">DN</div>";
		rml += "<div class=\"btn danger icon\" data-act=\"pack-remove\" data-idx=\"" + idx + "\">DEL</div>";
		rml += "</div></div>";
	}

	list->SetInnerRML(rml);
}

void SyncModulesView()
{
	if (g_doc == nullptr)
		return;

	const bool valid = LoaderConfig::IsValidGameDir(g_gameDir);
	std::string coreRml;
	std::string optRml;
	int enabledCount = 0;

	for (int i = 0; i < FeatureCatalog::Id_COUNT; ++i)
	{
		const auto id = static_cast<FeatureCatalog::Id>(i);
		const bool on = valid && LoaderConfig::ReadModuleState(g_gameDir, id);

		if (on)
			++enabledCount;

		const std::string idx = std::to_string(i);
		std::string card;
		card += "<div class=\"mod-card" + std::string(on ? " on" : "") + "\" data-act=\"mod-toggle\" data-idx=\"" + idx + "\">";
		card += "<div class=\"switch" + std::string(on ? " on" : "") + "\"><div class=\"switch-knob\"/></div>";
		card += "<div class=\"item-main\">";
		card += "<div class=\"item-name\">" + EscapeRml(LoaderLocale::ModuleName(id)) + "</div>";
		card += "<div class=\"item-sub\">" + EscapeRml(LoaderLocale::ModuleSummary(id)) + "</div>";
		card += "</div></div>";

		if (LoaderConfig::IsCoreModule(id))
			coreRml += card;
		else
			optRml += card;
	}

	SetText("count-modules", std::to_string(enabledCount));

	if (Rml::Element* const core = g_doc->GetElementById("core-modules-grid"))
		core->SetInnerRML(coreRml);

	if (Rml::Element* const opt = g_doc->GetElementById("opt-modules-grid"))
		opt->SetInnerRML(optRml);

	SetClass("switch-game-addon-check", "on", g_gameAddonCheckOnStart);
	SetClass("switch-auto-update", "on", g_updateOnStart);
}

bool SyncLogView()
{
	if (g_doc == nullptr)
		return false;

	const std::vector<std::string> lines = LoaderLog::Snapshot();
	const bool busy = AddonStore::IsBusy();
	const std::string status = AddonStore::BusyStatus();

	if (lines.size() == g_lastLogCount && busy == g_lastBusy && status == g_lastBusyStatus)
		return false;

	g_lastLogCount = lines.size();
	g_lastBusy = busy;
	g_lastBusyStatus = status;

	SetText("count-log", std::to_string(lines.size()));
	SetClass("foot-dot", "busy", busy);

	if (busy && !status.empty())
		SetText("foot-status", status);
	else if (!lines.empty())
		SetText("foot-status", lines.back());
	else
		SetText("foot-status", LoaderLocale::Text("foot_ready"));

	if (Rml::Element* const box = g_doc->GetElementById("log-list"))
	{
		std::string rml;
		for (auto it = lines.rbegin(); it != lines.rend(); ++it)
			rml += "<div class=\"log-line\">" + EscapeRml(*it) + "</div>";
		box->SetInnerRML(rml);
	}

	return true;
}

void SyncConfirmModal()
{
	if (g_doc == nullptr)
		return;

	const bool has = AddonStore::HasPendingConfirmation();
	SetClass("confirm-backdrop", "hidden", !has);

	if (!has)
		return;

	const AddonStore::PendingConfirmation pending = AddonStore::GetPendingConfirmation();
	SetText("modal-title", LoaderLocale::Text("modal_warn_title"));
	SetText("modal-target", pending.label);
	SetText("modal-sub", pending.sourceInfo);
	SetText("modal-note", LoaderLocale::Text("modal_warn_note"));
	SetText("btn-modal-cancel", LoaderLocale::Text("btn_cancel"));
	SetText("btn-modal-proceed", LoaderLocale::Text("btn_install_anyway"));

	if (Rml::Element* const list = g_doc->GetElementById("modal-warnings"))
	{
		std::string rml;
		for (const std::string& warning : pending.warnings)
			rml += "<div class=\"modal-warn-item\">\xE2\x80\xA2  " + EscapeRml(warning) + "</div>";
		list->SetInnerRML(rml);
	}
}

void SyncLanguageViews()
{
	ApplyStaticTranslations();
	SyncHeader();
	SyncAddonsList();
	SyncStoreList();
	SyncPacksList();
	SyncModulesView();
	SyncConfirmModal();
	g_lastLogCount = static_cast<size_t>(-1);
	SyncLogView();
}

void RefreshAll()
{
	AddonStore::Refresh(g_gameDir);
	PackStore::Refresh(g_gameDir);

	if (g_selectedAddon >= static_cast<int>(AddonStore::Items().size()))
		g_selectedAddon = -1;

	if (g_selectedAddon < 0 && !AddonStore::Items().empty())
		g_selectedAddon = 0;

	const std::vector<AddonCatalog::CatalogItem> catalog = AddonStore::CatalogItems();
	if (g_selectedStore >= static_cast<int>(catalog.size()))
		g_selectedStore = -1;

	if (g_selectedStore < 0 && !catalog.empty())
		g_selectedStore = 0;

	SyncLanguageViews();
}

void SetGameDir(const std::string& dir)
{
	g_gameDir = dir;

	if (!g_gameDir.empty() && g_gameDir.back() != '\\')
		g_gameDir.push_back('\\');

	LoaderLocale::Initialize(g_gameDir);

	if (LoaderConfig::IsValidGameDir(g_gameDir))
	{
		LoaderConfig::SaveGameDir(g_gameDir);
		g_updateOnStart = LoaderConfig::ReadUpdateOnStart(g_gameDir);
		g_gameAddonCheckOnStart = LoaderConfig::ReadCheckAddonUpdatesInGame(g_gameDir);
		LoaderLog::Add(std::string(LoaderLocale::Text("label_game_folder")) + ": " + g_gameDir);
	}
	else
	{
		LoaderLog::Add(LoaderLocale::Text("msg_select_game"));
	}

	RefreshAll();

	if (LoaderConfig::IsValidGameDir(g_gameDir) &&
		!LoaderConfig::ReadBundleBootstrapped(g_gameDir) &&
		AddonStore::Items().empty() &&
		!AddonStore::IsBusy())
	{
		LoaderLog::Add(LoaderLocale::Text("msg_bootstrap_bundle"));
		AddonStore::BeginBootstrapOfficialBundle(g_gameDir);
		SyncHeader();
		SyncStoreList();
	}
}

bool LoadDocument();

class UiEventListener final : public Rml::EventListener
{
public:
	void ProcessEvent(Rml::Event& event) override
	{
		Rml::Element* target = event.GetTargetElement();

		for (Rml::Element* walk = target; walk != nullptr; walk = walk->GetParentNode())
		{
			const Rml::String act = walk->GetAttribute<Rml::String>("data-act", "");

			if (!act.empty())
			{
				const int idx = walk->GetAttribute<int>("data-idx", -1);
				HandleAction(act, idx);
				event.StopPropagation();
				return;
			}

			const Rml::String id = walk->GetId();

			if (!id.empty() && HandleStaticId(id))
			{
				event.StopPropagation();
				return;
			}
		}
	}

private:
	void HandleAction(const std::string& act, int idx)
	{
		if (AddonStore::IsBusy())
			return;

		const bool valid = LoaderConfig::IsValidGameDir(g_gameDir);

		if (act == "addon-toggle" && valid)
		{
			const auto& items = AddonStore::Items();
			if (idx >= 0 && idx < static_cast<int>(items.size()))
			{
				const bool next = !items[idx].enabled;
				AddonStore::SetEnabled(g_gameDir, idx, next);
				LoaderLog::Add(items[idx].manifest.name + LoaderLocale::Text(next ? "msg_enabled" : "msg_disabled"));
				SyncAddonsList();
			}
			return;
		}

		if (act == "addon-select")
		{
			const auto& items = AddonStore::Items();
			if (idx >= 0 && idx < static_cast<int>(items.size()))
			{
				g_selectedAddon = idx;
				WriteUrlInput(items[idx].githubUrl);
				SyncAddonsList();
			}
			return;
		}

		if (act == "addon-up" && valid)
		{
			if (AddonStore::Move(g_gameDir, idx, -1))
			{
				g_selectedAddon = idx - 1;
				SyncAddonsList();
			}
			return;
		}

		if (act == "addon-down" && valid)
		{
			if (AddonStore::Move(g_gameDir, idx, 1))
			{
				g_selectedAddon = idx + 1;
				SyncAddonsList();
			}
			return;
		}

		if (act == "addon-open-item")
		{
			const auto& items = AddonStore::Items();
			if (idx >= 0 && idx < static_cast<int>(items.size()))
				OpenInExplorer(items[idx].folder);
			return;
		}

		if (act == "addon-remove" && valid)
		{
			std::string msg;
			if (AddonStore::Remove(g_gameDir, idx, msg))
				g_selectedAddon = -1;
			LoaderLog::Add(msg);
			SyncAddonsList();
			SyncStoreList();
			return;
		}

		if (act == "store-select")
		{
			const std::vector<AddonCatalog::CatalogItem> catalog = AddonStore::CatalogItems();
			if (idx >= 0 && idx < static_cast<int>(catalog.size()))
			{
				g_selectedStore = idx;
				SyncStoreList();
			}
			return;
		}

		if (act == "store-install" && valid)
		{
			const std::vector<AddonCatalog::CatalogItem> catalog = AddonStore::CatalogItems();
			if (idx >= 0 && idx < static_cast<int>(catalog.size()) && catalog[idx].resolved && !catalog[idx].downloadUrl.empty())
			{
				AddonStore::BeginInstallCatalogItem(g_gameDir, idx);
				SyncStoreList();
			}
			return;
		}

		if (act == "store-open-repo")
		{
			const std::vector<AddonCatalog::CatalogItem> catalog = AddonStore::CatalogItems();
			if (idx >= 0 && idx < static_cast<int>(catalog.size()))
				OpenInExplorer(ReleasePageForCatalogItem(catalog[idx]));
			return;
		}

		if (act == "pack-toggle" && valid)
		{
			const auto& packs = PackStore::Items();
			if (idx >= 0 && idx < static_cast<int>(packs.size()))
			{
				const bool next = !packs[idx].enabled;
				PackStore::SetEnabled(g_gameDir, idx, next);
				LoaderLog::Add(packs[idx].name + LoaderLocale::Text(next ? "msg_enabled" : "msg_disabled"));
				SyncPacksList();
			}
			return;
		}

		if (act == "pack-up" && valid)
		{
			if (PackStore::Move(g_gameDir, idx, -1))
				SyncPacksList();
			return;
		}

		if (act == "pack-down" && valid)
		{
			if (PackStore::Move(g_gameDir, idx, 1))
				SyncPacksList();
			return;
		}

		if (act == "pack-remove" && valid)
		{
			std::string msg;
			PackStore::Remove(g_gameDir, idx, msg);
			LoaderLog::Add(msg);
			SyncPacksList();
			return;
		}

		if (act == "mod-toggle" && valid)
		{
			if (idx >= 0 && idx < FeatureCatalog::Id_COUNT)
			{
				const auto id = static_cast<FeatureCatalog::Id>(idx);
				const bool next = !LoaderConfig::ReadModuleState(g_gameDir, id);
				LoaderConfig::WriteModuleState(g_gameDir, id, next);
				LoaderLog::Add(std::string(LoaderLocale::ModuleName(id)) + LoaderLocale::Text(next ? "msg_enabled" : "msg_disabled"));
				SyncModulesView();
			}
		}
	}

	bool HandleStaticId(const std::string& id)
	{
		const bool valid = LoaderConfig::IsValidGameDir(g_gameDir);
		const bool busy = AddonStore::IsBusy();

		if (id == "btn-modal-cancel")
		{
			AddonStore::ConfirmPendingInstall(false);
			SyncConfirmModal();
			return true;
		}

		if (id == "btn-modal-proceed")
		{
			AddonStore::ConfirmPendingInstall(true);
			SyncConfirmModal();
			return true;
		}

		if (id == "lang-en") { LoaderLocale::Set(LoaderLocale::Lang::En); SyncLanguageViews(); return true; }
		if (id == "lang-pt") { LoaderLocale::Set(LoaderLocale::Lang::PtBr); SyncLanguageViews(); return true; }
		if (id == "lang-es") { LoaderLocale::Set(LoaderLocale::Lang::EsLatam); SyncLanguageViews(); return true; }

		if (id == "nav-addons") { g_activeTab = Tab::Addons; SyncTabs(); return true; }
		if (id == "nav-store")
		{
			g_activeTab = Tab::Store;
			SyncTabs();
			if (!AddonStore::CatalogLoaded() && !busy)
			{
				AddonStore::BeginRefreshCatalog(g_gameDir);
				SyncStoreList();
			}
			return true;
		}
		if (id == "nav-packs") { g_activeTab = Tab::Packs; SyncTabs(); return true; }
		if (id == "nav-modules") { g_activeTab = Tab::Modules; SyncTabs(); return true; }
		if (id == "nav-log") { g_activeTab = Tab::Log; SyncTabs(); return true; }

		if (id == "btn-reload-ui")
		{
			LoadDocument();
			LoaderLog::Add("Reloaded RmlUi layout.");
			return true;
		}

		if (id == "btn-browse-game")
		{
			std::string picked;
			if (PickFolderDialog(picked))
				SetGameDir(picked);
			return true;
		}

		if (id == "btn-install-proxy" && valid)
		{
			InstallBundledProxyDll();
			SyncHeader();
			return true;
		}

		if (id == "btn-play-steam" && valid && !busy)
		{
			LaunchGameViaSteam();
			return true;
		}

		if (id == "btn-play-direct" && valid && !busy)
		{
			LaunchGameDirect();
			return true;
		}

		if (id == "btn-addon-zip" && valid && !busy)
		{
			std::string zip;
			std::string msg;
			if (PickZipDialog(zip))
			{
				AddonStore::InstallFromPath(g_gameDir, zip, msg);
				LoaderLog::Add(msg);
				RefreshAll();
			}
			return true;
		}

		if (id == "btn-addon-folder" && valid && !busy)
		{
			std::string folder;
			std::string msg;
			if (PickFolderDialog(folder))
			{
				AddonStore::InstallFromPath(g_gameDir, folder, msg);
				LoaderLog::Add(msg);
				RefreshAll();
			}
			return true;
		}

		if (id == "btn-addon-open" && valid)
		{
			OpenInExplorer(AddonStore::Root(g_gameDir));
			return true;
		}

		if (id == "btn-addon-update" && valid && !busy)
		{
			AddonStore::BeginUpdateLinked(g_gameDir);
			return true;
		}

		if (id == "btn-store-refresh" && !busy)
		{
			AddonStore::BeginRefreshCatalog(g_gameDir);
			SyncStoreList();
			return true;
		}

		if (id == "btn-store-bundle" && valid && !busy)
		{
			AddonStore::BeginBootstrapOfficialBundle(g_gameDir);
			SyncStoreList();
			return true;
		}

		if (id == "btn-store-update-all" && valid && !busy)
		{
			AddonStore::BeginUpdateLinked(g_gameDir);
			SyncStoreList();
			return true;
		}

		if (id == "btn-addon-refresh" || id == "btn-pack-refresh")
		{
			RefreshAll();
			return true;
		}

		if (id == "btn-save-link" && valid && !busy && g_selectedAddon >= 0)
		{
			std::string msg;
			AddonStore::SetGitHubLink(g_gameDir, g_selectedAddon, ReadUrlInput(), msg);
			LoaderLog::Add(msg);
			SyncAddonsList();
			return true;
		}

		if (id == "btn-install-url" && valid && !busy)
		{
			const std::string url = ReadUrlInput();
			if (!url.empty())
				AddonStore::BeginInstallFromUrl(g_gameDir, url);
			else
				LoaderLog::Add(LoaderLocale::Text("msg_paste_url"));
			return true;
		}

		if (id == "btn-pack-zip" && valid)
		{
			std::string zip;
			std::string msg;
			if (PickZipDialog(zip))
			{
				PackStore::InstallFromPath(g_gameDir, zip, msg);
				LoaderLog::Add(msg);
				RefreshAll();
			}
			return true;
		}

		if (id == "btn-pack-folder" && valid)
		{
			std::string folder;
			std::string msg;
			if (PickFolderDialog(folder))
			{
				PackStore::InstallFromPath(g_gameDir, folder, msg);
				LoaderLog::Add(msg);
				RefreshAll();
			}
			return true;
		}

		if (id == "btn-pack-open" && valid)
		{
			OpenInExplorer(PackStore::GameModsRoot(g_gameDir));
			return true;
		}

		if (id == "btn-reset-base" && valid)
		{
			LoaderConfig::ResetCleanBaseModules(g_gameDir);
			LoaderLog::Add(LoaderLocale::Text("msg_reset_base"));
			SyncModulesView();
			return true;
		}

		if (id == "btn-open-basepals" && valid)
		{
			const std::string basePals = LoaderConfig::ModRoot(g_gameDir) + "\\Assets\\BasePals";
			CreateDirectoryA(LoaderConfig::ModRoot(g_gameDir).c_str(), nullptr);
			CreateDirectoryA((LoaderConfig::ModRoot(g_gameDir) + "\\Assets").c_str(), nullptr);
			CreateDirectoryA(basePals.c_str(), nullptr);
			OpenInExplorer(basePals);
			return true;
		}

		if (id == "toggle-game-addon-check" && valid)
		{
			g_gameAddonCheckOnStart = !g_gameAddonCheckOnStart;
			LoaderConfig::SaveCheckAddonUpdatesInGame(g_gameDir, g_gameAddonCheckOnStart);
			SyncModulesView();
			return true;
		}

		if (id == "toggle-auto-update" && valid)
		{
			g_updateOnStart = !g_updateOnStart;
			LoaderConfig::SaveUpdateOnStart(g_gameDir, g_updateOnStart);
			SyncModulesView();
			return true;
		}

		return false;
	}
};

UiEventListener g_clickListener;

bool LoadDocument()
{
	if (g_context == nullptr)
		return false;

	if (g_doc != nullptr)
	{
		g_doc->RemoveEventListener(Rml::EventId::Click, &g_clickListener);
		g_doc->Close();
		g_doc = nullptr;
		Rml::Factory::ClearStyleSheetCache();
	}

	g_doc = g_context->LoadDocument("loader.rml");

	if (g_doc == nullptr)
		return false;

	g_doc->AddEventListener(Rml::EventId::Click, &g_clickListener);
	g_doc->Show();

	SyncTabs();
	SyncLanguageViews();
	return true;
}

void LoadFonts()
{
	static const char* const kFonts[] = {
		"fonts/Barlow-Regular.ttf",
		"fonts/BarlowCondensed-SemiBold.ttf",
		"fonts/BarlowCondensed-Bold.ttf",
		"fonts/BarlowCondensed-ExtraBold.ttf",
	};

	for (const char* font : kFonts)
		Rml::LoadFontFace(font);
}

bool PathContainsAddonManifest(const std::string& path)
{
	const DWORD attr = GetFileAttributesA(path.c_str());

	if (attr == INVALID_FILE_ATTRIBUTES)
		return false;

	if ((attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
	{
		if (GetFileAttributesA((path + "\\addon.ini").c_str()) != INVALID_FILE_ATTRIBUTES)
			return true;

		WIN32_FIND_DATAA found = {};
		const HANDLE search = FindFirstFileA((path + "\\*").c_str(), &found);

		if (search == INVALID_HANDLE_VALUE)
			return false;

		bool hasAddon = false;

		do
		{
			if (found.cFileName[0] == '.' || (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				continue;

			if (GetFileAttributesA((path + "\\" + found.cFileName + "\\addon.ini").c_str()) != INVALID_FILE_ATTRIBUTES)
			{
				hasAddon = true;
				break;
			}
		}
		while (FindNextFileA(search, &found) != 0);

		FindClose(search);
		return hasAddon;
	}

	std::vector<std::string> names;

	if (!ZipArchive::List(path, names))
		return false;

	for (const std::string& name : names)
	{
		const size_t slash = name.find_last_of("\\/");
		const std::string leaf = slash == std::string::npos ? name : name.substr(slash + 1);

		if (TextList::SameNoCase(leaf, "addon.ini"))
			return true;
	}

	return false;
}

}

bool LoaderUi::Initialize(HWND window, int width, int height)
{
	g_window = window;

	const std::string detectedDir = LoaderConfig::DetectGameDir();
	LoaderLocale::Initialize(detectedDir);

	g_system = std::make_unique<LoaderSystemInterface>();
	g_system->SetWindow(window);

	g_render = std::make_unique<RmlRenderGDI>();
	g_render->Resize(width, height);
	g_files = std::make_unique<RmlFileInterface>(ExeDir() + "ModLoaderUI");
	g_ime = std::make_unique<TextInputMethodEditor_Win32>();

	Rml::SetSystemInterface(g_system.get());
	Rml::SetRenderInterface(g_render.get());
	Rml::SetFileInterface(g_files.get());

	if (!Rml::Initialise())
		return false;

	LoadFonts();

	g_context = Rml::CreateContext("modloader", Rml::Vector2i(width, height), g_render.get(), g_ime.get());

	if (g_context == nullptr)
		return false;

	if (!LoadDocument())
		return false;

	SetGameDir(detectedDir);

	if (LoaderConfig::IsValidGameDir(g_gameDir) && g_updateOnStart)
		AddonStore::BeginUpdateLinked(g_gameDir);

	return true;
}

void LoaderUi::Shutdown()
{
	if (g_doc != nullptr)
	{
		g_doc->RemoveEventListener(Rml::EventId::Click, &g_clickListener);
		g_doc->Close();
		g_doc = nullptr;
	}

	if (g_context != nullptr)
	{
		Rml::RemoveContext("modloader");
		g_context = nullptr;
	}

	Rml::Shutdown();
	g_ime.reset();
	g_files.reset();
	g_render.reset();
	g_system.reset();
}

void LoaderUi::OnResize(int width, int height)
{
	if (width <= 0 || height <= 0)
		return;

	if (g_render != nullptr)
		g_render->Resize(width, height);

	if (g_context != nullptr)
		g_context->SetDimensions(Rml::Vector2i(width, height));
}

void LoaderUi::OnDropPath(const std::string& path)
{
	if (!LoaderConfig::IsValidGameDir(g_gameDir) || AddonStore::IsBusy())
		return;

	std::string msg;

	if (PathContainsAddonManifest(path))
		AddonStore::InstallFromPath(g_gameDir, path, msg);
	else
		PackStore::InstallFromPath(g_gameDir, path, msg);

	LoaderLog::Add(msg);
	RefreshAll();
}

bool LoaderUi::HandleMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_KEYDOWN && wParam == VK_F5)
	{
		LoadDocument();
		LoaderLog::Add("Reloaded RmlUi layout (F5).");
		return true;
	}

	if (g_context == nullptr || g_ime == nullptr)
		return false;

	return !RmlWin32::WindowProcedure(g_context, *g_ime, window, message, wParam, lParam);
}

bool LoaderUi::TickBackground()
{
	bool dirty = false;

	if (AddonStore::ConsumeRefreshNeeded())
	{
		RefreshAll();
		dirty = true;
	}

	if (SyncLogView())
		dirty = true;

	const bool urlEmpty = ReadUrlInput().empty();
	if (urlEmpty != g_lastUrlEmpty)
	{
		SyncUrlPlaceholder();
		dirty = true;
	}

	if (g_context != nullptr)
	{
		if (Rml::Element* const focus = g_context->GetFocusElement())
		{
			if (focus->GetId() == "url-input")
				dirty = true;
		}
	}

	return dirty;
}

void LoaderUi::RenderToDC(HDC targetDc, int width, int height)
{
	if (g_context == nullptr || g_render == nullptr || width <= 0 || height <= 0)
		return;

	g_render->Resize(width, height);
	g_context->Update();
	g_render->BeginFrame(kClearColorBgra);
	g_context->Render();
	g_render->Present(targetDc);
}
