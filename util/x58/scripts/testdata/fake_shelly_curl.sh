#!/usr/bin/env bash
# Test double for test_shelly_gen2_rpc.sh. It never opens a socket.

set -euo pipefail

url=
for argument in "$@"; do
	url=$argument
done

[[ -n "${SHELLY_TEST_LOG:-}" ]] || exit 90
printf '%s\n' "$url" >>"$SHELLY_TEST_LOG"

case "$url" in
*/rpc/Switch.GetStatus\?id=*)
	printf '{"id":%s,"output":%s,"apower":37.2}\n' \
		"${SHELLY_TEST_ID:-0}" "${SHELLY_TEST_OUTPUT:-true}"
	;;
*/rpc/Switch.Set\?*)
	printf '{"was_on":true}\n'
	;;
*)
	exit 91
	;;
esac
