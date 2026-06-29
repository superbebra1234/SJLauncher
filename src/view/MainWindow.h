#pragma once

#include "imgui.h"

// Оригинальные иконки FontAwesome без изменений
#define ICON_FA_HOUSE       "\xef\x80\x95"
#define ICON_FA_COMPASS     "\xef\x85\x8e"
#define ICON_FA_SHIRT       "\xef\x95\x93"
#define ICON_FA_BOOKS       "\xef\x97\xbd"
#define ICON_FA_SERVER      "\xef\x88\xb3"
#define ICON_FA_PLAY        "\xef\x81\x4b"
#define ICON_FA_ELLIPSIS_V  "\xef\x85\x82"
#define ICON_FA_HAMMER      "\xef\x9b\xa3"
#define ICON_FA_USER        "\xef\x80\x87"
#define ICON_FA_PLUS        "\xef\x81\xa7"
#define ICON_FA_COG         "\xef\x80\x93"
#define ICON_FA_SIGN_IN     "\xef\x82\x90"
#define ICON_FA_CHECK       "\xef\x80\x8c"
#define ICON_FA_TIMES       "\xef\x80\x8d"
#define ICON_FA_DOWNLOAD    "\xef\x80\x99"
#define ICON_FA_CUBE        "\xef\x86\xb2"

struct GLFWwindow; // Форвард-декларация, чтобы не тянуть весь glfw3.h

// Функции настройки и отрисовки из оригинального кода
void SetupFonts(ImGuiIO& io);
void SetupModrinthStyle();
void DrawSelectableButton(const char* label, int index, int& current);
void DrawDownloadToast(const ImGuiIO& io);

// Функция для рендера всего интерфейса (заменит содержимое цикла while)
void RenderMainWindow(GLFWwindow* window, ImGuiIO& io);