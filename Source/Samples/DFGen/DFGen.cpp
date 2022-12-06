/***************************************************************************
 # Copyright (c) 2015-22, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "DFGen.h"

FALCOR_EXPORT_D3D12_AGILITY_SDK

uint32_t mSampleGuiWidth = 250;
uint32_t mSampleGuiHeight = 200;
uint32_t mSampleGuiPositionX = 20;
uint32_t mSampleGuiPositionY = 40;

namespace
{
    //const char kManhattanGrassfire[] = "Samples/DFGen/ManhattanGrassfire.ps.slang";
    //const char kChessboard[] = "Samples/DFGen/Chessboard.ps.slang";
    //const char kErosion[] = "Samples/DFGen/Erosion.ps.slang";
    //const char kErosionSDF[] = "Samples/DFGen/ErosionSDF.ps.slang";
    const char kManhattanGrassfire[] = "E:/Projects/Falcor/Source/Samples/DFGen/ManhattanGrassfire.ps.slang";
    const char kChessboard[] = "E:/Projects/Falcor/Source/Samples/DFGen/Chessboard.ps.slang";
    const char kErosion[] = "E:/Projects/Falcor/Source/Samples/DFGen/Erosion.ps.slang";
    const char kErosionSDF[] = "E:/Projects/Falcor/Source/Samples/DFGen/ErosionSDF.ps.slang";
}

const Gui::DropdownList kDistanceFieldGenerationTypeList =
{
    { (uint32_t)DistanceFieldGenerationType::ManhattanGrassfire, "Manhattan Grassfire" },
    { (uint32_t)DistanceFieldGenerationType::Chessboard, "Chessboard" },
    { (uint32_t)DistanceFieldGenerationType::Erosion, "Erosion" },
    { (uint32_t)DistanceFieldGenerationType::ErosionSDF, "ErosionSDF" }
};

void DFGen::onGuiRender(Gui* pGui)
{
    Gui::Window w(pGui, "Falcor", { 250, 200 });
    gpFramework->renderGlobalUI(pGui);
    w.dropdown("DistanceField generation type", kDistanceFieldGenerationTypeList, GenType);
    if (w.button("Generate DistanceField"))
    {
        bGenDF = true;
    }
    w.checkbox("Gen Debug", bGenDFDebug);
}

void DFGen::onLoad(RenderContext* pRenderContext)
{
    Program::setGenerateDebugInfoEnabled(true);

    {
        mpDFGenPass[0] = FullScreenPass::create(kManhattanGrassfire);
        mpDFGenPass[1] = FullScreenPass::create(kChessboard);
        mpDFGenPass[2] = FullScreenPass::create(kErosion);
        mpErosionSDF = FullScreenPass::create(kErosionSDF);
    }

    std::filesystem::path path;
    FileDialogFilterVec filters = { {"bmp"}, {"jpg"}, {"dds"}, {"png"}, {"tiff"}, {"tif"}, {"tga"} };
    if (openFileDialog(filters, path))
        mpSource = Texture::createFromFile(path, false, true);
    if (openFileDialog(filters, path))
        mpSourceInv = Texture::createFromFile(path, false, true);

    OQPayload.Init();

    PositiveDF = Fbo::create2D(mpSource->getWidth(), mpSource->getHeight(), ResourceFormat::R8Unorm);
    SDFRT = Fbo::create2D(mpSource->getWidth(), mpSource->getHeight(), ResourceFormat::R8Unorm);
}

void DFGen::DFGenRenderer(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
    mpPingPong[0] = Fbo::create2D(mpSource->getWidth(), mpSource->getHeight(), ResourceFormat::R8Unorm);
    mpPingPong[1] = Fbo::create2D(mpSource->getWidth(), mpSource->getHeight(), ResourceFormat::R8Unorm);
    pRenderContext->blit(mpSource->getSRV(), mpPingPong[0]->getRenderTargetView(0));
    pRenderContext->clearRtv(mpPingPong[1]->getRenderTargetView(0).get(), float4(0, 0, 0, 0));

    mpPingPongPass = mpDFGenPass[clamp(GenType, 0u, 2u)];

    uint SourceIndex = 0;
    uint TargetIndex = 1;

    bool bErosionVertical = false;

    for (int i = 0; i < MaxExecNum; i++)
    {
        uint SourceIndex = i % 2;
        uint TargetIndex = !!SourceIndex ? SourceIndex - 1 : SourceIndex + 1;

        mpPingPongPass["gTex"] = mpPingPong[SourceIndex]->getColorTexture(0);

        if (GenType == 2 || GenType == 3)
        {
            mpPingPongPass["ErosionPayload"]["Beta"] = 2 * i - 1;
            mpPingPongPass["ErosionPayload"]["Offset0"] = bErosionVertical ? int2(0, 1) : int2(1, 0);
            mpPingPongPass["ErosionPayload"]["Offset1"] = bErosionVertical ? int2(0, -1) : int2(-1, 0);
            mpPingPongPass["ErosionPayload"]["MaxDistance"] = 255;
        }

        OQPayload.Begin(pRenderContext);
        mpPingPongPass->execute(pRenderContext, mpPingPong[TargetIndex]);
        OQPayload.End(pRenderContext);
        pRenderContext->blit(mpPingPong[TargetIndex]->getColorTexture(0)->getSRV(), mpPingPong[SourceIndex]->getRenderTargetView(0));

        if (!OQPayload.RasterizationCount)
        {
            if (GenType == 2 || GenType == 3)
            {
                if (!bErosionVertical)
                {
                    bErosionVertical = true;
                    i = -1;
                    continue;
                }
            }
            break;
        }
    }

    if (GenType == 3)
    {
        bErosionVertical = false;
        pRenderContext->blit(mpPingPong[TargetIndex]->getColorTexture(0)->getSRV(), PositiveDF->getRenderTargetView(0));
        pRenderContext->blit(mpSourceInv->getSRV(), mpPingPong[0]->getRenderTargetView(0));
        pRenderContext->clearRtv(mpPingPong[1]->getRenderTargetView(0).get(), float4(0, 0, 0, 0));

        for (int i = 0; i < MaxExecNum; i++)
        {
            uint SourceIndex = i % 2;
            uint TargetIndex = !!SourceIndex ? SourceIndex - 1 : SourceIndex + 1;

            mpPingPongPass["gTex"] = mpPingPong[SourceIndex]->getColorTexture(0);
            mpPingPongPass["ErosionPayload"]["Beta"] = 2 * i - 1;
            mpPingPongPass["ErosionPayload"]["Offset0"] = bErosionVertical ? int2(0, 1) : int2(1, 0);
            mpPingPongPass["ErosionPayload"]["Offset1"] = bErosionVertical ? int2(0, -1) : int2(-1, 0);
            mpPingPongPass["ErosionPayload"]["MaxDistance"] = 255;

            OQPayload.Begin(pRenderContext);
            mpPingPongPass->execute(pRenderContext, mpPingPong[TargetIndex]);
            OQPayload.End(pRenderContext);
            pRenderContext->blit(mpPingPong[TargetIndex]->getColorTexture(0)->getSRV(), mpPingPong[SourceIndex]->getRenderTargetView(0));

            if (!OQPayload.RasterizationCount)
            {
                    if (!bErosionVertical)
                    {
                        bErosionVertical = true;
                        i = -1;
                        continue;
                    }
                break;
            }
        }

        mpErosionSDF["gTexPos"] = PositiveDF->getColorTexture(0);
        mpErosionSDF["gTexNeg"] = mpPingPong[TargetIndex]->getColorTexture(0);
        mpErosionSDF->execute(pRenderContext, SDFRT);
    }

    bGenDF = false;
    if(GenType == 3)
        pRenderContext->blit(SDFRT->getColorTexture(0)->getSRV(), pTargetFbo->getRenderTargetView(0));
    else
        pRenderContext->blit(mpPingPong[TargetIndex]->getColorTexture(0)->getSRV(), pTargetFbo->getRenderTargetView(0));
}

void DFGen::onFrameRender(RenderContext* pRenderContext, const Fbo::SharedPtr& pTargetFbo)
{
    if (bGenDF || bGenDFDebug)
        DFGenRenderer(pRenderContext, pTargetFbo);
}

void DFGen::onShutdown()
{
}

bool DFGen::onKeyEvent(const KeyboardEvent& keyEvent)
{
    return false;
}

bool DFGen::onMouseEvent(const MouseEvent& mouseEvent)
{
    return false;
}

void DFGen::onHotReload(HotReloadFlags reloaded)
{
}

void DFGen::onResizeSwapChain(uint32_t width, uint32_t height)
{
}

int main(int argc, char** argv)
{
    DFGen::UniquePtr pRenderer = std::make_unique<DFGen>();

    SampleConfig config;
    config.windowDesc.title = "Falcor Project Template";
    config.windowDesc.resizableWindow = true;
    Sample::run(config, pRenderer);

    return 0;
}

void OcclusionQueryPayLoad::Init()
{
    spHeap = gpDevice->createQueryHeap(QueryHeap::Type::Occlusion, 1);
    pStagingBuffer = Buffer::create(sizeof(uint) * 2, Resource::BindFlags::None, Buffer::CpuAccess::Read, (void*)&RasterizationCount);
}

void OcclusionQueryPayLoad::Begin(RenderContext* pRenderContext)
{
    pRenderContext->getLowLevelData()->getCommandList()->BeginQuery(spHeap.lock()->getApiHandle(), D3D12_QUERY_TYPE::D3D12_QUERY_TYPE_OCCLUSION, 0);
}

void OcclusionQueryPayLoad::End(RenderContext* pRenderContext)
{
    pRenderContext->getLowLevelData()->getCommandList()->EndQuery(spHeap.lock()->getApiHandle(), D3D12_QUERY_TYPE::D3D12_QUERY_TYPE_OCCLUSION, 0);
    pRenderContext->getLowLevelData()->getCommandList()->ResolveQueryData(spHeap.lock()->getApiHandle(), D3D12_QUERY_TYPE::D3D12_QUERY_TYPE_OCCLUSION, 0, 1, pStagingBuffer->getD3D12Handle(), 0);

    uint* pData = reinterpret_cast<uint*>(pStagingBuffer->map(Buffer::MapType::Read));
    RasterizationCount = *pData;
    pStagingBuffer->unmap();
}
