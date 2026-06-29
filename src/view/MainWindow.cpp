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

void SetupFonts(ImGuiIO& io) {
    ImFontConfig fcfg;
    if (fs::exists("C:\\Windows\\Fonts\\segoeui.ttf")) {
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &fcfg, io.Fonts->GetGlyphRangesCyrillic());
    } else {
        io.Fonts->AddFontDefault();
    }
    ImFontConfig icfg; 
    icfg.MergeMode = true; 
    icfg.PixelSnapH = true; 
    icfg.GlyphMinAdvanceX = 18.0f; 
    static const ImWchar icR[] = { 0xe000, 0xf8ff, 0 }; 
    fs::path font1 = "assets/fonts/fa-solid-900.ttf";
    if (fs::exists(font1) && fs::file_size(font1) > 100000) {
        io.Fonts->AddFontFromFileTTF(font1.string().c_str(), 16.0f, &icfg, icR);
    }
}

void SetupModrinthStyle() {
    ImGuiStyle& style = ImGui::GetStyle(); 
    style.WindowRounding = 12.0f; 
    style.FrameRounding = 6.0f; 
    style.ChildRounding = 8.0f;
    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]      = ImVec4(0.09f, 0.10f, 0.11f, 1.00f);
    c[ImGuiCol_ChildBg]       = ImVec4(0.13f, 0.14f, 0.15f, 1.00f);
    c[ImGuiCol_FrameBg]       = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
    c[ImGuiCol_PopupBg]       = ImVec4(0.16f, 0.17f, 0.18f, 1.00f);
    c[ImGuiCol_Text]          = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    c[ImGuiCol_TextDisabled]  = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
    c[ImGuiCol_Button]        = ImVec4(0.20f, 0.21f, 0.22f, 1.00f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.26f, 0.27f, 1.00f);
    c[ImGuiCol_ButtonActive]  = ImVec4(0.15f, 0.16f, 0.17f, 1.00f);
    c[ImGuiCol_PlotHistogram] = ImVec4(0.11f, 0.83f, 0.44f, 1.00f);
}

void DrawSelectableButton(const char* label, int index, int& current) {
    bool sel = (current == index);
    ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(0.11f, 0.35f, 0.25f, 1.0f) : ImVec4(0.20f, 0.21f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(0.11f, 0.83f, 0.44f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, sel ? 1.0f : 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Border, sel ? ImVec4(0.11f, 0.83f, 0.44f, 1.0f) : ImVec4(0,0,0,0));
    std::string text = sel ? (std::string(ICON_FA_CHECK) + " " + label) : label;
    if (ImGui::Button(text.c_str(), ImVec2(0, 32))) current = index;
    ImGui::PopStyleColor(3); 
    ImGui::PopStyleVar();
}

void DrawDownloadToast(const ImGuiIO& io) {
    if (!isDownloading) return;
    std::lock_guard<std::mutex> lock(uiMutex);

    int totalF = downloadFilesTotal.load();
    int doneF = downloadFilesDone.load();

    uint64_t bytesTotal = downloadBytesTotal.load();
    uint64_t bytesDone = downloadBytesDone.load();
    float progressBytes = bytesTotal > 0 ? static_cast<float>(bytesDone) / static_cast<float>(bytesTotal) : 0.0f;
    
    float progressFloat = progressBytes;
    
    double speedMBps = downloadSpeedMBps.load();
    int etaSeconds = downloadETASeconds.load();
    
    auto formatBytes = [](uint64_t bytes) -> std::string {
        std::stringstream ss;
        if (bytes < 1024 * 1024) {
            ss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " КБ";
        } else if (bytes < 1024ULL * 1024 * 1024) {
            ss << std::fixed << std::setprecision(1) << (bytes / (1024.0 * 1024.0)) << " МБ";
        } else {
            ss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0 * 1024.0)) << " ГБ";
        }
        return ss.str();
    };

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 420, io.DisplaySize.y - 160), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400, 140), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.13f, 0.14f, 0.15f, 1.0f));
    ImGui::Begin("DownloadToast", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::Button("##toast_icon", ImVec2(46, 46));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::BeginGroup();

    ImGui::SetWindowFontScale(1.1f);
    ImGui::Text("%s %s", ICON_FA_DOWNLOAD, downloadingInstanceName.c_str());
    ImGui::SetWindowFontScale(1.0f);

    ImGui::SameLine(ImGui::GetWindowWidth() - 30);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    if (ImGui::Button(ICON_FA_TIMES "##cancel", ImVec2(20, 20))) {
        cancelDownload = true;
    }
    ImGui::PopStyleColor(2);

    if (totalF > 0) {
        ImGui::TextColored(ImVec4(0.11f, 0.83f, 0.44f, 1.0f), "%d%%", static_cast<int>(progressFloat * 100));
        ImGui::SameLine();
        ImGui::TextDisabled("• %.1f МБ/с • ~%d сек", speedMBps, etaSeconds);
        ImGui::TextDisabled("%s / %s", formatBytes(bytesDone).c_str(), formatBytes(bytesTotal).c_str());
        
        if (!downloadingCurrentFile.empty()) {
            ImGui::TextDisabled("Файл: %s", downloadingCurrentFile.c_str());
        }
        
        ImGui::TextDisabled("%d / %d файлов", doneF, totalF);
    } else {
        ImGui::TextDisabled("%s", downloadingStageLabel.c_str());
    }

    ImGui::EndGroup();

    ImVec2 p_min = ImGui::GetWindowPos();
    ImVec2 p_max = ImVec2(p_min.x + (ImGui::GetWindowWidth() * progressFloat), p_min.y + ImGui::GetWindowHeight());
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(p_min.x, p_max.y - 4),
        ImVec2(p_max.x, p_max.y),
        IM_COL32(28, 212, 112, 255),
        0.0f,
        ImDrawFlags_RoundCornersBottom
    );

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void RenderMainWindow(GLFWwindow* window, ImGuiIO& io) {
    static int activeTab = 0;
    static bool openCreateModal = false;
    static bool openSettingsModal = false;
    static char newInstanceName[128] = ""; 
    static int currentVersionIdx = 0, currentLoaderIdx = 0;
    static const char* loaders[] = { "Vanilla", "Fabric", "NeoForge", "Forge", "Quilt" };

    if (needReloadInstances) { LoadMyInstances(); needReloadInstances = false; }

    ImGui::SetNextWindowPos(ImVec2(0, 0)); 
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    // Sidebar
    ImGui::BeginChild("Sidebar", ImVec2(65, 0), true); 
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 25.0f); 
    const char* icons[] = { ICON_FA_HOUSE, ICON_FA_COMPASS, ICON_FA_SHIRT, ICON_FA_BOOKS, ICON_FA_SERVER };
    for (int i = 0; i < 5; i++) {
        ImGui::Spacing(); 
        bool sel = (activeTab == i);
        ImGui::PushStyleColor(ImGuiCol_Button, sel ? ImVec4(0.12f, 0.30f, 0.20f, 1.0f) : ImVec4(0,0,0,0)); 
        ImGui::PushStyleColor(ImGuiCol_Text, sel ? ImVec4(0.11f, 0.83f, 0.44f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 45) / 2); 
        if (ImGui::Button((std::string(icons[i]) + "##" + std::to_string(i)).c_str(), ImVec2(45, 45))) 
            activeTab = i;
        ImGui::PopStyleColor(2);
    }
    ImGui::Spacing(); 
    ImGui::Separator(); 
    ImGui::Spacing(); 
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 45) / 2);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0)); 
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
    if (ImGui::Button(ICON_FA_PLUS "##add", ImVec2(45, 45))) { 
        openCreateModal = true; 
        memset(newInstanceName, 0, sizeof(newInstanceName)); 
    }

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 110); 
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 45) / 2); 
    if (ImGui::Button(ICON_FA_COG "##settings", ImVec2(45, 45))) {
        openSettingsModal = true;
    }

    ImGui::PopStyleColor(2);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0)); 
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.11f, 0.83f, 0.44f, 1.0f));
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 45) / 2); 
    ImGui::Button(ICON_FA_SIGN_IN "##login", ImVec2(45, 45)); 
    ImGui::PopStyleColor(2); 
    ImGui::PopStyleVar(); 
    ImGui::EndChild();
    ImGui::SameLine();

    // MainArea
    ImGui::BeginChild("MainArea", ImVec2(ImGui::GetContentRegionAvail().x - 300, 0), false);
    ImGui::TextDisabled("<- -> Home"); 
    ImGui::Separator(); 
    ImGui::Spacing(); 
    ImGui::SetWindowFontScale(1.8f); 
    ImGui::Text("Welcome back, %s!", currentSettings.playerName.c_str()); 
    ImGui::SetWindowFontScale(1.0f); 
    ImGui::TextDisabled("Jump back in"); 
    ImGui::Spacing(); 
    ImGui::Spacing();

    if (myInstancesList.empty()) {
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.35f); 
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);  
        ImGui::SetWindowFontScale(4.0f);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize(ICON_FA_CUBE).x) / 2); 
        ImGui::Text("%s", ICON_FA_CUBE); 
        ImGui::SetWindowFontScale(1.2f);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("У вас пока нет сборок").x) / 2); 
        ImGui::Text("У вас пока нет сборок"); 
        ImGui::SetWindowFontScale(1.0f); 
        ImGui::PopStyleVar();
        ImGui::Spacing(); 
        ImGui::Spacing(); 
        ImGui::Spacing(); 
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 220) / 2);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.11f, 0.83f, 0.44f, 1.0f)); 
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,1));
        if (ImGui::Button(ICON_FA_PLUS " Create Instance", ImVec2(220, 45))) { 
            openCreateModal = true; 
            memset(newInstanceName, 0, sizeof(newInstanceName)); 
        }
        ImGui::PopStyleColor(2);
    } else {
        for (size_t i = 0; i < myInstancesList.size(); i++) { 
            auto& inst = myInstancesList[i];
            std::string id = "inst_" + std::to_string(i); 
            bool downloading = (inst.status == "downloading");
            
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.17f, 0.18f, 1.00f));
            ImGui::BeginChild(id.c_str(), ImVec2(0, downloading ? 85 : 70), true);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.2f, 1.0f)); 
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::Button((std::string(ICON_FA_CUBE) + "##img" + id).c_str(), ImVec2(50, 50));
            ImGui::PopStyleVar(); 
            ImGui::PopStyleColor();
            ImGui::SameLine(); 
            ImGui::BeginGroup(); 
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5); 
            ImGui::SetWindowFontScale(1.1f);
            ImGui::Text("%s", inst.name.c_str()); 
            ImGui::SetWindowFontScale(1.0f); 
            ImGui::TextDisabled("%s %s", ICON_FA_USER, inst.mc_version.c_str()); 
            ImGui::EndGroup();
            
            if (downloading) {
                ImGui::SetCursorPosX(60); 
                float progress = downloadFilesTotal > 0 ? static_cast<float>(downloadFilesDone) / static_cast<float>(downloadFilesTotal) : 0.0f;
                ImGui::ProgressBar(progress, ImVec2(-60.0f, 5.0f), " ");
            } else {
                ImGui::SameLine(ImGui::GetWindowWidth() - 320); 
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20); 
                ImGui::TextDisabled("%s %s", ICON_FA_HAMMER, inst.mod_loader.c_str());
                ImGui::SameLine(ImGui::GetWindowWidth() - 140); 
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 10);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.11f, 0.83f, 0.44f, 1.00f)); 
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.90f, 0.50f, 1.00f)); 
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
                
                if (ImGui::Button((std::string(ICON_FA_PLAY) + " Play##" + id).c_str(), ImVec2(80, 36))) {
                    LaunchGame(inst);
                }
                
                ImGui::PopStyleColor(3); 
                ImGui::SameLine(); 
                ImGui::Button((std::string(ICON_FA_ELLIPSIS_V) + "##opt" + id).c_str(), ImVec2(36, 36));
            }
            ImGui::EndChild(); 
            ImGui::PopStyleColor(); 
            ImGui::Spacing();
        }
    }
    ImGui::EndChild(); 
    ImGui::SameLine();

    // RightPanel
    ImGui::BeginChild("RightPanel", ImVec2(290, 0), true); 
    ImGui::TextDisabled("Playing as"); 
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.17f, 0.18f, 1.0f)); 
    ImGui::BeginChild("AuthCard", ImVec2(0, 105), true);
    ImGui::SetWindowFontScale(1.1f); 
    ImGui::Text("%s", currentSettings.playerName.c_str()); 
    ImGui::SetWindowFontScale(1.0f); 
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.11f, 0.83f, 0.44f, 1.0f)); 
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.90f, 0.50f, 1.0f)); 
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1.0f)); 
    ImGui::Button((std::string(ICON_FA_SIGN_IN) + " Sign in to Minecraft").c_str(), ImVec2(-1, 38)); 
    ImGui::PopStyleColor(3); 
    ImGui::EndChild(); 
    ImGui::PopStyleColor();
    ImGui::EndChild();

    DrawDownloadToast(io);

    // Окно настроек
    if (openSettingsModal) ImGui::OpenPopup("Settings");
    ImGui::SetNextWindowSize(ImVec2(500, 350), ImGuiCond_Appearing); 
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.09f, 0.95f)); 
    if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::Text("Настройки лаунчера"); 
        ImGui::SameLine(ImGui::GetWindowWidth() - 40); 
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        if (ImGui::Button(ICON_FA_TIMES "##close_settings")) { 
            SaveSettings(); 
            openSettingsModal = false; 
            ImGui::CloseCurrentPopup(); 
        } 
        ImGui::PopStyleColor(); 
        ImGui::Separator(); 
        ImGui::Spacing();
        
        ImGui::TextDisabled("Никнейм (Оффлайн)"); 
        static char nameBuf[64] = "";
        if (nameBuf[0] == '\0' && !currentSettings.playerName.empty()) {
            strcpy(nameBuf, currentSettings.playerName.c_str());
        }
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##player_name", nameBuf, IM_ARRAYSIZE(nameBuf))) {
            currentSettings.playerName = nameBuf;
        }
        ImGui::PopItemWidth();
        ImGui::Spacing(); 
        ImGui::Spacing();

        ImGui::TextDisabled("Выделение оперативной памяти (MB)");
        ImGui::PushItemWidth(-1);
        ImGui::SliderInt("##ram_slider", &currentSettings.ramMb, 1024, 16384, "%d MB");
        ImGui::PopItemWidth();
        ImGui::Spacing(); 
        ImGui::Spacing();

        ImGui::TextDisabled("Путь к Java (оставьте пустым для автопоиска)"); 
        static char javaBuf[256] = "";
        if (javaBuf[0] == '\0' && !currentSettings.javaPath.empty()) {
            strcpy(javaBuf, currentSettings.javaPath.c_str());
        }
        ImGui::PushItemWidth(-1);
        if (ImGui::InputText("##java_path", javaBuf, IM_ARRAYSIZE(javaBuf))) {
            currentSettings.javaPath = javaBuf;
        }
        ImGui::PopItemWidth();

        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 60); 
        ImGui::Separator(); 
        ImGui::Spacing(); 
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 170);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.11f, 0.83f, 0.44f, 1.0f)); 
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,1));
        if (ImGui::Button(ICON_FA_CHECK " Сохранить", ImVec2(150, 36))) {
            SaveSettings();
            openSettingsModal = false; 
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(); 

    // Окно создания сборки
    if (openCreateModal) ImGui::OpenPopup("Create instance");
    ImGui::SetNextWindowSize(ImVec2(450, 450), ImGuiCond_Appearing); 
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Create instance", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::Text("Create instance"); 
        ImGui::SameLine(ImGui::GetWindowWidth() - 40); 
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        if (ImGui::Button(ICON_FA_TIMES "##close")) { 
            openCreateModal = false; 
            ImGui::CloseCurrentPopup(); 
        } 
        ImGui::PopStyleColor(); 
        ImGui::Separator(); 
        ImGui::Spacing();
        
        ImGui::TextDisabled("Name"); 
        ImGui::PushItemWidth(-1); 
        ImGui::InputText("##name", newInstanceName, IM_ARRAYSIZE(newInstanceName)); 
        ImGui::PopItemWidth(); 
        std::string nameStr(newInstanceName); 
        bool isNameValid = true; 
        std::string nameError = "";
        if (nameStr.empty()) { 
            isNameValid = false; 
            nameError = "Имя не может быть пустым"; 
        } else { 
            for (char c : nameStr) { 
                if ((unsigned char)c > 127 || !(isalnum(c) || c == ' ' || c == '_' || c == '-')) { 
                    isNameValid = false; 
                    nameError = "Только англ. буквы, цифры, пробел, - и _"; 
                    break; 
                } 
            } 
        }
        if (!isNameValid) 
            ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", nameError.c_str()); 
        else 
            ImGui::Spacing(); 
        ImGui::Spacing();
        
        ImGui::TextDisabled("Loader"); 
        DrawSelectableButton("Vanilla", 0, currentLoaderIdx); 
        ImGui::SameLine(); 
        DrawSelectableButton("Fabric", 1, currentLoaderIdx); 
        ImGui::SameLine(); 
        DrawSelectableButton("NeoForge", 2, currentLoaderIdx); 
        DrawSelectableButton("Forge", 3, currentLoaderIdx); 
        ImGui::Spacing(); 
        ImGui::Spacing();
        
        ImGui::TextDisabled("Game version"); 
        ImGui::PushItemWidth(-1); 
        if (realMcVersions.empty()) {
            if (ImGui::BeginCombo("##ver", "Загрузка...")) 
                ImGui::EndCombo();
        } else {
            if (currentVersionIdx >= (int)realMcVersions.size()) 
                currentVersionIdx = 0;
            if (ImGui::BeginCombo("##ver", realMcVersions[currentVersionIdx].id.c_str())) {
                for (int i = 0; i < (int)realMcVersions.size(); i++) {
                    bool is_selected = (currentVersionIdx == i);
                    if (ImGui::Selectable(realMcVersions[i].id.c_str(), is_selected)) {
                        currentVersionIdx = i;
                    }
                    if (is_selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }
        ImGui::PopItemWidth();

        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 60); 
        ImGui::Separator(); 
        ImGui::Spacing(); 
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 250);
        if (ImGui::Button("<- Back", ImVec2(80, 36))) { 
            openCreateModal = false; 
            ImGui::CloseCurrentPopup(); 
        } 
        ImGui::SameLine();
        
        bool canCreate = isNameValid && !realMcVersions.empty() && !isDownloading;
        ImGui::PushStyleColor(ImGuiCol_Button, canCreate ? ImVec4(0.11f, 0.83f, 0.44f, 1.0f) : ImVec4(0.2f, 0.4f, 0.2f, 0.5f)); 
        ImGui::PushStyleColor(ImGuiCol_Text, canCreate ? ImVec4(0,0,0,1) : ImVec4(0,0,0,0.6f));
        if (ImGui::Button(ICON_FA_PLUS " Create instance", ImVec2(150, 36)) && canCreate) {
            std::string vId = realMcVersions[currentVersionIdx].id;
            std::string vUrl = realMcVersions[currentVersionIdx].url;
            std::thread(InstallInstanceThread, std::string(newInstanceName), vId, vUrl, loaders[currentLoaderIdx]).detach();
            openCreateModal = false; 
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2); 
        ImGui::EndPopup();
    }
    ImGui::End();
}