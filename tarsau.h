#ifndef TARSAU_H
#define TARSAU_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#define MAX_FILES 32
#define MAX_FILE_SIZE (200 * 1024 * 1024)  // 200 MB
#define MAX_PATH_LENGTH 4096
#define DEFAULT_ARCHIVE "a.sau"
#define HEADER_SIZE_LENGTH 10

// Dosya bilgisi yapısı
typedef struct {
    char name[MAX_PATH_LENGTH];
    mode_t permissions;
    long size;
} FileInfo;

// Arşiv yapısı
typedef struct {
    int file_count;
    FileInfo files[MAX_FILES];
    long total_size;
} Archive;

// Fonksiyon prototipleri
int create_archive(char **input_files, int file_count, const char *output_file);
int extract_archive(const char *archive_file, const char *output_dir);
int txt_mi(const char *filename);
int validate_archive(const char *filename);
void print_usage(void);
void create_directory_recursive(const char *path);

#endif // TARSAU_H
