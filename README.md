# WSA-Connection-Tools

This repository contains a bunch of tools that lets you use a more modern style of C++ with event driven
networking on Windows. 

## Contents (at the moment):

Tool 		|		Description
------------|------------------
Connection Class | Object wrapper around a connected socket that is multi-threaded for send and receive and event-driven.

Connection class has been completed to be able to be included into source files. Send and receive operations seem to be working fine
with proper event being triggered on message receive. 
Right now, the Connection class should NOT be copied as the new copy will not have a thread associated with it and operations will not work with the copy.
I may make a heap-allocated version with a reference counter so the Connection class can be copied and may behave like a safe pointer.

In order to include any classes in this repo, just include any header files and make sure your compiler can access the source/object files.

In the future, I may make a less resource intensive version by having all Connections have their receive and send operations
run from one or two threads encapsulated by a single ConnectionCluster class.
