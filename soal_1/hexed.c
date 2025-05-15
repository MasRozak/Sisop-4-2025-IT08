#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define ZIP_URL "https://drive.usercontent.google.com/u/0/uc?id=1hi_GDdP51Kn2JJMw02WmCOxuc3qrXzh5&export=download"
#define ZIP_NAME "anomali.zip"
#define BASE_DIR "anomali"
#define IMAGE_DIR "anomali/image"
#define LOG_FILE "anomali/conversion.log"
#ifndef DT_REG
#define DT_REG 8
#endif


void download_and_unzip() {
    if (access(ZIP_NAME, F_OK) != 0) {
        printf("Downloading zip...\n");
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "wget -q --show-progress -O %s '%s'", ZIP_NAME, ZIP_URL);
        system(cmd);
    }

    if (access(BASE_DIR"/1.txt", F_OK) != 0) {
        printf("Unzipping...\n");
        mkdir(BASE_DIR, 0755);
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "unzip -o -q %s -d temp_unzip && "
            "mkdir -p %s && "
            "mv temp_unzip/anomali/* %s/ && "
            "rm -rf temp_unzip && "
            "rm -f %s",
            ZIP_NAME, BASE_DIR, BASE_DIR, ZIP_NAME);
        system(cmd);
    }
}

void convert_hex_to_image(const char* txt_path, const char* image_path) {
    FILE *in = fopen(txt_path, "r");
    FILE *out = fopen(image_path, "wb");
    if (!in || !out) {
        if (in) fclose(in);
        if (out) fclose(out);
        return;
    }

    char hex[3] = {0};
    int c1, c2;
    while ((c1 = fgetc(in)) != EOF && (c2 = fgetc(in)) != EOF) {
        hex[0] = c1;
        hex[1] = c2;
        unsigned int byte;
        sscanf(hex, "%02x", &byte);
        fputc(byte, out);
    }

    fclose(in);
    fclose(out);
}

void log_conversion(const char* txt_name, const char* image_name) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char date[20], time_str[20];
    strftime(date, sizeof(date), "%Y-%m-%d", t);
    strftime(time_str, sizeof(time_str), "%H:%M:%S", t);

    fprintf(log, "[%s][%s]: Successfully converted hexadecimal text %s to %s.\n",
            date, time_str, txt_name, image_name);
    fclose(log);
}

void process_txt_files() {
    mkdir(IMAGE_DIR, 0755);

    DIR *dir = opendir(BASE_DIR);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_REG) continue;
        if (!strstr(entry->d_name, ".txt")) continue;

        char txt_path[512], img_path[512];
        snprintf(txt_path, sizeof(txt_path), "%s/%s", BASE_DIR, entry->d_name);

        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char date[20], time_str[20];
        strftime(date, sizeof(date), "%Y-%m-%d", t);
        strftime(time_str, sizeof(time_str), "%H:%M:%S", t);

        char img_name[256];
        char base[64];
        strncpy(base, entry->d_name, sizeof(base));
        base[strlen(base) - 4] = '\0';

        snprintf(img_name, sizeof(img_name), "%s_image_%s_%s.png", base, date, time_str);
        snprintf(img_path, sizeof(img_path), "%s/%s", IMAGE_DIR, img_name);

        if (access(img_path, F_OK) == 0) continue;

        convert_hex_to_image(txt_path, img_path);
        log_conversion(entry->d_name, img_name);
    }

    closedir(dir);
}

static int fs_getattr(const char *path, struct stat *st, struct fuse_file_info *fi) {
    (void) fi;
    char real[1024];
    snprintf(real, sizeof(real), "%s%s", BASE_DIR, path);

    if (stat(real, st) == -1)
        return -ENOENT;
    return 0;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    char real[1024];
    snprintf(real, sizeof(real), "%s%s", BASE_DIR, path);
    DIR *dp = opendir(real);
    if (!dp) return -ENOENT;

    struct dirent *de;
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    while ((de = readdir(dp)) != NULL) {
        filler(buf, de->d_name, NULL, 0, 0);
    }
    closedir(dp);
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    char real[1024];
    snprintf(real, sizeof(real), "%s%s", BASE_DIR, path);

    int fd = open(real, fi->flags);
    if (fd == -1)
        return -errno;
    close(fd);
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    char real[1024];
    snprintf(real, sizeof(real), "%s%s", BASE_DIR, path);

    int fd = open(real, O_RDONLY);
    if (fd == -1)
        return -errno;

    int res = pread(fd, buf, size, offset);
    if (res == -1)
        res = -errno;

    close(fd);
    return res;
}

static struct fuse_operations operations = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .open = fs_open,
    .read = fs_read,
};

int main(int argc, char *argv[]) {
    download_and_unzip();
    process_txt_files();
    return fuse_main(argc, argv, &operations, NULL);
}
