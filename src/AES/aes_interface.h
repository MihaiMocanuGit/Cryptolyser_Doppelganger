#ifndef CRYPTOLYSER_DOPPELGANGER_AES_AES_H_
#define CRYPTOLYSER_DOPPELGANGER_AES_AES_H_

#include <stdint.h>
#include <stdio.h>

#define AES_BLOCK_SIZE 16

struct aes_ctx_t;

struct aes_ctx_t *aes_ctx(void);

int aes_init(struct aes_ctx_t *encrypt_ctx, struct aes_ctx_t *decrypt_ctx,
             uint8_t key[static AES_BLOCK_SIZE]);

/// Encrypt len bytes of data. All data going in & out is considered binary
/// @param encrypt_ctx [in/out]
/// @param plaintext [in]
/// @param plaintext_len [in]
/// @param ciphertext [out] Make sure that is has enough space to hold the ciphertext
/// @param ciphertext_len [out]
/// @return
void aes_encrypt(struct aes_ctx_t *encrypt_ctx, uint8_t *plaintext, size_t plaintext_len,
                 uint8_t *ciphertext, size_t *ciphertext_len);

void aes_decrypt(struct aes_ctx_t *decrypt_ctx, uint8_t *ciphertext, size_t ciphertext_len,
                 uint8_t *plaintext, size_t *plaintext_len);

void aes_clean(struct aes_ctx_t *ctx);

void aes_log_status(FILE *stream);

#endif // CRYPTOLYSER_DOPPELGANGER_AES_AES_H_
