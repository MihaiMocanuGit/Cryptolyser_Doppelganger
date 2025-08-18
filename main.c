#include "AES/aes_interface.h"
#include "CacheFlush/cache_flush.h"
#include "ConnectionHandler/connection_handler.h"
#include "Cryptolyser_Common/connection_data_types.h"
#include "Cryptolyser_Common/cycle_timer.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void print_hex_line(const char *line_label, const uint8_t *input, uint32_t len);

static void parse_key(const char *keyStr, uint8_t key[PACKET_KEY_SIZE]);

static void fill_random(uint8_t data[static 1], size_t len);

struct in_out_time_t
{
    struct cycle_timer_t inbound;
    struct cycle_timer_t outbound;
};

static int construct_aes_ctx(struct aes_ctx_t *en[1], struct aes_ctx_t *de[1],
                             const uint8_t key[PACKET_KEY_SIZE], enum packet_type_e aes_type);

static struct in_out_time_t
    encrypt_and_time_ecb(struct aes_ctx_t *en, const uint8_t plaintext[static 1], size_t plain_len,
                         uint8_t ciphertext[static AES_BLOCK_SIZE], size_t cipher_len[1]);

static struct in_out_time_t
    encrypt_and_time_cbc(struct aes_ctx_t *en, const uint8_t plaintext[static 1], size_t plain_len,
                         uint8_t ciphertext[static AES_BLOCK_SIZE], size_t cipher_len[1],
                         const uint8_t iv[AES_BLOCK_SIZE]);

static struct in_out_time_t
    encrypt_and_time_ctr(struct aes_ctx_t *en, const uint8_t plaintext[static 1], size_t plain_len,
                         uint8_t ciphertext[static AES_BLOCK_SIZE], size_t cipher_len[1],
                         const uint8_t iv[AES_BLOCK_SIZE]);

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
    // a buffer holding the currently used key and the key received from network.
    uint8_t key_data_buffer[2][PACKET_KEY_SIZE] = {{0}};
    uint8_t buffer_flag = 0; // will only take two values: 0 and 1
    const uint8_t *key = key_data_buffer[buffer_flag];
    uint8_t *next_key = key_data_buffer[buffer_flag ^ 1];

    srand(time(NULL));

    struct aes_ctx_t *en_ecb, *de_ecb;
    construct_aes_ctx(&en_ecb, &de_ecb, key, packet_type_e_ECB);
    struct aes_ctx_t *en_cbc, *de_cbc;
    construct_aes_ctx(&en_cbc, &de_cbc, key, packet_type_e_CBC);
    struct aes_ctx_t *en_ctr, *de_ctr;
    construct_aes_ctx(&en_ctr, &de_ctr, key, packet_type_e_CTR);

    printf("Listening on port %s.\n", argv[1]);
    for (;;)
    {
        uint8_t plaintext[PACKET_BYTE_DATA_SIZE];
        uint32_t plaintext_len;
        uint32_t packet_id;
        enum packet_type_e aes_type;
        if (connection_receive_data_noalloc(server, &packet_id, plaintext, &plaintext_len,
                                            &aes_type, next_key))
        {
            perror("Could not receive data.\n");
            goto cleanup;
        }
        if (memcmp(key, next_key, PACKET_KEY_SIZE))
        {
            buffer_flag ^= 1;
            key = key_data_buffer[buffer_flag];
            next_key = key_data_buffer[buffer_flag ^ 1];
            // reinit the ctx only when the received key differs from the previous one.
            construct_aes_ctx(&en_ecb, &de_ecb, key, packet_type_e_ECB);
            construct_aes_ctx(&en_cbc, &de_cbc, key, packet_type_e_CBC);
            construct_aes_ctx(&en_ctr, &de_ctr, key, packet_type_e_CTR);
        }
#ifndef NDEBUG
        printf("Packet Id: %u\t Mode: %s\t Data size: %u\n", packet_id, packet_type_names[aes_type],
               plaintext_len);
        print_hex_line("\tKey: ", key, PACKET_KEY_SIZE);
        printf("\n");

#endif

        uint8_t ciphertext[PACKET_BYTE_DATA_SIZE + AES_BLOCK_SIZE];
        size_t ciphertext_len = 0;
        // Will encrypt only the first block of the plaintext, mimicking Bernstein's approach.
        const uint8_t encryption_length =
            plaintext_len < AES_BLOCK_SIZE ? plaintext_len : AES_BLOCK_SIZE;

        uint8_t iv[PACKET_AES_BLOCK_SIZE];
        if (aes_type != packet_type_e_ECB)
            fill_random(iv, PACKET_AES_BLOCK_SIZE);

        struct in_out_time_t timing_result;
        // Using different functions for every AES block mode in order to easily keep a constant
        // context through the whole application lifetime.
        //
        // It has been observed that a constantly chaning ctx results in a different memory
        // behaviour (i.e doppelganger vs victim). This behaviour difference must be protected.
        //
        // The downside of code duplication is a necessay evil.
        switch (aes_type)
        {
            case packet_type_e_ECB:
                timing_result = encrypt_and_time_ecb(en_ecb, plaintext, encryption_length,
                                                     ciphertext, &ciphertext_len);
                break;
            case packet_type_e_CBC:
                timing_result = encrypt_and_time_cbc(en_ecb, plaintext, encryption_length,
                                                     ciphertext, &ciphertext_len, iv);
                break;
            case packet_type_e_CTR:
                timing_result = encrypt_and_time_ctr(en_ctr, plaintext, encryption_length,
                                                     ciphertext, &ciphertext_len, iv);
                break;
            default:
                fprintf(stderr, "Do not know how to encrypt AES type: %u (%s)\n", aes_type,
                        (aes_type < packet_type_e_COUNT) ? packet_type_names[aes_type]
                                                         : "Corrupted value!");
                memset(&timing_result, 0, sizeof(timing_result));
                break;
        }

        if (connection_respond_back(server, packet_id, ciphertext, timing_result.inbound,
                                    timing_result.outbound, iv))
        {
            perror("Could not send back timing response.\n");
            goto cleanup;
        }
#ifndef NDEBUG
        printf("\t %ld.%ld -> %ld.%ld\n", timing_result.inbound.t1, timing_result.inbound.t2,
               timing_result.outbound.t1, timing_result.outbound.t2);
#endif
    }

cleanup:
    aes_ctx_clean(en_ecb);
    aes_ctx_clean(de_ecb);
    connection_cleanup(&server);
    return EXIT_FAILURE;
}

static void print_hex_line(const char *line_label, const uint8_t *input, uint32_t len)
{
    printf("%s", line_label);
    for (uint32_t i = 0; i < len; ++i)
        printf("%02X ", (unsigned int)input[i]);
}

static void parse_key(const char *keyStr, uint8_t key[PACKET_KEY_SIZE])
{
    char *keyTok = strdup(keyStr);
    char *value = strtok(keyTok, " ");
    unsigned index = 0;
    while (value && index < PACKET_KEY_SIZE)
    {
        key[index++] = strtoul(value, NULL, 16);
        value = strtok(NULL, " ");
    }
    free(keyTok);
}

static void fill_random(uint8_t data[static 1], size_t len)
{
    for (size_t i = 0; i < len; ++i)
        data[i] = rand() % 256; // TODO: can we just trim the int value to uint8_t ?
}

static int construct_aes_ctx(struct aes_ctx_t *en[1], struct aes_ctx_t *de[1],
                             const uint8_t key[PACKET_KEY_SIZE], enum packet_type_e aes_type)
{
    *en = aes_ctx();
    *de = aes_ctx();
    switch (aes_type)
    {
        case packet_type_e_ECB:
            if (aes_ecb_init(*en, *de, key))
                goto cleanup;
            break;
        case packet_type_e_CBC:
            if (aes_cbc_init(*en, *de, key))
                goto cleanup;
            break;
        case packet_type_e_CTR:
            if (aes_ctr_init(*en, *de, key))
                goto cleanup;
            break;
        default:
            fprintf(stderr, "Do not know how to init ctx for AES type: %u (%s)\n", aes_type,
                    (aes_type < packet_type_e_COUNT) ? packet_type_names[aes_type]
                                                     : "Corrupted value!");
            break;
    }
    return 0;
cleanup:
    *en = NULL;
    *de = NULL;
    fprintf(stderr, "Could not initialize AES context. Internal failure.\n");
    return -1;
}

// Macro that defines the common code inside all encrypt_and_time calls, before the encryption call
// to the specific mode.
#define ENCRYPT_AND_TIME_START                                                                     \
    /* atomic_thread_fence will both be a compiler barrier (disallowing the compiler to reorder*/  \
    /* instructions across the barrier) and a CPU barrier for that given thread (disallowing*/     \
    /* the CPU to reorder instructions across the barrier). */                                     \
    atomic_thread_fence(memory_order_seq_cst);                                                     \
    /* Flushing the cache to minimize possible timing interference from previous cached*/          \
    /* encryption runs.*/                                                                          \
    flush_cache();                                                                                 \
    const struct cycle_timer_t inbound_time = time_start();

// Macro that defines the common code inside all encrypt_and_time calls, after the encryption call
// to the specific mode.
#define ENCRYPT_AND_TIME_END                                                                       \
    const struct cycle_timer_t outbound_time = time_end();                                         \
    atomic_thread_fence(memory_order_seq_cst);                                                     \
    return (struct in_out_time_t) {.inbound = inbound_time, .outbound = outbound_time};

static struct in_out_time_t
    encrypt_and_time_ecb(struct aes_ctx_t *en, const uint8_t plaintext[static 1], size_t plain_len,
                         uint8_t ciphertext[static AES_BLOCK_SIZE], size_t cipher_len[1])
{
    ENCRYPT_AND_TIME_START
    aes_ecb_encrypt(en, plaintext, plain_len, ciphertext, cipher_len);
    ENCRYPT_AND_TIME_END
}

static struct in_out_time_t
    encrypt_and_time_cbc(struct aes_ctx_t *en, const uint8_t plaintext[static 1], size_t plain_len,
                         uint8_t ciphertext[static AES_BLOCK_SIZE], size_t cipher_len[1],
                         const uint8_t iv[AES_BLOCK_SIZE])
{
    aes_cbc_set_iv(en, iv);
    ENCRYPT_AND_TIME_START
    aes_cbc_encrypt(en, plaintext, plain_len, ciphertext, cipher_len);
    ENCRYPT_AND_TIME_END
}

static struct in_out_time_t
    encrypt_and_time_ctr(struct aes_ctx_t *en, const uint8_t plaintext[static 1], size_t plain_len,
                         uint8_t ciphertext[static AES_BLOCK_SIZE], size_t cipher_len[1],
                         const uint8_t iv[AES_BLOCK_SIZE])
{
    aes_ctr_set_iv(en, iv);
    ENCRYPT_AND_TIME_START
    aes_ctr_encrypt(en, plaintext, plain_len, ciphertext, cipher_len);
    ENCRYPT_AND_TIME_END
}
