#include <stdint.h>
#include <psxspu.h>

#include "driving.h"
#include "engine_sound.h"

#define ENGINE_ADDRESS 0x1010
#define REVERSE_ADDRESS 0x1050
#define SAMPLE_BLOCKS 4
#define SAMPLES_PER_BLOCK 28

static uint32_t sample_data[SAMPLE_BLOCKS * 4];

static int triangle(int phase, int period) {
	int half = period / 2;
	return phase < half ? -7 + phase * 14 / half : 7 - (phase - half) * 14 / half;
}

static void upload_loop(uint32_t address, int period) {
	uint8_t *bytes = (uint8_t *) sample_data;

	for (int block = 0; block < SAMPLE_BLOCKS; block++) {
		bytes[block * 16] = 1;
		bytes[block * 16 + 1] = block == 0 ? 4 : (block == SAMPLE_BLOCKS - 1 ? 3 : 0);
		for (int i = 0; i < 14; i++) {
			int index = block * SAMPLES_PER_BLOCK + i * 2;
			int a = triangle(index % period, period);
			int b = triangle((index + 1) % period, period);
			bytes[block * 16 + 2 + i] = (uint8_t) ((a & 15) | ((b & 15) << 4));
		}
	}
	SpuSetTransferStartAddr(address);
	SpuWrite(sample_data, sizeof(sample_data));
	SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
}

static void start_voice(int channel, uint32_t address, int pitch, int volume) {
	SpuSetVoiceStartAddr(channel, address);
	SPU_CH_LOOP_ADDR(channel) = getSPUAddr(address);
	SpuSetVoicePitch(channel, pitch);
	SpuSetVoiceVolume(channel, volume, volume);
	SPU_CH_ADSR1(channel) = 0x00ff;
	SPU_CH_ADSR2(channel) = 0x0000;
}

void engine_sound_init(void) {
	SpuInit();
	SpuSetCommonMasterVolume(0x3fff, 0x3fff);
	SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
	upload_loop(ENGINE_ADDRESS, 112);
	upload_loop(REVERSE_ADDRESS, 14);
	start_voice(0, ENGINE_ADDRESS, 0x700, 0x2200);
	start_voice(1, REVERSE_ADDRESS, 0x700, 0);
	SpuSetKey(1, 3);
}

void engine_sound_update(int speed, int throttle, uint32_t tick) {
	int magnitude = speed < 0 ? -speed : speed;
	int pitch = 0x700 + magnitude * 0xd00 / DRIVING_FORWARD_MAX;
	int volume = 0x2200 + magnitude * 0x600 / DRIVING_FORWARD_MAX;
	int reverse_volume = speed < 0 && (tick / 30) % 2 == 0 ? 0x1400 : 0;

	if (throttle && magnitude < DRIVING_FORWARD_MAX)
		pitch += 0x100;
	SpuSetVoicePitch(0, pitch);
	SpuSetVoiceVolume(0, volume, volume);
	SpuSetVoiceVolume(1, reverse_volume, reverse_volume);
}
