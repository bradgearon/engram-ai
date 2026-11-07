"""Native API test with tiny random Qwen tensors. This is not a pretrained quality benchmark."""
import pathlib
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(sys.argv[2]) / 'gguf-py'))
import numpy as np
import gguf


def fixture(path):
    writer = gguf.GGUFWriter(str(path), 'qwen3')
    writer.add_name('wds-qwen-api-fixture-random-no-knowledge')
    writer.add_context_length(2048)
    writer.add_embedding_length(32)
    writer.add_block_count(2)
    writer.add_feed_forward_length(64)
    writer.add_head_count(4)
    writer.add_head_count_kv(2)
    writer.add_layer_norm_rms_eps(1e-6)
    writer.add_rope_dimension_count(8)
    writer.add_rope_freq_base(1000000)
    writer.add_tokenizer_model('llama')
    tokens = ['<unk>', '<s>', '</s>'] + [f'<0x{i:02X}>' for i in range(256)]
    writer.add_token_list(tokens)
    writer.add_token_scores([0.] * len(tokens))
    writer.add_token_types([2, 3, 3] + [6] * 256)
    writer.add_unk_token_id(0); writer.add_bos_token_id(1); writer.add_eos_token_id(2)
    writer.add_add_bos_token(True)
    writer.add_chat_template("{% for message in messages %}{{ message['content'] }}{% endfor %}")
    rng = np.random.default_rng(603)
    def add(name, shape, norm=False):
        writer.add_tensor(name, np.ones(shape, dtype=np.float32) if norm else rng.normal(0, .02, shape).astype(np.float32))
    add('token_embd.weight', (len(tokens), 32))
    add('output.weight', (len(tokens), 32))
    add('output_norm.weight', (32,), True)
    for i in range(2):
        prefix = f'blk.{i}.'
        for name in ('attn_norm.weight', 'ffn_norm.weight'): add(prefix + name, (32,), True)
        for name in ('attn_q_norm.weight', 'attn_k_norm.weight'): add(prefix + name, (8,), True)
        for name, shape in [('attn_q.weight', (32, 32)), ('attn_k.weight', (16, 32)),
                            ('attn_v.weight', (16, 32)), ('attn_output.weight', (32, 32)),
                            ('ffn_gate.weight', (64, 32)), ('ffn_up.weight', (64, 32)), ('ffn_down.weight', (32, 64))]:
            add(prefix + name, shape)
    writer.write_header_to_file(); writer.write_kv_data_to_file(); writer.write_tensors_to_file(); writer.close()


with tempfile.TemporaryDirectory() as tmp:
    path = pathlib.Path(tmp) / 'random-qwen.gguf'
    fixture(path)
    subprocess.run([sys.argv[1], str(path)], check=True, timeout=60)
