#pragma once

#include <filesystem>
#include <optional>
#include <set>

class FilesystemBrowser {
  public:
    FilesystemBrowser();
    FilesystemBrowser(std::filesystem::path currentPath);

    void AddExtensionToFilter(const std::string &ext);
    void ClearExtensionFilter();

    void OnImGui(float lowerMargin);

  public:
    std::filesystem::path CurrentPath;
    std::filesystem::path ChosenFile;

  private:
    using ExtensionSet = std::set<std::string>;
    std::optional<ExtensionSet> mValidExtensions;
};