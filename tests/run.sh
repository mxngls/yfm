#!/bin/sh
set -e

BIN="${1:?usage: run.sh <binary>}"
TESTS_DIR="$(dirname "$0")"
PASS=0
FAIL=0

for infile in "$TESTS_DIR"/*.in.yaml; do
	name="$(basename "$infile" .in.yaml)"
	expected="$TESTS_DIR/$name.out.yaml"

	if [ ! -f "$expected" ]; then
		echo "SKIP $name (no .out.yaml file)"
		continue
	fi

	actual="$("$BIN" "$infile" 2>&1)" || true

	if [ "$actual" = "$(cat "$expected")" ]; then
		echo "PASS $name"
		PASS=$((PASS + 1))
	else
		echo "FAIL $name"
		diff --color=auto -u "$expected" - <<EOF
$actual
EOF
		FAIL=$((FAIL + 1))
	fi
done

echo ""
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
