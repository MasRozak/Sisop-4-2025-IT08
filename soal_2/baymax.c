#define FUSE_USE_VERSION 35
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#define RELICS_DIR "relics"
#define LOG_FILE "activity.log"
#define CHUNK_SIZE 1024
#define ZIP_URL "https://drive.usercontent.google.com/u/0/uc?id=1MHVhFT57Wa9Zcx69Bv9j5ImHc9rXMH1c&export=download"
#define ZIP_FILE "relics.zip"
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
char mount_path[PATH_MAX];

void log_activity(const char *fmt, ...) {
    char logpath[PATH_MAX + 64];
    snprintf(logpath, sizeof(logpath), "%s/%s", mount_path, LOG_FILE);
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);

    va_list args;
    va_start(args, fmt);
    vfprintf(log, fmt, args);
    va_end(args);

    fprintf(log, "\n");
    fclose(log);
}

void download_and_extract() {
    if (access(RELICS_DIR, F_OK) == 0) return;

    printf("Downloading relics.zip...\n");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "wget -O %s \"%s\"", ZIP_FILE, ZIP_URL);
    system(cmd);

    printf("Extracting...\n");
    mkdir(RELICS_DIR, 0755);
    snprintf(cmd, sizeof(cmd), "unzip -o %s -d %s", ZIP_FILE, RELICS_DIR);
    system(cmd);
    unlink(ZIP_FILE);
    printf("Ready.\n");
}

int list_chunks(const char *filename, char chunks[100][256]) {
    int count = 0;
    for (int i = 0; i < 100; ++i) {
        snprintf(chunks[i], 256, RELICS_DIR "/%s.%03d", filename, i);
        if (access(chunks[i], F_OK) != 0) break;
        count++;
    }
    return count;
}

static int baymax_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = __S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    char chunks[100][256];
    const char *filename = path + 1;
    int count = list_chunks(filename, chunks);
    if (count == 0) return -ENOENT;

    stbuf->st_mode = __S_IFREG | 0644;
    stbuf->st_nlink = 1;

    size_t total_size = 0;
    for (int i = 0; i < count; ++i) {
        struct stat st;
        if (stat(chunks[i], &st) == 0)
            total_size += st.st_size;
    }
    stbuf->st_size = total_size;
    return 0;
}

static int baymax_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi,
                          enum fuse_readdir_flags flags) {
    printf("readdir called on path: %s\n", path);
    (void) offset;
    (void) fi;
    (void) flags;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    DIR *d = opendir(RELICS_DIR);
    if (!d) return -ENOENT;

    struct dirent *entry;
    char seen[100][256];
    int seen_count = 0;

    while ((entry = readdir(d)) != NULL) {
        char *dot = strrchr(entry->d_name, '.');
        if (!dot) continue;

        size_t len = dot - entry->d_name;
        int exists = 0;
        for (int i = 0; i < seen_count; ++i) {
            if (strncmp(seen[i], entry->d_name, len) == 0) {
                exists = 1;
                break;
            }
        }

        if (!exists) {
            strncpy(seen[seen_count], entry->d_name, len);
            seen[seen_count][len] = '\0';
            seen_count++;
            filler(buf, seen[seen_count - 1], NULL, 0, 0);
        }
    }

    closedir(d);
    return 0;
}

static int baymax_open(const char *path, struct fuse_file_info *fi) {
    const char *filename = path + 1;
    char chunks[100][256];
    if (list_chunks(filename, chunks) == 0) return -ENOENT;

    log_activity("READ: %s", filename);
    return 0;
}

static int baymax_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    (void) fi;
    const char *filename = path + 1;
    char chunks[100][256];
    int count = list_chunks(filename, chunks);

    if (count == 0) return -ENOENT;

    size_t total = 0;
    for (int i = 0; i < count; ++i) {
        FILE *f = fopen(chunks[i], "rb");
        if (!f) continue;
        size_t len = fread(buf + total, 1, CHUNK_SIZE, f);
        fclose(f);
        total += len;
    }

    if ((size_t)offset >= total) return 0;
    if (offset + size > total) size = total - offset;
    memmove(buf, buf + offset, size);
    log_activity("READ: %s", filename);
    return size;
}

static int baymax_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) mode;
    const char *filename = path + 1;
    char temp[256];
    snprintf(temp, sizeof(temp), "/tmp/%s.tmp", filename);
    fi->fh = (uint64_t) fopen(temp, "wb+");
    if(fi->fh == 0) return -EIO;
    log_activity("CREATE: %s", filename);
    return 0;
}

static int baymax_write(const char *path, const char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi) {
    FILE *f = (FILE *) fi->fh;
    fseek(f, offset, SEEK_SET);
    return fwrite(buf, 1, size, f);
}

static int baymax_flush(const char *path, struct fuse_file_info *fi) {
    const char *filename = path + 1;
    FILE *f = (FILE *) fi->fh;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    int idx = 0;
    char chunk_path[256];
    char *buf = malloc(CHUNK_SIZE);
    size_t read_len;

    while ((read_len = fread(buf, 1, CHUNK_SIZE, f)) > 0) {
        snprintf(chunk_path, sizeof(chunk_path), RELICS_DIR "/%s.%03d", filename, idx++);
        FILE *out = fopen(chunk_path, "wb");
        fwrite(buf, 1, read_len, out);
        fclose(out);
    }

    free(buf);
    fclose(f);

    char logline[1024] = {0};
    strcat(logline, "WRITE: ");
    strcat(logline, filename);
    strcat(logline, " -> ");
    for (int i = 0; i < idx; ++i) {
        char part[64];
        snprintf(part, sizeof(part), "%s.%03d", filename, i);
        strcat(logline, part);
        if (i < idx - 1) strcat(logline, ", ");
    }
    log_activity("%s", logline);
    return 0;
}

static int baymax_unlink(const char *path) {
    const char *filename = path + 1;
    char chunks[100][256];
    int count = list_chunks(filename, chunks);
    if (count == 0) return -ENOENT;

    for (int i = 0; i < count; ++i)
        unlink(chunks[i]);

    log_activity("DELETE: %s.000 - %s.%03d", filename, filename, count - 1);
    return 0;
}

static struct fuse_operations baymax_oper = {
    .getattr = baymax_getattr,
    .readdir = baymax_readdir,
    .open = baymax_open,
    .read = baymax_read,
    .create = baymax_create,
    .write = baymax_write,
    .flush = baymax_flush,
    .unlink = baymax_unlink,
};

int main(int argc, char *argv[]) {
    printf("mount path: %s\n", argv[argc-1]);
    download_and_extract();
    realpath(argv[argc-1], mount_path);
    printf("Resolved mount_path: %s\n", mount_path);
    FILE *log_file = fopen(LOG_FILE, "a");
    if(!log_file){
        perror("Failed to create activity.log");
        exit(1);
    }
    fclose(log_file);

    return fuse_main(argc, argv, &baymax_oper, NULL);
}
