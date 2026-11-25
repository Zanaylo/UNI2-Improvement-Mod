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
	Packs = 1,
	Modules = 2,
	Log = 3
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
bool g_updateOnStart = false;
size_t g_lastLogCount = 0;
std::string g_lastBusyStatus;
bool g_lastBusy = false;

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

void WriteUrlInput(const std::string& value)
{
	if (g_doc == nullptr)
		return;

	if (auto* const input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(g_doc->GetElementById("url-input")))
		input->SetValue(value);
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
	SetClass("nav-packs", "active", g_activeTab == Tab::Packs);
	SetClass("nav-modules", "active", g_activeTab == Tab::Modules);
	SetClass("nav-log", "active", g_activeTab == Tab::Log);

	SetClass("view-addons", "hidden", g_activeTab != Tab::Addons);
	SetClass("view-packs", "hidden", g_activeTab != Tab::Packs);
	SetClass("view-modules", "hidden", g_activeTab != Tab::Modules);
	SetClass("view-log", "hidden", g_activeTab != Tab::Log);
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
	std::string rml;
	rml += "<div class=\"detail-title\">" + EscapeRml(item.manifest.name) + "</div>";
	rml += "<div class=\"detail-meta\">v" + EscapeRml(item.manifest.version.empty() ? "?" : item.manifest.version)
		+ "  \xC2\xB7  " + EscapeRml(LoaderLocale::Text("detail_by")) + " "
		+ EscapeRml(item.manifest.author.empty() ? LoaderLocale::Text("detail_unknown") : item.manifest.author) + "</div>";
	rml += "<div>" + BadgeHtml(item.manifest) + "</div>";
	rml += "<div class=\"detail-divider\"/>";
	rml += "<div class=\"detail-desc\">" + EscapeRml(item.manifest.description.empty()
		? LoaderLocale::Text("detail_no_desc") : item.manifest.description) + "</div>";
	rml += "<div class=\"detail-divider\"/>";
	rml += "<div class=\"detail-kv\"><span class=\"k\">ID</span>" + EscapeRml(item.manifest.id) + "</div>";

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
		std::string cls = "item-card";
		if (i == g_selectedAddon)
			cls += " selected";
		if (!item.enabled)
			cls += " off";

		const std::string idx = std::to_string(i);
		rml += "<div class=\"" + cls + "\" data-act=\"addon-select\" data-idx=\"" + idx + "\">";
		rml += "<div class=\"switch" + std::string(item.enabled ? " on" : "") + "\" data-act=\"addon-toggle\" data-idx=\"" + idx + "\"><div class=\"switch-knob\"/></div>";
		rml += "<div class=\"item-main\">";
		rml += "<div class=\"item-top\"><span class=\"item-name\">" + EscapeRml(item.manifest.name) + "</span>" + BadgeHtml(item.manifest) + "</div>";
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

void SyncLanguageViews()
{
	ApplyStaticTranslations();
	SyncHeader();
	SyncAddonsList();
	SyncPacksList();
	SyncModulesView();
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
		LoaderLog::Add(std::string(LoaderLocale::Text("label_game_folder")) + ": " + g_gameDir);
	}
	else
	{
		LoaderLog::Add(LoaderLocale::Text("msg_select_game"));
	}

	RefreshAll();
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

		if (id == "lang-en") { LoaderLocale::Set(LoaderLocale::Lang::En); SyncLanguageViews(); return true; }
		if (id == "lang-pt") { LoaderLocale::Set(LoaderLocale::Lang::PtBr); SyncLanguageViews(); return true; }
		if (id == "lang-es") { LoaderLocale::Set(LoaderLocale::Lang::EsLatam); SyncLanguageViews(); return true; }

		if (id == "nav-addons") { g_activeTab = Tab::Addons; SyncTabs(); return true; }
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
