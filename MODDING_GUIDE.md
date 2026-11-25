# Guia Completo: Criando Mods (`/mods`) e Add-ons (`UNI2-IM\Addons`)

Este guia ensina como criar, testar e distribuir **File Mods** (substituição de arquivos do jogo, em pasta ou `.zip` direto igual ao BBTAG) e **Add-ons** (Lua 5.4, plugins nativos C++ `.dll`, módulos opcionais e pacotes de assets) no novo sistema modular do **UNI2 Improvement Mod**.

---

## 1. Qual formato devo usar?

| Tipo | Onde fica | Formato aceito | Para que serve |
|------|-----------|----------------|----------------|
| **File Mod** | `<jogo>\mods\` ou `UNI2-IM\Packs\` | **Pasta** ou **`.zip` direto** (sem precisar extrair!) | Trocar arquivos do jogo: stages, retratos, sprites, sons, músicas, textos, `grpdat/`, `data/`, etc. |
| **Add-on Lua** | `UNI2-IM\Addons\<nome>\` | `addon.ini` + scripts `.lua` | Lógica de gameplay/online/UI sem compilar DLL (ex.: **Rank Wars**), janelas no F1, HUD na luta, requisições HTTP, leitura de memória. |
| **Add-on Nativo (C++)** | `UNI2-IM\Addons\<nome>\` | `addon.ini` + `.dll` 32-bit (Win32) | Sistemas complexos com renderização própria D3D9/RmlUI ou hooks nativos (ex.: **Great Lobby**). |
| **Add-on de Módulo / Assets** | `UNI2-IM\Addons\<nome>\` | `addon.ini` + `BasePals/`, `Patches/`, `StageIcons/`, `Shaders/` | Ativar módulos opcionais do IM e fornecer assets externos fora do `dinput8.dll`. |

> **Drag & Drop inteligente no `UNI2ModLoader.exe`**: você pode arrastar qualquer pasta ou `.zip` diretamente para a janela do Mod Loader. Se houver `addon.ini` dentro, ele instala automaticamente em `UNI2-IM\Addons\`; se não houver, ele instala automaticamente como File Mod em `<jogo>\mods\` (mantendo `.zip` fechado igual ao BBTAG!).

---

## 2. Como criar um File Mod (`/mods` — Pasta ou `.zip` direto)

O UNI2 Improvement Mod lê mods automaticamente de **`<Pasta do UNI2>\mods\`** e também de **`<Pasta do UNI2>\UNI2-IM\Packs\`**, aceitando tanto **pastas soltas** quanto **arquivos `.zip` fechados** (exatamente como no `BBTAG-Improvement-Mod`).

### Estrutura do Mod (em pasta ou dentro de `MeuMod.zip`)

```text
MeuMod.zip  (ou  <jogo>\mods\MeuMod\)
├── mod.ini                  (opcional, mas recomendado para nome/autor/prioridade)
└── grpdat\                  (espelhando o caminho interno do jogo)
    ├── CSel\
    │   └── stage_thumb01.dds
    └── ...
```

*(Se o seu `.zip` tiver uma única pasta raiz como `MeuMod/grpdat/...`, o mod loader detecta e ajusta o nível automaticamente — não precisa se preocupar com pasta dupla!)*

### Exemplo de `mod.ini` (opcional)

```ini
[Mod]
Id = meu-mod-visual
Name = Meu Mod Visual
Version = 1.0.0
Author = SeuNome
Description = Substitui texturas e icones do jogo.
```

Campos opcionais de regras de ordem em `[Mod]`:
- `Requires = outro-mod-id`
- `Conflicts = mod-incompativel`
- `Above = mod-que-fica-abaixo`
- `Below = mod-que-fica-acima`

### Como instalar e testar
1. **Direto no `/mods` (igual BBTAG)**: jogue `MeuMod.zip` ou a pasta `MeuMod\` dentro de `<Pasta do UNI2>\mods\`.
2. **Pelo `UNI2ModLoader.exe`**: abra a aba **FILE MODS**, clique em **INSTALL MOD ZIP** / **INSTALL MOD FOLDER** ou apenas arraste o `.zip` para dentro da janela.
3. Ative/desative pelo switch e ajuste a prioridade com **UP / DN** (mods mais altos têm prioridade quando dois mods alteram o mesmo arquivo).
4. **Performance**: na primeira vez que um `.zip` novo ou modificado é detectado, o `dinput8.dll` extrai para o cache interno (`UNI2-IM\Generated\ZipMods\`) com carimbo de data/tamanho — nas próximas aberturas do jogo o carregamento é instantâneo e não re-extrai nada.

---

## 3. Como criar um Add-on em Lua 5.4

Add-ons em Lua não precisam de compilador, não disparam falso-positivo de antivírus e podem ser **recarregados com o jogo aberto** (**F1 → Mod → Reload Lua add-ons**).

### Passo 1: Crie a pasta e o manifesto `addon.ini`

Crie `<Pasta do UNI2>\UNI2-IM\Addons\MeuAddon\addon.ini`:

```ini
[Addon]
Id = meu-addon
Name = Meu Add-on
Version = 1.0.0
Author = SeuNome
Description = Exemplo de add-on em Lua com janela no F1 e HUD na luta.
Entry = main.lua
Url = https://github.com/SeuUsuario/UNI2-MeuAddon
```

### Passo 2: Crie o script `main.lua`

```lua
local state = {
    matches_played = 0,
    show_bar = true,
}

-- 1. Registra uma janela no menu F1 (aba Mod)
uni2.ui.register_window("meu_addon", "Meu Add-on", {
    summary = "Contador simples de partidas e indicador no HUD.",
    width = 360,
    height = 220,
    min_width = 300,
    min_height = 180,
    open = false,
}, function()
    uni2.ui.text("Partidas jogadas nesta sessao: " .. tostring(state.matches_played))
    uni2.ui.separator()

    local changed, val = uni2.ui.checkbox("Mostrar indicador no HUD", state.show_bar)
    if changed then
        state.show_bar = val
        uni2.config.set_int("MeuAddon", "ShowBar", val and 1 or 0)
    end

    if uni2.ui.button("Testar Toast", uni2.ui.scaled(160), 0) then
        uni2.toast("success", "MEU ADD-ON", "Funcionando 100% em Lua 5.4!")
    end
end)

-- 2. Carrega configuracao salva
state.show_bar = uni2.config.get_int("MeuAddon", "ShowBar", 1) ~= 0

-- 3. Eventos do jogo
uni2.on("frame", function()
    -- Roda 1x por frame
end)

uni2.on("hud", function()
    -- Roda na camada nativa de HUD da batalha
    if state.show_bar and uni2.game.in_match() then
        uni2.draw.fill(610, 44, 60, 6, 0xFF10B981, 287)
    end
end)

uni2.on("game_started", function()
    uni2.log("Partida iniciada!")
end)

uni2.on("game_ended", function(report)
    state.matches_played = state.matches_played + 1
    uni2.toast("info", "MEU ADD-ON", "Partida #" .. tostring(state.matches_played) .. " encerrada (" .. report.ending .. ")")
end)

uni2.log("Meu Add-on carregado!")
```

### Referência rápida da API `uni2` (Lua 5.4)

| Namespace | Funções principais |
|-----------|--------------------|
| **Core** | `uni2.log(msg)`, `uni2.warn(msg)`, `uni2.error(msg)`, `uni2.time()`, `uni2.clock()`, `uni2.version()`, `uni2.language()`, `uni2.tr(text)`, `uni2.toast(kind, title, text)`, `uni2.notify(text)` |
| **Eventos** | `uni2.on("frame" \| "hud" \| "game_started" \| "game_ended" \| "shutdown", fn)` |
| **Config** | `uni2.config.get_string`, `get_int`, `set_string`, `set_int`, `command_line` |
| **Arquivos** | `uni2.fs.read`, `write`, `exists`, `mkdir`, `addon_dir`, `mod_root` |
| **Memória** | `uni2.mem.rva`, `sig`, `read_i32`, `read_u32`, `read_u16`, `read_u8`, `write_i32`, `read_rva_i32`, `write_rva_i32` |
| **Jogo & Sala** | `uni2.game.scene`, `scene_frames`, `in_match`, `is_training`, `is_online`, `is_spectating`, `local_side`, `character`, `character_name`, `claim_pad`, `enter_network`, `leave_training`, `can_join_room`, `join_room`, `room_create_*` |
| **Steam** | `uni2.steam.is_ready`, `own_id`, `lobby`, `lobby_owner`, `lobby_limit`, `launch_command_line`, `launch_query`, `set_rich_presence_note` |
| **Replays** | `uni2.replay.folder`, `can_play`, `is_loading_patch`, `refresh`, `save_record`, `describe_header`, `matchup_of`, `request_playback` |
| **HTTP & JSON** | `uni2.http.request({ method, url, headers, body }, cb)` *(assíncrono em thread dedicada)*, `json_encode`, `json_decode` |
| **Desenho Nativo** | `uni2.draw.fill`, `frame`, `text` |
| **UI (ImGui)** | `uni2.ui.register_window`, `set_open`, `is_open`, `text`, `text_wrapped`, `text_muted`, `text_warn`, `text_good`, `button`, `small_button`, `checkbox`, `input_text`, `slider_int`, `collapsing_header`, `begin_child_card`, `end_child_card`, `set_tooltip`, `set_clipboard`, `draw_*` |

> **Referência real completa**: veja o projeto standalone do Rank Wars ([addons/RankWars](addons/RankWars/)).

---

## 4. Como criar um Add-on Nativo em C++ (`.dll` 32-bit)

Indicado para plugins que precisam de renderização própria em D3D9 (como **RmlUi**) ou hooks diretos em código nativo sem criar um fork do `dinput8.dll`.

### Manifesto (`addon.ini`)
```ini
[Addon]
Id = meu-plugin
Name = Meu Plugin Nativo
Version = 1.0.0
Author = SeuNome
Description = Plugin C++ com renderizacao D3D9 propria.
Native = MeuPlugin.dll
```

### Código mínimo (`MeuPlugin.cpp` — Win32 / x86)

Inclua [src/Addons/AddonApi.h](src/Addons/AddonApi.h) (`UNI2_ADDON_API_VERSION = 2`) e exporte apenas duas funções C:

```cpp
#include "AddonApi.h"
#include <d3d9.h>

static const Uni2AddonHost* g_host = nullptr;

static void __stdcall OnFrame(void*) {
}

static void __stdcall OnD3D9Render(void*) {
    auto* device = static_cast<IDirect3DDevice9*>(g_host->getD3D9Device());
    if (!device) return;
}

extern "C" __declspec(dllexport) bool __stdcall UNI2_AddonInit(const Uni2AddonHost* host, const char* addonDir) {
    if (!host || host->apiVersion < 2u) return false;
    g_host = host;

    host->log(UNI2_LOG_INFO, "MeuPlugin iniciado!");
    host->registerFrameCallback(&OnFrame, nullptr);
    host->registerRenderCallback(&OnD3D9Render, nullptr);
    return true;
}

extern "C" __declspec(dllexport) void __stdcall UNI2_AddonShutdown() {
    g_host = nullptr;
}
```

O `Uni2AddonHost` v2 entrega tudo sem duplicar hooks:
- `getD3D9Device()`, `getGameWindow()`, `getImGuiContext()`
- `registerRenderCallback` (passagem dedicada dentro de `BeginScene`/`EndScene`, abaixo dos toasts/overlay do IM — ideal para RmlUi)
- `registerDeviceCallbacks` (`onLost` / `onReset`), `registerWndProcCallback`, `setOverlayBlocksInput`
- `createHook`, `createApiHook`, `setHookEnabled` (usa o **MinHook compartilhado** do Improvement Mod — zero conflito!)
- `getScene`, `isInMatch`, `isOnline`, `isSpectating`, `getLocalSide`, `getLoadedCharacter`, `getCharacterName`, `registerGameEvents`
- `getLocalSteamId`, `setRichPresenceNote`, `getReplayFolder`, `saveReplayRecord`, `configGet*` / `configSet*`

> **Exemplo pronto e compilável**: veja o SDK do Great Lobby em [addons/GreatLobby/sdk/](addons/GreatLobby/sdk/) e o guia [addons/GreatLobby/YUKO_SETUP.md](addons/GreatLobby/YUKO_SETUP.md).

---

## 5. Como criar um Add-on de Módulo ou Asset Pack

Módulos pesados/opcionais do Improvement Mod ficam desligados na base limpa e podem ser ativados automaticamente por um add-on via `Modules = ...` e `Assets = ...`:

```ini
[Addon]
Id = meu-pack-stages
Name = Stages & Stage Icons
Version = 1.0.0
Author = SeuNome
Description = Ativa o modulo de Stages e fornece o stage-icons.pack.
Modules = Stages
Assets = StageIcons
```

**Módulos suportados em `Modules =`** (separados por vírgula):
`Stages`, `Graphics`, `GamePatches`, `Music`, `Subtitles`, `VoicesAndSound`, `CharacterArt`.

**Pastas de assets reconhecidas automaticamente em add-ons ativados**:
- `BasePals\chrNNN*.png` → folhas PNG para o editor e thumbnails de palettes
- `Patches\*.pack` → pacotes de dados de versões anteriores (`GamePatches`)
- `StageIcons\stage-icons.pack` → cards de stages importados (`Stages`)
- `Shaders\*` → shaders de pós-processamento (`Graphics`)

Quando o add-on é desativado no `UNI2ModLoader.exe`, o módulo volta a ficar desligado e seus assets não são carregados.

---

## 6. Como publicar e atualizar pelo `UNI2ModLoader.exe`

### Opção A — Repositório no GitHub (com auto-update!)
1. Coloque `addon.ini` e os arquivos do seu add-on na raiz do repositório GitHub.
2. Publique um `.zip` na aba **Releases** (ou deixe apenas o branch `main` — o Mod Loader também sabe baixar o zipball automaticamente!).
3. Qualquer jogador pode colar `https://github.com/SeuUsuario/SeuAddon` no rodapé da aba **ADD-ONS** do `UNI2ModLoader.exe` e clicar em **DOWNLOAD & INSTALL**.
4. Sempre que você publicar uma nova Release no GitHub, basta o jogador clicar em **UPDATE LINKED**.

### Opção B — Bundle único no Google Drive (vários add-ons em 1 `.zip`)
1. Coloque várias pastas de add-ons (cada uma com seu `addon.ini`) dentro de um único `.zip` (ex.: `UNI2-IM-Addons-Bundle.zip`).
2. Suba o `.zip` no Google Drive com permissão **"Qualquer pessoa com o link"**.
3. O jogador cola o link do Drive no campo de URL do `UNI2ModLoader.exe` e clica em **DOWNLOAD & INSTALL** — o Mod Loader baixa automaticamente (inclusive lidando com o aviso de arquivo grande do Drive) e instala todos os add-ons do bundle em `UNI2-IM\Addons\` de uma só vez!
