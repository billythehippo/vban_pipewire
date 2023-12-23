#!/bin/bash
g++ -Wall main.cpp ../vban_common/vban_functions.cpp ../vban_common/udpsocket.cpp ../vban_common/pipewire_backend.cpp -o3 -o pw_vban_receptor $(pkg-config --cflags --libs libpipewire-0.3)