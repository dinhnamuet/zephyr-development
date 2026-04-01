/**
 * dfu_host.c — Host-side DFU tool (Ubuntu / Linux)
 *
 * Protocol:
 *   TX  → EP 0x01 OUT : [cmd] hoặc [cmd, data...]
 *   RX  ← EP 0x81 IN  : [cmd, status]  (status: 0x01=SUCCESS, 0x00=FAILED)
 *
 * Commands:
 *   DFU_START    = 0x03
 *   DFU_DOWNLOAD = 0x04
 *   DFU_END      = 0x05
 *
 * Build:
 *   sudo apt install libusb-1.0-0-dev
 *   gcc dfu_host.c -o dfu_host -lusb-1.0
 *
 * Usage:
 *   sudo ./dfu_host -f firmware.bin -v 0x1234 -p 0x5678
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <libusb-1.0/libusb.h>

/* ── Protocol ────────────────────────────────────────────────────────────── */
#define DFU_START       0x03
#define DFU_DOWNLOAD    0x04
#define DFU_END         0x05
#define STATUS_SUCCESS  0x01
#define STATUS_FAILED   0x00

#define EP_OUT          0x01
#define EP_IN           0x81
#define CHUNK_SIZE      63         /* bytes per DFU_DOWNLOAD packet        */
#define USB_TIMEOUT_MS  5000

/* ── ANSI colors ─────────────────────────────────────────────────────────── */
#define COL_OK    "\033[92m[OK]   \033[0m "
#define COL_ERR   "\033[91m[ERR]  \033[0m "
#define COL_INFO  "\033[96m[INFO] \033[0m "
#define COL_WARN  "\033[93m[WARN] \033[0m "

/* ── Globals ──────────────────────────────────────────────────────────────── */
static libusb_context       *ctx  = NULL;
static libusb_device_handle *hdev = NULL;
static int kernel_detached = 0;

/* ── Helpers ──────────────────────────────────────────────────────────────── */
static void print_progress(size_t done, size_t total)
{
    int pct   = (int)((done * 100) / total);
    int bars  = pct / 2;   /* 50 chars wide */
    printf("\r  [");
    for (int i = 0; i < 50; i++)
        putchar(i < bars ? '#' : ' ');
    printf("] %3d%%  (%zu / %zu bytes)  ", pct, done, total);
    fflush(stdout);
}

/**
 * send_and_ack - send payload to EP_OUT, read 2-byte ack from EP_IN.
 * @cmd   : command byte (for ack verification)
 * @buf   : payload buffer (must start with cmd byte)
 * @len   : total payload length
 * Returns 0 on success, -1 on error.
 */
static int send_and_ack(uint8_t cmd, const uint8_t *buf, int len)
{
    int transferred = 0;
    int ret;
    uint8_t ack[2];

    /* TX */
    ret = libusb_bulk_transfer(hdev, EP_OUT,
                               (uint8_t *)buf, len,
                               &transferred, USB_TIMEOUT_MS);
    if (ret != LIBUSB_SUCCESS) {
        fprintf(stderr, COL_ERR "TX failed: %s\n", libusb_error_name(ret));
        return -1;
    }
    if (transferred != len) {
        fprintf(stderr, COL_ERR "TX short write: %d/%d\n", transferred, len);
        return -1;
    }

    /* RX ack */
    ret = libusb_bulk_transfer(hdev, EP_IN,
                               ack, sizeof(ack),
                               &transferred, USB_TIMEOUT_MS);
    if (ret != LIBUSB_SUCCESS) {
        fprintf(stderr, COL_ERR "RX ack failed: %s\n", libusb_error_name(ret));
        return -1;
    }
    if (transferred < 2) {
        fprintf(stderr, COL_ERR "RX ack too short: %d bytes\n", transferred);
        return -1;
    }
    if (ack[0] != cmd) {
        fprintf(stderr, COL_ERR "Ack cmd mismatch: got 0x%02X, expect 0x%02X\n",
                ack[0], cmd);
        return -1;
    }
    if (ack[1] != STATUS_SUCCESS) {
        fprintf(stderr, COL_ERR "Device returned FAILED for cmd 0x%02X\n", cmd);
        return -1;
    }
    return 0;
}

/* ── DFU steps ────────────────────────────────────────────────────────────── */
static int dfu_start(void)
{
    uint8_t pkt = DFU_START;
    printf(COL_INFO "Sending DFU_START...\n");
    if (send_and_ack(DFU_START, &pkt, 1) != 0)
        return -1;
    printf(COL_OK "Device ready for DFU.\n");
    return 0;
}

static int dfu_download(const uint8_t *fw, size_t fw_size)
{
    /* packet layout: [DFU_DOWNLOAD, data...] */
    uint8_t pkt[1 + CHUNK_SIZE];
    size_t  offset = 0;

    printf(COL_INFO "Sending firmware (%zu bytes)...\n", fw_size);

    while (offset < fw_size) {
        size_t chunk = fw_size - offset;
        if (chunk > CHUNK_SIZE)
            chunk = CHUNK_SIZE;

        pkt[0] = DFU_DOWNLOAD;
        memcpy(&pkt[1], fw + offset, chunk);

        if (send_and_ack(DFU_DOWNLOAD, pkt, (int)(1 + chunk)) != 0) {
            fprintf(stderr, "\n" COL_ERR "Download failed at offset %zu\n", offset);
            return -1;
        }

        offset += chunk;
        print_progress(offset, fw_size);
    }
    printf("\n" COL_OK "All chunks sent.\n");
    return 0;
}

static int dfu_end(void)
{
    uint8_t pkt = DFU_END;
    printf(COL_INFO "Sending DFU_END...\n");
    if (send_and_ack(DFU_END, &pkt, 1) != 0)
        return -1;
    printf(COL_OK "DFU_END acknowledged. Device will reboot now.\n");
    return 0;
}

/* ── USB setup / teardown ─────────────────────────────────────────────────── */
static int usb_open(uint16_t vid, uint16_t pid)
{
    int ret;

    ret = libusb_init(&ctx);
    if (ret != LIBUSB_SUCCESS) {
        fprintf(stderr, COL_ERR "libusb_init: %s\n", libusb_error_name(ret));
        return -1;
    }

    hdev = libusb_open_device_with_vid_pid(ctx, vid, pid);
    if (!hdev) {
        fprintf(stderr, COL_ERR "Device not found (VID=0x%04X PID=0x%04X).\n"
                        "       Tip: run 'lsusb' to verify the device is connected.\n",
                vid, pid);
        libusb_exit(ctx);
        return -1;
    }
    printf(COL_OK "Device found (VID=0x%04X PID=0x%04X).\n", vid, pid);

    /* Detach kernel driver if active */
    if (libusb_kernel_driver_active(hdev, 0) == 1) {
        ret = libusb_detach_kernel_driver(hdev, 0);
        if (ret != LIBUSB_SUCCESS) {
            fprintf(stderr, COL_ERR "detach_kernel_driver: %s\n",
                    libusb_error_name(ret));
            libusb_close(hdev);
            libusb_exit(ctx);
            return -1;
        }
        kernel_detached = 1;
        printf(COL_INFO "Kernel driver detached.\n");
    }

    ret = libusb_claim_interface(hdev, 0);
    if (ret != LIBUSB_SUCCESS) {
        fprintf(stderr, COL_ERR "claim_interface: %s\n", libusb_error_name(ret));
        if (kernel_detached)
            libusb_attach_kernel_driver(hdev, 0);
        libusb_close(hdev);
        libusb_exit(ctx);
        return -1;
    }
    printf(COL_OK "Interface 0 claimed.\n");
    return 0;
}

static void usb_close(void)
{
    if (hdev) {
        libusb_release_interface(hdev, 0);
        if (kernel_detached)
            libusb_attach_kernel_driver(hdev, 0);
        libusb_close(hdev);
        hdev = NULL;
    }
    if (ctx) {
        libusb_exit(ctx);
        ctx = NULL;
    }
}

/* ── main ─────────────────────────────────────────────────────────────────── */
static void usage(const char *prog)
{
    printf("Usage: %s -f <firmware.bin> -v <VID> -p <PID>\n"
           "  -f  path to firmware binary\n"
           "  -v  USB Vendor  ID (hex, e.g. 0x1234)\n"
           "  -p  USB Product ID (hex, e.g. 0x5678)\n",
           prog);
}

int main(int argc, char *argv[])
{
    const char *fw_path = NULL;
    uint16_t vid = 0, pid = 0;

    /* Parse args */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-f") && i + 1 < argc)
            fw_path = argv[++i];
        else if (!strcmp(argv[i], "-v") && i + 1 < argc)
            vid = (uint16_t)strtoul(argv[++i], NULL, 16);
        else if (!strcmp(argv[i], "-p") && i + 1 < argc)
            pid = (uint16_t)strtoul(argv[++i], NULL, 16);
        else { usage(argv[0]); return 1; }
    }
    if (!fw_path || !vid || !pid) { usage(argv[0]); return 1; }

    /* Read firmware file */
    FILE *f = fopen(fw_path, "rb");
    if (!f) {
        fprintf(stderr, COL_ERR "Cannot open '%s': %s\n", fw_path, strerror(errno));
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long fw_size = ftell(f);
    rewind(f);

    uint8_t *fw = malloc(fw_size);
    if (!fw) {
        fprintf(stderr, COL_ERR "Out of memory\n");
        fclose(f);
        return 1;
    }
    if ((long)fread(fw, 1, fw_size, f) != fw_size) {
        fprintf(stderr, COL_ERR "Failed to read firmware file\n");
        free(fw); fclose(f);
        return 1;
    }
    fclose(f);
    printf(COL_INFO "Firmware: '%s' (%ld bytes)\n", fw_path, fw_size);

    /* DFU sequence */
    int ret = 0;
    if (usb_open(vid, pid) != 0) { free(fw); return 1; }

    printf("\n─── DFU Sequence ───────────────────────────────────\n");

    if (dfu_start()                       != 0) { ret = 1; goto done; }
    if (dfu_download(fw, (size_t)fw_size) != 0) { ret = 1; goto done; }
    if (dfu_end()                         != 0) { ret = 1; goto done; }

    printf("────────────────────────────────────────────────────\n");
    printf(COL_OK "DFU complete! Device is rebooting into new firmware.\n\n");

done:
    usb_close();
    free(fw);
    if (ret)
        fprintf(stderr, COL_ERR "DFU failed.\n\n");
    return ret;
}
