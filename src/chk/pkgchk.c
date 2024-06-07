#include "../../include/chk/pkgchk.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <math.h>

#define BUFFER 1025
#define HASH_SIZE 65

/**
 * Loads the package object from the given file path.
 * 
 * @param path The file path to the package file.
 * @return Pointer to the loaded package object, or NULL on failure.
 */
struct bpkg_obj* bpkg_load(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Error: File path does not exist: %s\n", path);
        return NULL;
    }

    struct bpkg_obj* obj = malloc(sizeof(struct bpkg_obj));
    if (!obj) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        fclose(file);
        return NULL;
    }

    memset(obj, 0, sizeof(struct bpkg_obj));

    obj->path = strdup(path);
    if (!obj) {
        fprintf(stderr, "Error: Failed to allocate memory\n");
        free(obj);
        fclose(file);
        return NULL;
    }

    // Read basic properties
    if (fscanf(file, "ident: %1024s\nfilename: %256s\nsize: %u\nnhashes: %u\n", 
               obj->ident, obj->filename, &obj->size, &obj->nhashes) != 4) {
        fprintf(stderr, "Failed to read essential properties\n");
        free(obj);
        fclose(file);
        return NULL;
    }

    // Allocate and read hashes
    obj->hashes = malloc(obj->nhashes * sizeof(char*));
    if (!obj->hashes) {
        fprintf(stderr, "Failed to allocate memory for hashes\n");
        free(obj);
        fclose(file);
        return NULL;
    }

    char line[BUFFER];
    if (fgets(line, sizeof(line), file) == NULL) {
        fprintf(stderr, "Failed to read the 'hashes:' descriptor line\n");
        bpkg_obj_destroy(obj);
        fclose(file);
        return NULL;
    }
    
    for (size_t i = 0; i < obj->nhashes; i++) {
        obj->hashes[i] = NULL; // Initialize to NULL for safe cleanup
        if (!fgets(line, sizeof(line), file)) {
            fprintf(stderr, "Failed to read hash line\n");
            bpkg_obj_destroy(obj);
            fclose(file);
            return NULL;
        }

        obj->hashes[i] = malloc(HASH_SIZE);
        if (!obj->hashes[i]) {
            fprintf(stderr, "Failed to allocate memory for hash\n");
            bpkg_obj_destroy(obj);
            fclose(file);
            return NULL;
        }

        if (sscanf(line, " %64s", obj->hashes[i]) != 1) {
            fprintf(stderr, "Failed to parse hash from line: %s\n", line);
            bpkg_obj_destroy(obj);
            fclose(file);
            return NULL;
        }
    }

    // Read the "nchunks" line
    if (fscanf(file, "nchunks: %u\n", &obj->nchunks) != 1) {
        fprintf(stderr, "Failed to read number of chunks\n");
        for (size_t i = 0; i < obj->nhashes; i++) free(obj->hashes[i]);
        free(obj->hashes);
        free(obj);
        fclose(file);
        return NULL;
    }

    // Allocate memory for chunks
    obj->chunks = malloc(obj->nchunks * sizeof(struct chunk));
    if (obj->chunks == NULL) {
        fprintf(stderr, "Failed to allocate memory for chunks\n");
        bpkg_obj_destroy(obj);
        fclose(file);
        return NULL;
    }

    char tempLabel[BUFFER];

    if (fgets(tempLabel, sizeof(tempLabel), file) == NULL) {
        fprintf(stderr, "Failed to read the 'chunks:' label\n");
        bpkg_obj_destroy(obj);
        fclose(file);
        return NULL;
    }
    // Read each chunk line
    for (uint32_t i = 0; i < obj->nchunks; i++) {
        if (fgets(line, sizeof(line), file) == NULL) {
            fprintf(stderr, "Failed to read chunk line %u\n", i);
            bpkg_obj_destroy(obj);
            fclose(file);
            return NULL;
        }
        // Skip initial whitespace characters
        char* start = line;
        while (*start == ' ' || *start == '\t') {
            start++;
        }
        if (sscanf(start, "%64s,%u,%u", obj->chunks[i].hash, &obj->chunks[i].offset, &obj->chunks[i].size) != 3) {
            fprintf(stderr, "Failed to parse chunk %u\n", i);
            bpkg_obj_destroy(obj);
            fclose(file);
            return NULL;
        }
    }

    fclose(file);
    return obj;
}

/**
 * Checks if the file specified in the package exists.
 * 
 * @param bpkg Pointer to the package object.
 * @return Query result indicating the status of the file check.
 */
struct bpkg_query bpkg_file_check(struct bpkg_obj* bpkg) {
    struct bpkg_query result = {0};
    result.hashes = malloc(sizeof(char*));  // Allocate memory for one string
    if (!result.hashes) {
        fprintf(stderr, "Failed to allocate memory for result hashes\n");
        result.len = 0;  // Indicate an error in allocation
        return result;
    }

    result.hashes[0] = malloc(50 * sizeof(char));  // Allocate memory for the string
    if (!result.hashes[0]) {
        fprintf(stderr, "Failed to allocate memory for result string\n");
        free(result.hashes);
        result.len = 0;  // Indicate an error in allocation
        return result;
    }
    result.len = 1;  // One string in the result

    if (!bpkg || bpkg->filename[0] == '\0') {
        strcpy(result.hashes[0], "Invalid input");
        return result;
    }

    struct stat buffer;
    // Checks if file exists
    if (stat(bpkg->filename, &buffer) == 0) {
        strcpy(result.hashes[0], "File Exists");
    } else {
        if (errno == ENOENT) {  // File does not exist
            // Attempt to create the file
            FILE *file = fopen(bpkg->filename, "w");
            if (file) {
                fclose(file);
                strcpy(result.hashes[0], "File Created");
            } else {
                strcpy(result.hashes[0], "Failed to create file");
            }
        } else { 
            strcpy(result.hashes[0], "Error checking file");
        }
    }

    return result;
}

/**
 * Retrieves all hashes from the package object.
 * 
 * @param bpkg Pointer to the package object.
 * @return Query result containing all hashes.
 */
struct bpkg_query bpkg_get_all_hashes(struct bpkg_obj* bpkg) {
    struct bpkg_query qry = {0};

    if (bpkg == NULL || (bpkg->nhashes == 0 && bpkg->nchunks == 0)) {
        fprintf(stderr, "No hashes available or invalid package object.\n");
        return qry;  // Return an empty query result with length 0
    }

    size_t total_hashes = bpkg->nhashes + bpkg->nchunks;
    qry.hashes = malloc(total_hashes * sizeof(char*));
    if (qry.hashes == NULL) {
        fprintf(stderr, "Failed to allocate memory for hash pointers.\n");
        return qry;  // Return an empty query result with length 0
    }

    // Copy each package hash string
    size_t i = 0;
    for (; i < bpkg->nhashes; i++) {
        qry.hashes[i] = strdup(bpkg->hashes[i]);
        if (qry.hashes[i] == NULL) {
            fprintf(stderr, "Failed to duplicate hash string.\n");
            while (i > 0) {
                free(qry.hashes[--i]);
            }
            free(qry.hashes);
            qry.hashes = NULL;
            qry.len = 0;
            return qry;  // Return an empty query result with length 0
        }
    }

    // Copy each chunk hash string
    for (size_t j = 0; j < bpkg->nchunks; j++, i++) {
        qry.hashes[i] = strdup(bpkg->chunks[j].hash);
        if (qry.hashes[i] == NULL) {
            fprintf(stderr, "Failed to duplicate chunk hash string.\n");
            while (i > 0) {
                free(qry.hashes[--i]);
            }
            free(qry.hashes);
            qry.hashes = NULL;
            qry.len = 0;
            return qry;  // Return an empty query result with length 0
        }
    }

    qry.len = total_hashes;
    return qry;  // Return the query with all hashes
}

/**
 * Retrieves the completed chunks from the package object.
 * 
 * @param bpkg Pointer to the package object.
 * @return Query result containing the completed chunks.
 */
struct bpkg_query bpkg_get_completed_chunks(struct bpkg_obj* bpkg) {
    struct bpkg_query qry = {0};

    if (!bpkg || !bpkg->hashes || !bpkg->hashes[0]) {
        fprintf(stderr, "Invalid package data or expected hashes are missing.\n");
        return qry;  // Return empty result if input is invalid
    }

    size_t* chunk_sizes = malloc(bpkg->nchunks * sizeof(size_t));
    if (!chunk_sizes) {
        fprintf(stderr, "Failed to allocate memory for chunk sizes.\n");
        return qry;
    }

    for (int i = 0; i < bpkg->nchunks; i++) {
        chunk_sizes[i] = bpkg->chunks[i].size;
    }

    struct merkle_tree* tree = build_merkle_tree_from_data(bpkg, chunk_sizes, bpkg->nchunks);
    free(chunk_sizes);

    if (!tree || !tree->hashes) {
        fprintf(stderr, "Failed to build the Merkle tree.\n");
        return qry;
    }

    // Determine if the Merkle root matches the expected root hash
    if (strcmp(tree->hashes[0], bpkg->hashes[0]) == 0) {
        // If the root matches, all chunks are considered complete
        qry.hashes = malloc(bpkg->nchunks * sizeof(char*));
        qry.len = bpkg->nchunks;
        for (int i = 0; i < bpkg->nchunks; i++) {
            char chunk_details[256];  // Assuming the details fit within 256 characters
            snprintf(chunk_details, sizeof(chunk_details), "%s, %u, %u",
                     bpkg->chunks[i].hash, bpkg->chunks[i].offset, bpkg->chunks[i].size);
            qry.hashes[i] = strdup(chunk_details);
        }
    } else {
        // If the root does not match, find and return only matching chunks
        char** temp_hashes = malloc(bpkg->nchunks * sizeof(char*));
        size_t count = 0;
        for (int i = 0; i < bpkg->nchunks; i++) {
            int leaf_index = tree->n_leaves - 1 + i;
            if (strcmp(tree->hashes[leaf_index], bpkg->chunks[i].hash) == 0) {
                char chunk_details[256];
                snprintf(chunk_details, sizeof(chunk_details), "%s, %u, %u",
                         bpkg->chunks[i].hash, bpkg->chunks[i].offset, bpkg->chunks[i].size);
                temp_hashes[count++] = strdup(chunk_details);
            }
        }
        qry.hashes = realloc(temp_hashes, count * sizeof(char*));  
        qry.len = count;
    }

    destroy_merkle_tree(tree);
    return qry;
}

/**
 * Retrieves the minimum set of hashes representing the current completion state.
 * 
 * @param bpkg Pointer to the package object.
 * @return Query result containing the minimum set of completed hashes.
 */
struct bpkg_query bpkg_get_min_completed_hashes(struct bpkg_obj* bpkg) {
    struct bpkg_query qry = {0};

    if (!bpkg || !bpkg->hashes) {
        fprintf(stderr, "Invalid package data.\n");
        return qry;
    }

    size_t* chunk_sizes = malloc(bpkg->nchunks * sizeof(size_t));
    if (!chunk_sizes) {
        fprintf(stderr, "Failed to allocate memory for chunk sizes.\n");
        return qry;
    }

    for (int i = 0; i < bpkg->nchunks; i++) {
        chunk_sizes[i] = bpkg->chunks[i].size;
    }

    struct merkle_tree* tree = build_merkle_tree_from_data(bpkg, chunk_sizes, bpkg->nchunks);
    free(chunk_sizes);

    if (!tree) {
        fprintf(stderr, "Failed to build Merkle tree.\n");
        return qry;
    }

    // Check if the root matches
    if (strcmp(tree->hashes[0], bpkg->hashes[0]) == 0) {
        qry.hashes = malloc(sizeof(char*));
        if (!qry.hashes) {
            fprintf(stderr, "Failed to allocate memory for query hashes.\n");
            destroy_merkle_tree(tree);
            return qry;
        }
        qry.hashes[0] = strdup(tree->hashes[0]);
        qry.len = 1;
        destroy_merkle_tree(tree);
        return qry;
    }

    // Traverse the tree and collect all relevant hashes
    qry.hashes = malloc((2 * bpkg->nchunks - 1) * sizeof(char*));
    if (!qry.hashes) {
        fprintf(stderr, "Failed to allocate memory for query hashes.\n");
        destroy_merkle_tree(tree);
        return qry;
    }

    int num_hashes = 0;
    bool* included = calloc(2 * bpkg->nchunks - 1, sizeof(bool));

    for (int i = 0; i < 2 * bpkg->nchunks - 1; i++) {
        if (i >= bpkg->nchunks - 1) {
            // Leaf nodes
            for (int j = 0; j < bpkg->nchunks; j++) {
                if (tree->hashes[i] && strcmp(tree->hashes[i], bpkg->chunks[j].hash) == 0) {
                    if (!included[i]) {
                        qry.hashes[num_hashes++] = strdup(tree->hashes[i]);
                        included[i] = true;
                    }
                    break;
                }
            }
        } else {
            // Internal nodes
            for (int j = 0; j < bpkg->nhashes; j++) {
                if (tree->hashes[i] && strcmp(tree->hashes[i], bpkg->hashes[j]) == 0) {
                    if (!included[i]) {
                        qry.hashes[num_hashes++] = strdup(tree->hashes[i]);
                        included[i] = true;
                    }
                    // Mark children as included
                    int left_child = 2 * i + 1;
                    int right_child = 2 * i + 2;
                    if (left_child < 2 * bpkg->nchunks - 1) included[left_child] = true;
                    if (right_child < 2 * bpkg->nchunks - 1) included[right_child] = true;
                    break;
                }
            }
        }
    }

    qry.len = num_hashes;
    destroy_merkle_tree(tree);
    free(included);
    return qry;
}

/**
 * Retrieves all chunk hashes starting from a given hash in the Merkle tree.
 * 
 * @param bpkg Pointer to the package object.
 * @param hash The starting hash.
 * @return Query result containing all descendant chunk hashes.
 */
struct bpkg_query bpkg_get_all_chunk_hashes_from_hash(struct bpkg_obj* bpkg, char* hash) {
    struct bpkg_query qry = {NULL, 0}; // Initialize the query result

    struct merkle_tree* tree = build_merkle_tree_from_bpkg(bpkg);

    if (!tree) {
        printf("Failed to build the Merkle tree.\n");
        return qry;
    }

    // Find the index of the given hash in the Merkle tree
    int index = find_hash_in_merkle_tree(tree, hash);
    if (index == -1) {
        printf("Hash not found in the Merkle tree.\n");
        destroy_merkle_tree(tree);
        return qry;
    }

    // Collect all descendant hashes starting from the found index
    collect_chunk_hashes(tree, index, &qry.hashes, &qry.len);

    destroy_merkle_tree(tree);

    return qry; 
}

/**
 * Destroys the query object, freeing all associated memory.
 * 
 * @param qry Pointer to the query object.
 */
void bpkg_query_destroy(struct bpkg_query* qry) {
    if (qry) {
    for (size_t i = 0; i < qry->len; i++) {
        free(qry->hashes[i]);
    }
    free(qry->hashes);
    qry->hashes = NULL;
    qry->len = 0;
    }
}

/**
 * Destroys the package object, freeing all associated memory.
 * 
 * @param obj Pointer to the package object.
 */
void bpkg_obj_destroy(struct bpkg_obj* obj) {
    if (obj) {
        if (obj->hashes) {
            for (size_t i = 0; i < obj->nhashes; i++) {
                free(obj->hashes[i]); 
            }
            free(obj->hashes);
        }
        free(obj->chunks);
        free(obj->path);
        free(obj);
    }
}
