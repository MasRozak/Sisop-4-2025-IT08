# Menggunakan image dasar Ubuntu
FROM ubuntu:20.04

# Mengatur environment agar tidak ada interaksi prompt saat instalasi
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies untuk FUSE dan compiler C
RUN apt-get update && apt-get install -y \
    fuse3 \
    libfuse3-dev \
    build-essential \
    gcc \
    make \
    git \
    && rm -rf /var/lib/apt/lists/*

# Salin program FUSE yang telah dibuat (antink.c)
COPY ./antink.c /usr/local/bin/antink.c

# Pastikan mount point dan log directory ada
RUN mkdir -p /mnt/antink_mount /mnt/original_files /var/log

# Menyalakan server FUSE
RUN gcc -D_FILE_OFFSET_BITS=64 -I/usr/include/fuse3 /usr/local/bin/antink.c -o /usr/local/bin/antink-server -lfuse3 && chmod +x /usr/local/bin/antink-server

# Jalankan server FUSE dengan output debug
CMD ["/usr/local/bin/antink-server"]
