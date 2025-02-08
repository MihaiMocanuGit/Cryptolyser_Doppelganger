#ifndef CRYPTOLYSER_DOPPELGANGER_CONNECTIONHANDLER_CONNECTION_HANDLER_H
#define CRYPTOLYSER_DOPPELGANGER_CONNECTIONHANDLER_CONNECTION_HANDLER_H

#include "Cryptolyser_Common/connection_data_types.h"
#include "Cryptolyser_Common/cycle_timer.h"

#include <stdint.h>

struct connection_t;

int connection_init(struct connection_t **connection, uint16_t port);

int connection_receive_data(struct connection_t *connection, uint32_t *packet_id, uint8_t **data,
                            uint32_t *data_len);

int connection_receive_data_noalloc(struct connection_t *connection, uint32_t *packet_id,
                                    uint8_t *data, uint32_t *data_len);

int connection_respond_back(struct connection_t *connection, uint32_t packet_id,
                            uint8_t data[static PACKET_RESPONSE_DATA_SIZE],
                            struct cycle_timer_t inbound_time, struct cycle_timer_t outbound_time,
                            uint8_t iv[static AES_BLOCK_BYTE_SIZE]);

void connection_close(struct connection_t *connection);

void connection_cleanup(struct connection_t **connection);

#endif // CRYPTOLYSER_DOPPELGANGER_CONNECTIONHANDLER_CONNECTION_HANDLER_H
