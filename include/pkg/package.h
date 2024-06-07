#ifndef PACKAGE_H
#define PACKAGE_H

#include "../net/packet.h"
#include "../chk/pkgchk.h"
#include "../config/config.h"

struct package {
    char identifier[1024];
    char filename[256];
    int size;
    int nchunks;
    int completed_chunks;
    struct chunk *chunks;
    struct package *next;
};

void add_package(struct package *new_package);
void remove_package(const char *identifier);
struct package *get_package_list();
struct package *find_package(const char *identifier);
int load_package(const char *filename);
void print_packages();

#endif
