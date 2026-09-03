#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

required_files='\
host/requirements.txt \
docs/PRODUCTION_READY_DOCUMENTATION.md \
docs/OPERATOR_GUIDE.md \
docs/RELEASE_PROCESS.md \
docs/VALIDATION_MATRIX.md \
docs/ADC_CALIBRATION.md \
docs/SAFETY_MEDICAL_GRADE_REVIEW.md \
dirty_jtag/VERSION \
west.yml'

for f in $required_files; do
    if [ ! -s "$f" ]; then
        echo "release validation: missing or empty $f" >&2
        exit 1
    fi
done

if ! grep -q 'revision: v4.4.1' west.yml; then
    echo 'release validation: west.yml is not pinned to reviewed Zephyr v4.4.1' >&2
    exit 1
fi

if ! grep -q '^VERSION_MAJOR = ' dirty_jtag/VERSION; then
    echo 'release validation: malformed dirty_jtag/VERSION' >&2
    exit 1
fi

python3 -m py_compile host/*.py tests/*.py

find host -type f -name '*.json' -print | sort | while IFS= read -r f; do
    python3 -m json.tool "$f" >/dev/null
 done

./scripts/test-native.sh
./scripts/check-zephyr-syntax.sh

echo 'release validation: PASS'
