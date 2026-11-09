/* #26: what does encrypting a record cost on the nRF54L15 (CRACEN via PSA)? AES-128-GCM over one
 * 8 s record (32000 B + 40 B header authenticated), wall clock and an energy estimate. */
#include <psa/crypto.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(cryptob, LOG_LEVEL_INF);

static uint8_t plain[32000], enc[32000 + 16], dec[32000], aad[40];

void crypto_bench(void)
{
	psa_key_id_t key;
	psa_key_attributes_t at = PSA_KEY_ATTRIBUTES_INIT;
	uint8_t k[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}, nonce[12] = {0};
	size_t olen;
	psa_status_t st = psa_crypto_init();

	psa_set_key_type(&at, PSA_KEY_TYPE_AES);
	psa_set_key_bits(&at, 128);
	psa_set_key_usage_flags(&at, PSA_KEY_USAGE_ENCRYPT | PSA_KEY_USAGE_DECRYPT);
	psa_set_key_algorithm(&at, PSA_ALG_GCM);
	st = st ? st : psa_import_key(&at, k, sizeof(k), &key);
	LOG_INF("psa init/import: %d", (int)st);
	for (size_t i = 0; i < sizeof(plain); i++) {
		plain[i] = i * 31;
	}
	for (int run = 0; run < 3 && !st; run++) {
		uint32_t t0 = k_cycle_get_32(), t1, t2;

		nonce[0] = run;
		st = psa_aead_encrypt(key, PSA_ALG_GCM, nonce, 12, aad, sizeof(aad), plain, sizeof(plain), enc,
				      sizeof(enc), &olen);
		t1 = k_cycle_get_32();
		st = st ? st : psa_aead_decrypt(key, PSA_ALG_GCM, nonce, 12, aad, sizeof(aad), enc, olen, dec, sizeof(dec), &olen);
		t2 = k_cycle_get_32();
		LOG_INF("AES-128-GCM 32000 B: encrypt %u us, decrypt+verify %u us, status %d, roundtrip %s", k_cyc_to_us_floor32(t1 - t0),
			k_cyc_to_us_floor32(t2 - t1), (int)st, !st && !memcmp(plain, dec, sizeof(plain)) ? "ok" : "BAD");
	}
}
