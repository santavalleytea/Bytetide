#include "../include/chk/pkgchk.h"
#include "../include/crypt/sha256.h"
#include "../include/tree/merkletree.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define SHA256_HEX_LEN (64)

int arg_select(int argc, char** argv, int* asel, char* harg) {
	char* cursor = argv[2];
	*asel = 0;
	if(argc < 3) {
		puts("bpkg or flag not provided");
		exit(1);
	}

	if(strcmp(cursor, "-all_hashes") == 0) {
		*asel = 1;
	}
	if(strcmp(cursor, "-chunk_check") == 0) {
		*asel = 2;
	}
	if(strcmp(cursor, "-min_hashes") == 0) {
		*asel = 3;
	}
	if(strcmp(cursor, "-hashes_of") == 0) {
		if(argc < 4) {
			puts("filename not provided");
			exit(1);
		}
		*asel = 4;
		strncpy(harg, argv[3], SHA256_HEX_LEN);
	}
	if(strcmp(cursor, "-file_check") == 0) {
		if(argc < 3) {
			puts("filename not provided");
			exit(1);
		}
		*asel = 5;
	}
	// Merkle Tree Testing
	if(strcmp(cursor, "-merkle_test") == 0) {
		*asel = 6;
	}
	if(strcmp(cursor, "-merkle_data") == 0) {
		*asel = 7;
	}

	return *asel;
}


void bpkg_print_hashes(struct bpkg_query* qry) {
	for(int i = 0; i < qry->len; i++) {
		printf("%.64s\n", qry->hashes[i]);
	}
}

void print_bpkg_details(const struct bpkg_obj* obj) {
    if (!obj) {
        printf("No data to display.\n");
        return;
    }

    printf("identifier: %s\n", obj->ident);
    printf("filename: %s\n", obj->filename);
    printf("size: %u\n", obj->size);
    printf("nhashes: %u\n", obj->nhashes);
    
    printf("hashes:\n");
    for (unsigned int i = 0; i < obj->nhashes; i++) {
        printf("  %s\n", obj->hashes[i]);
    }

    printf("nchunks: %u\n", obj->nchunks);
    printf("chunks:\n");
    for (unsigned int i = 0; i < obj->nchunks; i++) {
        printf("  Hash: %s, Offset: %u, Size: %u\n", obj->chunks[i].hash, obj->chunks[i].offset, obj->chunks[i].size);
    }
}

void print_merkle_tree(struct merkle_tree* tree) {
    if (!tree) {
        printf("Merkle tree construction failed.\n");
        return;
    }

    for (int i = 0; i < 2 * tree->n_leaves - 1; i++) {
        printf("Node %d Hash: %s\n", i, tree->hashes[i] ? tree->hashes[i] : "NULL");
    }
    destroy_merkle_tree(tree);
}

void test_merkle_tree_construction(const char* filename) {
    struct bpkg_obj* obj = bpkg_load(filename);
    if (!obj) {
        printf("Failed to load package object.\n");
        return;
    }

    // Construct Merkle tree from the package object
    struct merkle_tree* tree = build_merkle_tree_from_bpkg(obj);
    if (!tree) {
        printf("Failed to construct Merkle tree.\n");
        bpkg_obj_destroy(obj);
        return;
    }

    // Print Merkle tree hashes
    print_merkle_tree(tree);

    // Clean up
    bpkg_obj_destroy(obj);
}

void test_merkle_tree_construction_from_data(const char* filename) {
    // Load the package object from the given file
    struct bpkg_obj* obj = bpkg_load(filename);
    if (!obj) {
        printf("Failed to load package object.\n");
        return;
    }

    // Allocate memory for chunk sizes and fill it with the sizes of each chunk
    size_t* chunk_sizes = malloc(obj->nchunks * sizeof(size_t));
    if (!chunk_sizes) {
        printf("Failed to allocate memory for chunk sizes.\n");
        bpkg_obj_destroy(obj);
        return;
    }

    for (size_t i = 0; i < obj->nchunks; i++) {
        chunk_sizes[i] = obj->chunks[i].size;
    }

    // Construct the Merkle tree from the package object data
    struct merkle_tree* tree = build_merkle_tree_from_data(obj, chunk_sizes, obj->nchunks);
    if (!tree) {
        printf("Failed to construct Merkle tree.\n");
        free(chunk_sizes);
        bpkg_obj_destroy(obj);
        return;
    }

    // Print Merkle tree hashes
    print_merkle_tree(tree);

    // Clean up
    free(chunk_sizes);
    bpkg_obj_destroy(obj);
}

int main(int argc, char** argv) {
	int argselect = 0;
	char hash[SHA256_HEX_LEN + 1];

	if(arg_select(argc, argv, &argselect, hash)) {
		struct bpkg_query qry = { 0 };
		struct bpkg_obj* obj = bpkg_load(argv[1]);
		
		if(!obj) {
			puts("Unable to load pkg and tree");
			exit(1);
		}

		if(argselect == 1) {
			qry = bpkg_get_all_hashes(obj);
			bpkg_print_hashes(&qry);
			bpkg_query_destroy(&qry);
		} else if(argselect == 2) {

			qry = bpkg_get_completed_chunks(obj);
			bpkg_print_hashes(&qry);
			bpkg_query_destroy(&qry);
		} else if(argselect == 3) {

			qry = bpkg_get_min_completed_hashes(obj);
			bpkg_print_hashes(&qry);
			bpkg_query_destroy(&qry);
		} else if(argselect == 4) {

			qry = bpkg_get_all_chunk_hashes_from_hash(obj, 
					hash);
			bpkg_print_hashes(&qry);
			bpkg_query_destroy(&qry);
		} else if(argselect == 5) {

			qry = bpkg_file_check(obj);
			bpkg_print_hashes(&qry);
			bpkg_query_destroy(&qry);
		} else if(argselect == 6) {
			test_merkle_tree_construction(argv[1]);
		} else if(argselect == 7) {
			test_merkle_tree_construction_from_data(argv[1]);
		} else {
			puts("Argument is invalid");
			return 1;
		}
		bpkg_obj_destroy(obj);

	}

	return 0;
}   
