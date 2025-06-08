# myk-polypitch-detect-plugin
VST plugin that does realtime polyphonic audio to MIDI conversion

## Preparing to build
```
git clone https://github.com/yeeking/myk-polypitch-detect-plugin.git
cd myk-polypitch-detect-plugin
cd ThirdParty
git clone https://github.com/juce-framework/JUCE.git
git clone https://github.com/jatinchowdhury18/RTNeural.git
cd ..
./build-prep.sh
```

## then to actually do the build - on the CLI
```
cmake -B build .
cmake --build build --config Debug -j 20
```

## Most of the clever code is from NeuralNote... 
I am claiming minimal credit for the hard stuff in this project: see https://github.com/DamRsn/NeuralNote which I enthusiastically borrowed from. I have retained the Apache license from that project and left all credits in source code where they were.


