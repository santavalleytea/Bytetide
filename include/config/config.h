#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

typedef struct {
    char directory[256];  // Path to the directory for storing files
    int max_peers;        // Maximum number of peers
    uint16_t port;        // Listening port
} Config;

int load_config(const char* filepath, Config* cfg);
int create_directory(const char* dir);
char* trim_whitespace(char* str);

#endif
