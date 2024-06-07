#ifndef PKGCHK_H
#define PKGCHK_H

#include <stddef.h>
#include <stdint.h>
#include "../tree/merkletree.h"
#include "../crypt/sha256.h"

struct bpkg_query {
	char** hashes;
	size_t len;
};

struct bpkg_obj {
    char ident[1025]; // Identifier string
    char filename[257]; // Filename
    uint32_t size; // Size of the file in bytes
    uint32_t nhashes; // Number of non-leaf hashes
    char** hashes; // Array of string hashes
    uint32_t nchunks; // Number of data chunks
    struct chunk* chunks; // Array of data chunks
    char* path; // Store the path of .bpkg file
};

struct chunk {
    char hash[65]; // Hash of the data block
    uint32_t offset; // Offset within the file
    uint32_t size; // Size of the chunk in bytes
    char *data;
};

struct bpkg_obj* bpkg_load(const char* path);

struct bpkg_query bpkg_file_check(struct bpkg_obj* bpkg);

struct bpkg_query bpkg_get_all_hashes(struct bpkg_obj* bpkg);

struct bpkg_query bpkg_get_completed_chunks(struct bpkg_obj* bpkg);

struct bpkg_query bpkg_get_min_completed_hashes(struct bpkg_obj* bpkg); 

struct bpkg_query bpkg_get_all_chunk_hashes_from_hash(struct bpkg_obj* bpkg, char* hash);

void bpkg_query_destroy(struct bpkg_query* qry);

void bpkg_obj_destroy(struct bpkg_obj* obj);

#endif
