# Numeric artifacts, parameter separation, and updates

## `policy.wind` v1 (1,152 bytes)

All integer/float data are little-endian; float32 uses IEEE 754. Header padding is zero.

| Byte offset | Bytes | Meaning |
|---:|---:|---|
| 0 | 8 | `WDSWND1` plus NUL |
| 8 | 4 | format version = 1 |
| 12 | 4 | state dimension = 16 |
| 16 | 4 | alignment = 64 |
| 20 | 4 | float count = 272 |
| 24 | 40 | reserved |
| 64 | 1,024 | row-major W[16,16], output dimension first |
| 1,088 | 64 | b[16] |

File offsets are multiples of 64 for each tensor. The C++ loader decodes endian explicitly into `alignas(64) Policy`; it does not assume file mapping address alignment or cast an arbitrary byte buffer to floats. Counts, schema, magic, finiteness, parameter bounds and full-file checksum are verified before use.

The JSON manifest includes immutable ID, policy version, state/training/evaluator versions, dtype, shape, alignment, checksum, maturity, compatibility and evidence. `weights.json` is the lightweight edit/train format used by the converter. New versions use new directories; the packer rejects existing output directories. Existing instances pin the policy version and checksum.

FNV checksums provide accidental corruption detection only. Signing and a cryptographic supply-chain policy are future requirements for untrusted distribution.

## RNA microtiles

`RnaTile` is exactly 256 bytes and has 64-byte alignment:

| Float slots | Count | Meaning |
|---|---:|---|
| 0–31 | 32 | normalized hashed memory key |
| 32–47 | 16 | numeric features; slot 32 currently reliability, others reserved |
| 48–63 | 16 | reserved zero |

The authoritative record also has root ID, engram ID/version, packet ID, text, source, claim status, uncertainty/cost, route score and trace. These strings/IDs remain on the CPU graph side. They are not pointers embedded in GPU payloads.

`export_rna.py` validates the chain's checksum/sequence and reconstructs latest non-retired memories. It writes a 64-byte `WDSRNA1` header with schema, record count, stride 256 and alignment 64, then contiguous tiles. Its sibling `.index.json` maps row numbers to provenance and pins the journal offset/checksum. Export is explicit and should run against a stopped server or an immutable journal copy; it is not an incremental live GPU upload process.

## Hot-update design boundary

Implemented now: receive a bounded RNA message; validate; append/sync a new memory version; use it at the next attention step; convert current keys into aligned upload artifacts when requested. Fast Wind state changes remain per-instance checkpoints. Small learned theta candidates are separately converted into aligned Wind packages.

Next device integration: maintain immutable buffers per generation, upload only changed rows to a staging generation, fence completion, and publish the generation ID for subsequent batches. In-flight episodes retain old buffers until completion. Do not write into tensors referenced by an active graph. Batch by policy/schema/device/dtype; keep provenance and route decisions on the CPU. The GGUF backbone remains immutable throughout.

## Promotion evidence

A numerically accepted candidate has source provenance, parent checksum, declared objective, train/held-out sample counts, before/after loss, drift, and checksum of the exported policy. These fields describe only their stated evaluation scope. No route score or fact status changes as a side effect of Wind training. New source facts may enter independently; they remain attributable and revisable.


## v0.3 candidate metadata (unchanged binary ABI)

The native trainer emits the existing 1152-byte artifact and manifest schema 1. `action_domain`, `feature_schema`, `action_schema`, `actions`, `action_decoder`, `evaluator_version` and `training` are additive manifest metadata. Loaders continue to validate the same checksum, dimensions, bounds, alignment and maturity enum. No action table is appended to `policy.wind`.

Task action selection starts from zero state and feedback with the declared 16 feature lanes, calls the existing policy step, and takes argmax over its first seven outputs. Remaining outputs are retained to preserve the ABI. General language episodes keep the original recurrent movement rule. Exported candidates are `seeded`; reports never mutate them to a new maturity or activate them.

Native `.checkpoint.json` files contain job/parent/backend fingerprints, epoch, current/best weights and validation loss. They are optimizer state outside the loadable package. The SGD backend has no hidden momentum state. A package is staged, verified by the production loader and published under a new directory. Native epoch checkpoint replacement and package publishing support ordinary interruption; arbitrary power-loss transactions are not promised.
