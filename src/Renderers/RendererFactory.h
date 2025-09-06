#pragma once

#include "Renderer.h"

#include <memory>

enum class RendererType
{
    Hello,
    Minimal3D,
    MinimalPBR
};

class RendererFactory {
  public:
    RendererFactory(VulkanContext &ctx, FrameInfo &info, Camera &camera)
        : mCtx(ctx), mInfo(info), mCamera(camera)
    {
    }

    std::unique_ptr<IRenderer> MakeRenderer(RendererType type);

  private:
    VulkanContext &mCtx;
    FrameInfo &mInfo;
    Camera &mCamera;
};
