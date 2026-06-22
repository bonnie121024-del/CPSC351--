Section#: CPSC351 Summer 2026
Names: Bonnie Yang
Emails: bonnie121024@csu.fullerton.edu

Language: C++

How to run:
  1. Compile:       make
  2. Run receiver:  ./recv
  3. Run sender:    ./sender <filename> 

Example:
  Terminal 1:  ./recv
  Terminal 2:  ./sender song.mp3

The receiver saves the file as <original_filename>__recv.
Press Ctrl-C on the receiver to cleanly deallocate shared memory and message queue.
