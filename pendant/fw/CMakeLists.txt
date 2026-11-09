cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lifeos_pendant)

target_sources(app PRIVATE src/main.c src/audio.c src/store.c src/marker.c src/clock.c src/ble.c src/scenario.c src/supervisor.c src/ota.c
	lib/ima_adpcm/ima_adpcm.c lib/audio_dsp/audio_dsp.c lib/chunk/chunk.c lib/ring/ring.c lib/health/health.c lib/sync/sync.c
	lib/aad/aad.c lib/t5838/t5838.c lib/provision/provision.c)
target_sources_ifdef(CONFIG_PENDANT_CRYPTO_BENCH app PRIVATE src/crypto_bench.c)
target_include_directories(app PRIVATE lib/ima_adpcm lib/audio_dsp lib/chunk lib/ring lib/health lib/sync lib/aad lib/t5838 lib/provision)

if(CONFIG_PENDANT_REQUIRE_PROVISIONING)
  if(NOT CONFIG_PENDANT_BLE_SECURE)
    message(FATAL_ERROR "PENDANT_REQUIRE_PROVISIONING needs PENDANT_BLE_SECURE=y")
  endif()
  if(CONFIG_PENDANT_AUTO_RECORD)
    message(FATAL_ERROR "PENDANT_REQUIRE_PROVISIONING forbids PENDANT_AUTO_RECORD=y")
  endif()
  if(NOT CONFIG_PENDANT_BLE OR NOT CONFIG_BT_SMP OR NOT CONFIG_BT_SETTINGS OR
     NOT CONFIG_BT_PRIVACY OR NOT CONFIG_BT_FILTER_ACCEPT_LIST OR
     NOT CONFIG_MCUMGR_TRANSPORT_BT_PERM_RW_AUTHEN)
    message(FATAL_ERROR
      "PENDANT_REQUIRE_PROVISIONING needs BLE, SMP, settings, privacy, "
      "a filter accept list, and authenticated MCUmgr")
  endif()
endif()

if(CONFIG_PENDANT_RELEASE)
  if(NOT CONFIG_MCUBOOT_SIGNATURE_KEY_FILE OR
     CONFIG_MCUBOOT_SIGNATURE_KEY_FILE MATCHES ".*/bootloader/mcuboot/root-ed25519\\.pem")
    message(FATAL_ERROR "PENDANT_RELEASE must use a product MCUboot signing key, not the NCS dev key")
  endif()
  if(NOT CONFIG_MCUBOOT_BOOTLOADER_NO_DOWNGRADE)
    message(FATAL_ERROR "PENDANT_RELEASE needs MCUboot downgrade prevention")
  endif()
  if(NOT CONFIG_PENDANT_WDT)
    message(FATAL_ERROR "PENDANT_RELEASE needs PENDANT_WDT=y")
  endif()
  if(NOT CONFIG_PENDANT_BLE_SECURE OR NOT CONFIG_PENDANT_REQUIRE_PROVISIONING)
    message(FATAL_ERROR "PENDANT_RELEASE needs secure BLE and provisioning")
  endif()
endif()
