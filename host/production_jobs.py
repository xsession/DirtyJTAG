#!/usr/bin/env python3
"""Production-job descriptor validation for DirtyJTAG."""
from __future__ import annotations
import json, os
from typing import Any, Dict, List

class JobError(ValueError): pass

REQUIRED=('name','target','steps')
ALLOWED_STEPS={'power','measure','config','enter','identify','erase','program','verify','rtt-log','power-trace','bridge','safe','leave','serial-number'}
DESTRUCTIVE_STEPS={'erase','program','bridge'}

def load_job(path: str) -> Dict[str, Any]:
    with open(path,'r',encoding='utf-8') as f:
        job=json.load(f)
    validate_job(job)
    return job

def validate_job(job: Dict[str, Any]) -> None:
    for key in REQUIRED:
        if key not in job: raise JobError(f'missing required key {key}')
    if not isinstance(job.get('target'), dict):
        raise JobError('target must be an object')
    if not job['target'].get('protocol') or not job['target'].get('device'):
        raise JobError('target.protocol and target.device are required')
    if not isinstance(job['steps'], list) or not job['steps']:
        raise JobError('steps must be a non-empty list')
    if job['steps'][0].get('op') != 'safe' or job['steps'][-1].get('op') != 'safe':
        raise JobError('production jobs must start and end with safe steps')
    destructive = False
    program_indices: List[int] = []
    verify_indices: List[int] = []
    for idx, step in enumerate(job['steps']):
        if not isinstance(step, dict) or 'op' not in step:
            raise JobError(f'step {idx}: must be object with op')
        if step['op'] not in ALLOWED_STEPS:
            raise JobError(f'step {idx}: unsupported op {step["op"]!r}')
        if step['op'] in DESTRUCTIVE_STEPS:
            destructive = True
        if step['op'] == 'program':
            program_indices.append(idx)
            if not step.get('file'):
                raise JobError(f'step {idx}: program requires file')
        if step['op'] == 'verify':
            verify_indices.append(idx)
    if 'artifacts' not in job or not isinstance(job['artifacts'], dict):
        raise JobError('artifacts must be an object')
    if not job['artifacts'].get('report'):
        raise JobError('artifacts.report is required for traceability')
    if destructive:
        safety = job.get('safety')
        if not isinstance(safety, dict):
            raise JobError('destructive production jobs require a safety object')
        if safety.get('operator_confirmation_required') is not True:
            raise JobError('destructive production jobs require operator_confirmation_required=true')
        if safety.get('verify_after_program') is not True:
            raise JobError('destructive production jobs require verify_after_program=true')
    for pidx in program_indices:
        if not any(vidx > pidx for vidx in verify_indices):
            raise JobError(f'step {pidx}: program must be followed by a later verify step')

def summarize_job(job: Dict[str, Any]) -> str:
    lines=[f"job={job['name']} target={job['target'].get('device','-')} protocol={job['target'].get('protocol','-')}"]
    for idx, step in enumerate(job['steps'], 1):
        extra=' '.join(f'{k}={v}' for k,v in step.items() if k!='op')
        lines.append(f'  {idx:02d}. {step["op"]}' + (f' {extra}' if extra else ''))
    return '\n'.join(lines)

def make_default_job(name: str, protocol: str, device: str, image: str) -> Dict[str, Any]:
    return {
        'name': name,
        'target': {'protocol': protocol, 'device': device},
        'artifacts': {'image': image, 'report': f'{name}-report.json'},
        'safety': {'operator_confirmation_required': True, 'verify_after_program': True, 'lot_traceability_required': True},
        'steps': [
            {'op':'safe'}, {'op':'power','mode':'external'}, {'op':'measure'},
            {'op':'config','protocol':protocol,'device':device}, {'op':'enter'}, {'op':'identify'},
            {'op':'erase'}, {'op':'program','file':image}, {'op':'verify','file':image},
            {'op':'power-trace','samples':16,'interval_ms':10}, {'op':'leave'}, {'op':'safe'}
        ]
    }
