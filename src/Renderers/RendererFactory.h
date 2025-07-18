#pragma once

#include "Renderer.h"

enum class RendererType
{
    Hello,
    Minimal3D,
    MinimalPBR
};

class RendererFactory {
  public:
    RendererFactory(VulkanContext &ctx, FrameInfo &info, std::unique_ptr<Camera> &camera)
        : mCtx(ctx), mInfo(info), mCamera(camera)
    {
    }

    std::unique_ptr<IRenderer> MakeRenderer(RendererType type);

  private:
    VulkanContext &mCtx;
    FrameInfo &mInfo;
    std::unique_ptr<Camera> &mCamera;
};
