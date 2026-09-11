#include <pch.h>
#include "DLSSDFeature_Dx12.h"
#include <dxgi1_4.h>
#include <Config.h>
#include <algorithm>
#include <cstring>
#include <nvsdk_ngx_defs_dlssd.h>
#include <dlssnr/DlssNrFeature_Dx12.h>

namespace
{
uint16_t FloatToHalf(float f)
{
    uint32_t x;
    std::memcpy(&x, &f, sizeof(float));
    uint32_t sign = (x >> 16) & 0x8000;
    int32_t exp = ((x >> 23) & 0xFF) - 127 + 15;
    uint32_t mantissa = x & 0x007FFFFF;

    if (exp <= 0)
    {
        if (exp < -10) return (uint16_t)sign;
        mantissa = (mantissa | 0x00800000) >> (1 - exp);
        return (uint16_t)(sign | ((mantissa + 0x00001000) >> 13));
    }
    else if (exp >= 31)
    {
        return (uint16_t)(sign | 0x7C00);
    }
    return (uint16_t)(sign | (exp << 10) | ((mantissa + 0x00001000) >> 13));
}
} // namespace

void DLSSDFeatureDx12::ReleaseResponsivityResources()
{
    if (_responsivityMaskBuffer != nullptr)
    {
        _responsivityMaskBuffer->Release();
        _responsivityMaskBuffer = nullptr;
    }
    if (_responsivityUploadBuffer != nullptr)
    {
        _responsivityUploadBuffer->Release();
        _responsivityUploadBuffer = nullptr;
    }
    _responsivityWidth = 0;
    _responsivityHeight = 0;
    _lastResponsivityValue = -999.0f;
}

bool DLSSDFeatureDx12::InitInternal(ID3D12GraphicsCommandList* InCommandList, NVSDK_NGX_Parameter* InParameters)
{
    if (IsInited())
        return true;

    return InitDLSSD(InCommandList, InParameters);
}

bool DLSSDFeatureDx12::InitDLSSD(ID3D12GraphicsCommandList* InCommandList, NVSDK_NGX_Parameter* InParameters)
{
    if (NVNGXProxy::NVNGXModule() == nullptr)
    {
        LOG_ERROR("nvngx.dll not loaded!");
        return false;
    }

    if (!_dlssdInitedDx12)
    {
        _dlssdInitedDx12 = NVNGXProxy::InitDx12(Device);

        if (!_dlssdInitedDx12)
            return false;

        _moduleLoaded =
            (NVNGXProxy::D3D12_Init_ProjectID() != nullptr || NVNGXProxy::D3D12_Init_Ext() != nullptr) &&
            (NVNGXProxy::D3D12_Shutdown() != nullptr || NVNGXProxy::D3D12_Shutdown1() != nullptr) &&
            (NVNGXProxy::D3D12_GetParameters() != nullptr || NVNGXProxy::D3D12_AllocateParameters() != nullptr) &&
            NVNGXProxy::D3D12_DestroyParameters() != nullptr && NVNGXProxy::D3D12_CreateFeature() != nullptr &&
            NVNGXProxy::D3D12_ReleaseFeature() != nullptr && NVNGXProxy::D3D12_EvaluateFeature() != nullptr;

        // delay between init and create feature
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    LOG_INFO("Creating DLSSD feature");

    if (NVNGXProxy::D3D12_CreateFeature() != nullptr)
    {
        ProcessInitParams(InParameters);

        _p_dlssdHandle = &_dlssdHandle;

        NVSDK_NGX_Result nvResult;
        {
            ScopedSkipHeapCapture skipHeapCapture {};

            nvResult = NVNGXProxy::D3D12_CreateFeature()(InCommandList, NVSDK_NGX_Feature_RayReconstruction,
                                                         InParameters, &_p_dlssdHandle);
        }

        if (nvResult != NVSDK_NGX_Result_Success)
        {
            LOG_ERROR("_CreateFeature result: {0:X}", (unsigned int) nvResult);
            return false;
        }
        else
        {
            LOG_INFO("_CreateFeature result: NVSDK_NGX_Result_Success, HandleId: {0}", _p_dlssdHandle->Id);
        }
    }
    else
    {
        LOG_ERROR("_CreateFeature is nullptr");
        return false;
    }

    ReadVersion();

    SetInit(true);
    return true;
}

bool DLSSDFeatureDx12::EvaluateInternal(ID3D12GraphicsCommandList* InCommandList, NVSDK_NGX_Parameter* InParameters)
{
    if (!_moduleLoaded)
    {
        LOG_ERROR("nvngx.dll or _nvngx.dll is not loaded!");
        return false;
    }

    const auto& cfg = *Config::Instance();

    // Solution C: Replace game's guide buffers with NR-modified versions
    void* origDiffuseAlbedo = nullptr;
    void* origGBufferAlbedo = nullptr;
    void* origSpecularAlbedo = nullptr;
    void* origGBufferSpecularAlbedo = nullptr;
    bool modifiedGuidesInjected = false;

    if (cfg.DlssNrRrSolutionC.value_or_default() && DlssNr::HasModifiedGuides())
    {
        InParameters->Get(NVSDK_NGX_Parameter_DiffuseAlbedo, &origDiffuseAlbedo);
        InParameters->Get(NVSDK_NGX_Parameter_GBuffer_Albedo, &origGBufferAlbedo);
        InParameters->Get(NVSDK_NGX_Parameter_SpecularAlbedo, &origSpecularAlbedo);
        InParameters->Get(NVSDK_NGX_Parameter_GBuffer_SpecularAlbedo, &origGBufferSpecularAlbedo);

        auto* modDiffuse = DlssNr::GetModifiedDiffuseAlbedo();
        auto* modSpecular = DlssNr::GetModifiedSpecularAlbedo();

        if (modDiffuse != nullptr)
        {
            InParameters->Set(NVSDK_NGX_Parameter_DiffuseAlbedo, (void*) modDiffuse);
            InParameters->Set(NVSDK_NGX_Parameter_GBuffer_Albedo, (void*) modDiffuse);
        }
        if (modSpecular != nullptr)
        {
            InParameters->Set(NVSDK_NGX_Parameter_SpecularAlbedo, (void*) modSpecular);
            InParameters->Set(NVSDK_NGX_Parameter_GBuffer_SpecularAlbedo, (void*) modSpecular);
        }
        modifiedGuidesInjected = true;
    }

    // Sub-approach 3c: ColorBeforeTransparency guide trick
    void* origColorBeforeTrans = nullptr;
    bool colorBeforeTransInjected = false;
    if (cfg.DlssNrRrSolutionC.value_or_default() && cfg.DlssNrSolCUseTransparencyGuide.value_or_default())
    {
        void* colorResource = nullptr;
        InParameters->Get(NVSDK_NGX_Parameter_Color, &colorResource);
        if (colorResource != nullptr)
        {
            if (InParameters->Get(NVSDK_NGX_Parameter_DLSSD_ColorBeforeTransparency, &origColorBeforeTrans) != NVSDK_NGX_Result_Success || origColorBeforeTrans == nullptr)
            {
                InParameters->Set(NVSDK_NGX_Parameter_DLSSD_ColorBeforeTransparency, colorResource);
                colorBeforeTransInjected = true;
            }
        }
    }

    // Feature Request A: Standalone Responsivity Mask (Preset F only)
    void* origDlssdResponsivityMask = nullptr;
    void* origDlssResponsivityMask = nullptr;
    bool responsivityMaskInjected = false;

    if (cfg.DLSSDRRResponsivityMaskEnabled.value_or_default())
    {
        unsigned int renderWidth = 0, renderHeight = 0;
        GetRenderResolution(InParameters, &renderWidth, &renderHeight);
        if (renderWidth == 0 || renderHeight == 0)
        {
            renderWidth = _renderWidth;
            renderHeight = _renderHeight;
        }

        if (renderWidth > 0 && renderHeight > 0 && Device != nullptr)
        {
            const float maskValue = std::clamp(cfg.DLSSDRRResponsivityMaskValue.value_or_default(), -1.0f, 1.0f);

            // Preset F check + warning
            int preset = 0;
            NVSDK_NGX_PerfQuality_Value perfQuality = NVSDK_NGX_PerfQuality_Value_MaxPerf;
            InParameters->Get(NVSDK_NGX_Parameter_PerfQualityValue, (int*) &perfQuality);
            const char* presetKey = "RayReconstruction.Hint.Render.Preset.Quality";
            switch (perfQuality)
            {
                case NVSDK_NGX_PerfQuality_Value_DLAA: presetKey = "RayReconstruction.Hint.Render.Preset.DLAA"; break;
                case NVSDK_NGX_PerfQuality_Value_UltraQuality: presetKey = "RayReconstruction.Hint.Render.Preset.UltraQuality"; break;
                case NVSDK_NGX_PerfQuality_Value_MaxQuality: presetKey = "RayReconstruction.Hint.Render.Preset.Quality"; break;
                case NVSDK_NGX_PerfQuality_Value_Balanced: presetKey = "RayReconstruction.Hint.Render.Preset.Balanced"; break;
                case NVSDK_NGX_PerfQuality_Value_MaxPerf: presetKey = "RayReconstruction.Hint.Render.Preset.Performance"; break;
                case NVSDK_NGX_PerfQuality_Value_UltraPerformance: presetKey = "RayReconstruction.Hint.Render.Preset.UltraPerformance"; break;
                default: break;
            }
            if (InParameters->Get(presetKey, &preset) != NVSDK_NGX_Result_Success)
                preset = 0;

            static bool warnedPresetF = false;
            if (!warnedPresetF && preset != NVSDK_NGX_RayReconstruction_Hint_Render_Preset_F)
            {
                warnedPresetF = true;
                LOG_WARN("Responsivity Mask is active but Ray Reconstruction preset is {} (not Preset F). Preset F is required for responsivity mask.", preset);
            }

            if (_responsivityMaskBuffer == nullptr || _responsivityWidth != renderWidth || _responsivityHeight != renderHeight)
            {
                ReleaseResponsivityResources();

                D3D12_HEAP_PROPERTIES heapProps {};
                heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

                D3D12_RESOURCE_DESC texDesc {};
                texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                texDesc.Width = renderWidth;
                texDesc.Height = renderHeight;
                texDesc.DepthOrArraySize = 1;
                texDesc.MipLevels = 1;
                texDesc.Format = DXGI_FORMAT_R16_FLOAT;
                texDesc.SampleDesc.Count = 1;
                texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
                texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

                HRESULT hr = Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
                                                             D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, nullptr,
                                                             IID_PPV_ARGS(&_responsivityMaskBuffer));
                if (SUCCEEDED(hr))
                {
                    _responsivityWidth = renderWidth;
                    _responsivityHeight = renderHeight;
                    _lastResponsivityValue = -999.0f;
                }
                else
                {
                    LOG_ERROR("Failed to create responsivity mask texture: {:X}", (UINT64) hr);
                }
            }

            if (_responsivityMaskBuffer != nullptr && _lastResponsivityValue != maskValue)
            {
                D3D12_RESOURCE_DESC desc = _responsivityMaskBuffer->GetDesc();
                D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout {};
                UINT numRows = 0;
                UINT64 rowSizeInBytes = 0;
                UINT64 totalBytes = 0;
                Device->GetCopyableFootprints(&desc, 0, 1, 0, &layout, &numRows, &rowSizeInBytes, &totalBytes);

                if (_responsivityUploadBuffer != nullptr)
                {
                    _responsivityUploadBuffer->Release();
                    _responsivityUploadBuffer = nullptr;
                }

                D3D12_HEAP_PROPERTIES uploadHeapProps {};
                uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

                D3D12_RESOURCE_DESC bufDesc {};
                bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                bufDesc.Width = totalBytes;
                bufDesc.Height = 1;
                bufDesc.DepthOrArraySize = 1;
                bufDesc.MipLevels = 1;
                bufDesc.Format = DXGI_FORMAT_UNKNOWN;
                bufDesc.SampleDesc.Count = 1;
                bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                bufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

                HRESULT hr = Device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                             IID_PPV_ARGS(&_responsivityUploadBuffer));
                if (SUCCEEDED(hr))
                {
                    const uint16_t halfVal = FloatToHalf(maskValue);
                    void* mapped = nullptr;
                    D3D12_RANGE readRange { 0, 0 };
                    if (SUCCEEDED(_responsivityUploadBuffer->Map(0, &readRange, &mapped)) && mapped != nullptr)
                    {
                        uint8_t* dst = static_cast<uint8_t*>(mapped) + layout.Offset;
                        for (UINT y = 0; y < numRows; ++y)
                        {
                            uint16_t* row = reinterpret_cast<uint16_t*>(dst + y * layout.Footprint.RowPitch);
                            for (UINT x = 0; x < layout.Footprint.Width; ++x)
                                row[x] = halfVal;
                        }
                        _responsivityUploadBuffer->Unmap(0, nullptr);

                        ResourceBarrier(InCommandList, _responsivityMaskBuffer,
                                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                                        D3D12_RESOURCE_STATE_COPY_DEST);

                        D3D12_TEXTURE_COPY_LOCATION dstLoc {};
                        dstLoc.pResource = _responsivityMaskBuffer;
                        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                        dstLoc.SubresourceIndex = 0;

                        D3D12_TEXTURE_COPY_LOCATION srcLoc {};
                        srcLoc.pResource = _responsivityUploadBuffer;
                        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                        srcLoc.PlacedFootprint = layout;

                        InCommandList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

                        ResourceBarrier(InCommandList, _responsivityMaskBuffer,
                                        D3D12_RESOURCE_STATE_COPY_DEST,
                                        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

                        _lastResponsivityValue = maskValue;
                    }
                }
            }

            if (_responsivityMaskBuffer != nullptr)
            {
                InParameters->Get(NVSDK_NGX_Parameter_DLSSD_ResponsivityMask, &origDlssdResponsivityMask);
                InParameters->Get(NVSDK_NGX_Parameter_DLSS_ResponsivityMask, &origDlssResponsivityMask);

                InParameters->Set(NVSDK_NGX_Parameter_DLSSD_ResponsivityMask, (void*) _responsivityMaskBuffer);
                InParameters->Set(NVSDK_NGX_Parameter_DLSS_ResponsivityMask, (void*) _responsivityMaskBuffer);
                responsivityMaskInjected = true;
            }
        }
    }

    NVSDK_NGX_Result nvResult;

    if (NVNGXProxy::D3D12_EvaluateFeature() != nullptr)
    {
        ProcessEvaluateParams(InParameters);

        nvResult = NVNGXProxy::D3D12_EvaluateFeature()(InCommandList, _p_dlssdHandle, InParameters, NULL);

        if (nvResult != NVSDK_NGX_Result_Success)
        {
            LOG_ERROR("_EvaluateFeature result: {0:X}", (unsigned int) nvResult);
            return false;
        }
    }
    else
    {
        LOG_ERROR("_EvaluateFeature is nullptr");
        return false;
    }

    // Restore original parameters
    if (responsivityMaskInjected)
    {
        InParameters->Set(NVSDK_NGX_Parameter_DLSSD_ResponsivityMask, origDlssdResponsivityMask);
        InParameters->Set(NVSDK_NGX_Parameter_DLSS_ResponsivityMask, origDlssResponsivityMask);
    }
    if (colorBeforeTransInjected)
    {
        InParameters->Set(NVSDK_NGX_Parameter_DLSSD_ColorBeforeTransparency, origColorBeforeTrans);
    }
    if (modifiedGuidesInjected)
    {
        InParameters->Set(NVSDK_NGX_Parameter_DiffuseAlbedo, origDiffuseAlbedo);
        InParameters->Set(NVSDK_NGX_Parameter_GBuffer_Albedo, origGBufferAlbedo);
        InParameters->Set(NVSDK_NGX_Parameter_SpecularAlbedo, origSpecularAlbedo);
        InParameters->Set(NVSDK_NGX_Parameter_GBuffer_SpecularAlbedo, origGBufferSpecularAlbedo);
    }

    _frameCount++;

    return true;
}

DLSSDFeatureDx12::DLSSDFeatureDx12(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters)
    : IFeature(InHandleId, InParameters), IFeature_Dx12(InHandleId, InParameters),
      DLSSDFeature(InHandleId, InParameters)
{
    if (NVNGXProxy::NVNGXModule() == nullptr)
    {
        LOG_INFO("nvngx.dll not loaded, now loading");
        NVNGXProxy::InitNVNGX();
    }

    LOG_INFO("binding complete!");
}

DLSSDFeatureDx12::~DLSSDFeatureDx12()
{
    ReleaseResponsivityResources();

    if (State::Instance().isShuttingDown)
        return;

    if (NVNGXProxy::D3D12_ReleaseFeature() != nullptr && _p_dlssdHandle != nullptr)
        NVNGXProxy::D3D12_ReleaseFeature()(_p_dlssdHandle);
}
