## vdb

A single-header file C/C++ library for writing visualizations and quick interactive debugging tools that can be viewed from a browser. The library has no dependencies other than system headers (not even pthread!) and is ANSI C (C89) compliant.

The latest release version can be found in the `demo` directory, along with a couple of fun visualizations that you can inspire from.

## How it works

Your interface is a set of draw commands that you can submit in your application (the host). These are sent to the browser (the client) using Websockets, and rendered using WebGL and Javascript. Here is a simple example program demonstrating the loop mode:

    #include "vdb_release.h"
    #include <math.h>
    int main() {
        while (vdb_loop(60)) {
            static float t = 0.0f;
            vdb_point(cosf(t), sinf(t));
            t += 1.0f/60.0f;
        }
    }

This program will execute the code within the while block at 60 times per second, submitting the draw commands at the end of each loop and putting the thread to sleep. In the browser, you are presented with a point that rotates around, and a continue button, which will exit the loop when pressed.

The second mode is the begin/end block which submits draw commands without halting your program. This mode is useful when your program already has a loop and you want to visualize some output occasionally, without affecting performance. For example, say you are receiving video from a camera, and performing work on this, and you want to visualize the output in a streaming-fashion:

    int main() {
        while (recv_camera()) {
            // ... do your work

            vdb_begin();
            // visualize output
            vdb_end();
        }
    }

The browser would then receive the visualizations as they arrive, the rate of which is determined by how fast you call begin/end. vdb_begin will return false (0) if the network thread is already busy sending data, so you can use that to optimize your program if you are doing some complicated work inside the begin/end block.

## Developing

For normal development I've split up the project into several files, which get concatenated into a single file with the `make_release_lib.cpp` program.
