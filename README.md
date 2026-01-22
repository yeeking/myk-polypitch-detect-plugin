# myk-polypitch-detect-plugin
VST plugin that does realtime polyphonic audio to MIDI conversion

## Preparing to build
```
git clone https://github.com/yeeking/myk-polypitch-detect-plugin.git
cd myk-polypitch-detect-plugin
cd libs
# I always mess up submodules so do this instead:
git clone https://github.com/juce-framework/JUCE.git
git clone https://github.com/jatinchowdhury18/RTNeural.git
cd ..
./build-prep.sh
```

## then to actually do the build - on the CLI
```
cmake -B build .
cmake --build build --config Release -j 20
```

## Debug vs Release build

* The Debug build on my AMD Ryzen 7840U machine can analyse one second of audio in 2.6 seconds. So don't expect realtime with the Debug build!
* The Release build on the same machine does one second of audio in 0.026 seconds, allowing for realtime use with a one second latency. 

## Most of the clever code is from NeuralNote... 
I am claiming minimal credit for the hard stuff in this project: see https://github.com/DamRsn/NeuralNote which I enthusiastically borrowed from. I have retained the Apache license from that project and left all credits in source code where they were.

## TODO

* [done] apply downsampling to buffer prior to note extraction as per neuralnote
* possibly apply post processing on notes as per neuralnote
* write some proper tests to eveluate frame-> frame overlaps etc. 


