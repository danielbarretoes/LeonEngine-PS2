#include <leon/editor/EditorFileDialog.h>
#include <string>

#ifdef _WIN32
// clang-format off
// Must include windows.h before commdlg.h; WIN32_LEAN_AND_MEAN breaks prsht/commdlg.
#ifdef WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>
// clang-format on
#endif

namespace leon::editor {
namespace {

#ifdef _WIN32
std::string WideToUtf8(const wchar_t* wide) {
    if (wide == nullptr || wide[0] == L'\0') {
        return {};
    }
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) {
        return {};
    }
    std::string out(static_cast<std::size_t>(bytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), bytes, nullptr, nullptr);
    return out;
}

std::wstring Utf8ToWide(const char* utf8) {
    if (utf8 == nullptr || utf8[0] == '\0') {
        return {};
    }
    const int chars = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (chars <= 1) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(chars - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), chars);
    return out;
}
#endif

} // namespace

std::string EditorPickOpenFile(const char* filter, const char* title) {
#ifdef _WIN32
    char file[MAX_PATH] = {};
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter != nullptr ? filter : "All\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = title != nullptr ? title : "Open";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(file);
    }
    return {};
#else
    (void)filter;
    (void)title;
    return {};
#endif
}

std::string EditorPickSaveFile(const char* filter, const char* title, const char* defaultName) {
#ifdef _WIN32
    char file[MAX_PATH] = {};
    if (defaultName != nullptr) {
        strncpy_s(file, defaultName, _TRUNCATE);
    }
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter != nullptr ? filter : "Leon Level\0*.llev\0All\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = title != nullptr ? title : "Save As";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "llev";
    if (GetSaveFileNameA(&ofn) == TRUE) {
        return std::string(file);
    }
    return {};
#else
    (void)filter;
    (void)title;
    (void)defaultName;
    return {};
#endif
}

std::string EditorPickFolder(const char* title) {
#ifdef _WIN32
    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool needUninit = SUCCEEDED(hrInit) || hrInit == S_FALSE;

    IFileOpenDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog));
    if (FAILED(hr) || dialog == nullptr) {
        if (needUninit) {
            CoUninitialize();
        }
        return {};
    }

    DWORD options = 0;
    if (SUCCEEDED(dialog->GetOptions(&options))) {
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }
    if (title != nullptr && title[0] != '\0') {
        const std::wstring wideTitle = Utf8ToWide(title);
        if (!wideTitle.empty()) {
            dialog->SetTitle(wideTitle.c_str());
        }
    }

    std::string result;
    hr = dialog->Show(nullptr);
    if (SUCCEEDED(hr)) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path != nullptr) {
                result = WideToUtf8(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    if (needUninit) {
        CoUninitialize();
    }
    return result;
#else
    (void)title;
    return {};
#endif
}

} // namespace leon::editor
