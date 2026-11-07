# Experimental English sequence seed

This is the actual eight-epoch native training result on the included synthetic corpus. It is not automatically activated.

Validation bits per byte fell from 8.0003 to 2.7576. Locked-test bits per byte: 2.9881. The student matched **0 of 240** held-out expected final answers; its sampled outputs are not usable general English. None passed the default selected-symbol threshold. This package demonstrates the learning/artifact/provenance pipeline, not a completed language understanding system.

Use assisted mode with your existing Qwen model for useful conversation while collecting reviewed examples. See `evaluation.json` for the exact measurements and representative generated outputs. The test was evaluated after training and did not select an epoch.

`manifest.json` lists each tensor's name, shape, byte offset and precision. A trace's `output_weight_index` identifies a scalar inside the `output` tensor; its byte offset is `output.offset_bytes + 4 * output_weight_index`. The stored activation and contribution are local student measurements, not Qwen fact addresses.
