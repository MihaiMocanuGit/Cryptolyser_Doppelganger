#include "AES/aes_interface.h"
#include "CacheFlush/cache_flush.h"
#include "ConnectionHandler/connection_handler.h"
#include "Cryptolyser_Common/connection_data_types.h"
#include "Cryptolyser_Common/cycle_timer.h"

#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void printHexLine(const char *line_label, uint8_t *input, uint32_t len)
{
    printf("%s", line_label);
    for (uint32_t i = 0; i < len; ++i)
    {
        printf("%02X ", (unsigned int)input[i]);
    }
}

static void fillRandom(uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; ++i)
        data[i] = rand() % 256;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "Incorrect program parameter: <PORT>\n");
        return EXIT_FAILURE;
    }
    srand(time(NULL));

    aes_log_status(stdout);

    struct connection_t *server;
    if (connection_init(&server, atoi(argv[1])))
    {
        perror("Could not initialize connection.\n");
        connection_cleanup(&server);
        return EXIT_FAILURE;
    }

    uint8_t key_buffers[2][PACKET_KEY_BYTE_SIZE] = {{0}};
    unsigned current_buffer = 0;
    uint8_t *key = key_buffers[current_buffer % 2];

    struct aes_ctx_t *en = aes_ctx();
    struct aes_ctx_t *de = aes_ctx();
    if (aes_init(en, de, key))
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
        uint8_t *packet_key = key_buffers[(current_buffer + 1) % 2];
        if (connection_receive_data_noalloc(server, &packet_id, packet_key, plaintext,
                                            &plaintext_len))
        {
            perror("Could not receive data.\n");
            goto cleanup;
        }
        if (memcmp(key, packet_key, PACKET_KEY_BYTE_SIZE))
        {
            current_buffer++;
            key = packet_key;

            aes_clean(en);
            aes_clean(de);
            en = aes_ctx();
            de = aes_ctx();
            if (aes_init(en, de, key))
            {
                perror("Could not reinitialize AES cipher.\n");
                goto cleanup;
            }
        }
        uint8_t iv[AES_BLOCK_BYTE_SIZE];
        fillRandom(iv, AES_BLOCK_BYTE_SIZE);
        aes_set_iv(en, iv);
        aes_set_iv(de, iv);

        printf("Packet Id: %u\t Data size: %u", packet_id, plaintext_len);
        printHexLine("\t Key: ", key, PACKET_KEY_BYTE_SIZE);
        printHexLine("\t IV: ", iv, AES_BLOCK_BYTE_SIZE);
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

        if (connection_respond_back(server, packet_id, ciphertext, inbound_time, outbound_time, iv))
        {
            perror("Could not send back timing response.\n");
            goto cleanup;
        }
        printf("\t %ld.%ld -> %ld.%ld\n", inbound_time.t1, inbound_time.t2, outbound_time.t1,
               outbound_time.t2);
        uint8_t decrypted_plaintext[CONNECTION_DATA_MAX_SIZE];
        size_t decrypted_len;
        aes_decrypt(de, ciphertext, ciphertext_len, decrypted_plaintext, &decrypted_len);
        assert(plaintext_len <= decrypted_len);
        assert(memcmp(decrypted_plaintext, plaintext, plaintext_len));
    }

cleanup:
    aes_clean(en);
    aes_clean(de);
    connection_cleanup(&server);
    return EXIT_FAILURE;
}
