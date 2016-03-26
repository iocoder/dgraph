#!/usr/bin/python2

import sys

def find_path(graph, start, end, path=[]):
    dist = {};
    for node in graph:
        if node == start:
            dist[node] = 0
        else:
            dist[node] = 100000
    for i in graph:
        for node in graph:
            for adj in graph[node]:
                if not dist.has_key(adj):
                    if adj == start:
                        dist[adj] = 0
                    else:
                        dist[adj] = 100000
                if (dist[node]+1 < dist[adj]):
                    dist[adj] = dist[node]+1;
    return dist[end];

graph = {};

# read graph from file
f = open(sys.argv[1], 'r')
while 1:
    b = f.readline()
    src = b.split()[0]
    if src == 'S':
        break
    if not graph.has_key(src):
        graph[src] = [];
    dest = b.split()[1]
    graph[src].append(dest)

# read queries
while 1:
    b = f.readline()
    if b == '\n': # empty line
        continue;
    if b == 'Q\n': # empty line
        break;
    cmd = b.split()[0]
    if cmd == 'F': # end of batch
        continue;
    src = b.split()[1]
    dst = b.split()[2]
    # if cmd == 'A':
    # if cmd == 'D':
    if cmd == 'Q':
        print(find_path(graph, src, dst))

# close file
f.close()

