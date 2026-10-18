#!/bin/sh
# Every BowlingPlus check that runs without a phone (TEST_PLAN.md, part 1): the same list GitHub Actions runs
# (.github/workflows/checks.yml). Needs python3 and g++; javac/java for the Android half of the menu button check.
# Usage (from the repo root):  sh tools/tests/run_all.sh
cd "$(dirname "$0")/../.." || exit 1
fail=0
summary=""
run() {
    name="$1"; shift
    echo ""
    echo "== $name"
    if "$@"; then summary="$summary
  ok    $name"; else summary="$summary
  FAIL  $name"; fail=1; fi
}
run "static: shared code, settings, version, workflows, Python" python3 tools/tests/static_checks.py
run "Android native: compile + link on the host (stub headers)" sh tools/dev/host_check.sh
run "iOS: no function used before its declaration" python3 tools/dev/ios_order_scan.py
run "pin turns (pinsim)" python3 tools/dev/pinsim/run.py
run "obstacle racks (a new layout only for a new rack)" python3 tools/tests/rack/run.py
run "barstools (bounce, never through, into the gutter)" python3 tools/tests/stools/run.py
run "loft and landing" python3 tools/tests/ball/run.py
run "menu button spot (iOS + Android)" python3 tools/tests/button/run.py
find tools -name __pycache__ -type d -prune -exec rm -rf {} + 2>/dev/null
echo ""
echo "== summary$summary"
exit $fail
