"""Select existing native libraries; no model/server download and no CUDA compilation."""
import pathlib


def configure_pack(config, directory):
    pack = pathlib.Path(directory).expanduser().resolve()
    if not pack.is_dir():
        raise ValueError(f'Runtime pack directory not found: {pack}')
    if (pack / 'llama.dll').is_file():
        required = ['llama.dll', 'ggml.dll', 'ggml-base.dll']
        cpu = list(pack.glob('ggml-cpu*.dll')); cuda = pack / 'ggml-cuda.dll'
    else:
        suffix = 'dylib' if (pack / 'libllama.dylib').is_file() else 'so'
        required = [f'libllama.{suffix}', f'libggml.{suffix}', f'libggml-base.{suffix}']
        cpu = list(pack.glob(f'libggml-cpu*.{suffix}')); cuda = pack / f'libggml-cuda.{suffix}'
    missing = [name for name in required if not (pack / name).is_file()]
    if missing: raise ValueError('Runtime pack needs native libraries, not just EXEs: ' + ', '.join(missing))
    if not cpu: raise ValueError('Runtime pack needs its matching GGML CPU backend library')
    if config.get('gpu_layers', 0) > 0 and not cuda.is_file():
        raise ValueError('GPU layers requested but the runtime pack has no GGML CUDA backend')
    selected = dict(config)
    selected.update(runtime_pack=str(pack), build_dir='build-native-pack', llama=False, ggml=False,
                    cuda=False, trainer_backend='scalar-sgd')
    return selected
