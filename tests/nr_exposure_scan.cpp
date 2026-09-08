// Compile the production scanner with minimal configuration/logging substitutes.
// This test never records GPU work or claims a resource-state/lifetime contract.
#include "../OptiScaler/dlssnr/DlssNr_ExposureScan.cpp"
#include <cassert>
#include <future>
#include <thread>

namespace DlssNr::GpuSafety
{
Ticket Record(ID3D12GraphicsCommandList*) { assert(false); return {}; }
bool Reusable(const Ticket&) { assert(false); return false; }
bool Readable(const Ticket&) { assert(false); return false; }
}

namespace Scan = DlssNr::ExposureScan;

struct AllocationCounts
{
    unsigned int live = 0;
    unsigned int released = 0;
};

// Minimal reference-counted resource for the production ComPtr staging helper.
// No device mock or fabricated D3D12 resource state is needed for allocation faults.
struct AllocationResource
{
    explicit AllocationResource(AllocationCounts& counts) : counts(counts) { ++counts.live; }
    ULONG AddRef() { return ++references; }
    ULONG Release()
    {
        const auto remaining = --references;
        if (remaining == 0)
        {
            --counts.live;
            ++counts.released;
            delete this;
        }
        return remaining;
    }
    AllocationCounts& counts;
    ULONG references = 1;
};

static void TestReadbackAllocationFaults()
{
    // Include every existing-slot pattern, not just an empty ring, and fail each
    // missing allocation in turn. Existing pointers must survive byte-for-byte.
    for (unsigned int mask = 0; mask < (1u << Scan::kSlots); ++mask)
    {
        unsigned int existing = 0;
        for (unsigned int i = 0; i < Scan::kSlots; ++i)
            if (mask & (1u << i)) ++existing;
        const auto missing = Scan::kSlots - existing;
        for (unsigned int failAt = 0; failAt <= missing; ++failAt)
        {
            AllocationCounts counts;
            AllocationResource* ring[Scan::kSlots] {};
            AllocationResource* original[Scan::kSlots] {};
            for (unsigned int i = 0; i < Scan::kSlots; ++i)
                if (mask & (1u << i)) original[i] = ring[i] = new AllocationResource(counts);

            unsigned int calls = 0;
            const bool succeeded = Scan::EnsureReadbackBundle(ring,
                [&](const D3D12_HEAP_PROPERTIES& heap, const D3D12_RESOURCE_DESC& desc,
                    AllocationResource** resource) -> HRESULT {
                    assert(heap.Type == D3D12_HEAP_TYPE_READBACK);
                    assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);
                    assert(desc.Width == Scan::kStride * Scan::kMaxCandidates);
                    // The real creation hook may reenter the scanner's mutex.
                    (void) Scan::Examined();
                    for (unsigned int i = 0; i < Scan::kSlots; ++i) assert(ring[i] == original[i]);
                    if (calls++ == failAt) return E_OUTOFMEMORY;
                    *resource = new AllocationResource(counts);
                    return S_OK;
                });
            assert(succeeded == (failAt == missing));
            if (!succeeded)
            {
                assert(calls == failAt + 1 && counts.live == existing && counts.released == failAt);
                for (unsigned int i = 0; i < Scan::kSlots; ++i) assert(ring[i] == original[i]);
                unsigned int retryCalls = 0;
                assert(Scan::EnsureReadbackBundle(ring,
                    [&](const D3D12_HEAP_PROPERTIES&, const D3D12_RESOURCE_DESC&,
                        AllocationResource** resource) -> HRESULT {
                        ++retryCalls;
                        *resource = new AllocationResource(counts);
                        return S_OK;
                    }));
                assert(retryCalls == missing);
            }
            assert(counts.live == Scan::kSlots);
            for (unsigned int i = 0; i < Scan::kSlots; ++i)
            {
                assert(ring[i] != nullptr);
                if (original[i] != nullptr) assert(ring[i] == original[i]);
            }
            // A complete ring must not make another allocation call.
            assert(Scan::EnsureReadbackBundle(ring,
                [](const D3D12_HEAP_PROPERTIES&, const D3D12_RESOURCE_DESC&,
                   AllocationResource**) -> HRESULT { assert(false); return E_FAIL; }));
            for (auto* resource : ring) resource->Release();
            assert(counts.live == 0);
        }
    }
    std::puts("PASS: readback allocation faults at every missing slot, staged rollback, existing-slot preservation, retry and no-op reuse.");
}

static void AddMovingCandidate()
{
    std::lock_guard<std::mutex> lock(Scan::g_scanMutex);
    Scan::Tracked candidate;
    candidate.shape = "test buffer";
    candidate.latest = 2.0f;
    candidate.lowest = 1.0f;
    candidate.highest = 2.0f;
    candidate.moves = true;
    candidate.reads = 2;
    Scan::g_scan.tracked.push_back(candidate);
    Scan::g_scan.examined = 1;
}

int main()
{
    TestReadbackAllocationFaults();
    D3D12_RESOURCE_DESC desc {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    for (UINT64 width = 0; width <= 129; ++width)
    {
        desc.Width = width;
        std::string shape;
        unsigned int bytes = 0;
        bool isBuffer = false;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        const bool accepted = Scan::LooksLikeANumber(desc, &shape, &bytes, &isBuffer, &format);
        assert(accepted == (width >= sizeof(float) && width <= 128));
        if (accepted) assert(isBuffer && bytes == sizeof(float) && bytes <= width);
    }

    AddMovingCandidate();
    assert(Scan::Where() == Scan::Verdict::Found);
    const char* borrowed = Scan::Headline();
    const std::string expected = borrowed;
    std::promise<void> otherReported, releaseOther;
    auto release = releaseOther.get_future();
    auto reported = otherReported.get_future();
    std::thread other([&] {
        Scan::ReleaseTrackedResources();
        assert(Scan::Where() == Scan::Verdict::Waiting);
        assert(std::string(Scan::Headline()).find("none shaped") != std::string::npos);
        otherReported.set_value();
        release.wait();
    });
    reported.wait();
    assert(expected == borrowed); // another caller must not overwrite this thread's text
    releaseOther.set_value();
    other.join();

    // Exercise the former Found -> release -> unchecked candidate[0] interleaving.
    auto reader = [] {
        for (int i = 0; i < 20000; ++i)
        {
            const std::string line = Scan::Headline();
            assert(line.find("FOUND") != std::string::npos || line.find("none shaped") != std::string::npos);
        }
    };
    std::thread firstReader(reader), secondReader(reader);
    std::thread releaser([] {
        for (int i = 0; i < 20000; ++i)
        {
            AddMovingCandidate();
            Scan::ReleaseTrackedResources();
        }
    });
    firstReader.join();
    secondReader.join();
    releaser.join();
    Scan::Shutdown();
    std::puts("PASS: scanner buffer copy bounds, per-thread headline ownership, 40000 concurrent headline/release reads.");
    std::puts("LIMIT: no scanner GPU/state/foreign-heap safety is established by these CPU tests.");
}
