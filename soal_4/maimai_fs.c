#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zlib.h>
#include <stdarg.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define MAX_PATH 1024
#define MAX_NAME 256
#define MAX_BUFFER 1048576 // 1MB buffer limit

const char *dirpath = "/home/aryarefman/chiho";
const char *aes_key = "12345678901234567890123456789012"; // 32 bytes for AES-256

void log_message(const char *color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s[maimai_fs] ", color);
    vfprintf(stderr, format, args);
    fprintf(stderr, "%s\n", ANSI_COLOR_RESET);
    va_end(args);
}

void print_welcome() {
    printf(ANSI_COLOR_GREEN);
    printf("=====================================\n");
    printf("    maimai File System v1.0\n");
    printf("    Powered by SEGA & xAI\n");
    printf("=====================================\n");
    printf(ANSI_COLOR_RESET);
    printf("Mounting at %s...\n", dirpath);
}

void shift_filename(char *name, size_t len, int reverse) {
    if (!name || len == 0) return;
    for (size_t i = 0; i < len; i++) {
        int shift = reverse ? -(i + 1) : (i + 1);
        if (name[i] >= 'a' && name[i] <= 'z') {
            name[i] = 'a' + (name[i] - 'a' + shift + 26) % 26;
        } else if (name[i] >= 'A' && name[i] <= 'Z') {
            name[i] = 'A' + (name[i] - 'A' + shift + 26) % 26;
        }
        if (name[i] == '/' || name[i] == '\0') name[i] = '_';
    }
}

void rot13(char *data, size_t len) {
    if (!data || len == 0) return;
    for (size_t i = 0; i < len; i++) {
        if (data[i] >= 'a' && data[i] <= 'z') {
            data[i] = 'a' + (data[i] - 'a' + 13) % 26;
        } else if (data[i] >= 'A' && data[i] <= 'Z') {
            data[i] = 'A' + (data[i] - 'A' + 13) % 26;
        }
    }
}

int get_actual_path(const char *path, char *full_path, size_t full_path_size) {
    if (!path || !full_path) return -EINVAL;

    char temp[MAX_PATH] = {0};
    if (strncmp(path, "/7sref/", 7) == 0) {
        const char *filename = path + 7;
        const char *underscore = strchr(filename, '_');
        if (!underscore) {
            log_message(ANSI_COLOR_RED, "Invalid 7sref path format: %s", path);
            return -EINVAL;
        }

        char area[MAX_NAME] = {0};
        size_t area_len = underscore - filename;
        if (area_len >= MAX_NAME) return -ENAMETOOLONG;
        strncpy(area, filename, area_len);
        area[area_len] = '\0';

        const char *actual_filename = underscore + 1;
        char actual_name[MAX_NAME] = {0};
        size_t name_len = strlen(actual_filename);
        if (name_len >= MAX_NAME) return -ENAMETOOLONG;
        strncpy(actual_name, actual_filename, name_len);
        char *dot = strrchr(actual_name, '.');
        if (dot) *dot = '\0'; // Remove .txt for 7sref mapping

        const char *exts[] = {".mai", ".ccc", ".rot", ".bin", ".enc", ".gz", NULL};
        if (strcmp(area, "starter") == 0) {
            snprintf(temp, sizeof(temp), "/starter/test.text%s", exts[0]);
        } else if (strcmp(area, "metro") == 0) {
            snprintf(temp, sizeof(temp), "/metro/test.txt%s", exts[1]);
        } else if (strcmp(area, "dragon") == 0) {
            snprintf(temp, sizeof(temp), "/dragon/test.txt%s", exts[2]);
        } else if (strcmp(area, "blackrose") == 0) {
            snprintf(temp, sizeof(temp), "/blackrose/%s%s", actual_name, exts[3]); // Keep original for binary
        } else if (strcmp(area, "heaven") == 0) {
            snprintf(temp, sizeof(temp), "/heaven/test.txt%s", exts[4]);
        } else if (strcmp(area, "skystreet") == 0 || strcmp(area, "youth") == 0) {
            snprintf(temp, sizeof(temp), "/skystreet/test.text%s", exts[5]);
        } else {
            log_message(ANSI_COLOR_RED, "Unknown area: %s", area);
            return -EINVAL;
        }
    } else {
        size_t path_len = strlen(path);
        if (path_len >= sizeof(temp)) return -ENAMETOOLONG;
        strncpy(temp, path, path_len);
        temp[path_len] = '\0';

        char *area = strtok(strdup(temp), "/");
        char *file_name = strtok(NULL, "/");
        if (area && file_name) {
            char *dot = strrchr(file_name, '.');
            if (dot && strcmp(dot, ".txt") == 0) *dot = '\0';

            if (strcmp(area, "skystreet") == 0 || strcmp(area, "youth") == 0) {
                snprintf(temp, sizeof(temp), "/skystreet/test.text.gz");
            } else if (strcmp(area, "heaven") == 0) {
                snprintf(temp, sizeof(temp), "/heaven/test.txt.enc");
            } else if (strcmp(area, "blackrose") == 0) {
                snprintf(temp, sizeof(temp), "/blackrose/%s.bin", file_name);
            } else if (strcmp(area, "dragon") == 0) {
                snprintf(temp, sizeof(temp), "/dragon/test.txt.rot");
            } else if (strcmp(area, "metro") == 0) {
                snprintf(temp, sizeof(temp), "/metro/test.txt.ccc");
            } else if (strcmp(area, "starter") == 0) {
                snprintf(temp, sizeof(temp), "/starter/test.text.mai");
            }
        }
    }

    size_t dirpath_len = strlen(dirpath);
    size_t temp_len = strlen(temp);
    if (dirpath_len + temp_len + 1 > full_path_size) return -ENAMETOOLONG;
    snprintf(full_path, full_path_size, "%s%s", dirpath, temp);
    return 0;
}

int aes_encrypt(const char *path, const char *data, size_t size, off_t offset) {
    if (!path || !data) return -EINVAL;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -ENOMEM;

    unsigned char iv[16];
    if (!RAND_bytes(iv, 16)) {
        EVP_CIPHER_CTX_free(ctx);
        return -EIO;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (unsigned char *)aes_key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -EIO;
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        EVP_CIPHER_CTX_free(ctx);
        return -errno;
    }

    if (pwrite(fd, iv, 16, 0) != 16) {
        close(fd);
        EVP_CIPHER_CTX_free(ctx);
        return -EIO;
    }

    int out_len, final_len;
    unsigned char outbuf[MAX_BUFFER];
    if (EVP_EncryptUpdate(ctx, outbuf, &out_len, (unsigned char *)data, size) != 1 ||
        EVP_EncryptFinal_ex(ctx, outbuf + out_len, &final_len) != 1) {
        close(fd);
        EVP_CIPHER_CTX_free(ctx);
        return -EIO;
    }

    if (pwrite(fd, outbuf, out_len + final_len, 16) != out_len + final_len) {
        close(fd);
        EVP_CIPHER_CTX_free(ctx);
        return -EIO;
    }

    close(fd);
    EVP_CIPHER_CTX_free(ctx);
    return size;
}

int aes_decrypt(const char *path, char *buf, size_t size, off_t offset) {
    if (!path || !buf) return -EINVAL;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -ENOMEM;

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        EVP_CIPHER_CTX_free(ctx);
        return -errno;
    }

    unsigned char iv[16];
    if (pread(fd, iv, 16, 0) != 16) {
        close(fd);
        EVP_CIPHER_CTX_free(ctx);
        return -EIO;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        EVP_CIPHER_CTX_free(ctx);
        return -errno;
    }

    size_t ciphertext_len = st.st_size - 16;
    unsigned char ciphertext[ciphertext_len];
    if (pread(fd, ciphertext, ciphertext_len, 16) != ciphertext_len) {
        close(fd);
        EVP_CIPHER_CTX_free(ctx);
        return -EIO;
    }
    close(fd);

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (unsigned char *)aes_key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -EIO;
    }

    int out_len, final_len;
    unsigned char outbuf[MAX_BUFFER];
    if (EVP_DecryptUpdate(ctx, outbuf, &out_len, ciphertext, ciphertext_len) != 1 ||
        EVP_DecryptFinal_ex(ctx, outbuf + out_len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -EIO;
    }

    size_t plaintext_len = out_len + final_len;
    if (offset >= plaintext_len) {
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }

    size_t copy_len = (plaintext_len - offset) > size ? size : (plaintext_len - offset);
    memcpy(buf, outbuf + offset, copy_len);
    EVP_CIPHER_CTX_free(ctx);
    return copy_len;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char full_path[MAX_PATH];
    int res = get_actual_path(path, full_path, sizeof(full_path));
    if (res < 0) return res;

    int fd = open(full_path, fi->flags);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}

static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    char full_path[MAX_PATH];
    int res = get_actual_path(path, full_path, sizeof(full_path));
    if (res < 0) {
        if (strcmp(path, "/") == 0 || strcmp(path, "/7sref") == 0 ||
            strcmp(path, "/starter") == 0 || strcmp(path, "/metro") == 0 ||
            strcmp(path, "/dragon") == 0 || strcmp(path, "/blackrose") == 0 ||
            strcmp(path, "/heaven") == 0 || strcmp(path, "/skystreet") == 0) {
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
            return 0;
        }
        return res;
    }

    res = lstat(full_path, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    log_message(ANSI_COLOR_YELLOW, "xmp_readdir called for path: %s", path);

    if (strcmp(path, "/") == 0) {
        struct stat st = {0};
        st.st_mode = S_IFDIR | 0755;
        st.st_nlink = 2;
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        const char *dirs[] = {"starter", "metro", "dragon", "blackrose", "heaven", "skystreet", "7sref", NULL};
        for (int i = 0; dirs[i]; i++) {
            log_message(ANSI_COLOR_GREEN, "Listing directory in root: %s", dirs[i]);
            if (filler(buf, dirs[i], &st, 0, 0)) break;
        }
        return 0;
    }

    if (strcmp(path, "/7sref") == 0) {
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        
        // Set up stat for virtual files
        struct stat st = {0};
        st.st_mode = S_IFREG | 0644;
        st.st_nlink = 1;
        st.st_size = 1024;
        
        // Create area-specific files
        char display_name[MAX_NAME];
        
        // For each standard area, create the corresponding display file
        snprintf(display_name, MAX_NAME, "starter_test.txt");
        if (filler(buf, display_name, &st, 0, 0)) return 0;
        
        snprintf(display_name, MAX_NAME, "metro_test.txt");
        if (filler(buf, display_name, &st, 0, 0)) return 0;
        
        snprintf(display_name, MAX_NAME, "dragon_test.txt");
        if (filler(buf, display_name, &st, 0, 0)) return 0;
        
        // Special case for blackrose using actual file name
        snprintf(display_name, MAX_NAME, "blackrose_firefox.png.txt");
        if (filler(buf, display_name, &st, 0, 0)) return 0;
        
        snprintf(display_name, MAX_NAME, "heaven_test.txt");
        if (filler(buf, display_name, &st, 0, 0)) return 0;
        
        snprintf(display_name, MAX_NAME, "skystreet_test.txt");
        if (filler(buf, display_name, &st, 0, 0)) return 0;
        
        return 0;
    }
    
    // Standard area directories
    if (strcmp(path, "/starter") == 0 || 
        strcmp(path, "/metro") == 0 || 
        strcmp(path, "/dragon") == 0 || 
        strcmp(path, "/heaven") == 0 || 
        strcmp(path, "/skystreet") == 0) {
        
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        
        // Create a default test.txt file for this area
        struct stat st = {0};
        st.st_mode = S_IFREG | 0644;
        st.st_nlink = 1;
        st.st_size = 1024;
        
        if (filler(buf, "test.txt", &st, 0, 0)) return 0;
        return 0;
    }
    
    // Special case for blackrose
    if (strcmp(path, "/blackrose") == 0) {
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        
        struct stat st = {0};
        st.st_mode = S_IFREG | 0644;
        st.st_nlink = 1;
        st.st_size = 1024;
        
        if (filler(buf, "firefox.png", &st, 0, 0)) return 0;
        return 0;
    }

    // If we reach here, the path wasn't handled above, try to look it up in the real filesystem
    char full_path[MAX_PATH];
    int res = get_actual_path(path, full_path, sizeof(full_path));
    if (res == 0) {
        DIR *dp = opendir(full_path);
        if (!dp) {
            log_message(ANSI_COLOR_RED, "Failed to open directory: %s, error: %s", full_path, strerror(errno));
            return -errno;
        }

        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);

        struct dirent *de;
        while ((de = readdir(dp))) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;

            char display_name[MAX_NAME] = {0};
            size_t name_len = strlen(de->d_name);
            if (name_len >= MAX_NAME) {
                log_message(ANSI_COLOR_RED, "File name too long: %s", de->d_name);
                continue;
            }
            strncpy(display_name, de->d_name, name_len);

            char *ext = strrchr(display_name, '.');
            if (ext) {
                *ext = '\0'; // Remove chiho-specific extension
                
                // Handle blackrose case specially - keep the original filename
                char *parent_path = strrchr(path, '/');
                if (parent_path && strcmp(parent_path + 1, "blackrose") == 0) {
                    // For blackrose, just keep the base name without .bin
                    // Do nothing special
                } else {
                    // For other areas, standardize to test.txt
                    strcpy(display_name, "test.txt");
                }
            } else {
                strcpy(display_name, "test.txt"); // Default to test.txt
            }

            struct stat st;
            char file_path[MAX_PATH];
            snprintf(file_path, sizeof(file_path), "%s/%s", full_path, de->d_name);
            if (lstat(file_path, &st) == -1) {
                log_message(ANSI_COLOR_RED, "Failed to stat file: %s, error: %s", file_path, strerror(errno));
                continue;
            }
            log_message(ANSI_COLOR_YELLOW, "Listing in %s: %s", path, display_name);
            if (filler(buf, display_name, &st, 0, 0)) break;
        }
        closedir(dp);
        return 0;
    }

    return -ENOENT;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
    char full_path[MAX_PATH];
    int res = get_actual_path(path, full_path, sizeof(full_path));
    if (res < 0) return res;

    char *area = strtok(strdup(path), "/");
    char *file_name = strtok(NULL, "/");
    if (!area || !file_name) return -ENOENT;

    if (strcmp(area, "7sref") == 0) {
        char combined[MAX_BUFFER] = {0};
        size_t combined_len = 0;
        const char *areas[] = {"starter", "metro", "dragon", "blackrose", "heaven", "skystreet", NULL};
        const char *exts[] = {".mai", ".ccc", ".rot", ".bin", ".enc", ".gz", NULL};
        char base_name[MAX_NAME] = {0};
        strncpy(base_name, file_name, MAX_NAME - 1);
        char *underscore = strchr(base_name, '_');
        if (underscore) {
            *underscore = '\0';
            char *actual_filename = underscore + 1;
            char *dot = strrchr(actual_filename, '.');
            if (dot) *dot = '\0';
        }

        for (int i = 0; areas[i]; i++) {
            char src_path[MAX_PATH];
            snprintf(src_path, MAX_PATH, "%s/%s/test%s%s", dirpath, areas[i], (strcmp(areas[i], "skystreet") == 0 || strcmp(areas[i], "starter") == 0) ? ".text" : ".txt", exts[i]);
            log_message(ANSI_COLOR_YELLOW, "Reading from %s for 7sref", src_path);
            int fd = open(src_path, O_RDONLY);
            if (fd == -1) continue;

            char temp_buf[MAX_BUFFER];
            ssize_t len = pread(fd, temp_buf, MAX_BUFFER - 1, 0);
            close(fd);
            if (len <= 0) continue;

            if (strcmp(areas[i], "metro") == 0) shift_filename(temp_buf, len, 1);
            else if (strcmp(areas[i], "dragon") == 0) rot13(temp_buf, len);
            else if (strcmp(areas[i], "heaven") == 0) {
                res = aes_decrypt(src_path, temp_buf, len, 0);
                if (res < 0) continue;
                len = res;
            } else if (strcmp(areas[i], "skystreet") == 0) {
                gzFile gzf = gzopen(src_path, "rb");
                if (!gzf) continue;
                len = gzread(gzf, temp_buf, MAX_BUFFER - 1);
                gzclose(gzf);
            }

            if (combined_len + len < MAX_BUFFER) {
                memcpy(combined + combined_len, temp_buf, len);
                combined_len += len;
            }
        }

        if (offset >= combined_len) return 0;
        size = (combined_len - offset) > size ? size : (combined_len - offset);
        memcpy(buf, combined + offset, size);
        return size;
    }

    if (strcmp(area, "starter") == 0) {
        int fd = open(full_path, O_RDONLY);
        if (fd == -1) return -errno;
        res = pread(fd, buf, size, offset);
        close(fd);
        return res == -1 ? -errno : res;
    }

    if (strcmp(area, "metro") == 0) {
        int fd = open(full_path, O_RDONLY);
        if (fd == -1) return -errno;
        char *temp_buf = malloc(size);
        if (!temp_buf) {
            close(fd);
            return -ENOMEM;
        }
        res = pread(fd, temp_buf, size, offset);
        if (res > 0) shift_filename(temp_buf, res, 1);
        memcpy(buf, temp_buf, res);
        free(temp_buf);
        close(fd);
        return res == -1 ? -errno : res;
    }

    if (strcmp(area, "dragon") == 0) {
        int fd = open(full_path, O_RDONLY);
        if (fd == -1) return -errno;
        char *temp_buf = malloc(size);
        if (!temp_buf) {
            close(fd);
            return -ENOMEM;
        }
        res = pread(fd, temp_buf, size, offset);
        if (res > 0) rot13(temp_buf, res);
        memcpy(buf, temp_buf, res);
        free(temp_buf);
        close(fd);
        return res == -1 ? -errno : res;
    }

    if (strcmp(area, "heaven") == 0) {
        res = aes_decrypt(full_path, buf, size, offset);
        return res;
    }

    if (strcmp(area, "skystreet") == 0 || strcmp(area, "youth") == 0) {
        gzFile gzf = gzopen(full_path, "rb");
        if (!gzf) return -errno;
        if (gzseek(gzf, offset, SEEK_SET) == -1) {
            gzclose(gzf);
            return -errno;
        }
        res = gzread(gzf, buf, size);
        gzclose(gzf);
        return res == -1 ? -1 : res;
    }

    if (strcmp(area, "blackrose") == 0) {
        int fd = open(full_path, O_RDONLY);
        if (fd == -1) return -errno;
        res = pread(fd, buf, size, offset);
        close(fd);
        return res == -1 ? -errno : res;
    }

    return -ENOENT;
}

static int xmp_write(const char *path, const char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi) {
    char full_path[MAX_PATH];
    int res = get_actual_path(path, full_path, sizeof(full_path));
    if (res < 0) return res;

    char *area = strtok(strdup(path), "/");
    if (!area) return -ENOENT;

    if (strcmp(area, "starter") == 0) {
        int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) return -errno;
        res = pwrite(fd, buf, size, offset);
        close(fd);
        return res == -1 ? -errno : res;
    }

    if (strcmp(area, "metro") == 0) {
        char *temp_buf = malloc(size);
        if (!temp_buf) return -ENOMEM;
        memcpy(temp_buf, buf, size);
        shift_filename(temp_buf, size, 0);
        int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            free(temp_buf);
            return -errno;
        }
        res = pwrite(fd, temp_buf, size, offset);
        free(temp_buf);
        close(fd);
        return res == -1 ? -errno : res;
    }

    if (strcmp(area, "dragon") == 0) {
        char *temp_buf = malloc(size);
        if (!temp_buf) return -ENOMEM;
        memcpy(temp_buf, buf, size);
        rot13(temp_buf, size);
        int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            free(temp_buf);
            return -errno;
        }
        res = pwrite(fd, temp_buf, size, offset);
        free(temp_buf);
        close(fd);
        return res == -1 ? -errno : res;
    }

    if (strcmp(area, "heaven") == 0) {
        res = aes_encrypt(full_path, buf, size, offset);
        return res;
    }

    if (strcmp(area, "skystreet") == 0 || strcmp(area, "youth") == 0) {
        gzFile gzf = gzopen(full_path, "wb");
        if (!gzf) return -errno;
        res = gzwrite(gzf, buf, size);
        gzclose(gzf);
        return res == -1 ? -1 : size;
    }

    if (strcmp(area, "blackrose") == 0) {
        int fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) return -errno;
        res = pwrite(fd, buf, size, offset);
        close(fd);
        return res == -1 ? -errno : res;
    }

    return -ENOENT;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char full_path[MAX_PATH];
    int res = get_actual_path(path, full_path, sizeof(full_path));
    if (res < 0) return res;

    int fd = creat(full_path, mode);
    if (fd == -1) return -errno;
    fi->fh = fd;
    log_message(ANSI_COLOR_GREEN, "Created file: %s", full_path);
    return 0;
}

static int xmp_unlink(const char *path) {
    char full_path[MAX_PATH];
    int res = get_actual_path(path, full_path, sizeof(full_path));
    if (res < 0) return res;

    res = unlink(full_path);
    return res == -1 ? -errno : 0;
}

static const struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .open    = xmp_open,
    .read    = xmp_read,
    .write   = xmp_write,
    .create  = xmp_create,
    .unlink  = xmp_unlink,
};

int main(int argc, char *argv[]) {
    print_welcome();
    OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, NULL);
    umask(0);
    return fuse_main(argc, argv, &xmp_oper, NULL);
}
