#pragma once

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <utility>

class FilesystemBrowser {
  public:
    using CallbackFn   = std::function<void()>;
    using CheckFn      = std::function<bool(const std::string &)>;
    using ExtensionSet = std::set<std::string>;

  public:
    FilesystemBrowser();
    // Path is assumed to be utf8 encoded:
    FilesystemBrowser(const std::string &currentPath);

    void AddExtensionToFilter(const std::string &ext);
    void ClearExtensionFilter();

    void SetCallbackFn(CallbackFn callback)
    {
        mCallback = std::move(callback);
    }
    void SetCheckFn(CheckFn check)
    {
        mCheck = std::move(check);
    }

    void ImGuiLoadPopup(const std::string &name, bool &open);

    // Renders a child window with selectable entries
    // for files/directories, lowerMargin determines
    // the vertical size of child window, relative
    // to window bottom.
    void OnImGuiRaw(float lowerMargin);

  public:
    // Utf8 encoded:
    std::string CurrentPath;
    std::string ChosenFile;

  private:
    CallbackFn mCallback;
    CheckFn    mCheck;

    std::optional<ExtensionSet> mValidExtensions;
};