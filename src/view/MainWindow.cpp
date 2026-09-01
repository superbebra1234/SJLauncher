#define _CRT_SECURE_NO_WARNINGS
#include "MainWindow.h"
#include <filesystem>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <sstream>
#include <iomanip>
#include "../util/Downloader.h"
#include "../util/Launcher.h"
#include "../util/Settings.h"

namespace fs = std::filesystem;

// ─── Иконки FA дополнительные ────────────────────────────────────────────────
#define ICON_FA_JAVA        "\xef\x9d\xbe"
#define ICON_FA_SLIDERS     "\xef\x87\x9e"
#define ICON_FA_PAINT       "\xef\x87\xbc"
#define ICON_FA_WINDOW      "\xef\x84\x91"
#define ICON_FA_SHIELD      "\xef\x84\xa9"
#define ICON_FA_FOLDER      "\xef\x81\xbb"
#define ICON_FA_INFO        "\xef\x81\x9a"
#define ICON_FA_TOGGLE_ON   "\xef\x88\x85"
#define ICON_FA_TOGGLE_OFF  "\xef\x88\x84"
#define ICON_FA_SAVE        "\xef\x83\x87"

// ─── Цвета Modrinth ───────────────────────────────────────────────────────────
static const ImVec4 COL_GREEN      = ImVec4(0.11f, 0.83f, 0.44f, 1.0f);
static const ImVec4 COL_GREEN_DIM  = ImVec4(0.11f, 0.35f, 0.25f, 1.0f);
static const ImVec4 COL_BG_MAIN    = ImVec4(0.09f, 0.10f, 0.11f, 1.0f);
static const ImVec4 COL_BG_CARD    = ImVec4(0.13f, 0.14f, 0.15f, 1.0f);
static const ImVec4 COL_BG_FRAME   = ImVec4(0.20f, 0.21f, 0.22f, 1.0f);
static const ImVec4 COL_TEXT_DIM   = ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
static const ImVec4 COL_RED        = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
static const ImVec4 COL_YELLOW     = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
static const ImVec4 COL_BLACK      = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

// ─── Глобальные буферы UI ────────────────────────────────────────────────────
static bool s_settingsBuffersInit = false;
static char s_nameBuf[128]        = "";
static char s_javaBuf[512]        = "";
static char s_javaArgsBuf[512]    = "";
static char s_instDirBuf[512]     = "";
static char s_proxyHostBuf[256]   = "";
static char s_proxyPortBuf[16]    = "";
static char s_proxyUserBuf[128]   = "";
static char s_proxyPassBuf[128]   = "";

// Буфер Create instance
static char newInstanceName[128] = "";

// Текущая вкладка настроек
static int s_settingsTab = 0;

// ─── Буферы окна "Instance settings" (per-instance overrides, PolyMC-style) ─────
static std::string editingInstanceOriginalName;
static char s_instNameBuf[128]       = "";
static char s_instNotesBuf[1024]     = "";
static char s_instJavaPathBuf[512]   = "";
static char s_instJavaArgsBuf[512]   = "";
static bool s_instOverrideJava       = false;
static int  s_instMemStrategy        = 0;
static int  s_instRamMb              = 2048;
static int  s_instMemPercent         = 50;
static int  s_instGroupIdx           = 0; // 0 = Ungrouped, 1..N = существующие группы, N+1 = "+ New group"
static char s_instNewGroupBuf[128]   = "";
static std::vector<std::string> s_instGroupOptions;

// ─── Подтверждение удаления ──────────────────────────────────────────────────
static std::string pendingDeleteInstance;

// ─── Helpers ─────────────────────────────────────────────────────────────────

// Рисует Toggle-кнопку в стиле Modrinth (зелёный/серый)
static bool DrawToggle(const char* id, bool& val) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,
        val ? ImVec4(0.11f,0.83f,0.44f,1.0f) : ImVec4(0.30f,0.30f,0.32f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        val ? ImVec4(0.15f,0.90f,0.50f,1.0f) : ImVec4(0.36f,0.36f,0.38f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        val ? ImVec4(0.09f,0.70f,0.38f,1.0f) : ImVec4(0.25f,0.25f,0.27f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
    std::string label = std::string(val ? ICON_FA_TOGGLE_ON : ICON_FA_TOGGLE_OFF) + "##" + id;
    bool clicked = ImGui::Button(label.c_str(), ImVec2(46, 24));
    if (clicked) val = !val;
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    return clicked;
}

// Строка настройки: label слева, виджет справа
static void SettingRow(const char* label, const char* hint = nullptr) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
    ImGui::Text("%s", label);
    if (hint && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", hint);
    }
    ImGui::TableSetColumnIndex(1);
}

// Разделитель секции
static void SectionHeader(const char* text) {
    ImGui::Spacing();
    ImGui::TextColored(COL_GREEN, "%s", text);
    ImGui::Separator();
    ImGui::Spacing();
}

// Кнопка-вкладка сайдбара настроек
static bool SettingsTabButton(const char* icon, const char* label, int idx, int& cur) {
    bool sel = (cur == idx);
    ImGui::PushStyleColor(ImGuiCol_Button,
        sel ? COL_GREEN_DIM : ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text,
        sel ? COL_GREEN : COL_TEXT_DIM);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    std::string lbl = std::string(icon) + "  " + label + "##stab" + std::to_string(idx);
    bool clicked = ImGui::Button(lbl.c_str(), ImVec2(-1, 34));
    if (clicked) cur = idx;
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    return clicked;
}

// ─── Вкладка Java ─────────────────────────────────────────────────────────────
static void DrawSettingsJava() {
    SectionHeader("Java Runtime");

    if (ImGui::BeginTable("##java_tbl", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("widget", ImGuiTableColumnFlags_WidthStretch);

        // Auto-detect Java
        SettingRow("Auto-detect Java", "Launcher finds Java automatically");
        DrawToggle("auto_java", currentSettings.autoDetectJava);

        // Java path
        SettingRow("Java executable path", "Leave empty to auto-detect");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##java_path", s_javaBuf, sizeof(s_javaBuf)))
            currentSettings.javaPath = s_javaBuf;
        ImGui::PopItemWidth();

        // RAM
        SettingRow("Java memory", "RAM allocated to Minecraft");
        // выбор Fixed / Percent
        {
            bool fixed = (currentSettings.memStrategy == MemoryStrategy::Fixed);
            if (ImGui::RadioButton("Fixed (MB)", fixed))
                currentSettings.memStrategy = MemoryStrategy::Fixed;
            ImGui::SameLine();
            if (ImGui::RadioButton("Percentage", !fixed))
                currentSettings.memStrategy = MemoryStrategy::Percent;
        }

        SettingRow("", nullptr);
        if (currentSettings.memStrategy == MemoryStrategy::Fixed) {
            ImGui::PushItemWidth(-1);
            ImGui::SliderInt("##ram_slider", &currentSettings.ramMb, 512, 32768, "%d MB");
            ImGui::PopItemWidth();
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
            ImGui::Text("  Recommended: 2048 MB – 8192 MB");
            ImGui::PopStyleColor();
        } else {
            ImGui::PushItemWidth(-1);
            ImGui::SliderInt("##ram_pct", &currentSettings.memPercent, 10, 90, "%d%%");
            ImGui::PopItemWidth();
            ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
            ImGui::Text("  ~%d MB of available RAM", currentSettings.memPercent * 80); // placeholder
            ImGui::PopStyleColor();
        }

        // JVM args
        SettingRow("Extra JVM arguments", "Additional flags passed to Java");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##java_args", s_javaArgsBuf, sizeof(s_javaArgsBuf)))
            currentSettings.javaArgs = s_javaArgsBuf;
        ImGui::PopItemWidth();

        ImGui::EndTable();
    }
}

// ─── Вкладка Launcher ─────────────────────────────────────────────────────────
static void DrawSettingsLauncher() {
    SectionHeader("Player");
    if (ImGui::BeginTable("##lnch_tbl1", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("widget", ImGuiTableColumnFlags_WidthStretch);

        SettingRow("Username (Offline)", "Used in offline/cracked mode");
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##player_name", s_nameBuf, sizeof(s_nameBuf)))
            currentSettings.playerName = s_nameBuf;
        ImGui::PopItemWidth();

        ImGui::EndTable();
    }

    SectionHeader("Behaviour");
    if (ImGui::BeginTable("##lnch_tbl2", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("widget", ImGuiTableColumnFlags_WidthStretch);

        SettingRow("Keep launcher open after game launch", nullptr);
        DrawToggle("keep_open", currentSettings.keepLauncherOpen);

        SettingRow("Check for launcher updates", nullptr);
        DrawToggle("check_upd", currentSettings.checkUpdates);

        SettingRow("Send anonymous analytics", "Helps improve the launcher");
        DrawToggle("analytics", currentSettings.sendAnalytics);

        ImGui::EndTable();
    }

    SectionHeader("Version Filters");
    if (ImGui::BeginTable("##lnch_tbl3", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("widget", ImGuiTableColumnFlags_WidthStretch);

        SettingRow("Show snapshots", "Alpha/beta/snapshot versions in picker");
        DrawToggle("snapshots", currentSettings.showSnapshots);

        SettingRow("Show old beta versions", nullptr);
        DrawToggle("old_beta", currentSettings.showOldBeta);

        SettingRow("Show old alpha versions", nullptr);
        DrawToggle("old_alpha", currentSettings.showOldAlpha);

        ImGui::EndTable();
    }

    SectionHeader("Folders");
    if (ImGui::BeginTable("##lnch_tbl4", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("widget", ImGuiTableColumnFlags_WidthStretch);

        SettingRow("Instances directory", "Where your instances are stored");
        ImGui::PushItemWidth(-60);
        if (ImGui::InputText("##inst_dir", s_instDirBuf, sizeof(s_instDirBuf)))
            currentSettings.instancesDir = s_instDirBuf;
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, COL_BG_FRAME);
        if (ImGui::Button(ICON_FA_FOLDER "##browse", ImVec2(48, 0))) {
            // TODO: open folder browser dialog
        }
        ImGui::PopStyleColor();

        ImGui::EndTable();
    }
}

// ─── Вкладка Appearance ───────────────────────────────────────────────────────
static void DrawSettingsAppearance() {
    SectionHeader("Theme");
    if (ImGui::BeginTable("##app_tbl", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("widget", ImGuiTableColumnFlags_WidthStretch);

        SettingRow("Color theme", nullptr);
        {
            int th = (int)currentSettings.theme;
            if (ImGui::RadioButton("Dark##th", th == 0)) currentSettings.theme = AppTheme::Dark;
            ImGui::SameLine();
            if (ImGui::RadioButton("Light##th", th == 1)) currentSettings.theme = AppTheme::Light;
            ImGui::SameLine();
            if (ImGui::RadioButton("System##th", th == 2)) currentSettings.theme = AppTheme::System;
        }

        SettingRow("Compact instance cards", "Smaller instance list rows");
        DrawToggle("compact", currentSettings.compactInstanceCards);

        SettingRow("Show instance notes", nullptr);
        DrawToggle("notes", currentSettings.showInstanceNotes);

        ImGui::EndTable();
    }
}

// ─── Вкладка Window ───────────────────────────────────────────────────────────
static void DrawSettingsWindow() {
    SectionHeader("Window Size");
    if (ImGui::BeginTable("##win_tbl", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("widget", ImGuiTableColumnFlags_WidthStretch);

        SettingRow("Custom window size", nullptr);
        DrawToggle("custom_size", currentSettings.customWindowSize);

        if (currentSettings.customWindowSize) {
            SettingRow("Width", nullptr);
            ImGui::PushItemWidth(-1);
            ImGui::InputInt("##win_w", &currentSettings.windowWidth);
            ImGui::PopItemWidth();

            SettingRow("Height", nullptr);
            ImGui::PushItemWidth(-1);
            ImGui::InputInt("##win_h", &currentSettings.windowHeight);
            ImGui::PopItemWidth();
        }

        SettingRow("Start maximized", nullptr);
        DrawToggle("maximized", currentSettings.maximized);

        ImGui::EndTable();
    }
}

// ─── Вкладка Proxy ────────────────────────────────────────────────────────────
static void DrawSettingsProxy() {
    SectionHeader("Network Proxy");
    if (ImGui::BeginTable("##proxy_tbl", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 260.0f);
        ImGui::TableSetupColumn("widget", ImGuiTableColumnFlags_WidthStretch);

        SettingRow("Enable proxy", nullptr);
        DrawToggle("use_proxy", currentSettings.useProxy);

        if (currentSettings.useProxy) {
            SettingRow("Proxy host", nullptr);
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##proxy_host", s_proxyHostBuf, sizeof(s_proxyHostBuf)))
                currentSettings.proxyHost = s_proxyHostBuf;
            ImGui::PopItemWidth();

            SettingRow("Port", nullptr);
            ImGui::PushItemWidth(100);
            if (ImGui::InputText("##proxy_port", s_proxyPortBuf, sizeof(s_proxyPortBuf),
                                 ImGuiInputTextFlags_CharsDecimal))
                currentSettings.proxyPort = atoi(s_proxyPortBuf);
            ImGui::PopItemWidth();

            SettingRow("Username (optional)", nullptr);
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##proxy_user", s_proxyUserBuf, sizeof(s_proxyUserBuf)))
                currentSettings.proxyUser = s_proxyUserBuf;
            ImGui::PopItemWidth();

            SettingRow("Password (optional)", nullptr);
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##proxy_pass", s_proxyPassBuf, sizeof(s_proxyPassBuf),
                                 ImGuiInputTextFlags_Password))
                currentSettings.proxyPass = s_proxyPassBuf;
            ImGui::PopItemWidth();
        }

        ImGui::EndTable();
    }
}

// ─── Полное окно настроек (Modrinth-style с левым сайдбаром) ─────────────────
static void DrawSettingsWindow_Full(bool& open, const ImGuiIO& io) {
    if (open) ImGui::OpenPopup("##SettingsModal");

    ImGui::SetNextWindowSize(ImVec2(820, 560), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(
        ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.09f, 0.10f, 0.98f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);

    if (ImGui::BeginPopupModal("##SettingsModal", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {

        // ── Заголовок ──────────────────────────────────────────────────────
        ImGui::SetWindowFontScale(1.3f);
        ImGui::Text(ICON_FA_COG "  Settings");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine(ImGui::GetWindowWidth() - 44);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        if (ImGui::Button(ICON_FA_TIMES "##cls", ImVec2(32, 32))) {
            SaveSettings();
            open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::Separator();
        ImGui::Spacing();

        // ── Левый сайдбар ──────────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.12f, 0.13f, 1.0f));
        ImGui::BeginChild("##settings_nav", ImVec2(180, -52), true);

        ImGui::TextColored(COL_TEXT_DIM, "SETTINGS");
        ImGui::Spacing();
        SettingsTabButton(ICON_FA_JAVA,   "Java",       0, s_settingsTab);
        SettingsTabButton(ICON_FA_SLIDERS,"Launcher",   1, s_settingsTab);
        SettingsTabButton(ICON_FA_PAINT,  "Appearance", 2, s_settingsTab);
        SettingsTabButton(ICON_FA_WINDOW, "Window",     3, s_settingsTab);
        SettingsTabButton(ICON_FA_SHIELD, "Proxy",      4, s_settingsTab);

        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // ── Правая панель ──────────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.11f, 0.12f, 1.0f));
        ImGui::BeginChild("##settings_content", ImVec2(0, -52), true);

        switch (s_settingsTab) {
            case 0: DrawSettingsJava();       break;
            case 1: DrawSettingsLauncher();   break;
            case 2: DrawSettingsAppearance(); break;
            case 3: DrawSettingsWindow();     break;
            case 4: DrawSettingsProxy();      break;
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();

        // ── Нижняя панель ─────────────────────────────────────────────────
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 230);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f,0.21f,0.22f,1.0f));
        if (ImGui::Button("Discard changes", ImVec2(110, 32))) {
            LoadSettings();
            // сброс буферов
            s_settingsBuffersInit = false;
            open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, COL_GREEN);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f,0.90f,0.50f,1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, COL_BLACK);
        if (ImGui::Button(ICON_FA_SAVE "  Save", ImVec2(100, 32))) {
            SaveSettings();
            open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ─── Инициализация буферов из currentSettings ────────────────────────────────
static void InitSettingsBuffers() {
    strncpy(s_nameBuf,      currentSettings.playerName.c_str(),  sizeof(s_nameBuf)-1);
    strncpy(s_javaBuf,      currentSettings.javaPath.c_str(),    sizeof(s_javaBuf)-1);
    strncpy(s_javaArgsBuf,  currentSettings.javaArgs.c_str(),    sizeof(s_javaArgsBuf)-1);
    strncpy(s_instDirBuf,   currentSettings.instancesDir.c_str(),sizeof(s_instDirBuf)-1);
    strncpy(s_proxyHostBuf, currentSettings.proxyHost.c_str(),   sizeof(s_proxyHostBuf)-1);
    snprintf(s_proxyPortBuf, sizeof(s_proxyPortBuf), "%d", currentSettings.proxyPort);
    strncpy(s_proxyUserBuf, currentSettings.proxyUser.c_str(),   sizeof(s_proxyUserBuf)-1);
    strncpy(s_proxyPassBuf, currentSettings.proxyPass.c_str(),   sizeof(s_proxyPassBuf)-1);
    s_settingsBuffersInit = true;
}

// ─── SetupFonts ───────────────────────────────────────────────────────────────
void SetupFonts(ImGuiIO& io) {
    ImFontConfig fcfg;
    if (fs::exists("C:\\Windows\\Fonts\\segoeui.ttf")) {
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &fcfg,
                                     io.Fonts->GetGlyphRangesCyrillic());
    } else {
        io.Fonts->AddFontDefault();
    }
    ImFontConfig icfg;
    icfg.MergeMode = true; icfg.PixelSnapH = true; icfg.GlyphMinAdvanceX = 18.0f;
    static const ImWchar icR[] = { 0xe000, 0xf8ff, 0 };
    fs::path font1 = "assets/fonts/fa-solid-900.ttf";
    if (fs::exists(font1) && fs::file_size(font1) > 100000)
        io.Fonts->AddFontFromFileTTF(font1.string().c_str(), 16.0f, &icfg, icR);
}

// ─── SetupModrinthStyle ───────────────────────────────────────────────────────
void SetupModrinthStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.FrameRounding  = 6.0f;
    style.ChildRounding  = 8.0f;
    style.PopupRounding  = 10.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding   = 4.0f;
    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]      = COL_BG_MAIN;
    c[ImGuiCol_ChildBg]       = COL_BG_CARD;
    c[ImGuiCol_FrameBg]       = COL_BG_FRAME;
    c[ImGuiCol_PopupBg]       = ImVec4(0.16f, 0.17f, 0.18f, 1.0f);
    c[ImGuiCol_Text]          = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    c[ImGuiCol_TextDisabled]  = COL_TEXT_DIM;
    c[ImGuiCol_Button]        = COL_BG_FRAME;
    c[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.26f, 0.27f, 1.0f);
    c[ImGuiCol_ButtonActive]  = ImVec4(0.15f, 0.16f, 0.17f, 1.0f);
    c[ImGuiCol_Header]        = COL_GREEN_DIM;
    c[ImGuiCol_HeaderHovered] = ImVec4(0.13f, 0.42f, 0.28f, 1.0f);
    c[ImGuiCol_HeaderActive]  = ImVec4(0.09f, 0.30f, 0.20f, 1.0f);
    c[ImGuiCol_PlotHistogram] = COL_GREEN;
    c[ImGuiCol_CheckMark]     = COL_GREEN;
    c[ImGuiCol_SliderGrab]    = COL_GREEN;
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.09f, 0.70f, 0.38f, 1.0f);
    c[ImGuiCol_FrameBgHovered]= ImVec4(0.24f, 0.25f, 0.26f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.29f, 0.30f, 1.0f);
    c[ImGuiCol_Tab]           = COL_BG_FRAME;
    c[ImGuiCol_TabHovered]    = COL_GREEN_DIM;
    c[ImGuiCol_TabActive]     = COL_GREEN_DIM;
    c[ImGuiCol_Separator]     = ImVec4(0.20f, 0.21f, 0.22f, 1.0f);
    c[ImGuiCol_ScrollbarBg]   = ImVec4(0.08f, 0.09f, 0.10f, 1.0f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.31f, 0.32f, 1.0f);
}

// ─── SelectableButton ────────────────────────────────────────────────────────
void DrawSelectableButton(const char* label, int index, int& current) {
    bool sel = (current == index);
    ImGui::PushStyleColor(ImGuiCol_Button,
        sel ? ImVec4(0.11f,0.35f,0.25f,1.0f) : COL_BG_FRAME);
    ImGui::PushStyleColor(ImGuiCol_Text,
        sel ? COL_GREEN : ImVec4(0.7f,0.7f,0.7f,1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, sel ? 1.0f : 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border,
        sel ? COL_GREEN : ImVec4(0,0,0,0));
    std::string text = sel ? (std::string(ICON_FA_CHECK) + " " + label) : label;
    if (ImGui::Button(text.c_str(), ImVec2(0, 32))) current = index;
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
}

// ─── Download Toast ──────────────────────────────────────────────────────────
void DrawDownloadToast(const ImGuiIO& io) {
    if (!isDownloading) return;
    std::lock_guard<std::mutex> lock(uiMutex);

    uint64_t bytesTotal = downloadBytesTotal.load();
    uint64_t bytesDone  = downloadBytesDone.load();
    int totalF = downloadFilesTotal.load();
    int doneF  = downloadFilesDone.load();
    float progressBytes = bytesTotal > 0 ?
        static_cast<float>(bytesDone) / static_cast<float>(bytesTotal) : 0.0f;
    double speedMBps   = downloadSpeedMBps.load();
    int etaSeconds     = downloadETASeconds.load();

    auto fmtBytes = [](uint64_t b) -> std::string {
        std::stringstream ss;
        if      (b < 1024*1024)          ss << std::fixed << std::setprecision(1) << (b/1024.0)           << " КБ";
        else if (b < 1024ULL*1024*1024)  ss << std::fixed << std::setprecision(1) << (b/(1024.0*1024.0))  << " МБ";
        else                             ss << std::fixed << std::setprecision(2) << (b/(1024.0*1024.0*1024.0)) << " ГБ";
        return ss.str();
    };

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 420, io.DisplaySize.y - 160), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400, 140), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, COL_BG_CARD);
    ImGui::Begin("DownloadToast", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushStyleColor(ImGuiCol_Button, COL_BG_FRAME);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::Button("##toast_icon", ImVec2(46, 46));
    ImGui::PopStyleVar(); ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::BeginGroup();

    ImGui::SetWindowFontScale(1.1f);
    ImGui::Text("%s %s", ICON_FA_DOWNLOAD, downloadingInstanceName.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::SameLine(ImGui::GetWindowWidth() - 30);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
    if (ImGui::Button(ICON_FA_TIMES "##cancel", ImVec2(20, 20)))
        cancelDownload = true;
    ImGui::PopStyleColor(2);

    if (totalF > 0) {
        ImGui::TextColored(COL_GREEN, "%d%%", (int)(progressBytes * 100));
        ImGui::SameLine();
        ImGui::TextDisabled("• %.1f МБ/с • ~%d сек", speedMBps, etaSeconds);
        ImGui::TextDisabled("%s / %s", fmtBytes(bytesDone).c_str(), fmtBytes(bytesTotal).c_str());
        if (!downloadingCurrentFile.empty())
            ImGui::TextDisabled("Файл: %s", downloadingCurrentFile.c_str());
        ImGui::TextDisabled("%d / %d файлов", doneF, totalF);
    } else {
        ImGui::TextDisabled("%s", downloadingStageLabel.c_str());
    }
    ImGui::EndGroup();

    ImVec2 pm = ImGui::GetWindowPos();
    float  pw = ImGui::GetWindowWidth(), ph = ImGui::GetWindowHeight();
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(pm.x, pm.y + ph - 4),
        ImVec2(pm.x + pw * progressBytes, pm.y + ph),
        IM_COL32(28,212,112,255), 0.0f, ImDrawFlags_RoundCornersBottom);

    ImGui::End();
    ImGui::PopStyleColor(); ImGui::PopStyleVar();
}

// ─── Получение видимых версий с учётом фильтров ──────────────────────────────
static std::vector<McVersion> GetFilteredVersions() {
    std::vector<McVersion> out;
    for (const auto& v : realMcVersions) {
        if (v.type == "release")  { out.push_back(v); continue; }
        if (v.type == "snapshot" && currentSettings.showSnapshots) { out.push_back(v); continue; }
        if (v.type == "old_beta" && currentSettings.showOldBeta)   { out.push_back(v); continue; }
        if (v.type == "old_alpha" && currentSettings.showOldAlpha)  { out.push_back(v); continue; }
    }
    return out;
}

// Бейджик версии (release/snapshot/old_*)
static void DrawVersionBadge(const std::string& type) {
    ImVec4 col;
    const char* lbl;
    if      (type == "release")  { col = COL_GREEN;  lbl = "release"; }
    else if (type == "snapshot") { col = COL_YELLOW; lbl = "snapshot"; }
    else if (type == "old_beta") { col = ImVec4(0.6f,0.4f,1.0f,1.0f); lbl = "old-beta"; }
    else                          { col = ImVec4(0.7f,0.3f,0.3f,1.0f); lbl = "old-alpha"; }
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::Text("[%s]", lbl);
    ImGui::PopStyleColor();
}

// ─── Инициализация буферов окна Instance settings из выбранного инстанса ─────
static void InitInstanceSettingsBuffers(const MyInstance& inst) {
    editingInstanceOriginalName = inst.name;
    strncpy(s_instNameBuf,     inst.name.c_str(),                sizeof(s_instNameBuf)-1);      s_instNameBuf[sizeof(s_instNameBuf)-1] = 0;
    strncpy(s_instNotesBuf,    inst.notes.c_str(),                sizeof(s_instNotesBuf)-1);     s_instNotesBuf[sizeof(s_instNotesBuf)-1] = 0;
    strncpy(s_instJavaPathBuf, inst.overrides.javaPath.c_str(),   sizeof(s_instJavaPathBuf)-1);  s_instJavaPathBuf[sizeof(s_instJavaPathBuf)-1] = 0;
    strncpy(s_instJavaArgsBuf, inst.overrides.javaArgs.c_str(),   sizeof(s_instJavaArgsBuf)-1);  s_instJavaArgsBuf[sizeof(s_instJavaArgsBuf)-1] = 0;
    s_instOverrideJava = inst.overrides.overrideJava;
    s_instMemStrategy  = inst.overrides.memStrategy;
    s_instRamMb        = inst.overrides.ramMb;
    s_instMemPercent   = inst.overrides.memPercent;

    s_instGroupOptions = GetAllGroupNames();
    s_instGroupIdx = 0;
    for (size_t i = 0; i < s_instGroupOptions.size(); i++)
        if (s_instGroupOptions[i] == inst.group) { s_instGroupIdx = (int)(i + 1); break; }
    memset(s_instNewGroupBuf, 0, sizeof(s_instNewGroupBuf));
}

// ─── Модалка настроек инстанса (аналог Instance Settings из PolyMC) ─────────
static void DrawInstanceSettingsModal(bool& open, bool& openDeleteConfirm) {
    if (open) ImGui::OpenPopup("Instance settings");
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Instance settings", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {

        ImGui::SetWindowFontScale(1.2f);
        ImGui::Text(ICON_FA_COG "  %s", editingInstanceOriginalName.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine(ImGui::GetWindowWidth() - 40);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        if (ImGui::Button(ICON_FA_TIMES "##closeis")) { open = false; ImGui::CloseCurrentPopup(); }
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();

        ImGui::BeginChild("##is_scroll", ImVec2(0, -46));

        ImGui::TextDisabled("Instance name");
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##is_name", s_instNameBuf, sizeof(s_instNameBuf));
        ImGui::PopItemWidth();
        std::string newNamePreview(s_instNameBuf);
        if (newNamePreview.empty())
            ImGui::TextColored(COL_RED, "Name cannot be empty");
        ImGui::Spacing();

        ImGui::TextDisabled("Group");
        ImGui::PushItemWidth(-1);
        {
            std::string preview = (s_instGroupIdx == 0) ? "Ungrouped" :
                (s_instGroupIdx <= (int)s_instGroupOptions.size() ? s_instGroupOptions[s_instGroupIdx-1] : "+ New group...");
            if (ImGui::BeginCombo("##is_group", preview.c_str())) {
                if (ImGui::Selectable("Ungrouped", s_instGroupIdx == 0)) s_instGroupIdx = 0;
                for (int i = 0; i < (int)s_instGroupOptions.size(); i++)
                    if (ImGui::Selectable(s_instGroupOptions[i].c_str(), s_instGroupIdx == i + 1)) s_instGroupIdx = i + 1;
                if (ImGui::Selectable("+ New group...", s_instGroupIdx == (int)s_instGroupOptions.size() + 1))
                    s_instGroupIdx = (int)s_instGroupOptions.size() + 1;
                ImGui::EndCombo();
            }
        }
        ImGui::PopItemWidth();
        if (s_instGroupIdx == (int)s_instGroupOptions.size() + 1) {
            ImGui::PushItemWidth(-1);
            ImGui::InputTextWithHint("##is_new_group", "New group name", s_instNewGroupBuf, sizeof(s_instNewGroupBuf));
            ImGui::PopItemWidth();
        }
        ImGui::Spacing();

        ImGui::TextDisabled("Notes");
        ImGui::PushItemWidth(-1);
        ImGui::InputTextMultiline("##is_notes", s_instNotesBuf, sizeof(s_instNotesBuf), ImVec2(-1, 70));
        ImGui::PopItemWidth();

        SectionHeader("Java (per-instance override)");
        if (ImGui::BeginTable("##is_java_ovr_tbl", 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 260.0f);
            ImGui::TableSetupColumn("widget", ImGuiTableColumnFlags_WidthStretch);
            SettingRow("Override global Java settings", "Use custom Java path/args/memory for this instance only");
            DrawToggle("is_override_java", s_instOverrideJava);
            ImGui::EndTable();
        }

        if (s_instOverrideJava) {
            if (ImGui::BeginTable("##is_java_tbl", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 200.0f);
                ImGui::TableSetupColumn("widget", ImGuiTableColumnFlags_WidthStretch);

                SettingRow("Java executable path", "Leave empty to auto-detect");
                ImGui::PushItemWidth(-1);
                ImGui::InputText("##is_java_path", s_instJavaPathBuf, sizeof(s_instJavaPathBuf));
                ImGui::PopItemWidth();

                SettingRow("Memory", nullptr);
                {
                    bool fixed = (s_instMemStrategy == 0);
                    if (ImGui::RadioButton("Fixed (MB)##is", fixed)) s_instMemStrategy = 0;
                    ImGui::SameLine();
                    if (ImGui::RadioButton("Percentage##is", !fixed)) s_instMemStrategy = 1;
                }
                SettingRow("", nullptr);
                ImGui::PushItemWidth(-1);
                if (s_instMemStrategy == 0)
                    ImGui::SliderInt("##is_ram", &s_instRamMb, 512, 32768, "%d MB");
                else
                    ImGui::SliderInt("##is_ram_pct", &s_instMemPercent, 10, 90, "%d%%");
                ImGui::PopItemWidth();

                SettingRow("Extra JVM arguments", nullptr);
                ImGui::PushItemWidth(-1);
                ImGui::InputText("##is_java_args", s_instJavaArgsBuf, sizeof(s_instJavaArgsBuf));
                ImGui::PopItemWidth();

                ImGui::EndTable();
            }
        }

        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        if (ImGui::Button(ICON_FA_FOLDER "  Open folder", ImVec2(150, 30)))
            OpenInstanceFolder(editingInstanceOriginalName);
        ImGui::SameLine();
        if (ImGui::Button("Duplicate", ImVec2(100, 30))) {
            std::string err;
            DuplicateInstance(editingInstanceOriginalName, editingInstanceOriginalName + " copy", &err);
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f,0.15f,0.15f,1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
        if (ImGui::Button(ICON_FA_TIMES "  Delete instance", ImVec2(160, 30))) {
            pendingDeleteInstance = editingInstanceOriginalName;
            openDeleteConfirm = true;
            open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);

        ImGui::EndChild();

        ImGui::Separator();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 210);
        ImGui::PushStyleColor(ImGuiCol_Button, COL_BG_FRAME);
        if (ImGui::Button("Cancel", ImVec2(90, 32))) { open = false; ImGui::CloseCurrentPopup(); }
        ImGui::PopStyleColor();
        ImGui::SameLine();

        bool canSave = !newNamePreview.empty();
        ImGui::PushStyleColor(ImGuiCol_Button, canSave ? COL_GREEN : ImVec4(0.2f,0.4f,0.2f,0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, canSave ? COL_BLACK : ImVec4(0,0,0,0.6f));
        if (ImGui::Button(ICON_FA_SAVE "  Save", ImVec2(100, 32)) && canSave) {
            std::string finalName = editingInstanceOriginalName;
            if (newNamePreview != editingInstanceOriginalName) {
                std::string err;
                if (RenameInstance(editingInstanceOriginalName, newNamePreview, &err))
                    finalName = newNamePreview;
                // при ошибке (например, имя занято) продолжаем сохранять под старым именем
            }

            SaveInstanceNotes(finalName, s_instNotesBuf);

            InstanceOverrides ov;
            ov.overrideJava = s_instOverrideJava;
            ov.javaPath     = s_instJavaPathBuf;
            ov.javaArgs     = s_instJavaArgsBuf;
            ov.memStrategy  = s_instMemStrategy;
            ov.ramMb        = s_instRamMb;
            ov.memPercent   = s_instMemPercent;
            SaveInstanceOverrides(finalName, ov);

            std::string targetGroup;
            if (s_instGroupIdx == 0) targetGroup = "";
            else if (s_instGroupIdx <= (int)s_instGroupOptions.size()) targetGroup = s_instGroupOptions[s_instGroupIdx - 1];
            else targetGroup = std::string(s_instNewGroupBuf);
            SetInstanceGroup(finalName, targetGroup);

            needReloadInstances = true;
            open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);

        ImGui::EndPopup();
    }
}

// ─── Подтверждение удаления инстанса ─────────────────────────────────────────
static void DrawDeleteConfirmModal(bool& open) {
    if (open) ImGui::OpenPopup("Delete instance?");
    ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Delete instance?", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("Are you sure you want to permanently delete \"%s\"? This cannot be undone.",
            pendingDeleteInstance.c_str());
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 210);
        ImGui::PushStyleColor(ImGuiCol_Button, COL_BG_FRAME);
        if (ImGui::Button("Cancel", ImVec2(90, 32))) { open = false; ImGui::CloseCurrentPopup(); }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, COL_RED);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_BLACK);
        if (ImGui::Button("Delete", ImVec2(100, 32))) {
            DeleteInstance(pendingDeleteInstance);
            open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::EndPopup();
    }
}

// ─── Create Instance Modal ───────────────────────────────────────────────────
static void DrawCreateModal(bool& openCreateModal) {
    static int currentVersionIdx = 0;
    static int currentLoaderIdx  = 0;
    static int currentGroupIdx   = 0; // 0 = No group, 1..N = существующие группы, N+1 = "+ New group"
    static char newGroupBuf[128] = "";
    static const char* loaders[] = { "Vanilla", "Fabric", "NeoForge", "Forge", "Quilt" };

    if (openCreateModal) ImGui::OpenPopup("Create instance");
    ImGui::SetNextWindowSize(ImVec2(480, 500), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("Create instance", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {

        // Заголовок
        ImGui::SetWindowFontScale(1.2f);
        ImGui::Text(ICON_FA_PLUS "  Create instance");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::SameLine(ImGui::GetWindowWidth() - 40);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        if (ImGui::Button(ICON_FA_TIMES "##closeci")) {
            openCreateModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::Separator(); ImGui::Spacing();

        // Name
        ImGui::TextDisabled("Instance name");
        ImGui::PushItemWidth(-1);
        ImGui::InputText("##name", newInstanceName, sizeof(newInstanceName));
        ImGui::PopItemWidth();

        std::string nameStr(newInstanceName);
        bool nameOk = !nameStr.empty();
        if (nameOk) {
            for (char c : nameStr)
                if ((unsigned char)c > 127 || !(isalnum(c)||c==' '||c=='_'||c=='-'))
                    { nameOk = false; break; }
        }
        if (!nameOk && !nameStr.empty())
            ImGui::TextColored(COL_RED, "Only A-Z, 0-9, space, - and _");
        else if (nameStr.empty())
            ImGui::TextColored(COL_TEXT_DIM, "Name cannot be empty");
        else
            ImGui::Spacing();
        ImGui::Spacing();

        // Loader
        ImGui::TextDisabled("Mod loader");
        for (int i = 0; i < 5; i++) {
            if (i > 0 && i != 3) ImGui::SameLine();
            DrawSelectableButton(loaders[i], i, currentLoaderIdx);
        }
        ImGui::Spacing(); ImGui::Spacing();

        // Group
        ImGui::TextDisabled("Group (optional)");
        auto groupsList = GetAllGroupNames();
        if (currentGroupIdx > (int)groupsList.size() + 1) currentGroupIdx = 0;
        {
            std::string preview = (currentGroupIdx == 0) ? "No group" :
                (currentGroupIdx <= (int)groupsList.size() ? groupsList[currentGroupIdx - 1] : "+ New group...");
            ImGui::PushItemWidth(-1);
            if (ImGui::BeginCombo("##create_group", preview.c_str())) {
                if (ImGui::Selectable("No group", currentGroupIdx == 0)) currentGroupIdx = 0;
                for (int i = 0; i < (int)groupsList.size(); i++)
                    if (ImGui::Selectable(groupsList[i].c_str(), currentGroupIdx == i + 1)) currentGroupIdx = i + 1;
                if (ImGui::Selectable("+ New group...", currentGroupIdx == (int)groupsList.size() + 1))
                    currentGroupIdx = (int)groupsList.size() + 1;
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            if (currentGroupIdx == (int)groupsList.size() + 1) {
                ImGui::PushItemWidth(-1);
                ImGui::InputTextWithHint("##create_new_group", "New group name", newGroupBuf, sizeof(newGroupBuf));
                ImGui::PopItemWidth();
            }
        }
        ImGui::Spacing(); ImGui::Spacing();

        // Version picker
        ImGui::TextDisabled("Game version");

        // Фильтр прямо в диалоге — маленькие тоглы
        ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
        ImGui::Text("Show:");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        auto miniToggle = [](const char* id, const char* lbl, bool& val, ImVec4 col) {
            ImGui::PushStyleColor(ImGuiCol_Button,
                val ? ImVec4(col.x*0.4f,col.y*0.4f,col.z*0.4f,1.0f) : ImVec4(0.2f,0.2f,0.2f,1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, val ? col : COL_TEXT_DIM);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            if (ImGui::Button((std::string(lbl) + "##" + id).c_str(), ImVec2(0, 22)))
                val = !val;
            ImGui::PopStyleVar(); ImGui::PopStyleColor(2);
        };
        miniToggle("sn", "Snapshots", currentSettings.showSnapshots, COL_YELLOW);
        ImGui::SameLine();
        miniToggle("ob", "Old Beta",  currentSettings.showOldBeta,  ImVec4(0.6f,0.4f,1.0f,1.0f));
        ImGui::SameLine();
        miniToggle("oa", "Old Alpha", currentSettings.showOldAlpha, ImVec4(0.7f,0.3f,0.3f,1.0f));

        ImGui::PushItemWidth(-1);
        auto filtered = GetFilteredVersions();
        if (filtered.empty()) {
            if (ImGui::BeginCombo("##ver", realMcVersions.empty() ? "Loading..." : "No versions match filter"))
                ImGui::EndCombo();
        } else {
            if (currentVersionIdx >= (int)filtered.size()) currentVersionIdx = 0;
            const char* preview = filtered[currentVersionIdx].id.c_str();
            if (ImGui::BeginCombo("##ver", preview)) {
                for (int i = 0; i < (int)filtered.size(); i++) {
                    bool sel = (currentVersionIdx == i);
                    std::string lbl = filtered[i].id;
                    if (ImGui::Selectable(("##v" + std::to_string(i)).c_str(), sel,
                                          0, ImVec2(0,0))) {
                        currentVersionIdx = i;
                    }
                    ImGui::SameLine();
                    DrawVersionBadge(filtered[i].type);
                    ImGui::SameLine();
                    ImGui::Text("%s", filtered[i].id.c_str());
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        ImGui::PopItemWidth();

        // Нижняя панель
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 60);
        ImGui::Separator(); ImGui::Spacing();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 260);

        ImGui::PushStyleColor(ImGuiCol_Button, COL_BG_FRAME);
        if (ImGui::Button("<- Back", ImVec2(90, 36))) {
            openCreateModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();

        bool canCreate = nameOk && !filtered.empty() && !isDownloading;
        ImGui::PushStyleColor(ImGuiCol_Button,
            canCreate ? COL_GREEN : ImVec4(0.2f,0.4f,0.2f,0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            canCreate ? COL_BLACK : ImVec4(0,0,0,0.6f));
        if (ImGui::Button(ICON_FA_PLUS " Create instance", ImVec2(160, 36)) && canCreate) {
            std::string vId  = filtered[currentVersionIdx].id;
            std::string vUrl = filtered[currentVersionIdx].url;
            std::string groupToUse;
            if (currentGroupIdx == 0) groupToUse = "";
            else if (currentGroupIdx <= (int)groupsList.size()) groupToUse = groupsList[currentGroupIdx - 1];
            else groupToUse = std::string(newGroupBuf);
            std::thread(InstallInstanceThread,
                std::string(newInstanceName), vId, vUrl, loaders[currentLoaderIdx], groupToUse).detach();
            openCreateModal = false;
            currentGroupIdx = 0;
            memset(newGroupBuf, 0, sizeof(newGroupBuf));
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);

        ImGui::EndPopup();
    }
}

// ─── RenderMainWindow ─────────────────────────────────────────────────────────
void RenderMainWindow(GLFWwindow* window, ImGuiIO& io) {
    static int  activeTab                  = 0;
    static bool openCreateModal            = false;
    static bool openSettingsModal          = false;
    static bool openInstanceSettingsModal  = false;
    static bool openDeleteConfirm          = false;

    if (!s_settingsBuffersInit) InitSettingsBuffers();
    if (needReloadInstances) { LoadMyInstances(); needReloadInstances = false; }

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    // ── Sidebar ───────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.08f, 0.09f, 1.0f));
    ImGui::BeginChild("Sidebar", ImVec2(65, 0), true);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 25.0f);
    const char* icons[] = { ICON_FA_HOUSE, ICON_FA_COMPASS, ICON_FA_SHIRT, ICON_FA_BOOKS, ICON_FA_SERVER };
    for (int i = 0; i < 5; i++) {
        ImGui::Spacing();
        bool sel = (activeTab == i);
        ImGui::PushStyleColor(ImGuiCol_Button,
            sel ? ImVec4(0.12f,0.30f,0.20f,1.0f) : ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_Text,
            sel ? COL_GREEN : ImVec4(0.5f,0.5f,0.5f,1.0f));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-45)/2);
        if (ImGui::Button((std::string(icons[i])+"##"+std::to_string(i)).c_str(), ImVec2(45,45)))
            activeTab = i;
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
    // + Add instance
    ImGui::SetCursorPosX((ImGui::GetWindowWidth()-45)/2);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f,0.7f,0.7f,1.0f));
    if (ImGui::Button(ICON_FA_PLUS "##add", ImVec2(45,45))) {
        openCreateModal = true;
        memset(newInstanceName, 0, sizeof(newInstanceName));
    }
    ImGui::PopStyleColor(2);

    // Settings — прибит ко дну
    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 120);
    ImGui::SetCursorPosX((ImGui::GetWindowWidth()-45)/2);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text,
        openSettingsModal ? COL_GREEN : ImVec4(0.6f,0.6f,0.6f,1.0f));
    if (ImGui::Button(ICON_FA_COG "##settings", ImVec2(45,45))) {
        openSettingsModal = true;
        InitSettingsBuffers();
        s_settingsTab = 0;
    }
    ImGui::PopStyleColor(2);

    // Login
    ImGui::SetCursorPosX((ImGui::GetWindowWidth()-45)/2);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, COL_GREEN);
    ImGui::Button(ICON_FA_SIGN_IN "##login", ImVec2(45,45));
    ImGui::PopStyleColor(2);

    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::SameLine();

    // ── MainArea ──────────────────────────────────────────────────────────
    ImGui::BeginChild("MainArea", ImVec2(ImGui::GetContentRegionAvail().x - 300, 0), false);
    ImGui::TextDisabled("<- -> Home");
    ImGui::Separator(); ImGui::Spacing();
    ImGui::SetWindowFontScale(1.8f);
    ImGui::Text("Welcome back, %s!", currentSettings.playerName.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::TextDisabled("Jump back in");
    ImGui::Spacing(); ImGui::Spacing();

    if (myInstancesList.empty()) {
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.35f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        ImGui::SetWindowFontScale(4.0f);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(ICON_FA_CUBE).x)/2);
        ImGui::Text("%s", ICON_FA_CUBE);
        ImGui::SetWindowFontScale(1.2f);
        float tw = ImGui::CalcTextSize("You have no instances yet").x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - tw)/2);
        ImGui::Text("You have no instances yet");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleVar();
        ImGui::Spacing(); ImGui::Spacing(); ImGui::Spacing();
        ImGui::SetCursorPosX((ImGui::GetWindowWidth()-220)/2);
        ImGui::PushStyleColor(ImGuiCol_Button, COL_GREEN);
        ImGui::PushStyleColor(ImGuiCol_Text, COL_BLACK);
        if (ImGui::Button(ICON_FA_PLUS " Create Instance", ImVec2(220, 45))) {
            openCreateModal = true;
            memset(newInstanceName, 0, sizeof(newInstanceName));
        }
        ImGui::PopStyleColor(2);
    } else {
        int cardH = currentSettings.compactInstanceCards ? 55 : 70;

        // Группируем видимые инстансы по группам, в порядке из instgroups.json;
        // инстансы без группы ("Ungrouped") всегда показываются последними.
        std::vector<std::string> groupOrder = GetAllGroupNames();
        bool hasUngrouped = false;
        for (auto& inst : myInstancesList) if (inst.group.empty()) { hasUngrouped = true; break; }
        if (hasUngrouped) groupOrder.push_back("");

        for (const auto& grp : groupOrder) {
            std::vector<size_t> idxs;
            for (size_t i = 0; i < myInstancesList.size(); i++)
                if (myInstancesList[i].group == grp) idxs.push_back(i);
            if (idxs.empty()) continue;

            if (!grp.empty() || groupOrder.size() > 1) {
                ImGui::PushStyleColor(ImGuiCol_Text, COL_TEXT_DIM);
                ImGui::Text("%s", grp.empty() ? "UNGROUPED" : grp.c_str());
                ImGui::PopStyleColor();
                ImGui::Separator();
                ImGui::Spacing();
            }

            for (size_t i : idxs) {
                auto& inst = myInstancesList[i];
                std::string id = "inst_" + std::to_string(i);
                bool downloading = (inst.status == "downloading");

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f,0.17f,0.18f,1.0f));
                ImGui::BeginChild(id.c_str(), ImVec2(0, downloading ? 85 : cardH), true);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.4f,0.2f,1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                ImGui::Button((std::string(ICON_FA_CUBE)+"##img"+id).c_str(), ImVec2(46,46));
                ImGui::PopStyleVar(); ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
                ImGui::SetWindowFontScale(1.1f);
                ImGui::Text("%s", inst.name.c_str());
                ImGui::SetWindowFontScale(1.0f);
                ImGui::TextDisabled("%s %s", ICON_FA_USER, inst.mc_version.c_str());
                ImGui::EndGroup();

                if (downloading) {
                    ImGui::SetCursorPosX(60);
                    float p = downloadFilesTotal > 0 ?
                        (float)downloadFilesDone / (float)downloadFilesTotal.load() : 0.0f;
                    ImGui::ProgressBar(p, ImVec2(-60.0f, 5.0f), " ");
                } else {
                    ImGui::SameLine(ImGui::GetWindowWidth() - 320);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (cardH==55 ? 10 : 20));
                    ImGui::TextDisabled("%s %s", ICON_FA_HAMMER, inst.mod_loader.c_str());
                    ImGui::SameLine(ImGui::GetWindowWidth() - 140);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (cardH==55 ? 2 : 10));
                    ImGui::PushStyleColor(ImGuiCol_Button, COL_GREEN);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f,0.90f,0.50f,1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, COL_BLACK);
                    if (ImGui::Button((std::string(ICON_FA_PLAY)+" Play##"+id).c_str(), ImVec2(80,36)))
                        LaunchGame(inst);
                    ImGui::PopStyleColor(3);
                    ImGui::SameLine();

                    std::string ctxId = "ctx_" + id;
                    if (ImGui::Button((std::string(ICON_FA_ELLIPSIS_V)+"##opt"+id).c_str(), ImVec2(36,36)))
                        ImGui::OpenPopup(ctxId.c_str());

                    if (ImGui::BeginPopup(ctxId.c_str())) {
                        if (ImGui::MenuItem(ICON_FA_COG "  Edit / Instance settings")) {
                            InitInstanceSettingsBuffers(inst);
                            openInstanceSettingsModal = true;
                        }
                        if (ImGui::MenuItem(ICON_FA_FOLDER "  Open folder"))
                            OpenInstanceFolder(inst.name);
                        if (ImGui::MenuItem("Duplicate")) {
                            std::string err;
                            DuplicateInstance(inst.name, inst.name + " copy", &err);
                        }
                        ImGui::Separator();
                        if (ImGui::BeginMenu("Change group")) {
                            if (ImGui::MenuItem("Ungrouped", nullptr, inst.group.empty()))
                                SetInstanceGroup(inst.name, "");
                            for (const auto& g : GetAllGroupNames())
                                if (ImGui::MenuItem(g.c_str(), nullptr, inst.group == g))
                                    SetInstanceGroup(inst.name, g);
                            ImGui::EndMenu();
                        }
                        ImGui::Separator();
                        ImGui::PushStyleColor(ImGuiCol_Text, COL_RED);
                        if (ImGui::MenuItem("Delete")) {
                            pendingDeleteInstance = inst.name;
                            openDeleteConfirm = true;
                        }
                        ImGui::PopStyleColor();
                        ImGui::EndPopup();
                    }
                }
                ImGui::EndChild(); ImGui::PopStyleColor();
                ImGui::Spacing();
            }
            ImGui::Spacing();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ── RightPanel ────────────────────────────────────────────────────────
    ImGui::BeginChild("RightPanel", ImVec2(290, 0), true);
    ImGui::TextDisabled("Playing as");
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f,0.17f,0.18f,1.0f));
    ImGui::BeginChild("AuthCard", ImVec2(0, 105), true);
    ImGui::SetWindowFontScale(1.1f);
    ImGui::Text("%s", currentSettings.playerName.c_str());
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, COL_GREEN);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f,0.90f,0.50f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, COL_BLACK);
    ImGui::Button((std::string(ICON_FA_SIGN_IN)+" Sign in to Minecraft").c_str(), ImVec2(-1,38));
    ImGui::PopStyleColor(3);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::EndChild();

    // ── Оверлеи ───────────────────────────────────────────────────────────
    DrawDownloadToast(io);

    ImGui::End();

    // ── Модалки (рисуются вне Begin/End главного окна) ────────────────────
    DrawCreateModal(openCreateModal);
    DrawSettingsWindow_Full(openSettingsModal, io);
    DrawInstanceSettingsModal(openInstanceSettingsModal, openDeleteConfirm);
    DrawDeleteConfirmModal(openDeleteConfirm);
}
