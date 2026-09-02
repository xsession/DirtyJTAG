#!/usr/bin/env python3
from pathlib import Path
import tempfile
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'host'))
from vendor_features import load_profiles, filter_profiles, roadmap, status_summary, category_summary, export_csv

rows = load_profiles()
assert len(rows) >= 50, len(rows)
assert len({r['vendor'] for r in rows}) >= 50
assert any(r['vendor'] == 'SEGGER' for r in rows)
assert any('boundary' in ' '.join(r['features']).lower() or 'boundary' in r['category'].lower() for r in rows)
assert any(r['dirtyjtag_status'] == 'implemented-partial' for r in rows)
assert filter_profiles(rows, 'rtt')
assert filter_profiles(rows, 'c2000') or filter_profiles(rows, 'xds')
assert roadmap(rows, 5)[0]['priority'] >= roadmap(rows, 5)[-1]['priority']
assert status_summary(rows)
assert category_summary(rows)
with tempfile.NamedTemporaryFile('w+', encoding='utf-8', newline='') as f:
    export_csv(rows[:3], f)
    f.seek(0)
    data = f.read()
    assert 'vendor' in data and 'SEGGER' in data
print('test_vendor_features: PASS')
