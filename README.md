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



