#!/bin/bash
set -e

for PY in /opt/python/cp31{0,1,2,3,4}*/bin/python; do
    [ -x "$PY" ] || continue
    echo "=== Building with $PY ==="
    "$PY" -m pip wheel . --no-deps -w /tmp/wheels
done

for whl in /tmp/wheels/*.whl; do
    auditwheel repair "$whl" -w dist
done
