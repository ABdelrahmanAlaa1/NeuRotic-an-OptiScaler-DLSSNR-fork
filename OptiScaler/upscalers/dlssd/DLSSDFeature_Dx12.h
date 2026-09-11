#pragma once
#include "DLSSDFeature.h"
#include <upscalers/IFeature_Dx12.h>
#include <shaders/rcas/RCAS_Dx12.h>
#include <string>

class DLSSDFeatureDx12 : public DLSSDFeature, public IFeature_Dx12
{
  private:
    ID3D12Resource* _responsivityMaskBuffer = nullptr;
    ID3D12Resource* _responsivityUploadBuffer = nullptr;
    unsigned int _responsivityWidth = 0;
    unsigned int _responsivityHeight = 0;
    float _lastResponsivityValue = -999.0f;

    void ReleaseResponsivityResources();

  protected:
    bool InitDLSSD(ID3D12GraphicsCommandList* InCommandList, NVSDK_NGX_Parameter* InParameters);

  public:
    bool InitInternal(ID3D12GraphicsCommandList* InCommandList, NVSDK_NGX_Parameter* InParameters) override;
    bool EvaluateInternal(ID3D12GraphicsCommandList* InCommandList, NVSDK_NGX_Parameter* InParameters) override;

    feature_version Version() override { return DLSSDFeature::Version(); }
    Upscaler GetUpscalerType() const final { return DLSSDFeature::GetUpscalerType(); }
    API Api() const override { return IFeature_Dx12::Api(); }
    bool CallsUpscalerEndByItself() override { return IFeature_Dx12::CallsUpscalerEndByItself(); }

    bool IsWithDx12() override { return false; }

    DLSSDFeatureDx12(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters);
    ~DLSSDFeatureDx12();
};
