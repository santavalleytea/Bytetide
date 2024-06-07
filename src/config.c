#include "../include/config/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

// Function to trim leading and trailing whitespace
char* trim_whitespace(char* str) {
    char* end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) 
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    *(end + 1) = '\0';

    return str;
}

int create_directory(const char* dir) {
    struct stat st = {0};
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0700) == -1) {
            perror("Failed to create directory");
            return 3;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Provided path is a file, not a directory\n");
        return 3;
    }
    return 0;
}

int load_config(const char* filepath, Config* cfg) {
    FILE *file = fopen(filepath, "r");
    if (!file) {
        perror("Failed to open configuration file");
        return 1;
    }

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        char* token = strtok(line, ":");
        if (token) {
            char* key = trim_whitespace(token);
            token = strtok(NULL, "\n");
            if (token) {
                char* value = trim_whitespace(token);
                if (strcmp(key, "directory") == 0) {
                    strcpy(cfg->directory, value);
                    if (create_directory(cfg->directory) != 0) {
                        return 3;
                    }
                } else if (strcmp(key, "max_peers") == 0) {
                    cfg->max_peers = atoi(value);
                    if (cfg->max_peers < 1 || cfg->max_peers > 2048) {
                        fprintf(stderr, "Invalid max_peers: %d\n", cfg->max_peers);
                        return 4;
                    }
                } else if (strcmp(key, "port") == 0) {
                    cfg->port = (uint16_t)atoi(value);
                    if (cfg->port <= 1024 || cfg->port > 65535) {
                        fprintf(stderr, "Invalid port: %u\n", cfg->port);
                        return 5;
                    }
                } else {
                    fprintf(stderr, "Unrecognized configuration line: %s\n", line);
                }
            }
        }
    }

    fclose(file);
    return 0;
}
