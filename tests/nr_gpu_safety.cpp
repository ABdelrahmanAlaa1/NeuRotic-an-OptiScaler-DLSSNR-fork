// Runs the production submission/reset hooks on a software D3D12 device. No game or NR model.
#define NR_GPU_SAFETY_TEST
#include "../OptiScaler/dlssnr/NrGpuSafety.cpp"
#include "../OptiScaler/dlssnr/DlssNr_Capture.h"
#include <dxgi1_4.h>
#include <cassert>
#include <cstdio>

using Microsoft::WRL::ComPtr;
namespace Safety = DlssNr::GpuSafety;
static void Check(HRESULT hr) { assert(SUCCEEDED(hr)); }

int main()
{
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter> warp;
    ComPtr<ID3D12Device> device;
    Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
    Check(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
    Check(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
    ComPtr<ID3D12CommandQueue> queue, otherQueue;
    D3D12_COMMAND_QUEUE_DESC queueDesc {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)));
    Check(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&otherQueue)));
    ComPtr<ID3D12CommandAllocator> allocator, nextAllocator;
    Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
    Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&nextAllocator)));
    ComPtr<ID3D12GraphicsCommandList> list;
    Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list)));
    auto submit = [&](ID3D12CommandQueue* q) { ID3D12CommandList* lists[] = {list.Get()}; q->ExecuteCommandLists(1, lists); };

    auto abandoned = Safety::Record(list.Get());
    assert(abandoned && !Safety::Reusable(abandoned) && !Safety::Drain(0));
    Check(list->Close());
    Check(list->Reset(allocator.Get(), nullptr));
    assert(Safety::Reusable(abandoned) && !Safety::Readable(abandoned));

    ComPtr<ID3D12Fence> gate;
    Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate)));
    Check(queue->Wait(gate.Get(), 1));
    auto pending = Safety::Record(list.Get());
    auto retirement = Safety::Pending();
    Check(list->Close());
    submit(queue.Get());
    Check(list->Reset(nextAllocator.Get(), nullptr)); // list Reset is legal while old work runs
    for (int i = 0; i < 1000; ++i)
        assert(!Safety::Reusable(pending) && !Safety::Readable(pending) && !Safety::Reusable(retirement));
    assert(!Safety::Drain(1));
    Check(gate->Signal(1));
    assert(Safety::Drain(5000));
    assert(Safety::Reusable(pending) && Safety::Readable(pending) && Safety::Reusable(retirement));
    assert(Safety::TimestampFrequency(pending) > 0);

    auto replay = Safety::Record(list.Get());
    Check(list->Close());
    submit(queue.Get());
    assert(Safety::Drain(5000));
    assert(!Safety::Reusable(replay)); // completed but still executable
    Check(queue->Wait(gate.Get(), 2));
    submit(queue.Get());
    Check(list->Reset(allocator.Get(), nullptr));
    assert(!Safety::Reusable(replay) && !Safety::Drain(0));
    Check(gate->Signal(2));
    assert(Safety::Drain(5000) && Safety::Reusable(replay));

    // Track both queues without introducing a cycle into the host's Wait/Signal graph.
    Check(queue->Wait(gate.Get(), 3));
    auto first = Safety::Record(list.Get());
    Check(list->Close());
    submit(queue.Get());
    Check(list->Reset(nextAllocator.Get(), nullptr));
    auto second = Safety::Record(list.Get());
    Check(list->Close());
    submit(otherQueue.Get());
    Check(list->Reset(allocator.Get(), nullptr));
    assert(!Safety::Reusable(first));
    // This queue must be able to release the first queue, even though it submitted NR second.
    Check(otherQueue->Signal(gate.Get(), 3));
    assert(Safety::Drain(5000) && Safety::Reusable(first) && Safety::Reusable(second));

    // A single recording replayed on another queue needs both completion points.
    auto multiQueue = Safety::Record(list.Get());
    Check(list->Close());
    submit(queue.Get());
    assert(Safety::Drain(5000));
    Check(otherQueue->Wait(gate.Get(), 4));
    submit(otherQueue.Get());
    Check(list->Reset(nextAllocator.Get(), nullptr));
    assert(!Safety::Reusable(multiQueue) && !Safety::Readable(multiQueue));
    Check(gate->Signal(4));
    assert(Safety::Drain(5000) && Safety::Reusable(multiQueue));
    assert(Safety::TimestampFrequency(multiQueue) == 0); // no ambiguous timing calculation

    auto destroyedInFlight = Safety::Record(list.Get());
    Check(list->Close());
    Check(queue->Wait(gate.Get(), 5));
    submit(queue.Get());
    list.Reset();
    assert(!Safety::Reusable(destroyedInFlight));
    Check(gate->Signal(5));
    assert(Safety::Drain(5000) && Safety::Reusable(destroyedInFlight));
    Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list)));

    auto destroyed = Safety::Record(list.Get());
    list.Reset();
    assert(Safety::Reusable(destroyed) && !Safety::Readable(destroyed));
    assert(Safety::Drain(0));
    Safety::NewSession();
    assert(Safety::Pending().empty());

    // Capture must not copy a new shape into an old footprint or free an unsubmitted copy.
    Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list)));
    auto makeTexture = [&](UINT width)
    {
        ComPtr<ID3D12Resource> texture;
        D3D12_HEAP_PROPERTIES heap {}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = width; desc.Height = 4; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&texture)));
        return texture;
    };
    auto smallTexture = makeTexture(4), largeTexture = makeTexture(8);
    capture::FrameCapture capture;
    capture.request(2);
    capture.record(list.Get(), device.Get(), smallTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                   smallTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    assert(capture.progress() == 1);
    capture.record(list.Get(), device.Get(), largeTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                   largeTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    assert(capture.progress() == 1 && !capture.readyToWrite());
    Check(list->Close());
    Check(list->Reset(nextAllocator.Get(), nullptr)); // discard the old recording
    assert(capture.readyToWrite());
    assert(capture.write("unused-capture-test-path").empty());
    assert(capture.isActive() && capture.progress() == 0); // rearmed, no files written
    capture.release();
    list.Reset();

    // Fence failure is a permanent failure, never mistaken for permission to reclaim.
    auto failed = std::make_shared<Safety::Recording>();
    failed->sealed = true;
    failed->failed = true;
    assert(!Safety::Reusable(failed) && !Safety::Readable(failed));
    ComPtr<ID3D12Device5> removable;
    Check(device.As(&removable));
    removable->RemoveDevice();
    // The sentinel UINT64_MAX is device loss, not a very large successful fence value.
    assert(!Safety::Reusable(pending) && !Safety::Readable(pending));
    std::puts("PASS: unsubmitted cancellation, 1000 premature reuse/read checks, delayed GPU completion,");
    std::puts("      replay, independent queues/host dependencies, retirement, capture shape changes, failure and device loss.");
}
