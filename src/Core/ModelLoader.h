#pragma once

#include "GeometryProvider.h"

#include <string>

namespace ModelLoader
{
struct Config {
    std::string Filepath;
};

GeometryProvider PosTex(const Config &config);
} // namespace ModelLoader
