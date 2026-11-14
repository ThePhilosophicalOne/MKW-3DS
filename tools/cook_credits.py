#!/usr/bin/env python3
"""Credits movies: thp/ending/ending_normal.thp and ending_true.thp.

Retail plays them on page 0x3E of credits part 2 (sections 0x3B / 0x3C; the
_50 files are only for a PAL 50 Hz console). Both are 256x192 at 59.94 fps
with stereo 32 kHz DSP ADPCM audio. They are converted like every menu movie
(tools/cook_menu_movies.py): every source frame as a GPU-native ETC1 GMV1
stream (256x256 texture, 32 KiB per frame) and the movie's own audio as PCM16
WAV. models/credits.json lists the streams and the audio for packaging.
Adapted from upstream's tools/cook_credits.py; our cook_menu_movies.cook_audio
always writes movies/title.wav, so it runs in a scratch root here and the WAV
is moved to its own name.
"""
import argparse, json, shutil, tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import hashlib
import cook_menu_movies
from cook_menu_movies import ROOT, encode, power_of_two, digest_file
from menu_movies import ThpMovie, extract_movies

MOVIES = ['ending_normal', 'ending_true']


def spec_for(path, name, frames=None):
    movie = ThpMovie(path)
    width, height = movie.frame(0).size
    texture_width, texture_height = power_of_two(width), power_of_two(height)
    denominator = 100000
    return dict(name=name, source=path.relative_to(ROOT).as_posix(), sourceSha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                sourceSize=[width, height], size=[width, height], textureSize=[texture_width, texture_height],
                frames=frames or movie.count, fps=movie.fps, fpsNumerator=round(movie.fps*denominator), fpsDenominator=denominator,
                frameBytes=texture_width*texture_height//2,
                regions=[dict(key=name, uv=[0, 0, width/texture_width, height/texture_height], crop=[0, 0, width, height])],
                path=f'movies/{name}.gmv')


def cook_audio(path, name, out):
    with tempfile.TemporaryDirectory(prefix='mkw-credits-audio-') as scratch:
        saved = cook_menu_movies.OUT
        cook_menu_movies.OUT = Path(scratch)
        try: receipt = cook_menu_movies.cook_audio(path)
        finally: cook_menu_movies.OUT = saved
        destination = out/'movies'/f'{name}.wav'
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(Path(scratch)/'movies/title.wav'), destination)
    receipt = {**receipt, 'path': f'movies/{name}.wav', 'sha256': digest_file(destination)}
    destination.with_suffix('.audio.json').write_text(json.dumps(receipt, indent=2)+'\n')
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--quality', choices=['low', 'medium', 'high'], default='medium')
    parser.add_argument('--force', action='store_true')
    parser.add_argument('--output-root', type=Path, default=cook_menu_movies.OUT, help='default: sd-card/3ds/mkw-3ds')
    parser.add_argument('--frames', type=int, help='check run only: encode the first N frames (needs --output-root)')
    args = parser.parse_args()
    out = args.output_root.resolve()
    if args.frames and out == cook_menu_movies.OUT.resolve(): raise SystemExit('--frames is for check runs into another --output-root')
    paths = extract_movies([f'thp/ending/{name}.thp' for name in MOVIES])
    specs = [spec_for(path, name, args.frames) for path, name in zip(paths, MOVIES)]
    audio = [cook_audio(path, name, out) for path, name in zip(paths, MOVIES)]
    cook_menu_movies.OUT = out
    with ThreadPoolExecutor(max_workers=max(1, min(args.jobs, len(specs)))) as pool:
        results = list(pool.map(lambda spec: encode(spec, args.quality, args.force), specs))
    for result in results:
        print(f'{result["name"]}: ready {result["frames"]} frames, {result["bytes"]/1048576:.1f} MiB'+(' (cached)' if result.get('cached') else ''), flush=True)
    streams = []
    for spec in specs:
        receipt = json.loads((out/Path(spec['path']).with_suffix('.json')).read_text())
        streams.append({**spec, 'sha256': receipt['sha256'], 'bytes': receipt['bytes']})
    manifest = dict(version=1, format='GMV1 / GPU_ETC1', everySourceFrame=not args.frames, streams=streams, audio=audio,
                    music='STRM_O_ENDING_PART_A (part 1, sound archive)', staffRoll='baked by tools/cook_menu_ui.py')
    (out/'models').mkdir(parents=True, exist_ok=True)
    (out/'models/credits.json').write_text(json.dumps(manifest, indent=2)+'\n')


if __name__ == '__main__':
    main()
