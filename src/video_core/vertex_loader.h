#pragma once

#include <array>
#include "common/common_types.h"
#include "video_core/regs_pipeline.h"
#include "video_core/vertex_loader_base.h"

namespace Pica {

namespace DebugUtils {
class MemoryAccessTracker;
}

namespace Shader {
struct AttributeBuffer;
}

class VertexLoader : public VertexLoaderBase {
public:
    explicit VertexLoader(const PipelineRegs::VertexAttributes& vertex_attributes);
    void LoadVertex(u32 base_address, int index, int vertex, Shader::AttributeBuffer& input,
                    DebugUtils::MemoryAccessTracker& memory_accesses) override;

    int GetNumTotalAttributes() const {
        return num_total_attributes;
    }

private:
    std::array<u32, 16> vertex_attribute_sources;
    std::array<u32, 16> vertex_attribute_strides{};
    std::array<PipelineRegs::VertexAttributeFormat, 16> vertex_attribute_formats;
    std::array<u32, 16> vertex_attribute_elements{};
    std::array<bool, 16> vertex_attribute_is_default;
    int num_total_attributes = 0;
};

} // namespace Pica
