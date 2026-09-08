#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
tool="$script_dir/shelly_gen2_rpc.sh"
fake_curl="$script_dir/testdata/fake_shelly_curl.sh"
test_dir=$(mktemp -d)
trap 'rm -rf -- "$test_dir"' EXIT

export SHELLY_CURL=$fake_curl
export SHELLY_TEST_LOG=$test_dir/curl.log

fail()
{
	printf 'FAIL: %s\n' "$*" >&2
	exit 1
}

expect_fail()
{
	if "$@" >"$test_dir/stdout" 2>"$test_dir/stderr"; then
		fail "command unexpectedly succeeded: $*"
	fi
}

expect_success()
{
	"$@" >"$test_dir/stdout" 2>"$test_dir/stderr" ||
		fail "command unexpectedly failed: $*"
}

expect_fail "$tool" status
expect_fail "$tool" --host 'http://192.0.2.50' status
expect_fail "$tool" --host 192.0.2.50 --off-seconds 4 cycle

: >"$SHELLY_TEST_LOG"
expect_success "$tool" --host 192.0.2.50 status
grep -q '"output":true' "$test_dir/stdout" || fail "status JSON missing"
[[ $(wc -l <"$SHELLY_TEST_LOG") -eq 1 ]] || fail "status RPC count"
grep -qx 'http://192.0.2.50/rpc/Switch.GetStatus?id=0' \
	"$SHELLY_TEST_LOG" || fail "status URL"

: >"$SHELLY_TEST_LOG"
expect_success "$tool" --host 192.0.2.50 --off-seconds 15 cycle
grep -q '^DRY-RUN; no RPC sent:' "$test_dir/stdout" ||
	fail "cycle should default to dry-run"
[[ ! -s "$SHELLY_TEST_LOG" ]] || fail "dry-run contacted fake device"

: >"$SHELLY_TEST_LOG"
expect_success "$tool" --host 192.0.2.50 --switch-id 0 \
	--off-seconds 15 --apply cycle
[[ $(wc -l <"$SHELLY_TEST_LOG") -eq 2 ]] || fail "cycle RPC count"
grep -qx 'http://192.0.2.50/rpc/Switch.GetStatus?id=0' \
	"$SHELLY_TEST_LOG" || fail "cycle preflight URL"
grep -qx 'http://192.0.2.50/rpc/Switch.Set?id=0&on=false&toggle_after=15' \
	"$SHELLY_TEST_LOG" || fail "cycle timer URL"

: >"$SHELLY_TEST_LOG"
expect_fail "$tool" --host 192.0.2.50 --apply off
[[ ! -s "$SHELLY_TEST_LOG" ]] || fail "unconfirmed off contacted device"
expect_success "$tool" --host 192.0.2.50 --apply --allow-stay-off off
grep -qx 'http://192.0.2.50/rpc/Switch.Set?id=0&on=false' \
	"$SHELLY_TEST_LOG" || fail "off URL"

: >"$SHELLY_TEST_LOG"
export SHELLY_TEST_OUTPUT=false
expect_fail "$tool" --host 192.0.2.50 --apply cycle
[[ $(wc -l <"$SHELLY_TEST_LOG") -eq 1 ]] ||
	fail "off-state cycle should stop after preflight"
unset SHELLY_TEST_OUTPUT

printf 'PASS: shelly_gen2_rpc safety and URL tests\n'
