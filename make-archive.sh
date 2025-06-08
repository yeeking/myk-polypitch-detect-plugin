#!/usr/bin/env bash

set -xeuf -o pipefail

ORT_VERSION=v1.14.1
version="${1-}"
if test -z "$version"; then
	rev=$(git describe --always --abbrev=7 --dirty | tr '-' '.')
	version="neuralnote.git$rev"
fi

arch=$(uname -m)
os="windows"
builder="build-win.bat"
file="onnxruntime.lib"
if test "$(uname)" = "Darwin"; then
	os="macOS"
	builder="build-mac.sh"
	arch=universal
	file="libonnxruntime.a"
elif test "$(uname)" = "Linux"; then
	os="linux"
	builder="build-linux.sh"
fi

if ! test -e ./onnxruntime/.git; then
	git submodule update --init --recursive
fi
if test -n "$(git -C ./onnxruntime status --porcelain)"; then
	2>&1 echo "ERROR: local onnxruntime repo is not clean"
	exit 1
fi
#if test "$(git -C ./onnxruntime describe --always --tags)" != "$ORT_VERSION"; then#
#	2>&1 echo "ERROR: local onnxruntime repo is not clean"
#	exit 1
#fi

set -e
dir="onnxruntime-${version}-${os}-${arch}"
test -n "$dir"
archive="$dir.tar.gz"
rm -rf "./$dir"
./convert-model-to-ort.sh "model.onnx"
"./$builder" model.required_operators_and_types.with_runtime_opt.config "${version}"

# Believe it or not, git is the most cross platform archiving tool.
git add -f include lib model.with_runtime_opt.ort
git archive -o "$archive" --prefix "$dir/" $(git stash create) -- include lib model.with_runtime_opt.ort
git reset
git gc --prune=now
