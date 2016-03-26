#!/usr/bin/python2

def find_path(graph, start, end, path=[]):
    path = path + [start]
    if start == end:
        return path
    if not graph.has_key(start):
        return None
    for node in graph[start]:
        if node not in path:
            newpath = find_path(graph, node, end, path)
            if newpath: return newpath
    return None

def path_len(path):
    if path == None:
        return -1
    else:
        return len(path)-1

graph = {};

# read graph from file
f = open('graph1', 'r')
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
        print(path_len(find_path(graph, src, dst)))

# close file
f.close()

