#!/bin/bash
mkdir vban_utils
cd vban_emitter_jack
make
mv vban_emitter_jack ../vban_utils/vban_emitter_jack
make clean
cd ..
cd vban_emitter_pw
make
mv vban_emitter_pw ../vban_utils/vban_emitter_pw
make clean
cd ..
mkdir vban_utils
cd vban_receptor_jack
make
mv vban_receptor_jack ../vban_utils/vban_receptor_jack
make clean
cd ..
cd vban_receptor_pw
make
mv vban_receptor_pw ../vban_utils/vban_receptor_pw
make clean
cd ..
