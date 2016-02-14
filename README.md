Overview
========

FlowGraph is a library for defining and efficiently running tasks in a
dependency graph. For example, say you are running an RPC server that on each
request queries other services, performs tasks based on queried data, and logs
the results. This library allows you to define each of those tasks as a
separate unit that will run as soon as its dependencies are ready.

A dependency graph is a graph of tasks. Task produces an output and requires a
set of inputs. In the library, a function represents a task.  You build a
dependency graph by connecting tasks to Futures that produce input the task
depends on. By connecting a task to its dependencies you get a Future wrapping
the result of this task. This process is called lifting. 

See example in the `example/` directory to see how to use it.

Building & Installation
=======================

CMake is used to build and install the library. Use the following command to
build the library and the example:

    mkdir -p build && cd build && cmake .. && make

To install the library in a path of your own choosing run:

    cmake -DCMAKE_INSTALL_PREFIX:PATH=YOUR_PATH && make install

The library is called flow\_graph.
