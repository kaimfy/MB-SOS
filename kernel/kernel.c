#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"
#include <stdint.h>

char *strchr(const char *str, int c) {
    while (*str != '\0') {
        if (*str == (char)c) {
            return (char *)str; // Found the character
        }
        str++;
    }
    return NULL; // Character not found
}


#define DISK_SIZE 1024     // Size of RAM disk in bytes
#define BLOCK_SIZE 32      // Block size in bytes
#define NUM_BLOCKS (DISK_SIZE / BLOCK_SIZE)
#define MAX_FILES 10       // Maximum number of files

void *memset(void *ptr, int value, size_t num) {
    unsigned char *p = ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
    return ptr;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

char *strcpy(char *dest, const char *src) {
    char *ret = dest;
    while ((*dest++ = *src++)) {
        // Copy each character
    }
    return ret;
}

int strncmp(const char *str1, const char *str2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (str1[i] != str2[i] || str1[i] == '\0' || str2[i] == '\0') {
            return str1[i] - str2[i];
        }
    }
    return 0;
}

char *strtok(char *str, const char *delim) {
    static char *last;
    if (str) {
        last = str;
    }

    if (!last || *last == '\0') {
        return NULL;
    }

    // Skip leading delimiters
    char *start = last;
    while (*start && strchr(delim, *start)) {
        start++;
    }

    if (*start == '\0') {
        last = NULL;
        return NULL;
    }

    char *end = start;
    while (*end && !strchr(delim, *end)) {
        end++;
    }

    if (*end == '\0') {
        last = NULL;
    } else {
        *end = '\0';
        last = end + 1;
    }

    return start;
}

typedef struct {
    char name[16];
    int start_block;
    int size;
} File;

char *disk;            // Pointer to the RAM disk
int fat[NUM_BLOCKS];   // FAT table
File files[MAX_FILES]; // File metadata
int file_count = 0;

// Initialize the filesystem
void initialize_filesystem() {
    disk = (char *)kmalloc(DISK_SIZE, 0, 0); // Allocate RAM disk
    memset(disk, 0, DISK_SIZE);             // Clear disk
    memset(fat, -1, sizeof(fat));           // -1 indicates unused blocks
    memset(files, 0, sizeof(files));
    file_count = 0;
    kprint("Filesystem initialized!\n");
}

// Find a free block in the FAT
int find_free_block() {
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (fat[i] == -1) return i;
    }
    return -1; // No free block available
}

// Create a file in the filesystem
void create_file(const char *name, const char *data) {
    if (file_count >= MAX_FILES) {
        kprint("Error: File limit reached!\n");
        return;
    }

    int size = strlen(data);
    int blocks_needed = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int first_block = -1, prev_block = -1;

    for (int i = 0; i < blocks_needed; i++) {
        int block = find_free_block();
        if (block == -1) {
            kprint("Error: Not enough space on disk!\n");
            return;
        }

        if (first_block == -1) first_block = block;
        if (prev_block != -1) fat[prev_block] = block;
        prev_block = block;

        // Write data to the block
        int offset = block * BLOCK_SIZE;
        strncpy(&disk[offset], data + i * BLOCK_SIZE, BLOCK_SIZE);
    }
    fat[prev_block] = -1; // Mark end of file

    // Add to file directory
    strcpy(files[file_count].name, name);
    files[file_count].start_block = first_block;
    files[file_count].size = size;
    file_count++;
    kprint("File created successfully!\n");
}

// Read a file from the filesystem
void read_file(const char *name) {
    for (int i = 0; i < file_count; i++) {
        if (strcmp(files[i].name, name) == 0) {
            int block = files[i].start_block;
            kprint("Contents of file: ");
            while (block != -1) {
                char buffer[BLOCK_SIZE + 1] = {0};
                strncpy(buffer, &disk[block * BLOCK_SIZE], BLOCK_SIZE);
                kprint(buffer);
                block = fat[block];
            }
            kprint("\n");
            return;
        }
    }
    kprint("Error: File not found!\n");
}

// Handle user input
void user_input(char *input) {
    if (strcmp(input, "END") == 0) {
        kprint("Stopping the CPU. Bye!\n");
        __asm volatile("hlt");
    } else if (strcmp(input, "PAGE") == 0) {
        uint32_t phys_addr;
        uint32_t page = kmalloc(1000, 1, &phys_addr);
        char page_str[16] = "";
        hex_to_ascii(page, page_str);
        char phys_str[16] = "";
        hex_to_ascii(phys_addr, phys_str);
        kprint("Page: ");
        kprint(page_str);
        kprint(", physical address: ");
        kprint(phys_str);
        kprint("\n");
    } else if (strncmp(input, "CREATE ", 7) == 0) {
        char *args = input + 7;
        char *filename = strtok(args, " ");
        char *data = strtok(0, "");
        if (filename && data) {
            create_file(filename, data);
        } else {
            kprint("Usage: CREATE <filename> <content>\n");
        }
    } else if (strncmp(input, "READ ", 5) == 0) {
        char *filename = input + 5;
        read_file(filename);
    } else {
        kprint("Unknown command. Available: END, PAGE, CREATE, READ\n");
    }
    kprint("> ");
}

// Kernel main function
void kernel_main() {
    isr_install();
    irq_install();

    asm("int $2");
    asm("int $3");

    initialize_filesystem();

    kprint("Type something, it will go through the kernel\n"
           "Commands: END, PAGE, CREATE <filename> <content>, READ <filename>\n> ");
}
