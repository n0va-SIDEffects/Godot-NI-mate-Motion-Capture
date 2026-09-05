#!/usr/bin/env python3
"""Generate message_keys.auto.h from a Pebble project's package.json.

The real SDK build does this automatically; this reproduces it for the
offline compile check. Array form assigns ids from 10000 upwards, object
form uses the given numbers. A name with a trailing [N] reserves N ids.
"""
import json
import re
import sys


def message_keys(package_json_path):
    with open(package_json_path) as f:
        pkg = json.load(f)
    keys = pkg.get("pebble", {}).get("messageKeys", [])
    result = {}
    if isinstance(keys, dict):
        for name, value in keys.items():
            result[name] = int(value)
        return result
    next_id = 10000
    for entry in keys:
        m = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)(?:\[(\d+)\])?$", entry)
        if not m:
            raise SystemExit("invalid messageKeys entry: %r" % entry)
        name, count = m.group(1), int(m.group(2) or 1)
        result[name] = next_id
        next_id += count
    return result


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: gen_message_keys.py package.json out/message_keys.auto.h")
    keys = message_keys(sys.argv[1])
    with open(sys.argv[2], "w") as out:
        out.write("#pragma once\n")
        for name, value in keys.items():
            out.write("#define MESSAGE_KEY_%s %d\n" % (name, value))
    print("wrote %d message keys" % len(keys))


if __name__ == "__main__":
    main()
