// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>

#include "common/common_types.h"
#include "common/hash.h"

namespace Pica {
namespace DebugUtils {
class MemoryAccessTracker;
}

namespace Shader {
struct AttributeBuffer;
}

class VertexLoaderBase {
public:
    virtual void LoadVertex(u32 base_address, int index, int vertex, Shader::AttributeBuffer& input,
                            DebugUtils::MemoryAccessTracker& memory_accesses) = 0;
};

using VertexLoaderCache = std::unordered_map<u64, std::unique_ptr<VertexLoaderBase>>;

static VertexLoaderCache vertex_loader_cache;

} // namespace Pica
