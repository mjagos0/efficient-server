## Efficient Server
This application is a **high-performance TCP server** designed to process location data from vehicles and compute the shortest drivable distances in a city. The server listens for incoming TCP connections using a custom protocol (4 byte length-prefixed **protobuf** messages) and handles two types of messages:
- **Walk**: Vehicle-reported locations and distances that are used to build directed graph representing the city.
- **OneToOne / OneToAll**: Requests to compute shortest paths to all using the accumulated graph.

Each path is formed from discrete, noisy location data points. The server clusters locations within 50 cm into a single physical node and averages received edge lengths when respoding to shortest path requests. Full problem specification at https://esw.pages.fel.cvut.cz/labs/efficient-servers/

### Techniques & Architecture
- **TCP Epoll Server**: Edge-triggered epoll TCP server.
- **ThreadPool**: ThreadPool with thread-safe request/response queues.
- **Uniform Hash Grid**: Thread-safe data structure to identify unique locations. It uses 8-neighborhood-lock mechanism.
- **Flat Graph representation & Flat Heap**: Thread-safe graph in respect to writers with cache-friendly readers access.

Main thread runs an epoll-based server that manages both server and client sockets, handling socket creation, closure, reading, and writing. It reads incoming client requests and enqueues them into a local request queue (mutex-protected). It makes sure that the client exists in clientQueue (mutex-protected).

The main thread is accompanied by 31 worker threads, which wait for clients to appear in clientQueue. Each client is served by at most 1 worker thread at a time, which ensures that each client requests are processed in the same order as they were received.

The workers work on CPU-bound tasks fully in parallel, processing both write (Walk) requests and read (OneToOne / OneToAll) requests using thread-safe grid & graph data structures.

### Performance & Bottlenecks
The server has achieved best time 0.247 seconds on evaluation server, where it was tested against ~100 concurrent client connections and ~100 000 requests.

CPU cycles are currently redistributed on CPU-bound tasks inside the worker threads:
```
+ 27.41% Worker::processO2a (inlined)
+ 25.66% Worker::processO2o (inlined)
+ 22.15% Worker::processWalk (inlined)
+ 20.93% Worker::deserializeProtobuf (inlined)
```

The server has achieved second place among all participant solutions.
The ladder can be viewed here: https://rtime.felk.cvut.cz/esw/server/results/all/

### Limitations & Assumptions
To achieve the highest possible performance on evaluation servers, these limitations were put in place:
- The number of maximum concurrent clients is hardcoded in MAX_CONCURRENT_CLIENTS.
- The number of maximum number of graph nodes and outgoing edges is MAX_NODES, MAX_EDGES respectively.
- The maximum length of a single protobuf request is MAX_PROTO_SIZE, and the responses cannot exceed WRITE_BUFFER_SIZE

### Compile & Run
```
meson setup buildDir
meson compile -C buildDir
./buildDir/main
```

Send requests to the server
```
nc -N localhost 50088 < walk3000.pbf | hexdump -C
```

### Author
Created for Czech Technical University, Faculty of Electrical Engineering, Efficient Software (B4M36ESW)
2025, Marek Jagoš (jagosmar@cvut.cz)
