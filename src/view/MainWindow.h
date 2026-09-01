#pragma once
#include "imgui.h"

// ─── FontAwesome icons ────────────────────────────────────────────────────────
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
#define ICON_FA_JAVA        "\xef\x9d\xbe"
#define ICON_FA_SLIDERS     "\xef\x87\x9e"
#define ICON_FA_PAINT       "\xef\x87\xbc"
#define ICON_FA_WINDOW      "\xef\x84\x91"
#define ICON_FA_SHIELD      "\xef\x84\xa9"
#define ICON_FA_FOLDER      "\xef\x81\xbb"
#define ICON_FA_TOGGLE_ON   "\xef\x88\x85"
#define ICON_FA_TOGGLE_OFF  "\xef\x88\x84"
#define ICON_FA_SAVE        "\xef\x83\x87"

struct GLFWwindow;

void SetupFonts(ImGuiIO& io);
void SetupModrinthStyle();
void DrawSelectableButton(const char* label, int index, int& current);
void DrawDownloadToast(const ImGuiIO& io);
void RenderMainWindow(GLFWwindow* window, ImGuiIO& io);
