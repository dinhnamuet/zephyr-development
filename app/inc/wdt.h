#pragma once

int watchdog_init(unsigned long timeoutms);
void watchdog_daemon_start(unsigned long duration_ms);
