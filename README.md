#PIPEWIRE VBAN UTILS

Here are CLI pipewire VBAN receptor and emitter
They have NO autoconnect option (use qpwgraph, qjackctl, helvum, ray-session and others to connect)
to have access to ALL channels. Just run them with -h key for help (they are like Benoit's utils).
Main difference from Wim Taymans's modules: my anti-latency algorythm like vban_jack ones presents.
We use them in on-stage mode.

To build just run build.sh script (Sorry, I had no time to write normal makefile).

Enjoy! They just work!

Also you can use unix-pipes (named, stdin/stdout) instead of network sockets
(VBAN packet format presents).
