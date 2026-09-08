# Alpha 0.9.4 lifecycle checkpoint

Parent/control: `nr/stable-alpha-0.9.4`, commit `052f477f5b467be014400858de53301f52b1cd63`.
Implementation branch: `exp/alpha-0.9.4-lifecycle`.
Scope authorized September 8, 2026: the lifecycle checkpoint from the Alpha 0.4 audit,
revalidated against the promoted stable Alpha 0.9.4 source. No Alpha 0.4 patch was replayed.

## Hypothesis and changed variable

Correct ownership at an NGX session boundary prevents stale feature/parameter reuse after
shutdown and reinitialization. Stable 0.9.4 already includes readiness-driven transitions,
bounded retries, successful-evaluation accounting, and ordinary D3D12 NR-before-core shutdown.
Those existing fixes are retained. Remaining changes:

- Both D3D12 and Vulkan shutdown variants tear down NR before native NGX. Teardown is
  idempotent, including Shutdown1 calling the ordinary shutdown wrapper afterward.
- The direct and proxy D3D12 paths destroy their caller-owned capability parameters.
  Proxy retry/init and float-discovery state are reset for the next session.
- Forwarder exports explicitly shut down their initialized snippet generation. A failed
  native snippet shutdown prevents host NR reinitialization for the remainder of the process.
  DLL modules remain loaded to preserve callback/code addresses.
- Private DLAA live/retired features and outputs, and exposure-scan retained resources,
  join D3D12 session teardown. Cached dimensions/reset/failure state are cleared.
- D3D12 retains its owning device identity; a new device without explicit shutdown bypasses
  NR instead of reusing device-bound objects or immediately freeing pending GPU work.
- Host Pre-SR/After-SR evaluation and teardown are serialized. Session closure blocks new
  NR evaluations until explicit host initialization. This is not a general menu/config race fix.
- Vulkan checks device identity before reading old mapped memory. Its pre-existing
  unannounced-device-loss abandonment policy is retained, with forwarder state reset.
  Abandoned driver-owned parameter memory is not dereferenced on a presumed dead generation.

## Configuration and deployment

No INI or defaults changed. Build-only validation records the unchanged project INI at
`C:\OptiScaler-NR-Dev\docs\config-baselines\MHWilds\OptiScaler.ini`, SHA256
`DC356455405206E90AA2EF66F06A59EDCE7B80E4B70EB4AE26932488066A2D34`.
The observed live Wilds INI differs (SHA256
`CE569F2AE9052D7485F6921DDF427F4062EED79CF2C5D7B63C1770E8BCCF1353`);
neither is replaced by this implementation. No installation or new stable promotion is implied.
The two output DLLs must be delivered as a matched pair: an old forwarder lacks the required
shutdown exports and is rejected before feature creation.

## Validation and remaining gate

`tests/nr_forwarder_lifecycle.cpp` exercises the production forwarder with fake NGX entry points:
100 D3D12 restart cycles, same/different-device handling, cold/repeated shutdown, missing
cleanup export, init/shutdown failure, Vulkan restart, and abandonment without a driver call.
It does not load the model or validate GPU execution.

Release/x64 compilation, artifact hashes, source immutability, and strict patch verification
are recorded in the workspace build/handoff evidence. Runtime result remains **Inconclusive**;
decision: **keep experimental**. Runtime checks must include normal shutdown and Shutdown1,
same-device restart, device recreation, NR/Performance Mode switches, and private-DLAA/proxy
paths if enabled. Verify actual NR evaluation/output, not merely no crash.

GPU completion fences, ring reuse, retirement, readback completion, general shared-state
synchronization, and resource-state error epilogues belong to subsequent checkpoints.
In particular, this checkpoint does not certify teardown while unsubmitted/pending game
command lists still reference NR resources. The host must quiesce rendering before shutdown.
Vulkan unannounced device replacement retains the prior assumption that the old device was
destroyed; multi-live-device support is not added here.
