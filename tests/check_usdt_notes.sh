#!/usr/bin/env bash
#
# Verify that the built libjemalloc.so contains the expected USDT probe notes.
#
# Usage: check_usdt_notes.sh <path-to-libjemalloc.so>
#
# SPDX-License-Identifier: Apache-2.0

set -euo pipefail

LIB="${1:?Usage: $0 <path-to-libjemalloc.so>}"

if ! command -v readelf &>/dev/null; then
    echo "SKIP: readelf not found"
    exit 0
fi

echo "Checking USDT notes in: $LIB"

NOTES=$(readelf -n "$LIB" 2>/dev/null || true)

# Check for otel_memory provider with alloc probe
# readelf prints Provider and Name on separate lines, so check both exist
if ! echo "$NOTES" | grep -q "Provider: otel_memory"; then
    echo "FAIL: otel_memory provider not found in ELF notes"
    echo "$NOTES"
    exit 1
fi

if ! echo "$NOTES" | grep -q "Name: alloc"; then
    echo "FAIL: alloc probe not found in ELF notes"
    echo "$NOTES"
    exit 1
fi

if ! echo "$NOTES" | grep -q "Name: free"; then
    echo "FAIL: free probe not found in ELF notes"
    echo "$NOTES"
    exit 1
fi

echo "PASS: found otel_memory:alloc and otel_memory:free USDT probes"
