/**
 * tcp_client.c — DFU over TCP
 *
 * Build:
 *   gcc tcp_client.c -o dfu_host
 *
 * Usage:
 *   ./dfu_host -f firmware.bin -i 192.168.1.10 -p 5000
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

/* ── Protocol ───────────────────────────────────────── */
#define DFU_START       0x03
#define DFU_DOWNLOAD    0x04
#define DFU_END         0x05
#define STATUS_SUCCESS  0x01
#define STATUS_FAILED   0x00

#define CHUNK_SIZE      254

/* ── ANSI colors ───────────────────────────────────── */
#define COL_OK    "\033[92m[OK]   \033[0m "
#define COL_ERR   "\033[91m[ERR]  \033[0m "
#define COL_INFO  "\033[96m[INFO] \033[0m "

/* ── TCP socket ───────────────────────────────────── */
static int sock = -1;

/* ── Helpers ─────────────────────────────────────── */
static void print_progress(size_t done, size_t total)
{
    int pct = (int)((done * 100) / total);
    int bars = pct / 2;
    printf("\r  [");
    for (int i = 0; i < 50; i++)
        putchar(i < bars ? '#' : ' ');
    printf("] %3d%% (%zu/%zu bytes)", pct, done, total);
    fflush(stdout);
}

/* ── TCP send/recv ───────────────────────────────── */
static int send_and_ack(uint8_t cmd, const uint8_t *buf, int len)
{
    uint8_t ack[2];

    /* send */
    if (send(sock, buf, len, 0) != len) {
        perror("send");
        return -1;
    }

    /* recv ack */
    int r = recv(sock, ack, sizeof(ack), MSG_WAITALL);
    if (r != 2) {
        fprintf(stderr, COL_ERR "ACK recv failed\n");
        return -1;
    }

    if (ack[0] != cmd) {
        fprintf(stderr, COL_ERR "ACK cmd mismatch\n");
        return -1;
    }

    if (ack[1] != STATUS_SUCCESS) {
        fprintf(stderr, COL_ERR "Device returned FAIL\n");
        return -1;
    }

    return 0;
}

/* ── DFU steps ───────────────────────────────────── */
static int dfu_start(void)
{
    uint8_t pkt = DFU_START;
    printf(COL_INFO "DFU_START...\n");
    if (send_and_ack(DFU_START, &pkt, 1) != 0)
        return -1;
    printf(COL_OK "Device ready\n");
    return 0;
}

static int dfu_download(const uint8_t *fw, size_t fw_size)
{
    uint8_t pkt[1 + CHUNK_SIZE];
    size_t offset = 0;

    printf(COL_INFO "Sending firmware (%zu bytes)...\n", fw_size);

    while (offset < fw_size) {
        size_t chunk = fw_size - offset;
        if (chunk > CHUNK_SIZE)
            chunk = CHUNK_SIZE;

        pkt[0] = DFU_DOWNLOAD;
        memcpy(&pkt[1], fw + offset, chunk);

        if (send_and_ack(DFU_DOWNLOAD, pkt, 1 + chunk) != 0) {
            fprintf(stderr, "\n" COL_ERR "Failed at %zu\n", offset);
            return -1;
        }

        offset += chunk;
        print_progress(offset, fw_size);
    }

    printf("\n" COL_OK "Download done\n");
    return 0;
}

static int dfu_end(void)
{
    uint8_t pkt = DFU_END;
    printf(COL_INFO "DFU_END...\n");
    if (send_and_ack(DFU_END, &pkt, 1) != 0)
        return -1;
    printf(COL_OK "Device rebooting\n");
    return 0;
}

/* ── TCP connect ─────────────────────────────────── */
static int tcp_connect(const char *ip, int port)
{
    struct sockaddr_in server;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server.sin_addr) <= 0) {
        fprintf(stderr, COL_ERR "Invalid IP\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("connect");
        return -1;
    }

    printf(COL_OK "Connected to %s:%d\n", ip, port);
    return 0;
}

static void tcp_close(void)
{
    if (sock >= 0)
        close(sock);
}

/* ── main ────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    const char *fw_path = NULL;
    const char *ip = NULL;
    int port = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-f")) fw_path = argv[++i];
        else if (!strcmp(argv[i], "-i")) ip = argv[++i];
        else if (!strcmp(argv[i], "-p")) port = atoi(argv[++i]);
    }

    if (!fw_path || !ip || !port) {
        printf("Usage: %s -f fw.bin -i <ip> -p <port>\n", argv[0]);
        return 1;
    }

    /* read file */
    FILE *f = fopen(fw_path, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);

    uint8_t *fw = malloc(size);
    fread(fw, 1, size, f);
    fclose(f);

    printf(COL_INFO "Firmware size: %ld bytes\n", size);

    /* connect */
    if (tcp_connect(ip, port) != 0)
        return 1;

    printf("\n── DFU START ──\n");

    if (dfu_start() != 0) goto done;
    if (dfu_download(fw, size) != 0) goto done;
    if (dfu_end() != 0) goto done;

    printf(COL_OK "DFU SUCCESS\n");

done:
    tcp_close();
    free(fw);
    return 0;
}
