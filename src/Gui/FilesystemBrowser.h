#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <set>

class FilesystemBrowser {
  public:
    using CallbackFn = std::function<void()>;
    using CheckFn = std::function<bool(const std::filesystem::path &)>;
    using ExtensionSet = std::set<std::string>;

  public:
    FilesystemBrowser();
    FilesystemBrowser(std::filesystem::path currentPath);

    void AddExtensionToFilter(const std::string &ext);
    void ClearExtensionFilter();

    void SetCallbackFn(CallbackFn callback)
    {
        mCallback = callback;
    }
    void SetCheckFn(CheckFn check)
    {
        mCheck = check;
    }

    void ImGuiLoadPopup(const std::string &name, bool &open);

    // Renders a child window with selectable entries
    // for files/directories, lowerMargin determines
    // the vertical size of child window, relative
    // to window bottom.
    void OnImGuiRaw(float lowerMargin);

  public:
    std::filesystem::path CurrentPath;
    std::filesystem::path ChosenFile;

  private:
    CallbackFn mCallback;
    CheckFn mCheck;

    std::optional<ExtensionSet> mValidExtensions;
};