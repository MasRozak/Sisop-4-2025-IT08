# Sisop-4-2025-IT08

## Soal 3
[Author: Rafa / kookoon]
Nafis dan Kimcun merupakan dua mahasiswa anomali😱 yang paling tidak tahu sopan santun dan sangat berbahaya di antara angkatan 24. Maka dari itu, Pujo sebagai komting yang baik hati dan penyayang😍, memutuskan untuk membuat sebuah sistem pendeteksi kenakalan bernama Anti Napis Kimcun `(AntiNK)` untuk melindungi file-file penting milik angkatan 24. Pujo pun kemudian bertanya kepada Asisten bagaimana cara membuat sistem yang benar, para asisten pun merespon 

### A. Create Container Antink-Server and Antink-Logger
Pujo harus membuat sistem AntiNK menggunakan Docker yang menjalankan FUSE dalam container terisolasi. Sistem ini menggunakan docker-compose untuk mengelola container antink-server (FUSE Func.) dan antink-logger (Monitoring Real-Time Log). Asisten juga memberitahu bahwa docker-compose juga memiliki beberapa komponen lain yaitu

`1. it24_host (Bind Mount -> Store Original File)`

`2. antink_mount (Mount Point)`

`3. antink-logs (Bind Mount -> Store Log)`

```yaml
services:
  antink-server:
    build: .
    container_name: antink-server
    privileged: true
    volumes:
      - ./it24_host:/mnt/original_files   
      - ./antink-logs:/var/log            
    networks:
      - antink-network
    cap_add:
      - SYS_ADMIN
    devices:
      - /dev/fuse
    security_opt:
      - apparmor:unconfined
```

### B. FUSE Implementation (antink.c)
Sistem FUSE (Filesystem in Userspace) akan diimplementasikan dalam file `antink.c`. FUSE ini akan memodifikasi bagaimana file dan direktori ditampilkan dan diakses tanpa mengubah file aslinya.

**Fitur Utama:**
1.  **Deteksi File Berbahaya**: File dianggap "berbahaya" jika namanya mengandung substring "nafis" atau "kimcun" (case-insensitive).
2.  **Pengubahan Nama File Berbahaya**: Saat listing direktori (`readdir`), nama file berbahaya akan dibalik. Contoh: `nafis_document.txt` menjadi `txt.tnemucod_sifan`.
3.  **Enkripsi Konten File Normal**: Saat membaca file (`read`), konten file normal (tidak berbahaya) akan dienkripsi menggunakan ROT13. Enkripsi ini dilakukan pada seluruh konten file saat file diakses, bukan hanya pada bagian yang dibaca.
4.  **Konten File Berbahaya**: Konten file berbahaya akan tetap ditampilkan apa adanya (tidak dienkripsi).
5.  **Logging**: Semua aktivitas penting akan dicatat.

**Struktur Direktori dalam Container `antink-server`:**
*   `/mnt/original_files`: Titik mount untuk `it24_host` (direktori di host yang berisi file asli).
*   `/mnt/antink_mount`: Titik mount FUSE. Ini adalah tampilan virtual dari file-file di `/mnt/original_files` dengan modifikasi yang diterapkan oleh AntiNK.
*   `/var/log/it24.log`: File log utama tempat semua aktivitas dicatat.

### C. Logging
Semua aktivitas FUSE dan deteksi akan dicatat ke dalam file `/var/log/it24.log` di dalam container `antink-server`. Format log akan mencakup:
*   `INFO`: Untuk operasi standar seperti `OPEN`, `READ`, `READDIR`.
*   `ALERT`: Ketika file berbahaya terdeteksi.
    *   Contoh: `ALERT: File berbahaya terdeteksi: nafis_secret.docx`
*   `REVERSE`: Ketika nama file berbahaya dibalik.
    *   Contoh: `REVERSE: Nama file 'nafis_secret.docx' dibalik menjadi 'xcod.terces_sifan'`
*   `ENCRYPT`: Ketika konten file normal dienkripsi. Akan disertakan preview singkat dari konten sebelum dan sesudah enkripsi.
    *   Contoh: `ENCRYPT: File normal '/mnt/original_files/safe_document.txt' dienkripsi dengan ROT13. Sample: 'This is a test.' -> 'Guvf vf n grfg.'`

### D. Docker Setup

**1. Dockerfile (`soal_3/Dockerfile`)**
Dockerfile akan digunakan untuk membangun image `antink-server`.
*   Base image: `ubuntu:20.04`
*   Instalasi dependensi: `fuse3`, `libfuse3-dev`, `build-essential`, `gcc`, `make`, `git`.
*   Membuat direktori yang diperlukan di dalam container: `/mnt/antink_mount`, `/mnt/original_files`, `/var/log`.
*   Menyalin `antink.c` ke dalam image.
*   Mengkompilasi `antink.c` menggunakan `gcc` dengan library FUSE3.
*   Menetapkan `CMD` untuk menjalankan FUSE daemon (`./antink-server -f /mnt/antink_mount /mnt/original_files`).

**2. Docker Compose (`soal_3/docker-compose.yml`)**
`docker-compose.yml` akan mendefinisikan dan mengelola dua service: `antink-server` dan `antink-logger`.

```yaml
version: '3.8'

services:
  antink-server:
    build:
      context: .
      dockerfile: Dockerfile
    container_name: antink-server
    privileged: true
    cap_add:
      - SYS_ADMIN
    devices:
      - /dev/fuse
    security_opt:
      - apparmor:unconfined
    volumes:
      - ./it24_host:/mnt/original_files  # File asli dari host
      - ./antink-logs:/var/log          # Log FUSE ditulis di sini
    # antink_mount adalah FUSE mount point internal, bukan bind mount dari host

  antink-logger:
    image: alpine
    container_name: antink-logger
    volumes:
      - ./antink-logs:/var/log  # Membaca log dari volume yang sama
    command: sh -c "tail -f /var/log/it24.log" # Memantau log secara real-time
    depends_on:
      - antink-server

# Direktori di host yang perlu dibuat sebelum menjalankan docker-compose up:
# ./it24_host
# ./antink-logs
```

**Penjelasan Service:**
*   **`antink-server`**:
    *   Membangun image dari `Dockerfile` di direktori saat ini.
    *   Membutuhkan `privileged: true`, `cap_add: SYS_ADMIN`, `devices: /dev/fuse`, dan `security_opt: apparmor:unconfined` untuk menjalankan FUSE.
    *   **Volume Mounts**:
        *   `./it24_host:/mnt/original_files`: Memetakan direktori `it24_host` di host ke `/mnt/original_files` di container. Di sinilah file-file asli disimpan.
        *   `./antink-logs:/var/log`: Memetakan direktori `antink-logs` di host ke `/var/log` di container. File `it24.log` akan ditulis di sini.
    *   `/mnt/antink_mount` adalah titik mount FUSE yang dibuat dan dikelola oleh proses FUSE di dalam container. File asli tidak akan termodifikasi.

*   **`antink-logger`**:
    *   Menggunakan image `alpine` yang ringan.
    *   **Volume Mounts**:
        *   `./antink-logs:/var/log`: Memetakan direktori `antink-logs` yang sama dari host ke `/var/log` di container ini. Ini memungkinkan `antink-logger` untuk membaca file log yang ditulis oleh `antink-server`.
    *   **Command**: `tail -f /var/log/it24.log` untuk memantau dan menampilkan isi `it24.log` secara real-time.
    *   `depends_on: - antink-server`: Memastikan `antink-server` dimulai sebelum `antink-logger`.

### E. Cara Menjalankan
1.  **Pastikan Docker dan Docker Compose terinstal.**
2.  **Buat direktori yang diperlukan di host** (jika belum ada) di dalam direktori `soal_3`:
    ```bash
    mkdir -p soal_3/it24_host
    mkdir -p soal_3/antink-logs
    ```
3.  **Tempatkan file `antink.c`, `Dockerfile`, dan `docker-compose.yml`** di dalam direktori `soal_3`.
4.  **Isi direktori `soal_3/it24_host`** dengan beberapa file untuk diuji, termasuk file dengan nama yang mengandung "nafis" atau "kimcun".
5.  **Jalankan dari direktori `soal_3`**:
    ```bash
    sudo docker-compose up --build
    ```
6.  **Untuk melihat output FUSE (AntiNK filesystem)**, buka terminal baru dan masuk ke dalam container `antink-server`:
    ```bash
    sudo docker exec -it antink-server bash
    ```
    Kemudian, di dalam container, navigasi ke `/mnt/antink_mount`:
    ```bash
    ls /mnt/antink_mount
    cat /mnt/antink_mount/nama_file_normal.txt
    cat /mnt/antink_mount/nama_file_berbahaya_terbalik.txt
    ```
7.  **Untuk melihat log secara real-time**, output dari `antink-logger` akan ditampilkan di terminal tempat `docker-compose up` dijalankan. Anda juga bisa melihat file `soal_3/antink-logs/it24.log` di host.
8.  **Untuk menghentikan**: Tekan `Ctrl+C` di terminal `docker-compose`, lalu jalankan:
    ```bash
    sudo docker-compose down
    ```

## Soal 4
[Author: Nathan / etern1ty]

Seperti yang kamu tahu, kamu telah mendapatkan pekerjaan di SEGA sebagai chart designer untuk game maimai. Karena kamu berhasil (iya kan) menyelesaikan challenge manipulasi majdata tersebut, SEGA mengirimkan email baru kepadamu. Mereka sangat terkesan karena kamu menyelesaikan tantangan mereka, dan kamu pun dipromosikan menjadi administrator untuk sistem in-game mereka. Di universe maimai, terdapat suatu mekanisme progres yang bernama chiho, yang merupakan bagian dari suatu area. `https://maimai.sega.jp/area/` Terdapat 7 area di maimai saat ini, dan tugasmu sekarang yaitu memastikan 7 area ini berfungsi sebagaimana mestinya. 

a. Starter/Beginning Area - Starter Chiho: 
Sesuai namanya, ini adalah area pertama yang ada di universe maimai. Di sini, file yang masuk akan disimpan secara normal, tetapi ditambahkan file extension .mai di direktori asli. Di folder FUSE sendiri, ekstension .mai tidak ada.
![image](https://github.com/user-attachments/assets/a96d13c5-fbd7-498e-b4c7-afe5c1a28ffe)

b. World’s End Area - Metropolis Chiho: 
Area kedua, World’s End Area (yang sebenarnya terdiri dari 2 chiho) utamanya berisi Metropolis Chiho. Sama seperti starter, file yang masuk disimpan dengan normal, tetapi dilakukan perubahan di direktori asli. File di directory asli di shift sesuai lokasinya, misal:
E -> E,
n -> o (+(1 % 256)), 
e -> g (+(2 % 256)), 
r -> u (+(3 % 256))
![image](https://github.com/user-attachments/assets/432036b1-9d86-4013-9168-665e6600f48d)

c. World Tree Area - Dragon Chiho: 
Area ketiga adalah World Tree Area yang berisi Dragon Chiho. File yang disimpan di area ini dienkripsi menggunakan ROT13 di direktori asli.
![image](https://github.com/user-attachments/assets/9c822dea-40ec-4138-a18c-4474089f3bf6)

d. Black Rose Area - Black Rose Chiho: 
Area keempat adalah Black Rose Area - Black Rose Chiho. Area ini menyimpan data dalam format biner murni, tanpa enkripsi atau encoding tambahan.
![image](https://github.com/user-attachments/assets/66996a74-a7ef-4550-aad9-029eb71ff495)

e. Tenkai Area - Heaven Chiho: 
Area kelima adalah Tenkai Area yang berisi Heaven Chiho. File di area ini dilindungi dengan enkripsi AES-256-CBC (gunakan openssl), dengan setiap file memiliki IV yang implementasinya dibebaskan kepada praktikan (terdapat nilai tambahan jika IV tidak static).
![image](https://github.com/user-attachments/assets/ed8d06e1-3930-431f-86c3-ba3b6c6f78da)

f. Youth Area - Skystreet Chiho: 
Area keenam adalah Youth Area yang berisi Skystreet Chiho. File di area ini dikompresi menggunakan gzip untuk menghemat storage (gunakan zlib).
![image](https://github.com/user-attachments/assets/2cd337ea-002e-4b2d-b598-e0bef8271d6f)

g. Prism Area - 7sRef Chiho: 
Area ketujuh, atau terakhir adalah Prism Area yang berisi 7sRef Chiho. Jika kita melihat world map yang ada diawal, terlihat bahwa 7sRef ini merupakan “gateway” ke semua chiho lain. Sehingga, area ini adalah area spesial yang dapat mengakses semua area lain melalui sistem penamaan khusus.
![image](https://github.com/user-attachments/assets/cc4f7503-53ba-4acf-acb4-ce8d531827a3)
![image](https://github.com/user-attachments/assets/483bdee1-7ffe-4987-96f1-aa4ccf070537)
![image](https://github.com/user-attachments/assets/0a07a7ce-1a15-4dad-9cc9-5dfd373e2327)

--- PENJELASAN FUNGSI PENDUKUNG ---
1. log_message
```bash
void log_message(const char *color, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "%s[maimai_fs] ", color);
    vfprintf(stderr, format, args);
    fprintf(stderr, "%s\n", ANSI_COLOR_RESET);
    va_end(args);
}
```
- Tujuan: Mencetak pesan log ke stderr dengan warna untuk debugging.
- Menggunakan variabel argument (va_list) untuk mencetak pesan dengan format yang ditentukan, dengan warna ANSI (merah, hijau, dll.) untuk membedakan jenis pesan (error, info, dll.). Setiap pesan diakhiri dengan reset warna.

2. print_welcome
```bash
void print_welcome() {
    printf(ANSI_COLOR_GREEN);
    printf("=====================================\n");
    printf("    maimai File System v1.0\n");
    printf("    Powered by SEGA & xAI\n");
    printf("=====================================\n");
    printf(ANSI_COLOR_RESET);
    printf("Mounting at %s...\n", dirpath);
}
```
- Tujuan: Menampilkan pesan sambutan saat program dijalankan.
- Mencetak header berwarna hijau yang menunjukkan nama sistem file ("maimai File System v1.0"), sponsor (SEGA & xAI), dan direktori mount (dirpath).

3. shift_filename
```bash
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
```
- Tujuan: Menggeser karakter dalam nama file (enkripsi sederhana berbasis posisi).
- Menggeser huruf a-z atau A-Z berdasarkan posisi (indeks + 1 atau sebaliknya jika reverse), mengganti '/' atau '\0' dengan '_'. Catatan: Fungsi ini tidak digunakan dalam kode utama, mungkin untuk pengembangan tambahan.

4. index_encrypt
```bash
void index_encrypt(char *data, size_t len) {
    if (!data || len == 0) return;
    for (size_t i = 0; i < len; i++) {
        data[i] = data[i] + (i % 256); 
        if (data[i] == ' ') data[i] = '*'; 
    }
}
```
- Tujuan: Mengenkripsi data untuk area metro dengan pergeseran berbasis posisi.
- Menggeser setiap karakter sebanyak i % 256 dan mengubah spasi menjadi '*'. Digunakan untuk file di metro sebelum disimpan.

5. index_decrypt
```bash
void index_decrypt(char *data, size_t len) {
    if (!data || len == 0) return;
    for (size_t i = 0; i < len; i++) {
        if (data[i] == '*') {
            data[i] = ' '; 
        } else {
            data[i] = data[i] - (i % 256); 
        }
    }
}
```
- Tujuan: Mendekripsi data dari area metro.
- Mengembalikan pergeseran dengan mengurangi i % 256 dan mengubah '*' menjadi spasi. Digunakan saat membaca file dari metro.

6. rot13
```bash
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
```
- Tujuan: Mengenkripsi/mendekripsi data untuk area dragon menggunakan ROT13.
- Menggeser huruf alfabet (a-z, A-Z) sebanyak 13 posisi (misalnya, A → N, N → A). Hanya memengaruhi huruf, karakter lain tetap. Digunakan untuk file di dragon.

7. get_actual_path
```bash
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
        if (dot) *dot = '\0'; 

        const char *exts[] = {".mai", ".ccc", ".rot", ".bin", ".enc", ".gz", NULL};
        if (strcmp(area, "starter") == 0) {
            snprintf(temp, sizeof(temp), "/starter/test.text%s", exts[0]);
        } else if (strcmp(area, "metro") == 0) {
            snprintf(temp, sizeof(temp), "/metro/test.txt%s", exts[1]);
        } else if (strcmp(area, "dragon") == 0) {
            snprintf(temp, sizeof(temp), "/dragon/test.txt%s", exts[2]);
        } else if (strcmp(area, "blackrose") == 0) {
            snprintf(temp, sizeof(temp), "/blackrose/%s%s", actual_name, exts[3]); 
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
```
- Tujuan: Memetakan path virtual FUSE ke path aktual di sistem file.
- Mengubah path seperti /starter/test.txt menjadi /home/aryarefman/chiho/starter/test.text.mai atau /7sref/starter_test.txt ke path aktual berdasarkan area (dengan ekstensi seperti .mai, .ccc, dll.). Menangani validasi path dan error seperti nama terlalu panjang.

8. aes_encrypt
```bash
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
```
- Tujuan: Mengenkripsi data untuk area heaven menggunakan AES-256-CBC.
- Menggunakan OpenSSL untuk enkripsi dengan kunci statis (aes_key) dan IV acak (dihasilkan dengan RAND_bytes). IV disimpan di 16 byte pertama file, diikuti data terenkripsi. Digunakan saat menulis file di heaven.
 
9. aes_decrypt
```bash
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
```
- Tujuan: Mendekripsi data dari area heaven.
- Membaca IV dari 16 byte pertama file, lalu mendekripsi data menggunakan AES-256-CBC. Menangani offset untuk membaca sebagian data. Digunakan saat membaca file dari heaven.

--- PENJELASAN FUNGSI FUSE ---
1. xmp_open
```bash
static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char full_path[MAX_PATH];
    int res = get_actual_path(path, full_path, sizeof(full_path));
    if (res < 0) return res;

    int fd = open(full_path, fi->flags);
    if (fd == -1) return -errno;
    fi->fh = fd;
    return 0;
}
```
- Tujuan: Membuka file di path aktual untuk operasi baca/tulis.
- Memetakan path virtual ke path aktual dengan get_actual_path, lalu membuka file dengan open menggunakan flag dari fuse_file_info. File descriptor disimpan di fi->fh.

2. xmp_getattr
```bash
static int xmp_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    log_message(ANSI_COLOR_YELLOW, "xmp_getattr called for path: %s", path);
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (strcmp(path, "/7sref") == 0 ||
        strcmp(path, "/starter") == 0 || 
        strcmp(path, "/metro") == 0 || 
        strcmp(path, "/dragon") == 0 || 
        strcmp(path, "/blackrose") == 0 || 
        strcmp(path, "/heaven") == 0 || 
        strcmp(path, "/skystreet") == 0) {
        log_message(ANSI_COLOR_GREEN, "Returning attributes for directory: %s", path);
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    if (strncmp(path, "/7sref/", 7) == 0) {
        log_message(ANSI_COLOR_GREEN, "Returning attributes for file in 7sref: %s", path);
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = 1024; 
        return 0;
    }

    char full_path[MAX_PATH];
    int res = get_actual_path(path, full_path, sizeof(full_path));
    if (res == 0) {
        res = lstat(full_path, stbuf);
        if (res == -1) return -errno;
        return 0;
    }

    return -ENOENT;
}
```
- Tujuan: Mengembalikan atribut file atau direktori (mode, ukuran, dll.).
- Menangani direktori root (/), direktori area (/starter, /metro, dll.), direktori /7sref, dan file. Untuk direktori, mengatur mode sebagai S_IFDIR | 0755. Untuk file di /7sref, menggunakan ukuran statis (1024 byte). Untuk file lain, menggunakan lstat pada path aktual.

3. xmp_readdir
```bash
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
        log_message(ANSI_COLOR_CYAN, "Populating /7sref directory");
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        struct stat st = {0};
        st.st_mode = S_IFREG | 0644;
        st.st_nlink = 1;
        st.st_size = 1024;
        const char *files[] = {
            "starter_test.txt",
            "metro_test.txt",
            "dragon_test.txt",
            "blackrose_firefox.png.txt",
            "heaven_test.txt",
            "skystreet_test.txt",
            NULL
        };
        for (int i = 0; files[i]; i++) {
            log_message(ANSI_COLOR_YELLOW, "Adding to 7sref: %s", files[i]);
            filler(buf, files[i], &st, 0, 0);
        }
        return 0;
    }

    if (strcmp(path, "/starter") == 0 || 
        strcmp(path, "/metro") == 0 || 
        strcmp(path, "/dragon") == 0 || 
        strcmp(path, "/heaven") == 0 || 
        strcmp(path, "/skystreet") == 0) {
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        struct stat st = {0};
        st.st_mode = S_IFREG | 0644;
        st.st_nlink = 1;
        st.st_size = 1024;
        filler(buf, "test.txt", &st, 0, 0);
        return 0;
    }

    if (strcmp(path, "/blackrose") == 0) {
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        struct stat st = {0};
        st.st_mode = S_IFREG | 0644;
        st.st_nlink = 1;
        st.st_size = 1024;
        filler(buf, "firefox.png", &st, 0, 0);
        return 0;
    }

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
                *ext = '\0';
                char *parent_path = strrchr(path, '/');
                if (parent_path && strcmp(parent_path + 1, "blackrose") == 0) {
                } else {
                    strcpy(display_name, "test.txt");
                }
            } else {
                strcpy(display_name, "test.txt");
            }

            struct stat st;
            char file_path[MAX_PATH];
            snprintf(file_path, sizeof(file_path), "%s/%s", full_path, de->d_name);
            if (lstat(file_path, &st) == -1) {
                log_message(ANSI_COLOR_RED, "Failed to stat file: %s, error: %s", file_path, strerror(errno));
                continue;
            }
            log_message(ANSI_COLOR_YELLOW, "Listing in %s: %s", path, display_name);
            filler(buf, display_name, &st, 0, 0);
        }
        closedir(dp);
        return 0;
    }

    return -ENOENT;
}
```
- Tujuan: Menampilkan isi direktori.
- Penjelasan: 
  - Untuk root (/), menampilkan subdirektori: starter, metro, dragon, blackrose, heaven, skystreet, 7sref.
  - Untuk /7sref, menampilkan file virtual seperti starter_test.txt, metro_test.txt, dll.
  - Untuk direktori area, menampilkan test.txt (atau firefox.png untuk blackrose).
  - Untuk path aktual, membaca isi direktori dengan opendir dan readdir, menyesuaikan nama file (misalnya, menghapus ekstensi).

4. xmp_read
```bash
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

            if (strcmp(areas[i], "metro") == 0) index_decrypt(temp_buf, len);
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
        if (res > 0) index_decrypt(temp_buf, res); 
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
```
- Tujuan: Membaca data dari file dengan penanganan khusus per area.
- Penjelasan:
  - starter: Membaca langsung dengan pread.
  - metro: Membaca dengan pread, lalu mendekripsi dengan index_decrypt.
  - dragon: Membaca dengan pread, lalu mendekripsi dengan rot13.
  - blackrose: Membaca langsung dengan pread (data biner).
  - heaven: Membaca dan mendekripsi dengan aes_decrypt.
  - skystreet/youth: Membaca dan mendekompresi dengan gzopen dan gzread.
  - 7sref: Membaca file dari semua area, mendekripsi/mendekompresi sesuai area, lalu menggabungkan data.
 
5. xmp_write
```bash
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
        index_encrypt(temp_buf, size); 
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
```
- Tujuan: Menulis data ke file dengan penanganan khusus per area.
- Penjelasan:
  - starter: Menulis langsung dengan pwrite.
  - metro: Mengenkripsi dengan index_encrypt, lalu menulis dengan pwrite.
  - dragon: Mengenkripsi dengan rot13, lalu menulis dengan pwrite.
  - blackrose: Menulis langsung dengan pwrite (data biner).
  - heaven: Mengenkripsi dengan aes_encrypt.
  - skystreet/youth: Mengompresi dan menulis dengan gzopen dan gzwrite.
  - 7sref: Tidak mendukung penulisan (tidak diimplementasikan).
 
6. xmp_create
```bash
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
```
- Tujuan: Membuat file baru di path aktual.
- Memetakan path virtual ke path aktual dengan get_actual_path, lalu membuat file dengan creat menggunakan mode yang ditentukan. File descriptor disimpan di fi->fh.

7. xmp_unlink
```bash
static int xmp_unlink(const char *path) {
    char full_path[MAX_PATH];
    int res = get_actual_path(path, full_path, sizeof(full_path));
    if (res < 0) return res;

    res = unlink(full_path);
    return res == -1 ? -errno : 0;
}
```
- Tujuan: Menghapus file di path aktual.
- Memetakan path virtual ke path aktual dengan get_actual_path, lalu menghapus file dengan unlink.
