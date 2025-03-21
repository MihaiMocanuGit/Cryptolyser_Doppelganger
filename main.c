#include "AES/aes_interface.h"
#include "CacheFlush/cache_flush.h"
#include "ConnectionHandler/connection_handler.h"
#include "Cryptolyser_Common/connection_data_types.h"
#include "Cryptolyser_Common/cycle_timer.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

static void printHexLine(const char *line_label, uint8_t *input, uint32_t len)
{
    printf("%s", line_label);
    for (uint32_t i = 0; i < len; ++i)
    {
        printf("%02X ", (unsigned int)input[i]);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Incorrect program parameter: <PORT>\n");
        return EXIT_FAILURE;
    }
    aes_log_status(stdout);

    struct connection_t *server;
    if (connection_init(&server, atoi(argv[1])))
    {
        perror("Could not initialize connection.\n");
        connection_cleanup(&server);
        return EXIT_FAILURE;
    }

    struct aes_ctx_t *en = aes_ctx();
    struct aes_ctx_t *de = aes_ctx();

    unsigned char key_data[] = {127, 128, 129, 130, 131, 132, 133, 134,
                                135, 136, 137, 138, 139, 140, 141, 142};
    if (aes_init(en, de, key_data))
    {
        perror("Could not initialize AES cipher.\n");
        goto cleanup;
    }

    printf("Listening on port %s.\n", argv[1]);
    for (;;)
    {
        uint8_t plaintext[CONNECTION_DATA_MAX_SIZE];
        uint32_t plaintext_len;
        uint32_t packet_id;
        uint8_t key[PACKET_KEY_BYTE_SIZE];
        if (connection_receive_data_noalloc(server, &packet_id, key, plaintext, &plaintext_len))
        {
            perror("Could not receive data.\n");
            goto cleanup;
        }
        if (aes_init(en, de, key))
        {
            perror("Could not initialize AES cipher.\n");
            goto cleanup;
        }
        printf("Packet Id: %u\t Data size: %u", packet_id, plaintext_len);
        printHexLine("\t Key: ", key, PACKET_KEY_BYTE_SIZE);
        // atomic_thread_fence will both be a compiler barrier (disallowing the compiler to reorder
        // instructions across the barrier) and a CPU barrier for that given thread (disallowing
        // the CPU to reorder instructions across the barrier).
        atomic_thread_fence(memory_order_seq_cst);
        // Flushing the cache to minimize possible timing interference from previous cached
        // encryption runs.
        flush_cache();
        // Declaring input/output variables after the cache flush as the performance benefit might
        // help in reducing timing noise.
        uint8_t ciphertext[CONNECTION_DATA_MAX_SIZE + AES_BLOCK_SIZE];
        size_t ciphertext_len;

        // Will encrypt only the first block of the plaintext, mimicking Bernstein's approach.
        const uint8_t encryption_length =
            plaintext_len < AES_BLOCK_SIZE ? plaintext_len : AES_BLOCK_SIZE;

        const struct cycle_timer_t inbound_time = time_start();

        aes_encrypt(en, plaintext, encryption_length, ciphertext, &ciphertext_len);

        const struct cycle_timer_t outbound_time = time_end();
        atomic_thread_fence(memory_order_seq_cst);

        if (connection_respond_back(server, packet_id, ciphertext, inbound_time, outbound_time))
        {
            perror("Could not send back timing response.\n");
            goto cleanup;
        }
        printf("\t %ld.%ld -> %ld.%ld\n", inbound_time.t1, inbound_time.t2, outbound_time.t1,
               outbound_time.t2);
    }

cleanup:
    aes_clean(en);
    aes_clean(de);
    connection_cleanup(&server);
    return EXIT_FAILURE;
}
