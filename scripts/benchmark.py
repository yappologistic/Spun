#!/usr/bin/env python3
"""Compare isolated, muted offscreen scenes; never opens windows or connects to Cider."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time
import itertools
import shutil
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--binary', type=Path, default=Path(__file__).resolve().parents[1] / 'build/spun')
parser.add_argument('--baseline', type=Path, help='Optional executable built before the changes')
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--runs', type=int, default=3)
parser.add_argument('--startup-only', action='store_true', help='Exit after the first frame and report startup stages')
parser.add_argument('--renderer', choices=('opengl', 'software'), default='opengl')
parser.add_argument('--scale', type=float, default=1, help='Identical device scale for both builds')
parser.add_argument('--media', nargs='+', choices=('cd', 'vinyl', 'cassette'), default=['cd'])
parser.add_argument('--scenes', nargs='+', choices=('idle', 'playing', 'mini', 'cycle'), default=['idle', 'playing', 'mini'])
args = parser.parse_args()
if args.runs < 1:
    parser.error('--runs must be positive')
if args.scale <= 0:
    parser.error('--scale must be positive')
args.output.mkdir(parents=True, exist_ok=True)
versions = [('optimized', args.binary.resolve())]
if args.baseline:
    versions.insert(0, ('baseline', args.baseline.resolve()))
results = []
for trial in range(args.runs):
    for scene, medium in itertools.product(('startup',) if args.startup_only else args.scenes, args.media):
        # Alternate order to reduce systematic effects from warm caches and temperature.
        for version, binary in (versions if trial % 2 == 0 else list(reversed(versions))):
            with tempfile.TemporaryDirectory(prefix='spun-benchmark-') as cache:
                env = dict(os.environ, QT_QPA_PLATFORM='offscreen', QT_QPA_PLATFORMTHEME='',
                           QT_FORCE_STDERR_LOGGING='1', XDG_CACHE_HOME=cache, QSG_INFO='1',
                           QT_SCALE_FACTOR=str(args.scale), QSG_RHI_BACKEND='opengl',
                           QT_QUICK_BACKEND='software' if args.renderer == 'software' else 'rhi',
                           QSG_RENDER_LOOP='basic' if args.renderer == 'software' else 'threaded')
                log_path = args.output / f'{version}-{medium}-{scene}-{trial}.log'
                samples = []
                gpu_mib = None
                gpu_sampled = False
                with log_path.open('w') as log:
                    process = subprocess.Popen([str(binary), '--benchmark', scene, '--benchmark-medium', medium], env=env, stdout=log, stderr=log)
                    start, previous = time.monotonic(), None
                    try:
                        while process.poll() is None:
                            now = time.monotonic()
                            if now - start > 30:
                                raise TimeoutError(f'{version} {scene}: no result within 30 seconds')
                            try:
                                stat = Path(f'/proc/{process.pid}/stat').read_text().rsplit(')', 1)[1].split()
                                ticks = int(stat[11]) + int(stat[12])
                                # Same settled three-second window, excluding initialization and teardown.
                                if previous and 4.5 <= now - start <= 7.5:
                                    samples.append((ticks - previous[1]) / os.sysconf('SC_CLK_TCK') / (now - previous[0]) * 100)
                                previous = now, ticks
                            except FileNotFoundError:
                                pass
                            if not args.startup_only and not gpu_sampled and now - start >= 5.5:
                                gpu_sampled = True
                                if shutil.which('nvidia-smi'):
                                    try:
                                        telemetry = subprocess.run(['nvidia-smi', '-q', '-x'], capture_output=True, text=True, timeout=2, check=True)
                                        for item in ET.fromstring(telemetry.stdout).findall('.//process_info'):
                                            if item.findtext('pid') == str(process.pid):
                                                gpu_mib = int(item.findtext('used_memory').split()[0])
                                    except (subprocess.SubprocessError, ValueError, ET.ParseError, AttributeError):
                                        pass
                            time.sleep(.01 if args.startup_only else .25)
                    finally:
                        if process.poll() is None:
                            process.terminate()
                            try:
                                process.wait(timeout=5)
                            except subprocess.TimeoutExpired:
                                process.kill()
                                process.wait()
                    if process.returncode:
                        raise RuntimeError(f'{version} {scene}: exit {process.returncode}; see {log_path}')
                lines = log_path.read_text().splitlines()
                measurement = next((line[10:] for line in lines if line.startswith('BENCHMARK ')), None)
                if measurement is None:
                    raise RuntimeError(f'{version} {scene}: window closed before measurement; see {log_path}')
                row = json.loads(measurement)
                if args.startup_only:
                    row.update(version=version, trial=trial, medium=medium, renderer=args.renderer, scale=args.scale,
                               binaryBytes=binary.stat().st_size)
                    results.append(row)
                    (args.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
                    print(json.dumps(row), flush=True)
                    continue
                memory = row.pop('memory')
                for key in ('Rss', 'Pss', 'Private_Dirty'):
                    row[key + 'KiB'] = int(next(line.split()[1] for line in memory.splitlines() if line.startswith(key + ':')))
                row.update(version=version, trial=trial, maxCpu250ms=max(samples, default=0),
                           meanCpu250ms=sum(samples)/len(samples) if samples else 0,
                           validAnimation=scene in ('idle', 'cycle') or row['frames'] >= 300,
                           gpuMemoryMiB=gpu_mib,
                           renderer=args.renderer, scale=args.scale,
                           binaryBytes=binary.stat().st_size)
                results.append(row)
                (args.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
                print(json.dumps(row), flush=True)
