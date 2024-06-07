#ifndef MERKLE_TREE_H
#define MERKLE_TREE_H

#include <stddef.h>
#include "../chk/pkgchk.h"
#include "../crypt/sha256.h"

#define SHA256_HEXLEN (64)

struct bpkg_obj;

struct merkle_tree {
    char** hashes;  // Array to store hashes at each node
    int n_leaves;   // Number of leaves, which is also the number of data chunks
};

struct merkle_tree* init_merkle_tree(size_t n_leaves);

char* compute_sha256_hex(const uint8_t* data, size_t size);

struct merkle_tree* build_merkle_tree_from_data(struct bpkg_obj* bpkg, size_t* chunk_sizes, size_t n_chunks);

struct merkle_tree* build_merkle_tree_from_bpkg(struct bpkg_obj* bpkg);

void destroy_merkle_tree(struct merkle_tree* tree);

int find_hash_in_merkle_tree(struct merkle_tree* tree, const char* hash);

void collect_chunk_hashes(struct merkle_tree* tree, int index, char*** hash_list, size_t* hash_count);

#endif
