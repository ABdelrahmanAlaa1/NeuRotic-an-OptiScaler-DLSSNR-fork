# Alpha 0.9.4 GPU-safety checkpoint (part 2)

## Provenance and scope

Parent/control: `nr/stable-alpha-0.9.4`, full commit
`dcb3e083a4951ec9579b1e6b84d1e590d50243a2`, worktree
`C:\OptiScaler-NR-Dev\worktrees\stable-alpha-0.9.4`.
This is the user's merged UI/lifecycle/RR/defaults source, not an earlier Alpha or experiment.
The parent's pending `README.md`, `Changelog.md`, and untracked `ALPHA-0.9.4.md` are
documentation-only, excluded from this experiment, and left untouched.

Implementation: `exp/alpha-0.9.4-gpu-safety`, worktree
`C:\OptiScaler-NR-Dev\worktrees\experiments\alpha-0.9.4-gpu-safety`.
Changed variable: D3D12 NR GPU completion/lifetime handling (original audit H3/H5).
No UI, model tuning, shader math, INI, or defaults changes; no promotion or deployment.
The unchanged inherited worktree INI is recorded for the build, SHA256
`3B637C004A3002222C1A6E3B20774B9CA3FAE68D8E21AD71D8616CF9E6342445`.

Hypothesis: replacing frame-age assumptions with actual submission completion prevents
premature resource retirement, descriptor/upload overwrite, and readback mapping when the
GPU or submission thread lags. A completed closed command list is still replayable, so
completion alone is insufficient for CPU overwrite or ordinary resource retirement.

## Implementation and ownership

- `NrGpuSafety` attaches reference-counted recording cookies to native command lists and
  fence timelines to submitting queues. Cookies own tickets/fences, never the list/queue
  that owns them: there is no COM reference cycle. The tracker observes native
  ExecuteCommandLists and successful Reset; list destruction also invalidates recordings.
- Register before recording NR commands. A submission signals a private fence on the
  actual queue after ExecuteCommandLists. Replays update every relevant queue's completion
  point. Reset may happen before GPU completion: the ticket survives that Reset.
- Reuse requires recording invalidation AND completion of all observed executions.
  Reset/destruction of an unsubmitted list safely cancels its ticket but does not produce
  readable data. Device loss (`UINT64_MAX`) and failed submission tracking never count as
  successful completion. Unsupported hooks or exhausted capacity bypass further NR work.
- The tracker does not introduce queue Wait calls or CPU waits in ordinary rendering.
  The host retains responsibility for ordering shared rendering inputs/history across
  queues. Completion tracking is not an automatic repair of unordered multi-queue NR use.
- NR/private-DLAA retired features and scratch resources retain completion snapshots.
  Supersampling filter changes retire the old scaler (including its PSO, descriptors,
  upload/query resources). Optional proxy features and their caller-owned parameters are
  retired together, with parameters destroyed after the feature.
- The compose shader's existing 48 slots are completion-gated. NR-owned OS scalers get
  48 independent descriptor/upload slots each; ordinary Output Scaling keeps its existing
  allocation path. Shared CPU constants become per-call locals. A busy compose pool skips
  NR before its output transitions; a busy scaler uses the existing caller fallback.
- NR timers (3 slots), meter/calibration (4 each), and exposure scan readbacks only reuse
  completed invalidated recordings. CPU reads additionally require an actual submission.
  NR timing uses the submitting queue's frequency; multi-queue ambiguous timing is omitted.
  Exposure scan pairs readbacks with recorded source identities/generations. It does not
  extend foreign placed-resource lifetimes beyond the existing release notification.
- Capture stops recording at the requested bound, checks format/dimensions/sample/mip/array
  compatibility before copies, and waits for completion tickets. A resized or canceled
  batch is safely discarded and rearmed. Partial allocation is released before any copy.
- Explicit host shutdown closes NR admission, then polls submitted fences for at most
  two seconds. Unsubmitted live recordings fail immediately. Failure retains the generation,
  blocks native NGX teardown/restart, and logs the reason; a process restart is required.
  This is deliberate fail-closed retention, not successful cleanup or automatic recovery.

## Synchronization, bounds, and consequences

The inherited lifecycle/render locks serialize host evaluations and shutdown. The tracker
has its own recursive mutex; Execute/Reset/cookie callbacks only use that tracker mutex,
not NR lifecycle/render locks. Host recording acquires lifecycle/render, then tracker.
Hook installation has a separate mutex. The tracker and hooks have process lifetime and
pin their module so late list/queue cookie callbacks do not target unloaded code.
Normal completed ticket/fence ownership is reclaimed; failed generations are not freed
by NR shader/timer static destructors while work may still reference them.

Capacity is finite: 256 outstanding recordings, 48 compose slots, 48 slots per NR scaler,
128 general retired objects plus the current transition batch, and 32 retired private-DLAA
or proxy generations. Saturation bypasses NR/rebuilds instead of guessing completion.
Closed lists deliberately retained for replay can keep slots busy even after GPU completion.
Consequences: some NR frames/telemetry samples can be skipped under backlog; memory remains
live until proven safe. New CPU bookkeeping, completion polls, per-submission fence signals,
and additional NR scaler upload/descriptor allocations need real-game cost measurement.
No claim of zero performance cost is made.

Shutdown still requires the host to stop future submissions/replays once NGX shutdown
begins. A drain cannot prevent a host from replaying an old completed list after shutdown.
Unknown/native-wrapper hook combinations can conservatively retain unobserved recordings;
their integration must be checked in the intended game before this becomes a candidate.

## Evidence and remaining gate

Run the existing standalone software-D3D12 test from PowerShell:

```powershell
& 'C:\OptiScaler-NR-Dev\worktrees\experiments\alpha-0.9.4-gpu-safety\tests\Run-NrGpuSafety.cmd'
```

It compiles the production tracker/capture code and uses Microsoft's WARP device, no game
or NR model. Verified checks: unsubmitted cancellation, 1,000 premature reuse/read attempts
with GPU work deliberately blocked, Reset before completion, completed-list replay,
multiple completion points for cross-queue replay, a host inter-queue Wait/Signal dependency
without an injected cycle, in-flight list destruction, retirement snapshots, capture resize
and cancellation, explicit failure, and actual software-device removal. Exit code: 0.
Two inherited `_wfopen` deprecation warnings are not test failures.
Test output: `C:\OptiScaler-NR-Dev\logs\nr-gpu-safety-test.txt`.

Build through the canonical build-only entry point; no game/configuration installation:

```powershell
& 'C:\OptiScaler-NR-Dev\scripts\Build-Install.cmd' alpha-0.9.4-gpu-safety -BuildOnly -RecordIniPath 'C:\OptiScaler-NR-Dev\worktrees\experiments\alpha-0.9.4-gpu-safety\OptiScaler.ini'
```

Build exit, exact commit, retained matched DLL hashes, configuration snapshot, and source
immutability are recorded by that builder's manifest. Do not infer build success from this
document or timestamps. The handoff points to the actual manifest.

Game-runtime result: **Inconclusive / not tested**. Decision: **keep experimental**.
The user explicitly requested no game launch. Neither deployment nor game testing is
authorized in this checkpoint run. Later user-run validation should check live NR output
and advancing counters, NR/Performance Mode toggles, scale/filter/resolution switches,
capture during resize, normal exit/restart, and a long session for memory and timing trends.
Keep the same INI for source comparison; change one setting at a time for transition tests.

Remaining audit work is not silently claimed fixed: resource-state/error-return epilogues
(H4), broader allocation-bundle cleanup (M1), shared menu/config synchronization (M2), and
the optional exposure scanner's assumed resource states/foreign heap contract (M3).
This checkpoint does not extend Vulkan behavior, add multi-live-device support, revive the
disabled private-creation-queue experiment, or certify arbitrary unordered NR submissions.
