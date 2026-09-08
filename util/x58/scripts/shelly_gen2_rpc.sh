#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-only

set -euo pipefail

usage()
{
	cat <<'EOF'
Usage:
  shelly_gen2_rpc.sh --host HOST [--switch-id ID] status
  shelly_gen2_rpc.sh --host HOST [--switch-id ID] [--apply] on
  shelly_gen2_rpc.sh --host HOST [--switch-id ID] [--apply] \
    [--off-seconds SECONDS] cycle
  shelly_gen2_rpc.sh --host HOST [--switch-id ID] --apply \
    --allow-stay-off off

Safety defaults:
  status is the only command that contacts the device without --apply.
  on, off, and cycle are dry-runs unless --apply is present.
  off additionally requires --allow-stay-off.
  cycle uses Shelly's local toggle_after timer; it never sleeps between two
  separate off/on requests.

HOST is an IPv4 address or simple DNS name, without a URL scheme or path.
The default switch ID is 0 and the default cycle interval is 15 seconds.
EOF
}

die()
{
	printf 'error: %s\n' "$*" >&2
	exit 2
}

host=
switch_id=0
off_seconds=15
apply=0
allow_stay_off=0
command=

while (($#)); do
	case "$1" in
	--host)
		(($# >= 2)) || die "--host requires a value"
		host=$2
		shift 2
		;;
	--switch-id)
		(($# >= 2)) || die "--switch-id requires a value"
		switch_id=$2
		shift 2
		;;
	--off-seconds)
		(($# >= 2)) || die "--off-seconds requires a value"
		off_seconds=$2
		shift 2
		;;
	--apply)
		apply=1
		shift
		;;
	--allow-stay-off)
		allow_stay_off=1
		shift
		;;
	-h|--help)
		usage
		exit 0
		;;
	status|on|off|cycle)
		[[ -z "$command" ]] || die "only one command may be selected"
		command=$1
		shift
		;;
	*)
		die "unknown argument: $1"
		;;
	esac
done

[[ -n "$command" ]] || die "a command is required"
[[ -n "$host" ]] || die "--host is required"
[[ "$host" =~ ^[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?$ ]] ||
	die "HOST must be a simple IPv4 address or DNS name"
[[ "$switch_id" =~ ^[0-9]+$ ]] || die "switch ID must be decimal"
((switch_id <= 255)) || die "switch ID must be between 0 and 255"
[[ "$off_seconds" =~ ^[0-9]+$ ]] || die "off interval must be decimal"
((off_seconds >= 5 && off_seconds <= 120)) ||
	die "off interval must be between 5 and 120 seconds"

curl_bin=${SHELLY_CURL:-curl}
jq_bin=${SHELLY_JQ:-jq}
python_bin=${SHELLY_PYTHON:-python3}

command -v "$curl_bin" >/dev/null 2>&1 || die "curl executable not found"
if ! command -v "$jq_bin" >/dev/null 2>&1; then
	jq_bin=
	command -v "$python_bin" >/dev/null 2>&1 ||
		die "neither jq nor python3 JSON parser found"
fi

json_status_valid()
{
	local response=$1
	local expected_id=$2

	if [[ -n "$jq_bin" ]]; then
		"$jq_bin" -e --argjson expected_id "$expected_id" \
			'type == "object" and .id == $expected_id and
			 (.output | type == "boolean")' \
			>/dev/null <<<"$response"
		return
	fi

	"$python_bin" -c '
import json
import sys

try:
    value = json.load(sys.stdin)
    valid = (
        isinstance(value, dict)
        and value.get("id") == int(sys.argv[1])
        and isinstance(value.get("output"), bool)
    )
except (ValueError, TypeError, json.JSONDecodeError):
    valid = False
sys.exit(0 if valid else 1)
' "$expected_id" <<<"$response"
}

json_is_object()
{
	local response=$1

	if [[ -n "$jq_bin" ]]; then
		"$jq_bin" -e 'type == "object"' >/dev/null <<<"$response"
		return
	fi

	"$python_bin" -c '
import json
import sys

try:
    value = json.load(sys.stdin)
except json.JSONDecodeError:
    sys.exit(1)
sys.exit(0 if isinstance(value, dict) else 1)
' <<<"$response"
}

json_output_is_true()
{
	local response=$1

	if [[ -n "$jq_bin" ]]; then
		"$jq_bin" -e '.output == true' >/dev/null <<<"$response"
		return
	fi

	"$python_bin" -c '
import json
import sys

try:
    value = json.load(sys.stdin)
except json.JSONDecodeError:
    sys.exit(1)
sys.exit(0 if isinstance(value, dict) and value.get("output") is True else 1)
' <<<"$response"
}

rpc_url()
{
	local method=$1
	local query=$2

	printf 'http://%s/rpc/%s?%s' "$host" "$method" "$query"
}

rpc_call()
{
	local method=$1
	local query=$2

	"$curl_bin" --fail --silent --show-error \
		--connect-timeout 3 --max-time 10 \
		"$(rpc_url "$method" "$query")"
}

get_status()
{
	local response

	response=$(rpc_call Switch.GetStatus "id=$switch_id") || return
	if ! json_status_valid "$response" "$switch_id"; then
		printf 'error: invalid Switch.GetStatus response: %s\n' \
			"$response" >&2
		return 1
	fi
	printf '%s\n' "$response"
}

if [[ "$command" == status ]]; then
	get_status
	exit
fi

case "$command" in
on)
	method=Switch.Set
	query="id=$switch_id&on=true"
	;;
off)
	((allow_stay_off)) ||
		die "off requires --allow-stay-off in addition to --apply"
	method=Switch.Set
	query="id=$switch_id&on=false"
	;;
cycle)
	method=Switch.Set
	query="id=$switch_id&on=false&toggle_after=$off_seconds"
	;;
esac

if ((!apply)); then
	printf 'DRY-RUN; no RPC sent: %s\n' "$(rpc_url "$method" "$query")"
	exit 0
fi

if [[ "$command" == cycle ]]; then
	status_json=$(get_status) || die "cycle preflight status failed"
	if ! json_output_is_true "$status_json"; then
		die "cycle refused because the relay is not currently on"
	fi
	printf 'preflight: switch:%s is on; arming local %ss return timer\n' \
		"$switch_id" "$off_seconds" >&2
fi

response=$(rpc_call "$method" "$query") || die "$command RPC failed"
if ! json_is_object "$response"; then
	die "$command RPC returned invalid JSON: $response"
fi
printf '%s\n' "$response"

if [[ "$command" == cycle ]]; then
	printf 'cycle accepted; Shelly switch:%s should turn back on locally after %ss\n' \
		"$switch_id" "$off_seconds" >&2
fi
