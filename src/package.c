#include "../include/pkg/package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct package *package_list = NULL;
extern Config config;

// Function to add a package to the end of the list
void add_package(struct package *new_package) {
    new_package->next = NULL;
    if (!package_list) {
        package_list = new_package;
    } else {
        struct package *current = package_list;
        while (current->next) {
            current = current->next;
        }
        current->next = new_package;
    }
}

// Function to remove a package
void remove_package(const char *identifier) {
    struct package **current = &package_list;
    while (*current && strcmp((*current)->identifier, identifier) != 0) {
        current = &(*current)->next;
    }
    if (*current) {
        struct package *to_free = *current;
        *current = (*current)->next;
        for (int i = 0; i < to_free->nchunks; i++) {
            free(to_free->chunks[i].data);
        }
        free(to_free->chunks);
        free(to_free);
    }
}

// Function to get the package list
struct package *get_package_list() {
    return package_list;
}

// Function to find a package by identifier
struct package *find_package(const char *identifier) {
    struct package *current = package_list;
    while (current && strncmp(current->identifier, identifier, strlen(identifier)) != 0) {        
        current = current->next;
    }
    return current;
}

// Function to load a package from a file
int load_package(const char *pkg_filename) {
    if (!pkg_filename || strlen(pkg_filename) == 0) {
        printf("Missing file argument.\n");
        return 0;
    }

    // Create the full path to the .bpkg file using the directory from the config
    char full_pkg_path[512];
    snprintf(full_pkg_path, sizeof(full_pkg_path) + 1, "%s/%s", config.directory, pkg_filename);

    // Load the .bpkg file
    struct bpkg_obj *pkg = bpkg_load(full_pkg_path);
    if (!pkg) {
        printf("Unable to parse bpkg file: %s\n", full_pkg_path);
        return 0;
    }

    struct chunk *chunks = malloc(pkg->nchunks * sizeof(struct chunk));
    if (!chunks) {
        fprintf(stderr, "Failed to allocate memory for chunks\n");
        bpkg_obj_destroy(pkg);
        return 0;
    }

    for (int i = 0; i < pkg->nchunks; i++) {
        strncpy(chunks[i].hash, pkg->chunks[i].hash, 65);
        chunks[i].offset = pkg->chunks[i].offset;
        chunks[i].size = pkg->chunks[i].size;
        chunks[i].data = malloc(chunks[i].size);
        if (!chunks[i].data) {
            fprintf(stderr, "Failed to allocate memory for chunk data\n");
            for (int j = 0; j < i; j++) {
                free(chunks[j].data);
            }
            free(chunks);
            bpkg_obj_destroy(pkg);
            return 0;
        }
        memset(chunks[i].data, 0, chunks[i].size); // Initialize the data buffer
    }

    // Create the full path to the binary file using the directory from the config
    char full_data_path[512];
    snprintf(full_data_path, sizeof(full_data_path) + 1, "%s/%s", config.directory, pkg->filename);

    // Use bpkg_get_completed_chunks to check if the package is complete
    struct bpkg_query completed_chunks = bpkg_get_completed_chunks(pkg);
    int is_complete = (completed_chunks.len == pkg->nchunks);

    // Create and add the new package to the list
    struct package *new_package = (struct package *)malloc(sizeof(struct package));
    strncpy(new_package->identifier, pkg->ident, 1024);
    strncpy(new_package->filename, pkg->filename, 256);
    new_package->size = pkg->size;
    new_package->nchunks = pkg->nchunks;
    new_package->completed_chunks = is_complete ? new_package->nchunks : 0;
    new_package->chunks = chunks;
    new_package->next = NULL;

    add_package(new_package);

    bpkg_obj_destroy(pkg);
    bpkg_query_destroy(&completed_chunks);

    return 1;
}

void print_packages() {
    struct package *current = get_package_list();
    if (!current) {
        printf("No packages managed\n");
    } else {
        int count = 1;
        while (current) {
            printf("%d. %.32s, %s/%s : %s\n", count, current->identifier, config.directory, current->filename,
                   (current->completed_chunks == current->nchunks) ? "COMPLETED" : "INCOMPLETE");
            current = current->next;
            count++;
        }
    }
}
