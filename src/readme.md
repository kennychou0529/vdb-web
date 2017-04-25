A short reading guide
---------------------
This directory contains a bunch of source files which get concatenated into a 
single header-file library with the script `make_release_lib.cpp`.
The script creates a file `vdb_release.h` which contains `vdb.h` and all the source files 
in this directory. At the end it embeds `app.html` which contains the Javascript/HTML 
application that the user receives in their browser.

Below is a short description of the files and their role in the project.

File | What does it do
---|---
test.cpp              | Used to debug and test features.
vdb.h                 | Contains changelog and user interface (with inline documentation).
vdb.c                 | `#include`s all source files and contains shared declarations.
vdb_begin_end.c       | Implements vdb_begin() and vdb_end() part of the user interface
vdb_draw_commands.c   | Implements the draw commands part of the user interface
vdb_handle_message.c  | Parses messages sent from browser to application
vdb_network_threads.c | Responds to incoming connections and transmits draw calls to browser
vdb_push_buffer.c     | Implements the internal representation of the list of draw calls
tcp.c                 | UNIX/Windows wrapper around blocking TCP sockets
sha1.h/sha1.c         | Third party library for computing accept-key hash (required by websocket)
websocket.c           | Parses and forms websocket packet headers
