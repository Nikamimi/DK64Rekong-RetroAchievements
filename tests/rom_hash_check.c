#include "rc_hash.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    char hash[33] = {0};
    const char* expected = "9ec41abf2519fc386cadd0731f6e868c";
    if (argc != 2) {
        fprintf(stderr, "Usage: rom_hash_check <path-to-your-own-DK64-ROM>\n");
        return 2;
    }
    if (!rc_hash_generate_from_file(hash, RC_CONSOLE_NINTENDO_64, argv[1])) {
        fprintf(stderr, "Could not generate an N64 ROM hash.\n");
        return 2;
    }
    if (strcmp(hash, expected) != 0) {
        fprintf(stderr, "Unsupported ROM hash: %s\n", hash);
        return 1;
    }
    printf("Supported DK64 retail ROM hash: %s\n", hash);
    return 0;
}
