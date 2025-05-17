#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>

#define ORIGINAL_DIR "/mnt/original_files"
#define LOG_FILE "/var/log/it24.log"

// ROT13 untuk string
void rot13(char *str) {
    char *p = str;
    while (*p) {
        if ((*p >= 'a' && *p <= 'z')) {
            *p = (*p - 'a' + 13) % 26 + 'a';
        } else if (*p >= 'A' && *p <= 'Z') {
            *p = (*p - 'A' + 13) % 26 + 'A';
        }
        p++;
    }
}

// Balik string (untuk nama file berbahaya)
void reverse_str(char *str) {
    int n = strlen(str);
    for (int i = 0; i < n / 2; i++) {
        char tmp = str[i];
        str[i] = str[n - i - 1];
        str[n - i - 1] = tmp;
    }
}

// Cek apakah nama file berbahaya
int is_dangerous(const char *name) {
    const char *basename = name;
    // Jika ada path, ambil hanya basename-nya
    const char *last_slash = strrchr(name, '/');
    if (last_slash) basename = last_slash + 1;
    
    int result = strstr(basename, "nafis") || strstr(basename, "kimcun");
    if (result) {
        log_activity("ALERT: File berbahaya terdeteksi: %s", basename);
    }
    return result;
}

// Logging aktivitas
void log_activity(const char *fmt, ...) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;
    va_list args;
    va_start(args, fmt);
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log, "[%04d-%02d-%02d %02d:%02d:%02d] ",
        t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
    vfprintf(log, fmt, args);
    fprintf(log, "\n");
    va_end(args);
    fclose(log);
}

// Perbaikan untuk FUSE3
static int antink_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    char real_path[4096];
    snprintf(real_path, sizeof(real_path), "%s%s", ORIGINAL_DIR, path);
    DIR *dp = opendir(real_path);
    if (!dp) return -errno;
    struct dirent *de;
    while ((de = readdir(dp)) != NULL) {
        char name[256];
        strncpy(name, de->d_name, sizeof(name));
        name[sizeof(name)-1] = '\0';
        if (is_dangerous(name)) {
            char original_name[256];
            strncpy(original_name, name, sizeof(original_name));
            reverse_str(name);
            log_activity("REVERSE: Nama file '%s' dibalik menjadi '%s'", original_name, name);
        }
        filler(buf, name, NULL, 0, 0); // FUSE3 butuh 5 argumen
    }
    closedir(dp);
    log_activity("READDIR: %s", path);
    return 0;
}

static int antink_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    // Membuat path asli dari path FUSE
    char real_path[4096];
    char fixed_path[4096];
    
    // Jika path mengandung nama file berbahaya yang telah di-reverse, kita perlu mengembalikannya
    if (path[0] == '/') {
        strcpy(fixed_path, path);
        char *last_slash = strrchr(fixed_path, '/');
        if (last_slash) {
            char *filename = last_slash + 1;
            // Jika nama file sudah di-reverse (berakhiran nafis atau kimcun terbalik)
            if (strstr(filename, "sifan") || strstr(filename, "nucmik")) {
                // Di-reverse lagi (kembali ke nama asli)
                reverse_str(filename);
            }
        }
    } else {
        strcpy(fixed_path, path);
    }
    
    snprintf(real_path, sizeof(real_path), "%s%s", ORIGINAL_DIR, fixed_path);
    int res = lstat(real_path, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int antink_open(const char *path, struct fuse_file_info *fi) {
    char real_path[4096];
    char fixed_path[4096];
    
    // Perbaiki path untuk nama file berbahaya
    if (path[0] == '/') {
        strcpy(fixed_path, path);
        char *last_slash = strrchr(fixed_path, '/');
        if (last_slash) {
            char *filename = last_slash + 1;
            // Jika nama file sudah di-reverse
            if (strstr(filename, "sifan") || strstr(filename, "nucmik")) {
                // Di-reverse lagi (kembali ke nama asli)
                reverse_str(filename);
            }
        }
    } else {
        strcpy(fixed_path, path);
    }
    
    snprintf(real_path, sizeof(real_path), "%s%s", ORIGINAL_DIR, fixed_path);
    int fd = open(real_path, O_RDONLY);
    if (fd == -1) return -errno;
    close(fd);
    log_activity("OPEN: %s (fixed: %s)", path, fixed_path);
    return 0;
}

static int antink_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char real_path[4096];
    char fixed_path[4096];
    
    // Perbaiki path untuk nama file berbahaya
    if (path[0] == '/') {
        strcpy(fixed_path, path);
        char *last_slash = strrchr(fixed_path, '/');
        if (last_slash) {
            char *filename = last_slash + 1;
            // Jika nama file sudah di-reverse
            if (strstr(filename, "sifan") || strstr(filename, "nucmik")) {
                // Di-reverse lagi (kembali ke nama asli)
                reverse_str(filename);
            }
        }
    } else {
        strcpy(fixed_path, path);
    }
    
    snprintf(real_path, sizeof(real_path), "%s%s", ORIGINAL_DIR, fixed_path);
    int fd = open(real_path, O_RDONLY);
    if (fd == -1) return -errno;
    
    // Periksa apakah file berbahaya berdasarkan nama asli (fixed_path)
    int dangerous = is_dangerous(fixed_path);
    
    // Baca seluruh isi file ke buffer
    char *file_content = NULL;
    ssize_t total_size = 0;
    struct stat st;
    
    if (fstat(fd, &st) == 0) {
        // Alokasi buffer untuk seluruh file
        file_content = malloc(st.st_size + 1);
        if (file_content) {
            // Baca seluruh file
            total_size = pread(fd, file_content, st.st_size, 0);
            if (total_size > 0) {
                file_content[total_size] = '\0';
                
                // Jika file normal, enkripsi seluruhnya
                if (!dangerous) {
                    // Log konten asli (hanya sebagian) sebelum enkripsi
                    char original_preview[101] = {0};
                    size_t preview_size = total_size < 100 ? total_size : 100;
                    strncpy(original_preview, file_content, preview_size);
                    original_preview[preview_size] = '\0';
                    
                    // Enkripsi seluruh file
                    rot13(file_content);
                    
                    // Log aktivitas enkripsi
                    log_activity("ENCRYPT: File normal '%s' dienkripsi dengan ROT13. Sample: '%s' -> '...'", path, original_preview);
                }
            }
        }
    }
    
    ssize_t res;
    if (file_content && total_size > 0) {
        // Salin bagian yang diminta ke buffer hasil
        if (offset < total_size) {
            size_t copy_size = (offset + size > total_size) ? (total_size - offset) : size;
            memcpy(buf, file_content + offset, copy_size);
            res = copy_size;
        } else {
            res = 0;
        }
        
        free(file_content);
    } else {
        // Fallback jika alokasi gagal: gunakan cara lama
        res = pread(fd, buf, size, offset);
        if (res > 0 && !dangerous) {
            rot13(buf);
        }
    }
    
    close(fd);
    if (res == -1) res = -errno;
    
    log_activity("READ: %s (fixed: %s) %s", path, fixed_path, dangerous ? "[DANGEROUS]" : "[NORMAL]");
    return res;
}

static struct fuse_operations antink_oper = {
    .getattr = antink_getattr,
    .readdir = antink_readdir,
    .open = antink_open,
    .read = antink_read,
};

int main(int argc, char *argv[]) {
    char *fuse_argv[] = { argv[0], "/mnt/antink_mount", "-d", "-f", "-s" };
    return fuse_main(5, fuse_argv, &antink_oper, NULL);
}

