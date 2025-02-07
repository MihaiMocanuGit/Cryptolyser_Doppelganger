#ifndef CRYPTOLYSER_VICTIM_CONNECTIONHANDLER_CONNECTION_HANDLER_H
#define CRYPTOLYSER_VICTIM_CONNECTIONHANDLER_CONNECTION_HANDLER_H

#include <stdint.h>
#include <time.h>

struct connection_t;

int connection_init(struct connection_t **connection, uint16_t port);
int connection_receive_data(struct connection_t *connection, uint32_t *packet_id, uint8_t **data,
                            uint32_t *data_len);
int connection_receive_data_noalloc(struct connection_t *connection, uint32_t *packet_id,
                                    uint8_t *data, uint32_t *data_len);
int connection_respond_back(struct connection_t *connection, uint32_t packet_id,
                            struct timespec inbound_time, struct timespec outbound_time);
void connection_close(struct connection_t *connection);

void connection_cleanup(struct connection_t **connection);

#endif // CRYPTOLYSER_VICTIM_CONNECTIONHANDLER_CONNECTION_HANDLER_H
