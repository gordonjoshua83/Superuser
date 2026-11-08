#!/bin/bash

tmpdir="$(mktemp -d)"
ORIG="$(pwd)"

rm -f update-su.zip
pushd "$tmpdir"
mkdir -p META-INF/com/google/android
cp "$ORIG"/assets/update-binary META-INF/com/google/android/update-binary
cp "$ORIG"/assets/install-recovery.sh .
cp -R "$ORIG"/libs/{armeabi-v7a,arm64-v8a,x86,x86_64} .

zip -r "$ORIG"/update-su.zip *
popd
rm -Rf "$tmpdir"
