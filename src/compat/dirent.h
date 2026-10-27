#ifndef SF_DIRENT_H
#define SF_DIRENT_H

#ifdef _WIN32
#include <windows.h>
#include <stdlib.h>
#include <string.h>

struct dirent {
    char d_name[MAX_PATH];
};

typedef struct {
    HANDLE hFind;
    WIN32_FIND_DATAA findData;
    struct dirent entry;
    int first;
} DIR;

static DIR *opendir(const char *name) {
    char search_path[MAX_PATH];
    DIR *dir = (DIR *)malloc(sizeof(DIR));
    if (!dir) return NULL;
    snprintf(search_path, sizeof(search_path), "%s\\*.*", name);
    dir->hFind = FindFirstFileA(search_path, &dir->findData);
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }
    dir->first = 1;
    return dir;
}

static struct dirent *readdir(DIR *dir) {
    if (!dir || dir->hFind == INVALID_HANDLE_VALUE) return NULL;
    if (dir->first) {
        dir->first = 0;
    } else {
        if (!FindNextFileA(dir->hFind, &dir->findData)) return NULL;
    }
    strncpy(dir->entry.d_name, dir->findData.cFileName, sizeof(dir->entry.d_name) - 1);
    dir->entry.d_name[sizeof(dir->entry.d_name) - 1] = 0;
    return &dir->entry;
}

static int closedir(DIR *dir) {
    if (!dir) return -1;
    if (dir->hFind != INVALID_HANDLE_VALUE) FindClose(dir->hFind);
    free(dir);
    return 0;
}

#else
#include <dirent.h>
#endif

#endif

