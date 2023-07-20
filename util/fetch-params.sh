#!/usr/bin/env bash
#
# Copyright (c) 2015-2020 The Zcash developers
# Copyright (c) 2020-2021 The hemis Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

set -eu

if [ -n "${1:-}" ]; then
    PARAMS_DIR="$1"
else
    if [[ "$OSTYPE" == "darwin"* ]]; then
        PARAMS_DIR="$HOME/Library/Application Support/hemisParams"
    else
        PARAMS_DIR="$HOME/.hemis-params"
    fi
fi

SAPLING_SPEND_NAME='sapling-spend.params'
SAPLING_OUTPUT_NAME='sapling-output.params'
DOWNLOAD_URL="https://download.z.cash/downloads"
IPFS_HASH="/ipfs/QmXRHVGLQBiKwvNq7c2vPxAKz1zRVmMYbmt7G5TQss7tY7"

SHA256CMD="$(command -v sha256sum || echo shasum)"
SHA256ARGS="$(command -v sha256sum >/dev/null || echo '-a 256')"

WGETCMD="$(command -v wget || echo '')"
IPFSCMD="$(command -v ipfs || echo '')"
CURLCMD="$(command -v curl || echo '')"

# fetch methods can be disabled with ZC_DISABLE_SOMETHING=1
ZC_DISABLE_WGET="${ZC_DISABLE_WGET:-}"
ZC_DISABLE_IPFS="${ZC_DISABLE_IPFS:-}"
ZC_DISABLE_CURL="${ZC_DISABLE_CURL:-}"

function fetch_wget {
    if [ -z "$WGETCMD" ] || [ -n "$ZC_DISABLE_WGET" ]; then
        return 1
    fi

    local filename="$1"
    local dlname="$2"

    cat <<EOF

Retrieving (wget): $DOWNLOAD_URL/$filename
EOF

    wget \
        --progress=dot:giga \
        --output-document="$dlname" \
        --continue \
        --retry-connrefused --waitretry=3 --timeout=30 \
        "$DOWNLOAD_URL/$filename"
}

function fetch_ipfs {
    if [ -z "$IPFSCMD" ] || [ -n "$ZC_DISABLE_IPFS" ]; then
        return 1
    fi

    local filename="$1"
    local dlname="$2"

    cat <<EOF

Retrieving (ipfs): $IPFS_HASH/$filename
EOF

    ipfs get --output "$dlname" "$IPFS_HASH/$filename"
}

function fetch_curl {
    if [ -z "$CURLCMD" ] || [ -n "$ZC_DISABLE_CURL" ]; then
        return 1
    fi

    local filename="$1"
    local dlname="$2"

    cat <<EOF

Retrieving (curl): $DOWNLOAD_URL/$filename
EOF

    curl \
        --output "$dlname" \
        -# -L -C - \
        "$DOWNLOAD_URL/$filename"

}

function fetch_failure {
    cat >&2 <<EOF

Failed to fetch the hemis zkSNARK parameters!
Try installing one of the following programs and make sure you're online:

 * ipfs
 * wget
 * curl

EOF
    exit 1
}

function fetch_params {
    local filename="$1"
    local output="$2"
    local dlname="${output}.dl"
    local expectedhash="$3"

    if ! [ -f "$output" ]
    then
        for i in 1 2
        do
            for method in wget ipfs curl failure; do
                if "fetch_$method" "${filename}.part.${i}" "${dlname}.part.${i}"; then
                    echo "Download of part ${i} successful!"
                    break
                fi
            done
        done

        for i in 1 2
        do
            if ! [ -f "${dlname}.part.${i}" ]
            then
                fetch_failure
            fi
        done

        cat "${dlname}.part.1" "${dlname}.part.2" > "${dlname}"
        rm "${dlname}.part.1" "${dlname}.part.2"

        "$SHA256CMD" "$SHA256ARGS" -c <<EOF
$expectedhash  $dlname
EOF

        # Check the exit code of the shasum command:
        CHECKSUM_RESULT=$?
        if [ $CHECKSUM_RESULT -eq 0 ]; then
            mv -v "$dlname" "$output"
        else
            echo "Failed to verify parameter checksums!" >&2
            exit 1
        fi
    fi
}

# Use flock to prevent parallel execution.
function lock() {
    local lockfile=/tmp/fetch_params.lock
    if [[ "$OSTYPE" == "darwin"* ]]; then
        if shlock -f ${lockfile} -p $$; then
            return 0
        else
            return 1
        fi
    else
        # create lock file
        eval "exec 200>$lockfile"
        # acquire the lock
        flock -n 200 \
            && return 0 \
            || return 1
    fi
}

function exit_locked_error {
    echo "Only one instance of fetch-params.sh can be run at a time." >&2
    exit 1
}

function main() {

    lock fetch-params.sh \
    || exit_locked_error

    cat <<EOF
hemis - fetch-params.sh

This script will fetch the hemis zkSNARK parameters and verify their
integrity with sha256sum.

If they already exist locally, it will exit now and do nothing else.
EOF

    # Now create PARAMS_DIR and insert a README if necessary:
    if ! [ -d "$PARAMS_DIR" ]
    then
        mkdir -p "$PARAMS_DIR"
        README_PATH="$PARAMS_DIR/README"
        cat >> "$README_PATH" <<EOF
This directory stores common hemis zkSNARK parameters. Note that it is
distinct from the daemon's -datadir argument because the parameters are
large and may be shared across multiple distinct -datadir's such as when
setting up test networks.
EOF

        # This may be the first time the user's run this script, so give
        # them some info, especially about bandwidth usage:
        cat <<EOF
If the files are already present and have the correct
sha256sum, no networking is used.

Creating params directory. For details about this directory, see:
$README_PATH

EOF
    fi

    cd "$PARAMS_DIR"

    # Sapling parameters:
    fetch_params "$SAPLING_SPEND_NAME" "$PARAMS_DIR/$SAPLING_SPEND_NAME" "8e48ffd23abb3a5fd9c5589204f32d9c31285a04b78096ba40a79b75677efc13"
    fetch_params "$SAPLING_OUTPUT_NAME" "$PARAMS_DIR/$SAPLING_OUTPUT_NAME" "2f0ebbcbb9bb0bcffe95a397e7eba89c29eb4dde6191c339db88570e3f3fb0e4"
}

if [ "x${1:-}" = 'x--testnet' ]
then
    echo "NOTE: testnet now uses the mainnet parameters, so the --testnet argument"
    echo "is no longer needed (ignored)"
    echo ""
fi

main
rm -f /tmp/fetch_params.lock
exit 0
