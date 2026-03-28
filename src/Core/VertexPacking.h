#pragma once

#include "GeometryData.h"
#include "GltfImporter.h"
#include "VertexLayout.h"

namespace VertexPacking
{
GeometryData Encode(PrimitiveData &prim, Vertex::Layout layout);
} // namespace VertexPacking
