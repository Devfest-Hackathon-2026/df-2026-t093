# Host tests for the pure-C libraries. `make test` (about 2.5 min, ring dominates), `make quick` (~30 s).
CC ?= cc
CFLAGS = -std=c99 -O2 -g -Wall -Wextra -Wsign-compare -Werror -Ichunk -Iima_adpcm -Iring -Ibitrate -Ipowerstate -Irtc -It5838 -Iaad -Ihealth -Isync -Iprovision
OUT = .build

.PHONY: test quick ima chunk ring ring-quick bitrate powerstate rtc t5838 aad health sync rx audio_dsp provision clean
test: ima chunk ring bitrate powerstate rtc t5838 aad health sync rx audio_dsp provision
quick: ima chunk ring-quick bitrate powerstate rtc t5838 aad health sync-quick rx audio_dsp provision

$(OUT):
	mkdir -p $(OUT)

ima: | $(OUT)
	$(CC) $(CFLAGS) -DMAX_ERR=64 -o $(OUT)/ima ima_adpcm/test_ima_adpcm.c ima_adpcm/ima_adpcm.c && $(OUT)/ima

chunk: | $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/chunk chunk/test_chunk.c chunk/chunk.c && $(OUT)/chunk

$(OUT)/ring: ring/test_ring.c ring/ring.c chunk/chunk.c | $(OUT)
	$(CC) $(CFLAGS) -o $@ ring/test_ring.c ring/ring.c chunk/chunk.c
ring: $(OUT)/ring
	$(OUT)/ring
ring-quick: $(OUT)/ring
	$(OUT)/ring quick

bitrate: | $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/bitrate bitrate/test_bitrate.c bitrate/bitrate.c ring/ring.c chunk/chunk.c && $(OUT)/bitrate

powerstate: | $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/powerstate powerstate/test_powerstate.c powerstate/powerstate.c && $(OUT)/powerstate


rtc: | $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/rtc rtc/test_rv3032.c rtc/rv3032.c && $(OUT)/rtc

t5838: | $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/t5838 t5838/test_t5838.c t5838/t5838.c && $(OUT)/t5838

aad: | $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/aad aad/test_aad.c aad/aad.c t5838/t5838.c chunk/chunk.c && $(OUT)/aad

health: | $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/health health/test_health.c health/health.c && $(OUT)/health

sync: | $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/sync sync/test_sync.c sync/sync.c ring/ring.c chunk/chunk.c && $(OUT)/sync

sync-quick: | $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/sync sync/test_sync.c sync/sync.c ring/ring.c chunk/chunk.c && $(OUT)/sync quick

audio_dsp: | $(OUT)
	$(CC) $(CFLAGS) -Iaudio_dsp -o $(OUT)/audio_dsp audio_dsp/test_audio_dsp.c audio_dsp/audio_dsp.c -lm && $(OUT)/audio_dsp

provision: | $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/provision provision/test_provision.c provision/provision.c && $(OUT)/provision

rx:
	cd ../../tools && python3 test_rx.py

clean:
	rm -rf $(OUT)
