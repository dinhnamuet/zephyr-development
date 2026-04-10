#include <zephyr/kernel.h>
#include <zephyr/mgmt/updatehub.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

LOG_MODULE_REGISTER(updatehub_ota);

#define UPDATEHUB_THREAD_PRIO    5
#define UPDATEHUB_POLL_INTERVAL  K_MINUTES(CONFIG_UPDATEHUB_POLL_INTERVAL)
#define UPDATEHUB_STACK_SIZE     4096

K_THREAD_STACK_DEFINE(updatehub_stack, 4096);

static struct k_work_q updatehub_wq;
static struct k_work_delayable updatehub_w;

static void updatehub_poll(struct k_work *work)
{
    switch (updatehub_probe()) {
	case UPDATEHUB_UNCONFIRMED_IMAGE:
		LOG_ERR("Image is unconfirmed. Rebooting to revert back to previous"
			"confirmed image.");
		updatehub_report_error();
		LOG_PANIC();
		updatehub_reboot();
		return;

	case UPDATEHUB_HAS_UPDATE:
		switch (updatehub_update()) {
		case UPDATEHUB_OK:
			LOG_PANIC();
			updatehub_reboot();
			return;

		default:
            LOG_ERR("Error installing update.");
			break;
		}

		break;

	case UPDATEHUB_NO_UPDATE:
		break;

	default:
		break;
	}
    k_work_reschedule_for_queue(&updatehub_wq, &updatehub_w, UPDATEHUB_POLL_INTERVAL);
}

void updatehub_init(void)
{
    k_work_queue_init(&updatehub_wq);
    k_work_queue_start(&updatehub_wq, updatehub_stack,
        K_THREAD_STACK_SIZEOF(updatehub_stack), UPDATEHUB_THREAD_PRIO, NULL);
    
    k_work_init_delayable(&updatehub_w, updatehub_poll);
}

void updatehub_start(void)
{
    k_work_schedule_for_queue(&updatehub_wq, &updatehub_w, K_NO_WAIT);
}
