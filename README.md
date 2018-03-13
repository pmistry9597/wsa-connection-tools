# WSA-Connection-Tools

This repository contains a bunch of tools that lets you use a more modern style of C++ with event driven
networking on Windows. 

## Contents (at the moment):

Tool 		|		Description
------------|------------------
Connection Class | Object wrapper around a connected socket that is multi-threaded for send and receive and event-driven.

Currently, I am migrating my current Connection class into a form that can be easily included with headers.
The Connection class is incomplete right now.

In the future, I may make a less resource intensive version by having all Connections have their receive and send operations
run from one or two threads encapsulated by a single ConnectionCluster class.
