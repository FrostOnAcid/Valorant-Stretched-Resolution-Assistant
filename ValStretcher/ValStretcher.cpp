#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <cstdlib>
#include <vector>
#include <limits>
#include <sstream>
#include <tlhelp32.h>
#include <algorithm>
#include <cctype>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

struct Monitor {
    int index;
    bool isPrimary;
    std::wstring deviceName;
};

std::vector<Monitor> monitors;
DEVMODE originalMode = {};

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    MONITORINFOEX miex;
    miex.cbSize = sizeof(miex);
    if (GetMonitorInfo(hMonitor, &miex)) {
        DEVMODE devMode;
        devMode.dmSize = sizeof(devMode);
        if (EnumDisplaySettings(miex.szDevice, ENUM_CURRENT_SETTINGS, &devMode)) {
            Monitor mon;
            mon.index = monitors.size() + 1;
            mon.isPrimary = (miex.dwFlags & MONITORINFOF_PRIMARY) != 0;
            mon.deviceName = miex.szDevice;
            monitors.push_back(mon);
            std::wcout << L"Monitor " << mon.index << L": " << mon.deviceName << (mon.isPrimary ? L" (MAIN)" : L"") << std::endl;
        }
    }
    return TRUE;
}

int ChooseMonitor() {
    monitors.clear();
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
    if (monitors.size() > 1) {
        std::wcout << L"Select a monitor (1-" << monitors.size() << L"): ";
        int choice;
        std::wcin >> choice;
        while (std::wcin.fail() || choice < 1 || choice > monitors.size()) {
            std::wcin.clear();
            std::wcin.ignore(std::numeric_limits<std::streamsize>::max(), L'\n');
            std::wcout << L"Invalid input. Please select a valid monitor number (1-" << monitors.size() << L"): ";
            std::wcin >> choice;
        }
        return choice - 1;
    }
    return 0;
}

bool SetDisplayResolutionForMonitor(int monitorIndex, int width, int height) {
    if (monitorIndex < 0 || monitorIndex >= monitors.size()) {
        std::cout << "Invalid Monitor Index";
        return false;
    }

    DEVMODE devMode = {};
    devMode.dmSize = sizeof(devMode);
    devMode.dmPelsWidth = width;
    devMode.dmPelsHeight = height;
    devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

    Monitor& selectedMonitor = monitors[monitorIndex];
    LONG result = ChangeDisplaySettingsEx(selectedMonitor.deviceName.c_str(), &devMode, NULL, CDS_UPDATEREGISTRY, NULL);
    if (result == DISP_CHANGE_SUCCESSFUL) {
        std::cout << "Successfully changed resolution for monitor " << selectedMonitor.index << std::endl;
        return true;
    }
    else {
        std::cerr << "Failed to change resolution for monitor " << selectedMonitor.index << ". Error code: " << result << std::endl;
        return false;
    }
}

void RestoreOriginalResolution() {
    for (auto& monitor : monitors) {
        ChangeDisplaySettingsEx(monitor.deviceName.c_str(), &originalMode, NULL, CDS_UPDATEREGISTRY, NULL);
    }
}

void GetOriginalResolution() {
    EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &originalMode);
    originalMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
}

bool CheckAndModifyGameSettings(const std::string& directory) {
    namespace fs = std::filesystem;
    bool folderChecked = false;

    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (entry.is_directory() && entry.path().filename() == "Windows") {
            std::string iniPath = entry.path().string() + "\\GameUserSettings.ini";
            if (fs::exists(iniPath)) {
                folderChecked = true;
                std::ifstream file(iniPath);
                std::string line;
                std::string content;
                bool found = false;

                while (getline(file, line)) {
                    std::size_t pos = line.find("bShouldLetterbox=");
                    if (pos != std::string::npos) {
                        std::string valuePart = line.substr(pos + 17);
                        if (valuePart.find("True") != std::string::npos || valuePart.find("true") != std::string::npos) {
                            line.replace(pos + 17, 4, "False");
                            found = true;
                        }
                    }
                    content += line + "\n";
                }
                file.close();

                if (found) {
                    std::ofstream outFile(iniPath);
                    outFile << content;
                    outFile.close();
                }
            }
        }
    }
    return folderChecked;
}

HWND FindValorantWindow() {
    std::wstring targetName = L"VALORANT";
    std::transform(targetName.begin(), targetName.end(), targetName.begin(), [](wchar_t c) {
        return std::tolower(c);
        });

    HWND hwndFound = NULL;
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        const int bufferSize = 256;
        wchar_t windowTitle[bufferSize];
        if (GetWindowTextW(hwnd, windowTitle, bufferSize) > 0) {
            std::wstring title = windowTitle;
            std::transform(title.begin(), title.end(), title.begin(), [](wchar_t c) {
                return std::tolower(c);
                });
            if (title.find(L"valorant") != std::wstring::npos) {
                *reinterpret_cast<HWND*>(lParam) = hwnd;
                return FALSE;
            }
        }
        return TRUE;
        }, reinterpret_cast<LPARAM>(&hwndFound));

    if (hwndFound != NULL) {
        std::wcout << L"Window found." << std::endl;
    }
    else {
        std::wcout << L"Window not found." << std::endl;
    }
    return hwndFound;
}

void countdownAnimation(int seconds) {
    for (int i = seconds; i > 0; --i) {
        std::cout << "Returning to menu in " << i << " seconds...\r" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << std::endl;
}

int main() {
    GetOriginalResolution();

    char* appDataPath;
    size_t size;
    _dupenv_s(&appDataPath, &size, "LOCALAPPDATA");
    std::string valorantPath(appDataPath);
    valorantPath += "\\VALORANT\\Saved\\Config";
    free(appDataPath);

    int choice;
    while (true) {
        system("cls");
        std::cout << "Welcome to ValStretcher by FrostChanger. What would you like to do?\n";
        std::cout << "1. Fix Gamefiles\n";
        std::cout << "2. Change Resolution\n";
        std::cin >> choice;

        switch (choice) {
        case 1:
            std::cout << "Checking your Gamefiles...\n";
            if (!fs::exists(valorantPath) || !CheckAndModifyGameSettings(valorantPath)) {
                std::cout << "\x1B[31mCouldn't find your local VALORANT folder, please enter path or install VALORANT.\033[0m\n";
                std::cout << "Enter a valid path: ";
                std::cin >> valorantPath;
            }
            if (!CheckAndModifyGameSettings(valorantPath)) {
                std::cout << "Invalid path. Returning to menu.\n";
                countdownAnimation(3);
                continue;
            }
            else {
                std::cout << "\x1B[32mSUCCESSFULLY FIXED YOUR GAMEFILES\033[0m\n";
                countdownAnimation(3);
                continue;
            }
            break;
        case 2:
        {
            int selectedMonitorIndex = ChooseMonitor();

            std::cout << "Enter the width: ";
            int width;
            std::cin >> width;

            std::cout << "Enter the height: ";
            int height;
            std::cin >> height;

            if (SetDisplayResolutionForMonitor(selectedMonitorIndex, width, height)) {
                std::cout << "\x1B[32mSUCCESSFULLY CHANGED RESOLUTION\033[0m\n";
                HWND valorantWindow = FindValorantWindow();
                if (valorantWindow == NULL) {
                    std::cout << "\x1B[31mFAILED TO FIND VALORANT WINDOW\033[0m\n";
                    countdownAnimation(3);
                    return false;
                }

                SetWindowLong(valorantWindow, GWL_STYLE, GetWindowLong(valorantWindow, GWL_STYLE) & ~WS_BORDER);
                std::cout << "Border removed\n";

                HMONITOR hMonitor = MonitorFromWindow(valorantWindow, MONITOR_DEFAULTTOPRIMARY);
                if (hMonitor == NULL) {
                    std::cout << "\x1B[31mFAILED TO GET MONITOR INFO\033[0m\n";
                    countdownAnimation(3);
                    return false;
                }

                MONITORINFO monitorInfo;
                monitorInfo.cbSize = sizeof(MONITORINFO);
                if (!GetMonitorInfo(hMonitor, &monitorInfo)) {
                    std::cout << "\x1B[31mFAILED TO RETRIEVE MONITOR INFO\033[0m\n";
                    countdownAnimation(3);

                    return false;
                }

                RECT windowRect;
                GetWindowRect(valorantWindow, &windowRect);
                int frameWidth = (windowRect.right - windowRect.left) - width;
                int frameHeight = (windowRect.bottom - windowRect.top) - height;

                if (!SetWindowPos(valorantWindow, NULL,
                    monitorInfo.rcMonitor.left - (frameWidth / 2),
                    monitorInfo.rcMonitor.top - (frameHeight / 2),
                    monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left + frameWidth,
                    monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top + frameHeight,
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED)) {
                    std::cout << "\x1B[31mFAILED TO ADJUST WINDOW POSITION AND SIZE\033[0m\n";
                    countdownAnimation(3);

                    return false;
                }
                std::cout << "Window position and size adjusted\n";

                std::cout << "\x1B[32mSUCCESSFULLY FIXED VALORANT\033[0m\n";
            }
            else {
                std::cout << "\x1B[31mFAILED TO CHANGE RESOLUTION\033[0m\n";
                std::cout << "Error code: " << GetLastError() << std::endl;
                countdownAnimation(3);
            }

            break;
        }
        }

        system("cls");
        countdownAnimation(3);
    }

    return 0;
}
