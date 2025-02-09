/*
 * Copyright (c) 2024 Tareq Mhisen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "events_svc.h"

LOG_MODULE_REGISTER(events_svc);

K_MSGQ_DEFINE(event_msq, sizeof(struct event), EVENT_QUEUE_SIZE, 4);

char *events_svc_type_to_text(enum event_type type)
{
	switch (type) {
	case EVENT_NETWORK_CONNECTED:
		return "EVENT_NETWORK_CONNECTED";
	case EVENT_NETWORK_NOT_CONNECTED:
		return "EVENT_NETWORK_NOT_CONNECTED";
	case EVENT_ZIGBEE_DATA_WIPED:
		return "EVENT_ZIGBEE_DATA_WIPED";
	default:
		return "UNKNOWN";
	}
}

int events_svc_send_event(struct event *evt)
{
	return k_msgq_put(&event_msq, evt, K_NO_WAIT);
}

int events_svc_get_event(struct event *evt)
{
	return k_msgq_get(&event_msq, evt, K_FOREVER);
}