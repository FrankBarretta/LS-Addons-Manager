#pragma once
#include <windows.h>
#include <string>

class AddonManager;

class GuiManager {
public:
    static void StartGuiThread(AddonManager* manager);

private:
    static DWORD WINAPI GuiThread(LPVOID lpParam);
};
