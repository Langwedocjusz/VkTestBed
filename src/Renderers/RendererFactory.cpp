#include "RendererFactory.h"

#include "HelloRenderer.h"
#include "Minimal3D.h"
#include "MinimalPBR.h"

std::unique_ptr<IRenderer> RendererFactory::MakeRenderer(RendererType type)
{
    switch (type)
    {
    case RendererType::Hello:
        return std::make_unique<HelloRenderer>(mCtx, mInfo, mCamera);
    case RendererType::Minimal3D:
        return std::make_unique<Minimal3DRenderer>(mCtx, mInfo, mCamera);
    case RendererType::MinimalPBR:
        return std::make_unique<MinimalPbrRenderer>(mCtx, mInfo, mCamera);
    }
}