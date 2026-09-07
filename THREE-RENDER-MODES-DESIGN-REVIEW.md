# Alpha 0.9.1 three rendering modes -- design review

## Parent and control

- Parent: `6a8619e5d860e7a715e2fdcab8f52622e67b8f21` (`test 1.0: complete dual-mode RR NR routing`), the recorded Alpha 0.9.1 / Test 1.0 design.
- Control preserved: Alpha 0.9.2, `52fc896b99327f0e1b963d8a54b6c869b6d00a7d`.
- Proposed routes: Quality = `RR -> DLSS SR -> NR`; Performance = `RR -> NR -> DLSS SR`; Private Queue = the Performance route plus a creation-time private-queue experiment.

## Reference review

Neural Upstream issue 3 reports a Stellar Blade compatibility fix, not a measured scheduling optimization. Its actionable claims are: use the actual Color dimensions, bind output before feature creation, evaluate through the module that created the feature, use a UAV barrier before consumption, and defer/fence destruction across resolution changes. The Alpha parent already uses actual resource descriptions, a retained forwarder for both creation/evaluation, an output barrier before resolve consumption, and deferred retirement. This experiment does not import an external binary or replace `nvngx_dlssnr.dll`.

The Liteonfire repository distributes only a ReShade add-on binary and README. It describes the same Color/subrect mismatch fix and explicitly says not to replace `nvngx_dlssnr.dll`. It supplies no source, timing, overlap, or asynchronous-execution evidence. It is reference-only.

## Historical private-queue review

Commit `08ea4edf432a41087bcc601665739bd5036d7abd` creates an internal SR feature on a private direct queue, executes its creation command list, signals a fence, and blocks the CPU until it completes. Its own commit message calls this a one-time creation hitch avoidance. It does **not** submit NR evaluation per frame to that queue and does not establish GPU overlap. It does not disable or bypass RR.

Therefore this experiment labels the option **Private Queue**, not Async. The mode uses a private queue only for NR feature creation/recreation, fences it to completion, and keeps the selected Performance route. Per-frame NR remains on the game command list and is not claimed asynchronous.

## Hypothesis

The Alpha 0.9.1 dual route can be exposed as a clear three-mode UI while preserving one NR evaluation per rendered frame. A private queue may make NR feature creation/recreation safer on games sensitive to creation commands in the game command list; it is not expected to reduce steady-state NR model time.
