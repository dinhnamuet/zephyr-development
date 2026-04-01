#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/clock.h>
#include <zephyr/logging/log.h>

#include "serial.h"

LOG_MODULE_REGISTER(serial);

#define BITS_PER_UART_BYTE    10U
#define UART_IDLE_BYTE_COUNT  4U
#define UART_MPS              64U

#define UART_TIMEOUT(baud) \
    ((BITS_PER_UART_BYTE * UART_IDLE_BYTE_COUNT * USEC_PER_SEC) / (baud))

struct uart_frame {
    size_t size;
    uint8_t data[];
};

K_HEAP_DEFINE(uart_heap, 4 * (sizeof(struct uart_frame) + UART_MPS));
K_THREAD_STACK_DEFINE(uart_stack, 512);
K_FIFO_DEFINE(uart_fifo);

static uint8_t __nocache uart_buffer[2][UART_MPS];
static uint8_t *free_buf = uart_buffer[1];

static const struct device *const uart_inst = DEVICE_DT_GET(DT_NODELABEL(usart1));

static void uart_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
    switch (evt->type) {
    case UART_RX_RDY:
        struct uart_frame *msg;
        msg = k_heap_alloc(&uart_heap, sizeof(*msg) + evt->data.rx.len, K_NO_WAIT);
        if (!msg) {
            LOG_WRN("RX frame dropped — heap full");
            return;
        }
        msg->size = evt->data.rx.len;
        memcpy(msg->data, evt->data.rx.buf + evt->data.rx.offset, \
               evt->data.rx.len);
        k_fifo_put(&uart_fifo, msg);
        break;

    case UART_RX_BUF_REQUEST:
        uart_rx_buf_rsp(dev, free_buf, UART_MPS);
        break;

    case UART_RX_BUF_RELEASED:
        free_buf = evt->data.rx_buf.buf;
        break;

    default:
        break;
    }
}

static void uart_entry(void *p1, void *p2, void *p3)
{
    uart_cb_t cb = p1;
    struct uart_frame *msg;

    while (true) {
        msg = k_fifo_get(&uart_fifo, K_FOREVER);
        cb(msg->data, msg->size);
        k_heap_free(&uart_heap, msg);
    }
}

int serial_init(uart_cb_t cb)
{
    int ret;
    static struct k_thread uart_thread;

    if (!device_is_ready(uart_inst)) {
        LOG_ERR("UART not found");
        return -ENODEV;
    } else if (!cb) {
        LOG_ERR("Callback invalid");
        return -EFAULT;
    }
    ret = uart_callback_set(uart_inst, uart_cb, NULL);
    if (ret) {
        LOG_ERR("Could not set callback %d", ret);
        return ret;
    }
    ret = uart_rx_enable(uart_inst, uart_buffer[0], UART_MPS, UART_TIMEOUT(115200));
    if (ret) {
        LOG_ERR("Could not Enable UART Rx");
        return ret;
    }
    k_thread_create(&uart_thread, uart_stack, K_THREAD_STACK_SIZEOF(uart_stack), uart_entry, \
        (void *)cb, NULL, NULL, 5, 0, K_NO_WAIT);
    return 0;
}

int serial_write(const uint8_t *data, size_t size)
{
    if (!device_is_ready(uart_inst)) {
        LOG_ERR("UART not found");
        return -ENODEV;
    } else if (!data && size) {
        LOG_ERR("Data invalid");
        return -EINVAL;
    }

    for (int i = 0; i < size; i++) {
		uart_poll_out(uart_inst, data[i]);
	}
    return 0;
}
