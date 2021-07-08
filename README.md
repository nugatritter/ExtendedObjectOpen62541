# ExtendedObjectOpen62541 - custom data types for Open62541
:construction:
This module is under development.
Please contact the [author](mailto:nugatritter@gmail.com) for details.
:construction:

This module is an extension for [Open62541](https://github.com/open62541/open62541/) to support custom data types
on the client side.
Both the type directories and the data types from the node tree are evaluated.
The following custom data types can be handled:
- structures
- structures with optional fields
- option sets
- enumerations
- unions

At the moment, only reading values is supported.

## Target systems
This project works for 64bit Windows and Linux systems.<br>
This project has been tested with
- Windows 10 21H1 (64bit)
- Ubuntu 20.04 LTS (64bit)
## Prerequisites

-   [open62541](https://open62541.org) (tested with v1.2.2 #1180574)
-   [libxml2](http://xmlsoft.org) (tested with v2.9.3)

## Building project

- [build Open62541](https://open62541.org/doc/current/building.html) with the following options:
  - BUILD_SHARED_LIBS
  - UA_ENABLE_STATUSCODE_DESCRIPTIONS (enabled by default)
- [install or build](http://xmlsoft.org/downloads.html) libxml2 (libxml2-dev)
- don't forget to update the cache for the linker (ldconfig)
- compile and link this project
  - g++ -IPATH_TO_OPEN62541/include -IPATH_TO_OPEN62541/arch -IPATH_TO_OPEN62541/plugins/include -IPATH_TO_OPEN62541/build/src_generated -I/usr/include/libxml2 -O0 -g3 -Wall -c -fmessage-length=0 -Wno-unknown-pragmas -MMD -MP -MF"ExtendedObjectOpen62541.d" -MT"ExtendedObjectOpen62541.o" -o "ExtendedObjectOpen62541.o" "PATH_TO_ExtendedObjectOpen62541/ExtendedObjectOpen62541.cpp" 
  - g++ -LPATH_TO_OPEN62541/build/bin -L/usr/lib/x86_64-linux-gnu -o "ExtendedObjectOpen62541"  ./ExtendedObjectOpen62541.o   -lopen62541 -lxml2

## Usage
1. open an OPC UA client session
2. call *initializeCustomDataTypes*
3. your'e done, custom data types can be processed; e.g. call *UA_Client_readValueAttribute* and *UA_PrintValue*

You will find an example in the main function.
