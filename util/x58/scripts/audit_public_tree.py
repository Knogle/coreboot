#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Fail-closed release checks of the X58 delta, not upstream's existing tree.

Print categories, paths and line numbers only: never echo suspected secrets.
This is an additional review aid, not a guarantee that arbitrary data is safe
to publish. Manually review documentation, hardware dumps and commit metadata.
No firmware builds, external services or hardware access are performed.
"""

import argparse
import ipaddress
from pathlib import Path
import re
import subprocess

PUBLIC_BASE = "fe3e08197177d3ce6ef6ea9ef238069e5b1d33a2"
FORBIDDEN_SUFFIXES = {
    ".rom", ".bin", ".raw", ".aml", ".dat", ".efi", ".elf", ".fd",
    ".zip", ".7z", ".xz", ".gz", ".pem", ".key", ".pyc", ".o",
}
PRIVATE_NETWORKS = tuple(ipaddress.ip_network(value) for value in (
    "10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16",
))
PATTERNS = {
    "private-key": re.compile(r"-----BEGIN (?:OPENSSH |RSA |EC |DSA )?PRIVATE KEY-----"),
    "public-ssh-identity": re.compile(r"\bssh-(?:rsa|ed25519) [A-Za-z0-9+/]{32,}"),
    "github-credential": re.compile(r"\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{35,})"),
    "credential-url": re.compile(r"https?://[^\s/:@]+:[^\s/@]+@"),
    "personal-home-path": re.compile(r"/(?:home|Users)/[A-Za-z0-9._-]+/"),
    "private-key-path": re.compile(r"(?:\.ssh/|\bid_(?:rsa|ed25519|default)\b)"),
}
IPV4 = re.compile(r"(?<![\w.])(?:[0-9]{1,3}\.){3}[0-9]{1,3}(?![\w.])")


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args])


def audit(root, base):
    git(root, "cat-file", "-e", base + "^{commit}")
    changed = git(root, "diff", "--name-only", "-z", base, "--")
    untracked = git(root, "ls-files", "--others", "--exclude-standard", "-z")
    paths = sorted(set(value.decode() for value in (changed + untracked).split(b"\0") if value))
    problems = []
    for relative in paths:
        path = root / relative
        if not path.exists():
            continue
        if path.is_symlink():
            problems.append((relative, 0, "new-symlink-needs-review"))
            continue
        if path.suffix.lower() in FORBIDDEN_SUFFIXES or "blobs-local" in path.parts:
            problems.append((relative, 0, "private-or-generated-artifact"))
            continue
        data = path.read_bytes()
        try:
            content = data.decode("utf-8")
            if "\0" in content:
                raise UnicodeError("NUL-bearing file")
        except UnicodeError:
            problems.append((relative, 0, "non-text-artifact"))
            continue
        for number, line in enumerate(content.splitlines(), 1):
            for category, pattern in PATTERNS.items():
                if category == "private-key-path" and relative in (
                    ".gitignore", "util/x58/scripts/audit_public_tree.py",
                ):
                    continue  # Ignore rules and scanner patterns, not credentials.
                if pattern.search(line):
                    problems.append((relative, number, category))
            for value in IPV4.findall(line):
                try:
                    address = ipaddress.ip_address(value)
                except ValueError:
                    continue
                if any(address in network for network in PRIVATE_NETWORKS):
                    # The scanner's own generic network policy is not lab data.
                    if path.name == "audit_public_tree.py" and value in ("10.0.0.0", "172.16.0.0", "192.168.0.0"):
                        continue
                    problems.append((relative, number, "private-network-coordinate"))
    for relative, number, category in problems:
        print(f"{category}: {relative}:{number}")
    print(f"Public-tree audit: {len(paths)} changed/new paths, {len(problems)} findings.")
    return bool(problems)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default=PUBLIC_BASE)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    return audit(root, args.base)


if __name__ == "__main__":
    raise SystemExit(main())
