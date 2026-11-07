#!/usr/bin/env python3
"""Read Six Stars manifests; reuse its GGUF and optionally its native runtime pack."""
import argparse
import json
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def installation(path):
    base = pathlib.Path(path).expanduser().resolve()
    if base.is_file() and base.name in ('models.json', 'runtime-manifest.json'):
        base = base.parent
    if base.name.lower() == 'config' and (base / 'models.json').is_file():
        base = base.parent
    manifest = base / 'config/models.json'
    if not manifest.is_file():
        raise ValueError(
            f'Six Stars model manifest not found: {manifest}. '
            'Pass the installation folder containing config/models.json, or that config folder itself. '
            'For your installation: --six-stars "U:\\six-stars-solution-2"'
        )
    return base, manifest


def shortcut_target(path):
    """Read an existing Windows Shell Link; never execute its target or save it."""
    if os.name != 'nt':
        raise ValueError('Windows models.lnk needs Windows, or pass --models-root with its actual target.')
    script = (
        '[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new(); '
        '$s = New-Object -ComObject WScript.Shell; '
        '$s.CreateShortcut($env:WDS_SIXSTARS_SHORTCUT).TargetPath | ConvertTo-Json -Compress'
    )
    try:
        result = subprocess.run(
            ['powershell.exe', '-NoProfile', '-NonInteractive', '-Command', script],
            env={**os.environ, 'WDS_SIXSTARS_SHORTCUT': str(path)},
            check=True, capture_output=True, encoding='utf-8-sig', timeout=10,
        )
        target = json.loads(result.stdout.strip())
        if not isinstance(target, str) or not target:
            raise ValueError('empty shortcut target')
        return pathlib.Path(target).expanduser().resolve()
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise ValueError(
            f'Cannot read models shortcut {path}: {error}. '
            'Pass --models-root with the actual directory containing llm.'
        ) from error


def model_path(base, manifest, args):
    value = read_json(manifest).get('models', {}).get('llm', {}).get('localPath')
    if not isinstance(value, str) or not value:
        raise ValueError(f'Missing models.llm.localPath in {manifest}')
    relative = pathlib.Path(value.replace('\\', '/'))
    models_root = args.models_root or os.environ.get('SIX_STARS_MODELS_ROOT')
    if args.model:
        return pathlib.Path(args.model).expanduser().resolve()
    if relative.is_absolute():
        return relative.resolve()
    parts = relative.parts
    tail = parts[1:] if parts and parts[0].lower() == 'models' else parts
    if models_root:
        configured = pathlib.Path(models_root).expanduser()
        return ((configured if configured.is_absolute() else base / configured).joinpath(*tail)).resolve()
    target = (base / relative).resolve()
    link = base / 'models.lnk'
    if not target.is_file() and parts and parts[0].lower() == 'models' and link.is_file():
        return shortcut_target(link).joinpath(*tail).resolve()
    return target


def runtime_details(base):
    """Report the server executable separately; WDS never launches this server."""
    path = base / 'config/runtime-manifest.json'
    details = {'manifest': str(path), 'used_by_wds': False}
    if not path.is_file():
        return {**details, 'status': 'manifest absent; not required for GGUF reuse'}
    manifest = read_json(path)
    packs = [p for p in manifest.get('packs', []) if p.get('kind') == 'llm']
    order = manifest.get('selectionOrder', {}).get('llm', [])
    pack = next((p for name in order for p in packs if p.get('id') == name), None)
    if pack is None:
        pack = next(iter(packs), None)
    if not pack:
        return {**details, 'status': 'no LLM pack declared'}
    declared = pathlib.Path(pack.get('path', '').replace('\\', '/'))
    configured = declared if declared.is_absolute() else base / declared
    candidates = [configured]
    if declared.name == 'llama-server.exe' and declared.parent.name:
        candidates.append(base / 'runtime-packs/win-x64' / declared.parent.name / declared.name)
    return {
        **details, 'pack_id': pack.get('id'), 'configured_path': str(configured),
        'checked_paths': [str(p) for p in candidates],
        'detected_path': next((str(p) for p in candidates if p.is_file()), None),
        'gpu_policy': manifest.get('gpuPolicy', {}),
        'status': 'server diagnostic only; native library reuse is selected separately with --runtime-pack',
    }


def configure(args):
    base, manifest = installation(args.six_stars)
    target = model_path(base, manifest, args)
    if not target.is_file():
        raise ValueError(
            f'GGUF not found: {target}. This is the model, not llama-server.exe. '
            'If models is a shortcut or the files were moved, pass --models-root with its real target, '
            'or --model with the existing GGUF file. No model is downloaded or replaced by this helper.'
        )
    with target.open('rb') as file:
        if file.read(4) != b'GGUF':
            raise ValueError('Selected file is not a GGUF: ' + str(target))
    config = read_json(ROOT / 'configs/local.json')
    pack = getattr(args, 'runtime_pack', None)
    config.update({
        'build_dir': 'build-sixstars', 'model': str(target), 'llama': True, 'ggml': True,
        'cuda': args.cuda, 'gpu_layers': args.gpu_layers if args.gpu_layers is not None else (99 if args.cuda or pack else 0),
        'data': 'sixstars-wds-data', 'runs': 'sixstars-wds-runs', 'port': args.port,
    })
    if config['gpu_layers'] < 0 or (config['gpu_layers'] > 0 and not args.cuda and not pack):
        raise ValueError('GPU layers require --cuda in this setup helper; CPU uses 0.')
    if args.llama_source:
        source = pathlib.Path(args.llama_source).expanduser().resolve()
        if not (source / 'include/llama.h').is_file():
            raise ValueError('--llama-source requires a source checkout, not engines or runtime-packs binaries')
        config['llama_source'] = str(source)
    if pack:
        from runtime_pack import configure_pack
        config = configure_pack(config, pack)
    output = (ROOT / args.output).resolve()
    reused = False
    if output.exists() and not args.replace:
        if not getattr(args, 'reuse', False):
            raise ValueError('Configuration exists; use --replace explicitly or choose --output.')
        existing = read_json(output)
        if existing.get('schema') != 1 or pathlib.Path(existing.get('model', '')).resolve() != target:
            raise ValueError(f'{output} already targets another configuration/model; use a different --config or explicitly regenerate with configure_sixstars.py --replace.')
        if pack and existing.get('runtime_pack') != config['runtime_pack']:
            raise ValueError('Existing configuration does not use this runtime pack; use workshop.py --config CONFIG start --runtime-pack DIR to switch while retaining its paths, or regenerate with --replace.')
        for key, requested in (('cuda', (args.cuda or None) if not pack else None), ('gpu_layers', args.gpu_layers), ('llama_source', config.get('llama_source') if args.llama_source else None)):
            if requested is not None and existing.get(key) != requested:
                raise ValueError(f'Existing {key} differs; regenerate with configure_sixstars.py --replace before starting.')
        config = existing
        reused = True
    # Resolve diagnostics before writing so a malformed manifest leaves no partial setup.
    runtime = runtime_details(base)
    if not reused:
        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = output.with_suffix(output.suffix + '.tmp')
        temporary.write_text(json.dumps(config, indent=2) + '\n', encoding='utf-8')
        temporary.replace(output)
    print(json.dumps({
        'config': str(output), 'reused_configuration': reused, 'six_stars': str(base),
        'model_manifest': str(manifest), 'gguf': str(target), 'six_stars_runtime': runtime,
        'backend': 'b9870 native runtime pack (no CUDA compiler)' if config.get('runtime_pack') else 'CUDA source build (CUDA development toolkit required)' if config.get('cuda') else 'CPU source build',
        'reuses': 'GGUF and native llama/GGML libraries; no chat server' if config.get('runtime_pack') else 'GGUF file only; no Six Stars EXE/DLL or running server reuse',
        'start': ['python', 'tools/workshop.py', '--config', str(output), 'start', '--ui', '--upkeep'],
    }, indent=2))
    return output


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--six-stars', required=True, help='Installation root, its config folder, or config/models.json')
    p.add_argument('--models-root')
    p.add_argument('--model')
    p.add_argument('--llama-source')
    p.add_argument('--runtime-pack', help='Existing b9870 library folder; no CUDA source build')
    p.add_argument('--cuda', action='store_true')
    p.add_argument('--gpu-layers', type=int)
    p.add_argument('--port', type=int, default=8765)
    p.add_argument('--output', default='configs/sixstars.local.json')
    p.add_argument('--replace', action='store_true')
    p.add_argument('--reuse', action='store_true', help='Reuse a matching existing WDS config without rewriting it')
    try:
        configure(p.parse_args())
    except Exception as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
