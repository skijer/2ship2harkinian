#include "ModDialogs.h"

#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <string>

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <spdlog/spdlog.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_messagebox.h>
#include <SDL2/SDL_video.h>

#include "2s2h/ModApi/ModPermissions/ModPermissions.h"
#include "2s2h/ModApi/AssetSetup/AssetSetup.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

bool sIsHeadless = false;

#if defined(_WIN32)
struct GameWindows {
    int total = 0;
    int visible = 0;
};

// The game window is SDL's with OpenGL and libultraship's own with DirectX.
BOOL CALLBACK CountGameWindow(HWND window, LPARAM windows) {
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    wchar_t className[16] = {};
    if (process != GetCurrentProcessId() || GetClassNameW(window, className, 16) == 0) {
        return TRUE;
    }
    if (std::wcscmp(className, L"SDL_app") == 0 || std::wcscmp(className, L"N64GAME") == 0) {
        auto* counted = reinterpret_cast<GameWindows*>(windows);
        counted->total++;
        counted->visible += IsWindowVisible(window) ? 1 : 0;
    }
    return TRUE;
}
#endif

// Fleet runs the games headless or hidden to make seeds and to test, where a modal dialog waits for a click nobody
// can give. Before the game window exists, as during ModInit, dialogs still show.
bool IsNobodyWatching() {
    if (sIsHeadless) {
        return true;
    }
#if defined(_WIN32)
    GameWindows windows;
    EnumWindows(CountGameWindow, reinterpret_cast<LPARAM>(&windows));
    return windows.total > 0 && windows.visible == 0;
#else
    SDL_Window* window = SDL_GL_GetCurrentWindow();
    return window != nullptr && (SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN) != 0;
#endif
}

enum class DialogChoice : int {
    No,
    Yes,
};

// The asset loaders are the mods that read the player's files; Fleet's setup answers only for them.
bool IsAnsweredByAssetSetup(const ModIdentity& mod) {
    return AssetSetup_IsRunning() &&
           std::find(mod.declared.begin(), mod.declared.end(), ModPermission::Files) != mod.declared.end();
}

bool AskFor(const ModIdentity* mod, const char* title, const std::string& message, const char* yes, const char* no) {
    if (mod == nullptr) {
        return false;
    }
    const std::string fullTitle = mod->name + ": " + (title != nullptr ? title : "");
    if (IsAnsweredByAssetSetup(*mod)) {
        SPDLOG_INFO("[ModApi] {} asked during Fleet's asset setup, answered '{}': {}", fullTitle, yes, message);
        return true;
    }
    if (IsNobodyWatching()) {
        SPDLOG_INFO("[ModApi] {} asked with nobody watching, answered '{}': {}", fullTitle, no, message);
        return false;
    }
    const SDL_MessageBoxButtonData buttons[] = {
        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, static_cast<int>(DialogChoice::No), no },
        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, static_cast<int>(DialogChoice::Yes), yes },
    };
    const SDL_MessageBoxData box = {
        SDL_MESSAGEBOX_INFORMATION, nullptr, fullTitle.c_str(), message.c_str(),
        SDL_arraysize(buttons),     buttons, nullptr,
    };
    int choice = static_cast<int>(DialogChoice::No);
    if (SDL_ShowMessageBox(&box, &choice) != 0) {
        SPDLOG_ERROR("[ModApi] Could not show '{}', so it counts as '{}': {}", fullTitle, no, SDL_GetError());
        return false;
    }
    return choice == static_cast<int>(DialogChoice::Yes);
}

void TellFor(const ModIdentity* mod, const char* title, const std::string& message) {
    if (mod == nullptr) {
        return;
    }
    const std::string fullTitle = mod->name + ": " + (title != nullptr ? title : "");
    if (AssetSetup_IsRunning() || IsNobodyWatching()) {
        SPDLOG_INFO("[ModApi] {}: {}", fullTitle, message);
        return;
    }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, fullTitle.c_str(), message.c_str(), nullptr);
}

#if defined(_WIN32)
std::wstring GetOriginalArguments() {
    const wchar_t* line = GetCommandLineW();
    if (*line == L'"') {
        line = wcschr(line + 1, L'"');
        return line == nullptr ? L"" : line + 1;
    }
    line = wcschr(line, L' ');
    return line == nullptr ? L"" : line;
}

bool ScheduleRelaunch() {
    wchar_t executable[MAX_PATH];
    if (GetModuleFileNameW(nullptr, executable, MAX_PATH) == 0) {
        return false;
    }
    std::wstring command = L"cmd.exe /c ping -n 6 127.0.0.1 >nul & start \"\" \"" + std::wstring(executable) + L"\"" +
                           GetOriginalArguments();
    const std::wstring directory = std::filesystem::path(executable).parent_path().wstring();
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr,
                        directory.c_str(), &startup, &process)) {
        return false;
    }
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return true;
}
#endif

bool RequestRestartFor(const ModIdentity* mod, const char* reason) {
    if (mod == nullptr) {
        return false;
    }
    if (AssetSetup_IsRunning()) {
        AssetSetup_NoteRestart(mod->name);
        return true;
    }
    const std::string message = "'" + mod->name + "' (" + mod->trustLabel + ") needs 2Ship to restart" +
                                (reason != nullptr ? std::string(": ") + reason : std::string(".")) +
                                "\n\nRestart now?";
    if (!AskFor(mod, "Restart", message, "Restart now", "Later")) {
        return false;
    }
#if defined(_WIN32)
    if (ScheduleRelaunch()) {
        SPDLOG_INFO("[ModApi] Restarting for '{}'", mod->name);
        Ship::Context::GetRawInstance()->GetWindow()->Close();
        return true;
    }
#endif
    TellFor(mod, "Restart", "Close 2Ship and open it again to finish.");
    return false;
}

} // namespace

extern "C" bool ModDialogs_Ask(const char* title, const char* message) {
    return AskFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), title, message != nullptr ? message : "", "Yes",
                  "No");
}

extern "C" void ModDialogs_Tell(const char* title, const char* message) {
    TellFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), title, message != nullptr ? message : "");
}

extern "C" bool ModDialogs_RequestRestart(const char* reason) {
    return RequestRestartFor(ModPermissions_FindCaller(MOD_CALLER_ADDRESS()), reason);
}

extern "C" void ModDialogs_SetHeadless(bool isHeadless) {
    sIsHeadless = isHeadless;
}
