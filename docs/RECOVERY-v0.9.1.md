# v0.9.1 — recover a completed English training run

The supplied Windows log records all eight training epochs and the final epoch's validation result, then stops before `language.finished`. The old supervisor only displayed the executable path; it discarded the exit status. That evidence places the failure after optimization, during candidate finalization, but does not establish the exact Windows exception.

The training, checkpoint loading and candidate verification paths contained several large stack-resident tensors and return-value temporaries. v0.9.1 keeps these buffers in aligned heap allocations and decodes into their destination directly. The on-disk tensor layout, optimization objective, job fingerprint and `policy.wind` ABI are unchanged. This removes a reproduced low-stack crash risk without increasing the process stack reservation. Windows generally has a smaller default stack than the Linux environment used for the original release; see [Microsoft's stack-size documentation](https://learn.microsoft.com/en-us/cpp/build/reference/f-set-stack-size).

## Resume your existing run

1. Stop the workshop with Ctrl+C.
2. Extract this update over `U:\wds-ai`. Keep your local configuration, data, runs and candidate/checkpoint directories. Do not delete them or use `--replace`.
3. Restart with the existing working model and runtime-pack configuration:

```bat
python tools/workshop.py --config configs/sixstars.local.json start --ui --trace --language-mode assisted
```

4. Refresh the browser and confirm v0.9.1 in the footer.
5. Select the same project and **Winds & learning → English → Train / resume Wind**.

With the same curriculum, approved examples and eight-epoch job, the trainer restores the completed checkpoint and proceeds to packaging. It does not repeat completed epochs. New approved examples or changed training settings create a different job. The final candidate still requires explicit activation; this update never activates it automatically.

## What you will see

The workshop now reports loading/restoring, packaging, copying learning traces, generating activation traces, validating, publishing, evaluating and registering. Native failures retain the numeric exit code, a readable Windows exception when known, and the error text. The final epoch's score is not a publication gate.

Each English run keeps append-only diagnostic files under its existing run folder:

- `sixstars-wds-runs/english-<run-id>/trainer.log`
- `sixstars-wds-runs/english-<run-id>/evaluation.log`
- `sixstars-wds-runs/english-<run-id>/report.json` after successful evaluation

These logs are written even when general trace recording is disabled. If the run fails again, the displayed error names the full log path. A Windows status such as `0xC00000FD` identifies stack overflow; other statuses are preserved rather than guessed. See [Microsoft's exception reference](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/debugging-a-stack-overflow).

## Verification and limits

The previous Linux executable crashed with a 256 KiB stack limit. The updated native trainer completes fresh training, checkpoint restoration, candidate export and evaluation under that limit. A regression deliberately blocks export after the last epoch, removes the fixture obstruction, then verifies successful recovery with byte-identical checkpoint files and no repeated epochs. Existing candidates are also checked for tensor corruption on retry.

The full original eight-epoch v0.9 checkpoint was resumed under the same stack limit; its published package remained byte-for-byte unchanged. This establishes checkpoint compatibility on the tested Linux build. It does not prove the original Windows exit was stack overflow, and the user's Windows executable/GPU cannot be run here.

This patch fixes execution/recovery and diagnostics. It does not improve the experimental student's language quality or change the earlier held-out evaluation.

Final checks: 27/27 CTest tests passed for the native llama/CPU build and 27/27 for the runtime-pack build with synthetic compatible libraries. The React production bundle builds; no browser interaction tests were run.
