#include "video_core/clipper.h"
#include "video_core/renderer_vulkan/vk_rasterizer.h"

RasterizerVulkan::RasterizerVulkan(){}

RasterizerVulkan::~RasterizerVulkan(){}

void RasterizerVulkan::AddTriangle(const Pica::Shader::OutputVertex & v0, const Pica::Shader::OutputVertex & v1, const Pica::Shader::OutputVertex & v2) {
    Pica::Clipper::ProcessTriangle(v0, v1, v2);
}

void RasterizerVulkan::DrawTriangles(){}

void RasterizerVulkan::FlushAll(){}

void RasterizerVulkan::NotifyPicaRegisterChanged(u32 id){}

void RasterizerVulkan::FlushRegion(PAddr addr, u32 size){}

void RasterizerVulkan::FlushAndInvalidateRegion(PAddr addr, u32 size){}
