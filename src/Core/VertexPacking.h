#pragma once

#include "GeometryData.h"
#include "GltfImporter.h"
#include "VertexLayout.h"

namespace VertexPacking
{
GeometryData Encode(GltfPrimitive& prim, Vertex::Layout layout);
} // namespace ModelLoader
