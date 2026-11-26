#include "ModLoader/LoaderLocale.h"

#include "ModLoader/LoaderConfig.h"

#include <Windows.h>

#include <cstring>

namespace {

struct Entry
{
	const char* key;
	const char* en;
	const char* pt;
	const char* es;
};

constexpr Entry kStrings[] = {
	{ "app_title", "UNI2 MOD LOADER", "UNI2 MOD LOADER", "UNI2 MOD LOADER" },
	{ "brand_sub", "IMPROVEMENT MOD", "IMPROVEMENT MOD", "IMPROVEMENT MOD" },
	{ "status_no_game", "NO GAME FOLDER", "PASTA NÃO ENCONTRADA", "SIN CARPETA DEL JUEGO" },
	{ "status_ready", "MOD READY", "MOD PRONTO", "MOD LISTO" },
	{ "status_missing", "DINPUT8.DLL MISSING", "FALTA DINPUT8.DLL", "FALTA DINPUT8.DLL" },
	{ "btn_install_proxy", "INSTALL dinput8.dll", "INSTALAR dinput8.dll", "INSTALAR dinput8.dll" },
	{ "btn_play_steam", "PLAY UNI2 (STEAM)", "JOGAR UNI2 (STEAM)", "JUGAR UNI2 (STEAM)" },
	{ "btn_play_direct", "RUN EXE", "ABRIR EXE", "EJECUTAR EXE" },
	{ "label_game_folder", "GAME FOLDER", "PASTA DO JOGO", "CARPETA DEL JUEGO" },
	{ "game_missing", "Game folder not found - click Browse to select the uni2.exe folder", "Pasta do jogo não encontrada - clique em Procurar para selecionar a pasta do uni2.exe", "Carpeta del juego no encontrada - haz clic en Buscar para seleccionar la carpeta de uni2.exe" },
	{ "btn_browse", "BROWSE...", "PROCURAR...", "BUSCAR..." },
	{ "nav_section", "MANAGEMENT", "GERENCIAMENTO", "GESTIÓN" },
	{ "nav_addons", "ADD-ONS", "ADD-ONS", "ADD-ONS" },
	{ "nav_packs", "MODS (/MODS)", "MODS (/MODS)", "MODS (/MODS)" },
	{ "nav_modules", "IM", "IM", "IM" },
	{ "nav_log", "ACTIVITY LOG", "ATIVIDADES", "REGISTRO" },
	{ "drop_title", "DRAG & DROP READY", "ARRASTE E SOLTE", "ARRASTRAR Y SOLTAR" },
	{ "drop_text", "Drop any add-on .zip, bundle, or file mod directly onto this window.", "Arraste qualquer add-on .zip, bundle ou mod de arquivo direto nesta janela.", "Arrastra cualquier add-on .zip, paquete o mod de archivo aquí." },
	{ "addons_title", "ADD-ONS", "ADD-ONS", "ADD-ONS" },
	{ "addons_sub", "Installed & available add-ons in UNI2-IM\\Addons", "Add-ons instalados e disponíveis em UNI2-IM\\Addons", "Add-ons instalados y disponibles en UNI2-IM\\Addons" },
	{ "addons_section_installed", "INSTALLED", "INSTALADOS", "INSTALADOS" },
	{ "addons_section_available", "AVAILABLE TO INSTALL", "DISPONÍVEIS PARA INSTALAR", "DISPONIBLES PARA INSTALAR" },
	{ "addons_catalog_loading", "Checking catalog...", "Verificando catálogo...", "Consultando catálogo..." },
	{ "btn_addon_zip", "INSTALL .ZIP", "INSTALAR .ZIP", "INSTALAR .ZIP" },
	{ "btn_addon_folder", "INSTALL FOLDER", "INSTALAR PASTA", "INSTALAR CARPETA" },
	{ "btn_open_folder", "OPEN FOLDER", "ABRIR PASTA", "ABRIR CARPETA" },
	{ "btn_update_linked", "UPDATE ADD-ONS", "ATUALIZAR ADD-ONS", "ACTUALIZAR ADD-ONS" },
	{ "btn_refresh", "RELOAD", "RECARREGAR", "RECARGAR" },
	{ "btn_download_install", "DOWNLOAD & INSTALL", "BAIXAR E INSTALAR", "DESCARGAR E INSTALAR" },
	{ "btn_remove", "REMOVE", "REMOVER", "ELIMINAR" },
	{ "addons_empty_title", "NO ADD-ONS INSTALLED YET", "NENHUM ADD-ON INSTALADO", "NO HAY ADD-ONS INSTALADOS" },
	{ "addons_empty_text", "Install from Available below, drop a .zip here, or paste a GitHub / Google Drive link.", "Instale pelos Disponíveis abaixo, arraste um .zip aqui ou cole um link do GitHub / Google Drive.", "Instala desde Disponibles abajo, arrastra un .zip aquí o pega un enlace de GitHub / Google Drive." },
	{ "detail_empty_title", "NO ADD-ON SELECTED", "NENHUM ADD-ON SELECIONADO", "NINGÚN ADD-ON SELECCIONADO" },
	{ "detail_empty_text", "Select an add-on on the left to inspect details, update, or install in one click.", "Selecione um add-on à esquerda para ver detalhes, atualizar ou instalar em 1 clique.", "Selecciona un add-on a la izquierda para ver detalles, actualizar o instalar en 1 clic." },
	{ "detail_no_desc", "No description provided in addon.ini.", "Sem descrição informada no addon.ini.", "Sin descripción en addon.ini." },
	{ "detail_by", "by", "por", "por" },
	{ "detail_unknown", "Unknown", "Desconhecido", "Desconocido" },
	{ "detail_lua", "LUA ENTRY", "ENTRADA LUA", "ENTRADA LUA" },
	{ "detail_dll", "NATIVE DLL", "DLL NATIVA", "DLL NATIVA" },
	{ "detail_modules", "MODULES", "MÓDULOS", "MÓDULOS" },
	{ "detail_assets", "ASSETS", "ASSETS", "RECURSOS" },
	{ "detail_source", "SOURCE", "ORIGEM", "ORIGEN" },
	{ "detail_latest", "LATEST", "VERSÃO", "ÚLTIMA" },
	{ "detail_installed", "INSTALLED", "INSTALADO", "INSTALADO" },
	{ "detail_sha256", "SHA-256", "SHA-256", "SHA-256" },
	{ "detail_repo", "REPOSITORY", "REPOSITÓRIO", "REPOSITORIO" },
	{ "source_release", "Official Release", "Release Oficial", "Release Oficial" },
	{ "source_github", "GitHub Release", "Release GitHub", "Release GitHub" },
	{ "btn_download_assets", "DOWNLOAD IM ASSETS", "BAIXAR ASSETS DO IM", "DESCARGAR ASSETS DE IM" },
	{ "btn_install", "INSTALL", "INSTALAR", "INSTALAR" },
	{ "btn_update", "UPDATE", "ATUALIZAR", "ACTUALIZAR" },
	{ "btn_reinstall", "REINSTALL", "REINSTALAR", "REINSTALAR" },
	{ "btn_open_release", "OPEN RELEASE", "ABRIR RELEASE", "ABRIR RELEASE" },
	{ "badge_official", "OFFICIAL", "OFICIAL", "OFICIAL" },
	{ "badge_github", "GITHUB", "GITHUB", "GITHUB" },
	{ "badge_installed", "INSTALLED", "INSTALADO", "INSTALADO" },
	{ "badge_update", "UPDATE", "ATUALIZAR", "ACTUALIZAR" },
	{ "store_no_release", "No published release found yet.", "Nenhuma release publicada encontrada ainda.", "Aún no se encontró ninguna release publicada." },
	{ "packs_title", "MODS (/MODS)", "MODS (/MODS)", "MODS (/MODS)" },
	{ "packs_sub", "Folders or .zip archives in <game>\\mods\\ or UNI2-IM\\Packs\\", "Pastas ou arquivos .zip direto em <jogo>\\mods\\ ou UNI2-IM\\Packs\\", "Carpetas o archivos .zip directo en <juego>\\mods\\ o UNI2-IM\\Packs\\" },
	{ "btn_pack_zip", "INSTALL .ZIP", "INSTALAR .ZIP", "INSTALAR .ZIP" },
	{ "btn_pack_folder", "INSTALL FOLDER", "INSTALAR PASTA", "INSTALAR CARPETA" },
	{ "btn_pack_open", "OPEN /MODS", "ABRIR /MODS", "ABRIR /MODS" },
	{ "packs_empty_title", "NO FILE MODS INSTALLED", "NENHUM MOD DE ARQUIVO INSTALADO", "NO HAY MODS DE ARCHIVO" },
	{ "packs_empty_text", "Drop mod folders or .zip archives into <game>\\mods\\ or UNI2-IM\\Packs\\, or use the buttons above.", "Jogue pastas ou arquivos .zip direto em <jogo>\\mods\\ ou UNI2-IM\\Packs\\, ou use os botões acima.", "Coloca carpetas o archivos .zip directo en <juego>\\mods\\ o UNI2-IM\\Packs\\, o usa los botones de arriba." },
	{ "modules_title", "IM", "IM", "IM" },
	{ "modules_sub", "Toggle core & optional built-in modules directly", "Ative ou desative módulos principais e opcionais diretamente", "Activa o desactiva módulos principales y opcionales directamente" },
	{ "modules_core", "CORE MODULES (CLEAN BASE)", "MÓDULOS PRINCIPAIS (BASE LIMPA)", "MÓDULOS PRINCIPALES (BASE LIMPIA)" },
	{ "modules_opt", "OPTIONAL BUILT-IN MODULES", "MÓDULOS OPCIONAIS INTEGRADOS", "MÓDULOS OPCIONALES INTEGRADOS" },
	{ "modules_opt_sub", "Controlled directly here; IM assets (BasePals, Patches, StageIcons, Shaders) live in UNI2-IM\\Assets", "Controlados diretamente aqui; os assets do IM (BasePals, Patches, StageIcons, Shaders) ficam em UNI2-IM\\Assets", "Controlados aquí; los assets de IM (BasePals, Patches, StageIcons, Shaders) están en UNI2-IM\\Assets" },
	{ "btn_reset_base", "RESET TO CLEAN BASE", "RESTAURAR BASE LIMPA", "RESTABLECER BASE LIMPIA" },
	{ "btn_open_basepals", "OPEN ASSETS FOLDER", "ABRIR PASTA ASSETS", "ABRIR CARPETA ASSETS" },
	{ "auto_update_label", "Auto-check add-on updates when opening Mod Loader", "Verificar atualizações dos add-ons ao abrir o Mod Loader", "Buscar actualizaciones de add-ons al abrir el Mod Loader" },
	{ "game_addon_check_label", "Check add-on updates in background when UNI2 starts", "Verificar atualizações de add-ons em segundo plano ao abrir o UNI2", "Buscar actualizaciones de add-ons en segundo plano al abrir UNI2" },
	{ "log_title", "ACTIVITY LOG", "ATIVIDADES", "REGISTRO DE ACTIVIDAD" },
	{ "log_sub", "Downloads, installs, and loader diagnostics", "Downloads, instalações e diagnósticos do Mod Loader", "Descargas, instalaciones y diagnósticos del cargador" },
	{ "foot_ready", "Ready.", "Pronto.", "Listo." },
	{ "msg_paste_url", "Paste a Google Drive, GitHub, or direct .zip URL first.", "Cole primeiro um link do Google Drive, GitHub ou .zip direto.", "Primero pega un enlace de Google Drive, GitHub o .zip directo." },
	{ "msg_launch_steam", "Launching UNI2 through Steam (zero DLL injection).", "Iniciando UNI2 pela Steam (sem injeção de DLL).", "Iniciando UNI2 desde Steam (sin inyección de DLL)." },
	{ "msg_reset_base", "Modules reset to Clean Improvement Mod base.", "Módulos restaurados para a base limpa do Improvement Mod.", "Módulos restablecidos a la base limpia de Improvement Mod." },
	{ "msg_select_game", "Select your UNDER NIGHT IN-BIRTH II Sys Celes folder (containing uni2.exe).", "Selecione a pasta do UNDER NIGHT IN-BIRTH II Sys Celes (onde fica o uni2.exe).", "Selecciona la carpeta de UNDER NIGHT IN-BIRTH II Sys Celes (donde está uni2.exe)." },
	{ "msg_enabled", " enabled.", " ativado.", " activado." },
	{ "msg_disabled", " disabled.", " desativado.", " desactivado." },
	{ "msg_bootstrap_bundle", "First run detected - downloading IM Assets (BasePals, Patches, StageIcons, Shaders) into UNI2-IM\\Assets...", "Primeira execução detectada - baixando Assets do IM (BasePals, Patches, StageIcons, Shaders) para UNI2-IM\\Assets...", "Primer inicio detectado - descargando Assets de IM (BasePals, Patches, StageIcons, Shaders) en UNI2-IM\\Assets..." },
	{ "url_placeholder", "Paste GitHub, Google Drive, or .zip link (e.g. https://github.com/Zanaylo/UNI2-Rank-Wars-Addon)", "Cole um link do GitHub, Google Drive ou .zip (ex: https://github.com/Zanaylo/UNI2-Rank-Wars-Addon)", "Pega un enlace de GitHub, Google Drive o .zip (ej: https://github.com/Zanaylo/UNI2-Rank-Wars-Addon)" },
	{ "modal_warn_title", "PACKAGE VALIDATION WARNING", "AVISO DE VALIDAÇÃO DO PACOTE", "ADVERTENCIA DE VALIDACIÓN DEL PAQUETE" },
	{ "modal_warn_note", "Review the alerts above before proceeding. Only install if you trust the author and source of this package.", "Revise os alertas acima antes de continuar. Só instale se você confia no autor e na origem deste pacote.", "Revisa las alertas antes de continuar. Instala solo si confías en el autor y origen del paquete." },
	{ "btn_cancel", "CANCEL", "CANCELAR", "CANCELAR" },
	{ "btn_install_anyway", "INSTALL ANYWAY", "INSTALAR MESMO ASSIM", "INSTALAR DE TODOS MODOS" },
};

struct ModuleText
{
	const char* enName;
	const char* enSummary;
	const char* ptName;
	const char* ptSummary;
	const char* esName;
	const char* esSummary;
};

constexpr ModuleText kModules[FeatureCatalog::Id_COUNT] = {
	{ "Music", "Soundpacks, your tracks and the rules for what plays where.",
	  "Música", "Soundpacks, faixas personalizadas e regras de onde cada música toca.",
	  "Música", "Soundpacks, pistas personalizadas y reglas de dónde suena cada tema." },
	{ "Subtitles", "On-screen lines for what each character says.",
	  "Legendas", "Legendas na tela para as falas dos personagens.",
	  "Subtítulos", "Subtítulos en pantalla para las líneas de cada personaje." },
	{ "Privacy", "Hides opponent names, room names and IR.",
	  "Privacidade", "Oculta nomes de oponentes, nomes de salas e IR.",
	  "Privacidad", "Oculta nombres de rivales, nombres de salas e IR." },
	{ "Netcode patches", "Time sync and round trip tuning for online matches.",
	  "Ajustes de Netcode", "Ajustes de sincronia de tempo e round-trip para partidas online.",
	  "Parches de Netcode", "Ajustes de sincronización y latencia para partidas en línea." },
	{ "Netplay window", "Rollback, ping, room info and match history.",
	  "Janela Netplay", "Rollback, ping, informações da sala e histórico de partidas.",
	  "Ventana Netplay", "Rollback, ping, información de sala e historial de partidas." },
	{ "Training", "Hitboxes, frame meter, frame stepping, dummy and training HUD.",
	  "Treino", "Hitboxes, frame meter, avanço frame a frame, dummy e HUD de treino.",
	  "Entrenamiento", "Hitboxes, medidor de frames, paso cuadro a cuadro, dummy y HUD." },
	{ "Palettes", "Custom palettes, colour editor and online palette sharing.",
	  "Palettes", "Palettes customizadas, editor de cores e compartilhamento online.",
	  "Paletas", "Paletas personalizadas, editor de colores y envío en línea." },
	{ "Stages", "Added and hidden stages, stage cards and random stage picker.",
	  "Stages", "Stages adicionados/ocultos, cards e seletor aleatório de stages.",
	  "Escenarios", "Escenarios añadidos/ocultos, tarjetas y selección aleatoria." },
	{ "Replays", "Replay files, Start Replay and saving replays on exit.",
	  "Replays", "Arquivos de replay, iniciar replay e salvar replays ao fechar.",
	  "Repeticiones", "Archivos de repetición, reproducir y guardar al cerrar." },
	{ "Voices and sound", "Voice swaps and sound effect packs.",
	  "Vozes e Sons", "Troca de vozes de personagens e pacotes de efeitos sonoros.",
	  "Voces y Sonido", "Cambio de voces y paquetes de efectos de sonido." },
	{ "Game patches", "Play older balance versions and watch older replays.",
	  "Patches de Versão", "Jogue versões anteriores de balanceamento e assista replays antigos.",
	  "Parches de Versión", "Juega versiones anteriores y mira repeticiones antiguas." },
	{ "Character art", "Alternate character art and MBTL announcers.",
	  "Artes e Narradores", "Artes alternativas de personagens e narradores do MBTL.",
	  "Arte y Narradores", "Arte alternativo de personajes y narradores de MBTL." },
	{ "Mod packs", "File mods in /mods and UNI2-IM\\Packs (folders and .zip).",
	  "Mods de Arquivo", "Mods de arquivo em /mods e UNI2-IM\\Packs (pastas e .zip).",
	  "Paquetes de Mods", "Mods de archivo en /mods y UNI2-IM\\Packs (carpetas y .zip)." },
	{ "Graphics", "Output/stage resolution, shaders, POTATO MODE and frame pacing.",
	  "Gráficos", "Resolução de saída/stage, shaders, POTATO MODE e frame pacing.",
	  "Gráficos", "Resolución de salida/escenario, shaders, POTATO MODE y frame pacing." },
};

LoaderLocale::Lang g_lang = LoaderLocale::Lang::En;

LoaderLocale::Lang ParseCode(const std::string& code)
{
	if (_stricmp(code.c_str(), "pt") == 0 || _stricmp(code.c_str(), "pt-br") == 0 || _stricmp(code.c_str(), "portuguese") == 0)
		return LoaderLocale::Lang::PtBr;

	if (_stricmp(code.c_str(), "es") == 0 || _stricmp(code.c_str(), "es-la") == 0 || _stricmp(code.c_str(), "spanish") == 0)
		return LoaderLocale::Lang::EsLatam;

	return LoaderLocale::Lang::En;
}

LoaderLocale::Lang DetectOsLanguage()
{
	const LANGID langId = GetUserDefaultUILanguage();
	const WORD primary = PRIMARYLANGID(langId);

	if (primary == LANG_PORTUGUESE)
		return LoaderLocale::Lang::PtBr;

	if (primary == LANG_SPANISH)
		return LoaderLocale::Lang::EsLatam;

	return LoaderLocale::Lang::En;
}

bool IsExplicitCode(const std::string& code)
{
	return !code.empty() && _stricmp(code.c_str(), "auto") != 0 && _stricmp(code.c_str(), "default") != 0;
}

}

void LoaderLocale::Initialize(const std::string& gameDir)
{
	const std::string saved = LoaderConfig::ReadLanguage();

	if (IsExplicitCode(saved))
	{
		g_lang = ParseCode(saved);
		return;
	}

	if (LoaderConfig::IsValidGameDir(gameDir))
	{
		const std::string modIni = LoaderConfig::ModRoot(gameDir) + "\\Config\\Mod.ini";
		char buf[32] = {};
		GetPrivateProfileStringA("Mod", "Language", "", buf, sizeof(buf), modIni.c_str());

		if (IsExplicitCode(buf))
		{
			g_lang = ParseCode(buf);
			return;
		}
	}

	g_lang = DetectOsLanguage();
}

LoaderLocale::Lang LoaderLocale::Current()
{
	return g_lang;
}

const char* LoaderLocale::Code(Lang lang)
{
	switch (lang)
	{
	case Lang::PtBr: return "pt";
	case Lang::EsLatam: return "es";
	default: return "en";
	}
}

void LoaderLocale::Set(Lang lang)
{
	g_lang = lang;
	LoaderConfig::SaveLanguage(Code(lang));
}

const char* LoaderLocale::Text(const char* key)
{
	if (key == nullptr)
		return "";

	for (const Entry& e : kStrings)
	{
		if (strcmp(e.key, key) == 0)
		{
			if (g_lang == Lang::PtBr)
				return e.pt;
			if (g_lang == Lang::EsLatam)
				return e.es;
			return e.en;
		}
	}

	return key;
}

const char* LoaderLocale::ModuleName(FeatureCatalog::Id id)
{
	if (id < 0 || id >= FeatureCatalog::Id_COUNT)
		return FeatureCatalog::Get(id).name;

	const ModuleText& m = kModules[id];
	if (g_lang == Lang::PtBr)
		return m.ptName;
	if (g_lang == Lang::EsLatam)
		return m.esName;
	return m.enName;
}

const char* LoaderLocale::ModuleSummary(FeatureCatalog::Id id)
{
	if (id < 0 || id >= FeatureCatalog::Id_COUNT)
		return FeatureCatalog::Get(id).summary;

	const ModuleText& m = kModules[id];
	if (g_lang == Lang::PtBr)
		return m.ptSummary;
	if (g_lang == Lang::EsLatam)
		return m.esSummary;
	return m.enSummary;
}
