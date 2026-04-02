#pragma once

#define ALARM_REPEAT    0
#define ALARM_ONESHOT   1

typedef int (*ct_cb_t)(uint8_t chan_id, uint32_t us);

int ct_init(void);
void ct_destroy(void);
int ct_start(void);
int ct_stop(void);
int ct_rst(void);
int ct_set_val(uint32_t ticks);
int ct_get_val(uint32_t *ticks);
int ct_set_alarm(uint8_t chan_id, uint32_t us, ct_cb_t cb);
int ct_remove_alarm(uint8_t chan_id);
