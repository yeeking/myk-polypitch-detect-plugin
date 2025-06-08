#!/usr/bin/env bash
set -euf -o pipefail
set -x
os=windows
arch="$(uname -m)"
file=onnxruntime.lib
version=v1.14.1-neuralnote.1
test_enabled=ON

if test "$(uname -s)" = "Darwin"; then
	os=macOS
	arch=universal
	file=libonnxruntime.a
	# To remove warnings about minimum macOS target versions
	version=v1.14.1-neuralnote.2
fi
if test "$(uname -s)" = "Linux"; then
	os=linux
	arch="$(uname -m)"
	file=libonnxruntime.lib
	# To remove warnings about minimum macOS target versions
	version=v1.14.1-neuralnote.0
	test_enabled=OFF
#        git submodule update --init --recursive --depth=1
        # onnx runtime - into the same folder for CODEX to be easily able to do it
        git clone https://github.com/polygon/libonnxruntime-neuralnote.git
        cd libonnxruntime-neuralnote
        git submodule update --init
        git checkout linux # switch to linux branch
        # replace the non-working make archive script with a working one
        cp ../make-archive.sh ./
        # make penv
        python3.10 -m venv venv # 3.11 or lower
        source venv/bin/activate
        pip install -r requirements.txt
        ./convert-model-to-ort.sh model.onnx
        chmod 755 build-linux.sh
        ./build-linux.sh model.required_operators_and_types.with_runtime_opt.config
        ./make-archive.sh v1.14.1-neuralnote.0
        cd ../
        echo Linux onnx-runtime and models wrapped up and saved
fi
dir="onnxruntime-${version}-${os}-${arch}"
archive="$dir.tar.gz"

if test "$(uname -s)" = "Linux"; then
	cp ./libonnxruntime-neuralnote/$archive .
fi

# If either the library or the ort model is missing or if an archive was found
# then fetch ort model and library again.
if test ! -f "Lib/ModelData/features_model.ort" -o ! -f "ThirdParty/onnxruntime/lib/$file" -o -f "$archive"; then
	if ! test -f "$archive"; then
		curl -fsSLO "https://github.com/tiborvass/libonnxruntime-neuralnote/releases/download/${version}/${archive}"
	fi
	rm -rf ThirdParty/$dir ThirdParty/onnxruntime
	tar -C ThirdParty/ -xvf "$archive"
	mv "ThirdParty/$dir" ThirdParty/onnxruntime
	mv ThirdParty/onnxruntime/model.with_runtime_opt.ort Lib/ModelData/features_model.ort
	rm "$archive"
fi

#ncpus="$(getconf _NPROCESSORS_ONLN || echo 1)"
#
#config=Release
#cmake -S . -B build -DCMAKE_BUILD_TYPE="${config}" -DBUILD_UNIT_TESTS="${test_enabled}" && cmake --build build -j "${ncpus}"

#if [[ "$test_enabled" = "ON" ]]; then
#	./build/Tests/UnitTests_artefacts/Release/UnitTests
#fi

#echo
#if test "$(uname -s)" = "Linux"; then
#	echo "Run: ./build/NeuralNote_artefacts/Release/Standalone/NeuralNote"
#else
#	echo "Run: ./build/NeuralNote_artefacts/Release/Standalone/NeuralNote.app/Contents/MacOS/NeuralNote"
#fi

echo Things look ready for a build. Now open your CMake aware IDE in the current folder.
