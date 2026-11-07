#include <stdint.h>
#include <psxspu.h>

#include "driving.h"
#include "engine_sound.h"

#define ENGINE_ADDRESS 0x1010
#define ENGINE_IDLE_PITCH 0x500

extern const uint32_t rally_engine[];
extern const uint32_t rally_engine_size;

void engine_sound_init(void) {
	SpuInit();
	SpuSetCommonMasterVolume(0x3fff, 0x3fff);
	SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
	SpuSetTransferStartAddr(ENGINE_ADDRESS);
	SpuWrite(rally_engine, rally_engine_size);
	SpuIsTransferCompleted(SPU_TRANSFER_WAIT);

	SpuSetVoiceStartAddr(0, ENGINE_ADDRESS);
	SPU_CH_LOOP_ADDR(0) = getSPUAddr(ENGINE_ADDRESS);
	SpuSetVoicePitch(0, ENGINE_IDLE_PITCH);
	SpuSetVoiceVolume(0, 0x2400, 0x2400);
	SPU_CH_ADSR1(0) = 0x00ff;
	SPU_CH_ADSR2(0) = 0x0000;
	SpuSetKey(1, 1);
}

void engine_sound_update(int speed, int throttle) {
	int magnitude = speed < 0 ? -speed : speed;
	int pitch = ENGINE_IDLE_PITCH + magnitude * 0x800 / DRIVING_FORWARD_MAX;
	int volume = 0x2400 + magnitude * 0x800 / DRIVING_FORWARD_MAX;

	if (throttle)
		pitch += 0x80;
	SpuSetVoicePitch(0, pitch);
	SpuSetVoiceVolume(0, volume, volume);
}
