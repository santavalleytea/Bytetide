#include "../../include/tree/merkletree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <libgen.h>

#define BUFFER 1025

// Function to compute SHA256 hash and return it as a hex string
char* compute_sha256_hex(const uint8_t* data, size_t size) {
    char* output = malloc(SHA256_HEXLEN + 1);
    if (!output) return NULL;

    struct sha256_compute_data sha_data;
    sha256_compute_data_init(&sha_data);
    sha256_update(&sha_data, (void*)data, size);
    sha256_finalize(&sha_data, (uint8_t*)output);
    sha256_output_hex(&sha_data, output);
    output[SHA256_HEXLEN] = '\0';  // Ensure null termination
    return output;
}

// Initialize the Merkle tree
struct merkle_tree* init_merkle_tree(size_t n_leaves) {
    struct merkle_tree* tree = malloc(sizeof(struct merkle_tree));
    if (!tree) return NULL;

    int n_total_nodes = 2 * n_leaves - 1;
    tree->hashes = malloc(n_total_nodes * sizeof(char*));
    if (!tree->hashes) {
        free(tree);
        return NULL;
    }

    memset(tree->hashes, 0, n_total_nodes * sizeof(char*));
    tree->n_leaves = n_leaves;
    return tree;
}

// Build the Merkle tree from the provided data chunks
struct merkle_tree* build_merkle_tree_from_data(struct bpkg_obj* bpkg, size_t* chunk_sizes, size_t n_chunks) {
    char file_path[BUFFER] = {0}; 

    // Find the last '/' in the path to isolate the directory
    char *last_slash = strrchr(bpkg->path, '/');
    if (last_slash != NULL) {
        size_t len_dir = last_slash - bpkg->path + 1;  // Calculate length of directory path
        strncpy(file_path, bpkg->path, len_dir);  // Copy the directory part
        file_path[len_dir] = '\0';  // Null-terminate the directory path
        strncat(file_path, bpkg->filename, BUFFER - len_dir - 1);  // Concatenate the filename
    } else {
        // If no '/' found, assume only filename in current directory
        strncpy(file_path, bpkg->filename, BUFFER - 1);
        file_path[BUFFER - 1] = '\0';  // Ensure null termination
    }

    FILE* file = fopen(file_path, "rb");
    if (!file) {
        perror("Error opening file");
        fprintf(stderr, "Error: Unable to open data file %s\n", file_path);
        return NULL;
    }

    struct merkle_tree* tree = init_merkle_tree(n_chunks);
    if (!tree) {
        fclose(file);
        return NULL;
    }

    // Compute and store hashes for leaf nodes
    int base_index = n_chunks - 1; // Index in the array where leaves start
    for (int i = 0; i < n_chunks; i++) {
        uint8_t* data_block = malloc(chunk_sizes[i]);
        if (!data_block) {
            fclose(file);
            destroy_merkle_tree(tree);
            return NULL;
        }

        fread(data_block, 1, chunk_sizes[i], file);
        tree->hashes[base_index + i] = compute_sha256_hex(data_block, chunk_sizes[i]);
        free(data_block);
    }
    fclose(file);

    // Compute hashes for interior nodes
    for (int i = base_index - 1; i >= 0; --i) {
        char left_hash[SHA256_HEXLEN + 1], right_hash[SHA256_HEXLEN + 1];
        strcpy(left_hash, tree->hashes[2 * i + 1]);
        if (2 * i + 2 < 2 * n_chunks - 1) {
            strcpy(right_hash, tree->hashes[2 * i + 2]);
        } else {
            right_hash[0] = '\0';  // Handle odd number of leaves
        }

        char concat_hashes[2 * SHA256_HEXLEN + 1];
        sprintf(concat_hashes, "%s%s", left_hash, right_hash);
        tree->hashes[i] = compute_sha256_hex((uint8_t*)concat_hashes, strlen(concat_hashes));
    }

    return tree;
}

struct merkle_tree* build_merkle_tree_from_bpkg(struct bpkg_obj* bpkg) {
    struct merkle_tree* tree = init_merkle_tree(bpkg->nchunks);
    if (!tree) {
        return NULL;
    }

    // Compute and store hashes for leaf nodes
    int base_index = bpkg->nchunks - 1;
    for (int i = 0; i < bpkg->nchunks; i++) {
        tree->hashes[base_index + i] = strdup(bpkg->chunks[i].hash);
        if (!tree->hashes[base_index + i]) {
            destroy_merkle_tree(tree);
            return NULL;
        }
    }

    // Compute hashes for interior nodes
    for (int i = base_index - 1; i >= 0; --i) {
        char left_hash[SHA256_HEXLEN + 1] = {0}, right_hash[SHA256_HEXLEN + 1] = {0};
        strcpy(left_hash, tree->hashes[2 * i + 1]);
        if (2 * i + 2 < 2 * bpkg->nchunks - 1) {
            strcpy(right_hash, tree->hashes[2 * i + 2]);
        }

        char concat_hashes[2 * SHA256_HEXLEN + 1] = {0};
        sprintf(concat_hashes, "%s%s", left_hash, right_hash);
        tree->hashes[i] = compute_sha256_hex((uint8_t*)concat_hashes, strlen(concat_hashes));
        if (!tree->hashes[i]) {
            destroy_merkle_tree(tree);
            return NULL;
        }
    }

    return tree;
}

int find_hash_in_merkle_tree(struct merkle_tree* tree, const char* hash) {
    if (!tree || !hash) return -1; // Check for null pointers

    for (int i = 0; i < 2 * tree->n_leaves - 1; i++) {
        if (tree->hashes[i] && strcmp(tree->hashes[i], hash) == 0) {
            return i; // Return the index if the hash matches
        }
    }
    // Hash not found
    return -1; 
}

void collect_chunk_hashes(struct merkle_tree* tree, int index, char*** hash_list, size_t* hash_count) {
    if (index >= 2 * tree->n_leaves - 1) return; // Check bounds
    if (index >= tree->n_leaves - 1) { 
        // Allocate more space for new hash
        *hash_list = realloc(*hash_list, (*hash_count + 1) * sizeof(char*));
        (*hash_list)[*hash_count] = strdup(tree->hashes[index]); // Duplicate the hash
        (*hash_count)++;
    } else {
        // Recursively collect from children
        collect_chunk_hashes(tree, 2 * index + 1, hash_list, hash_count);
        if (2 * index + 2 < 2 * tree->n_leaves - 1) {
            collect_chunk_hashes(tree, 2 * index + 2, hash_list, hash_count);
        }
    }
}

// Free the Merkle tree and all associated memory
void destroy_merkle_tree(struct merkle_tree* tree) {
    if (tree) {
        for (int i = 0; i < 2 * tree->n_leaves - 1; i++) {
            free(tree->hashes[i]);
        }
        free(tree->hashes);
        free(tree);
    }
}
