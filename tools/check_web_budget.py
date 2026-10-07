#!/usr/bin/env python3
"""Measure the files actually stored on the miner, after gzip processing."""
import argparse
import csv
import json
from pathlib import Path


def partition_size(path, name):
    for row in csv.reader(path.read_text().splitlines()):
        if not row or row[0].lstrip().startswith('#'):
            continue
        if row[0].strip() == name:
            value = row[4].strip().upper()
            return int(value[:-1], 0) * {'K': 1024, 'M': 1024 ** 2}[value[-1]] if value[-1] in 'KM' else int(value, 0)
    raise ValueError(f'Missing {name} partition')


def measure(directory, partition_bytes):
    files = []
    for path in directory.rglob('*'):
        if path.is_symlink():
            raise ValueError(f'Web bundle contains a symlink: {path}')
        if path.is_file():
            files.append({'path': str(path.relative_to(directory)), 'bytes': path.stat().st_size})
    if not files or not (directory / 'index.html.gz').is_file():
        raise ValueError('Expected a completed, compressed production build')
    payload = sum(row['bytes'] for row in files)
    return {
        'www_partition_bytes': partition_bytes,
        'payload_bytes': payload,
        'payload_percent': round(100 * payload / partition_bytes, 2),
        'partition_headroom_bytes_before_filesystem_overhead': partition_bytes - payload,
        'compressed_javascript_bytes': sum(row['bytes'] for row in files if row['path'].endswith('.js.gz')),
        'file_count': len(files),
        'largest_files': sorted(files, key=lambda row: row['bytes'], reverse=True)[:10],
        'note': 'Payload size excludes SPIFFS metadata. The full SPIFFS image is separately checked by ESP-IDF.',
    }


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--web-dir', type=Path, default=root / 'main/http_server/axe-os/dist/axe-os')
    parser.add_argument('--partitions', type=Path, default=root / 'partitions.csv')
    parser.add_argument('--max-payload-bytes', type=int, default=1024 ** 2)
    parser.add_argument('--baseline-report', type=Path, default=root / 'tools/upstream-web-budget.json')
    parser.add_argument('--max-growth-bytes', type=int, default=128 * 1024)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    result = measure(args.web_dir, partition_size(args.partitions, 'www'))
    limits = [args.max_payload_bytes, result['www_partition_bytes'] * 3 // 4]
    if args.baseline_report:
        baseline = json.loads(args.baseline_report.read_text())['payload_bytes']
        result['baseline_payload_bytes'] = baseline
        result['growth_bytes'] = result['payload_bytes'] - baseline
        limits.append(baseline + args.max_growth_bytes)
    result['payload_budget_bytes'] = min(limits)
    result['passed'] = result['payload_bytes'] <= result['payload_budget_bytes']
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
