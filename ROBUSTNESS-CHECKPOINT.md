# Alpha 0.9.4 robustness checkpoint

Parent/control: `22ee1a119019598c7d5dc20aed8afedd8f3b96e2` (NR-toggle safety),
extending the user-merged 0.9.4 UI/lifecycle and GPU-safety work. The parent was
verified clean before creating this isolated worktree. No other branch is an audit target.

Branch: `exp/alpha-0.9.4-robustness`.
Worktree: `C:\OptiScaler-NR-Dev\worktrees\experiments\alpha-0.9.4-robustness`.

Hypothesis: incomplete resource allocation, independently changing NR settings,
capture shape changes, and unsynchronized readiness/scanner observations can
leave partial state or misreport a transition. Fix those concrete paths without
changing NR defaults, model choice, resolution, or native temporal inputs.

## Implementation

- D3D12 scratch replacement stages all five size-dependent resources before
  publishing replacements. Failure releases only unpublished objects; old live
  resources retain their existing GPU-retirement ownership. Meter/readback rings
  and scanner readbacks publish complete allocation bundles. Guide-clone resize
  allocates a replacement before retiring the old reference.
- A dispatch-local state ledger restores only resources transitioned during this
  invocation, including early failures and unused retained guide clones. Encode,
  downsample and resolve failure bypass further work; successful-evaluation
  readiness advances only after resolve succeeds. The root-state envelope also
  covers feature creation. The existing timer End is idempotent; scoped cleanup
  now covers early returns.
- The disabled-by-default driver proxy follows the same resolve/cleanup path as
  the forwarder. Previously it could count success without composing the result.
  It receives the original jitter as well as motion scaling. No provider fallback
  or new proxy default is introduced.
- All 44 NR options use a shared, process-lifetime configuration mutex and owned
  read values; the underlying optional serialization semantics are preserved.
  Rendering takes one immutable snapshot covering both sides of native upscaling,
  so changing placement during that call cannot schedule both Pre- and Post-SR.
  The routing pair and enable/resume-generation publication are transactional.
  Snapshot allocation failure bypasses NR rather than unwinding through the host.
- Configuration locking covers copies/updates only, never a GPU call. Public DX12
  state/reporting/capture/retry calls follow lifecycle -> backend -> config lock
  order. Vulkan reporting uses its backend mutex. Scanner reads use their scanner
  mutex; resource creation intentionally stays outside it because creation hooks
  can reenter the scanner. No config transaction calls scanner serialization.
- Capture preserves an armed request through unsupported intermediate shapes and
  retires pending copies before starting a replacement-size batch. Null inputs do
  not record work; a second request cannot clear an active capture's directory.
- Scanner candidate selection and headline formatting use one critical section.
  Returned headline text is thread-local; buffers shorter than a float are rejected.
- Readiness distinguishes disabled, loaded, waiting/reset, quarantine, failure,
  and successful evaluation of the current route/resume generation. Stale timing
  is hidden while the requested NR path is not ready. Retry cannot reopen a
  terminal shutdown failure.
- Native Vulkan allocation helpers unwind failed image/memory/view and readback
  construction; the NR shader rejects missing sampler/layout/pool/descriptor sets.

## Build and non-game test gate

Commit first, then use the permanent build-only runner:

```powershell
& 'C:\OptiScaler-NR-Dev\scripts\Build-Install.cmd' alpha-0.9.4-robustness -BuildOnly -RecordIniPath 'C:\Program Files (x86)\Steam\steamapps\common\Crimson Desert\bin64\OptiScaler.ini'
& 'C:\OptiScaler-NR-Dev\worktrees\experiments\alpha-0.9.4-robustness\tests\Run-NrRobustness.cmd'
```

The test runner executes D3D12 WARP lifetime/capture transitions, scanner CPU
concurrency and allocation faults, config coverage/serialization/concurrency and
snapshot-copy faults, scratch allocation rollback/resource-state epilogues, and
readiness-policy transitions. Logs: `C:\OptiScaler-NR-Dev\logs\nr-robustness`;
config details also in `C:\OptiScaler-NR-Dev\logs\nr-config`.

The build runner retains exact Release/x64 DLLs, SHA256, configuration evidence,
source identity and build logs. Recording an INI is read-only, not installation.
Results and exact build provenance belong in the workspace handoff after testing.

## Limits and remaining runtime gate

This is an experimental checkpoint, not stable promotion. The user accepted the
parent's NR-toggle spam result as sufficient to continue, but that short run does
not resolve the separately reported, unlogged startup crash with NR enabled.
No new game launch, deployment, INI change, or visual validation is authorized here.

WARP tests exercise real D3D12 recording/fences and the selected production helpers;
they do not execute NVIDIA Feature 18. CPU tests cannot certify foreign resource
states, aliasing, or foreign-heap lifetime in the optional exposure scan. Its
inherited source-state assumptions remain a limitation; no scanner default changes.
Native Vulkan changes receive compile/code review, not equivalent Vulkan runtime
fault coverage. Whole-middleware config synchronization, arbitrary CPU out-of-memory
recovery, and shared Vulkan base helpers outside NR are not claimed fixed.

After a separately authorized test deployment, retain the live configuration and
test cold startup with NR saved ON, NR enable/disable stress, Performance/Quality
switching, capture during resize, window-mode transitions, and long-session/image
quality comparison against the named control. Preserve each log before another
launch. Status/timers or lack of a crash alone do not prove functioning NR.

Result pending build/tests: Inconclusive. Decision: keep experimental pending the
build-only gate and later user runtime evidence; no defaults/baseline promotion.

## Runtime correction after the first deployment

The first game test produced a confirmed `nvlddmkm` Event 153 at 07:35:20.566,
immediately after enable generation 57 restarted NR on the native RR -> NR path.
Generations 1-56 had completed, followed by roughly ten minutes of successful
4K NR work. The OptiScaler log contains no model error or device-removal return;
the driver faulted while processing submitted work.

The correction no longer resets a retained Feature 18 temporal session in place
after re-enable. It retires that exact handle under its existing GPU completion
snapshot, waits without touching output until vendor release is safe, then creates
a fresh model session. Scratch resources remain live. This costs one or more
bypassed frames plus model-creation latency after each off -> on transition, but
adds no steady-state work and avoids overlapping two full-resolution model
allocations. Runtime validation remains required; this evidence narrows the fault
boundary but does not provide a driver crash dump or internal NVIDIA fault address.
