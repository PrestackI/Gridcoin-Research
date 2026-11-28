#!/usr/bin/env bash
#
# Copyright (c) 2026 The Gridcoin developers
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or https://opensource.org/licenses/mit-license.php.
#
# Remove the apt sources that point at packages.microsoft.com.
#
# Usage: ci-drop-unused-apt-sources.sh [<sources-dir>]
#
# For CI runners. The GitHub-hosted Ubuntu image ships a package source for
# packages.microsoft.com. No job installs a package from it, but every
# `apt-get update` fetches its index, and the update fails, and the job with it,
# when that server refuses the request. Run this before the first package
# command of a job.
#
# The directory defaults to /etc/apt/sources.list.d. Without an argument the
# script only runs on a GitHub Actions runner, so that running it by mistake
# leaves the sources of a developer's own machine alone.
#
# Only active entries count: a one-line "deb" or "deb-src" entry, or a deb822
# "URIs:" field. A file is removed when every active entry in it names that
# host. A file that also holds an entry for another host is kept, with a
# warning, because removing it would take a source the job may need.
#
# Finding nothing to remove is not an error: the image may stop shipping the
# source.

export LC_ALL=C
set -euo pipefail

if [ "$#" -eq 0 ] && [ "${GITHUB_ACTIONS:-}" != "true" ]; then
    echo "ci-drop-unused-apt-sources: not on a GitHub Actions runner; pass the sources directory to run it anyway." >&2
    exit 1
fi

SOURCES_DIR="${1:-/etc/apt/sources.list.d}"
UNUSED_HOST='packages\.microsoft\.com'
ACTIVE_ENTRY='^[[:space:]]*(deb|deb-src|URIs:)[[:space:]]'
REMOVED=0

for SOURCE_FILE in "${SOURCES_DIR}"/*.list "${SOURCES_DIR}"/*.sources; do
    [ -f "${SOURCE_FILE}" ] || continue
    ENTRIES=$(grep -i -E "${ACTIVE_ENTRY}" "${SOURCE_FILE}" || true)
    UNUSED_ENTRIES=$(printf '%s\n' "${ENTRIES}" | grep -c -E "${UNUSED_HOST}" || true)
    OTHER_ENTRIES=$(printf '%s\n' "${ENTRIES}" | grep -v -E "${UNUSED_HOST}" | grep -c '[^[:space:]]' || true)
    if [ "${UNUSED_ENTRIES}" -eq 0 ]; then
        continue
    fi
    if [ "${OTHER_ENTRIES}" -ne 0 ]; then
        echo "::warning::Kept mixed package source: ${SOURCE_FILE}"
        continue
    fi
    if [ -w "${SOURCES_DIR}" ]; then
        rm -f -- "${SOURCE_FILE}"
    else
        sudo rm -f -- "${SOURCE_FILE}"
    fi
    echo "Removed package source: ${SOURCE_FILE}"
    REMOVED=$((REMOVED + 1))
done

echo "Package sources removed: ${REMOVED}"
