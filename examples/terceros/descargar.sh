#!/bin/sh
# descargar.sh - fetch the third-party sources used by the examples and
# check their SHA-256.  They are not in the repository (size, licenses).
#
#   sh examples/terceros/descargar.sh            # everything
#   sh examples/terceros/descargar.sh sqlite     # only one: sqlite, microdot,
#                                                # mongoose, stb, tflite-micro
#
# Leaves them in examples/terceros/<name>/.  Versions and hashes are the
# ones tested on the board (2026-11-23).
set -eu
cd "$(dirname "$0")"

SQLITE_VER=3540100
SQLITE_ZIP=sqlite-amalgamation-$SQLITE_VER.zip
SQLITE_SHA=0a9c8e34352dad46006aa82836d5383bff9dc6bc9ce5a60553c05aa30e6ad1f9
MICRODOT_VER=v2.8.0
MONGOOSE_VER=7.24
STB_COMMIT=6b5799bae8a1fe3792691ef4b72ae26434c3e799   # stb_image v2.34
# tflite-micro has no releases: a fixed commit of the main branch.
TFLM_COMMIT=${TFLM_COMMIT:-38a8f0f87302817ddfa99e6a995c7b71ef7118fe}

sha() { if command -v sha256sum >/dev/null; then sha256sum "$1"; else shasum -a 256 "$1"; fi | cut -d' ' -f1; }

check() {  # check FILE SHA256
    got=$(sha "$1")
    if [ "$got" != "$2" ]; then
        echo "ERROR: $1 sha256 $got, expected $2" >&2
        exit 1
    fi
    echo "ok  $1"
}

sqlite() {
    mkdir -p sqlite && cd sqlite
    [ -f "$SQLITE_ZIP" ] || curl -sSfLO "https://sqlite.org/2026/$SQLITE_ZIP"
    check "$SQLITE_ZIP" "$SQLITE_SHA"
    rm -rf "sqlite-amalgamation-$SQLITE_VER" && unzip -q "$SQLITE_ZIP"
    ln -sfn "sqlite-amalgamation-$SQLITE_VER" src
    cd ..
}

microdot() {
    mkdir -p microdot && cd microdot
    base=https://raw.githubusercontent.com/miguelgrinberg/microdot/$MICRODOT_VER
    for f in __init__.py microdot.py helpers.py cors.py auth.py; do
        [ -f "$f" ] || curl -sSfLO "$base/src/microdot/$f"
    done
    [ -f LICENSE ] || curl -sSfL -o LICENSE "$base/LICENSE"
    check __init__.py 441d3ba518a7eff10af69bb27df3967cc1918729eef97a01f105f7a81207d183
    check microdot.py 00121223e8b639b1b324e57a429d6bd486475f48db7ec511633986e187513125
    check helpers.py 561d019dcc69971f3fd7e3ad8ced69eb6d24683da6aa6c790b45f20635af359d
    check cors.py 78db309fbd3cada2cf6528f8aca1c4a2cbb6f03f68068648cb18a6e13eb08869
    check auth.py 072f83bf20b3aa4a2945a99093e8e931b8a75e4f5f3f93d0aea649dc80989d28
    cd ..
}

mongoose() {
    mkdir -p mongoose && cd mongoose
    base=https://raw.githubusercontent.com/cesanta/mongoose/$MONGOOSE_VER
    for f in mongoose.c mongoose.h LICENSE; do
        [ -f "$f" ] || curl -sSfLO "$base/$f"
    done
    check mongoose.c e9da98e6b76b15b8955802ee6b8aa7c8be00f691d52f6212ff9299b244ca2018
    check mongoose.h d12bfdd9dcc88df21825bd6e93577177714b0d42f601af94e56c6f4512a97d3c
    cd ..
}

stb() {
    mkdir -p stb && cd stb
    [ -f stb_image.h ] || curl -sSfLO "https://raw.githubusercontent.com/nothings/stb/$STB_COMMIT/stb_image.h"
    check stb_image.h dfc2aadda6295f67f677587773f25e5bf8c8ad427ed0db7c518525c91e72e020
    cd ..
}

tflite_micro() {
    if [ ! -d tflite-micro/.git ]; then
        git clone -q --depth 1 https://github.com/tensorflow/tflite-micro.git
    fi
    if [ -n "$TFLM_COMMIT" ]; then
        git -C tflite-micro fetch -q --depth 1 origin "$TFLM_COMMIT"
        git -C tflite-micro checkout -q "$TFLM_COMMIT"
    fi
    echo "ok  tflite-micro $(git -C tflite-micro rev-parse --short HEAD)"
    # Standalone source tree (also downloads flatbuffers, kissfft and
    # printf into tflite-micro/tensorflow/lite/micro/tools/make/downloads).
    if [ ! -d tflm-tree ]; then
        (cd tflite-micro && python3 tensorflow/lite/micro/tools/project_generation/create_tflm_tree.py ../tflm-tree)
    fi
    echo "ok  tflm-tree"
}

if [ $# -eq 0 ]; then set -- sqlite microdot mongoose stb tflite-micro; fi
for p in "$@"; do
    case $p in
        sqlite) sqlite ;;
        microdot) microdot ;;
        mongoose) mongoose ;;
        stb) stb ;;
        tflite-micro) tflite_micro ;;
        *) echo "unknown: $p" >&2; exit 2 ;;
    esac
done
