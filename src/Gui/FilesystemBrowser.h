#pragma once

#include <filesystem>

class FilesystemBrowser {
  public:
    FilesystemBrowser();
    FilesystemBrowser(std::filesystem::path currentPath);

    void OnImGui(float lowerMargin);

  public:
    std::filesystem::path CurrentPath;
    std::filesystem::path ChosenFile;
};