#!/usr/bin/env python3
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import tempfile, json, os
from host.production_jobs import make_default_job, validate_job, summarize_job, load_job

job=make_default_job('unit','simplelink-swd','CC2652R','fw.bin')
validate_job(job)
assert 'program' in summarize_job(job)
with tempfile.NamedTemporaryFile('w',delete=False) as f:
    json.dump(job,f); name=f.name
try:
    loaded=load_job(name)
    assert loaded['name']=='unit'
finally:
    os.unlink(name)
try:
    validate_job({'name':'bad','target':{},'steps':[{'op':'unknown'}]})
    raise AssertionError('expected bad op')
except Exception:
    pass
print('test_production_jobs: PASS')
