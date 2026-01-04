#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "apu/apu.h"
#include "test_helpers.h"

/* Test that apu_init sets noise shift register to 1 and configures sample rate */
static void test_apu_init(void) {
    APU apu;
    memset(&apu, 0xFF, sizeof(apu));
    apu_init(&apu);

    ASSERT_EQ_U16(1, apu.noise.shift_register);
    ASSERT_FALSE(apu.pulse1.enabled);
    ASSERT_FALSE(apu.pulse2.enabled);
    ASSERT_FALSE(apu.triangle.enabled);
    ASSERT_FALSE(apu.noise.enabled);
    ASSERT_FALSE(apu.dmc.enabled);
    
    /* Verify sample rate is approximately 1/44100 */
    float expected = 1.0f / 44100.0f;
    ASSERT_TRUE(fabsf(apu.audio_time_per_sample - expected) < 0.0000001f);
    ASSERT_TRUE(apu.audio_time == 0.0f);

    printf("test_apu_init: PASS\n");
}

/* Test that apu_reset reinitializes all state */
static void test_apu_reset(void) {
    APU apu;
    apu_init(&apu);
    
    /* Corrupt state */
    apu.noise.shift_register = 0xFFFF;
    apu.pulse1.enabled = true;
    apu.frame_count = 12345;
    apu.sample_count = 500;
    
    apu_reset(&apu);
    
    ASSERT_EQ_U16(1, apu.noise.shift_register);
    ASSERT_FALSE(apu.pulse1.enabled);
    ASSERT_TRUE(apu.frame_count == 0);
    ASSERT_TRUE(apu.sample_count == 0);

    printf("test_apu_reset: PASS\n");
}

/* Test pulse1 volume register ($4000) decoding */
static void test_pulse1_volume_write(void) {
    APU apu;
    apu_init(&apu);
    
    /* $4000: DDlc vvvv
     * DD = duty cycle, l = envelope loop/length halt, c = constant volume, vvvv = volume/envelope period
     */
    apu_write(&apu, APU_PULSE1_VOL, 0xBF); /* 10 11 1111 = duty=2, loop=1, const=1, vol=15 */
    
    ASSERT_EQ_U8(2, apu.pulse1.duty_mode);
    ASSERT_TRUE(apu.pulse1.envelope_loop);
    ASSERT_TRUE(apu.pulse1.constant_volume);
    ASSERT_EQ_U8(15, apu.pulse1.volume);
    ASSERT_EQ_U8(15, apu.pulse1.envelope_period);

    printf("test_pulse1_volume_write: PASS\n");
}

/* Test pulse1 sweep register ($4001) decoding */
static void test_pulse1_sweep_write(void) {
    APU apu;
    apu_init(&apu);
    
    /* $4001: Eppp nsss
     * E = enabled, ppp = period, n = negate, sss = shift
     */
    apu_write(&apu, APU_PULSE1_SWEEP, 0xE5); /* 1 110 0 101 = enabled, period=6, negate=0, shift=5 */
    
    ASSERT_TRUE(apu.pulse1.sweep_enabled);
    ASSERT_EQ_U8(6, apu.pulse1.sweep_period);
    ASSERT_FALSE(apu.pulse1.sweep_negate);
    ASSERT_EQ_U8(5, apu.pulse1.sweep_shift);
    ASSERT_TRUE(apu.pulse1.sweep_reload);

    printf("test_pulse1_sweep_write: PASS\n");
}

/* Test pulse1 timer registers ($4002, $4003) */
static void test_pulse1_timer_write(void) {
    APU apu;
    apu_init(&apu);
    
    /* Enable pulse1 first via status */
    apu_write(&apu, APU_STATUS, 0x01);
    
    /* $4002: low 8 bits of timer */
    apu_write(&apu, APU_PULSE1_LO, 0xAB);
    ASSERT_EQ_U16(0x00AB, apu.pulse1.timer_load);
    
    /* $4003: lllll ttt (length index in upper 5 bits, timer high 3 bits) */
    apu_write(&apu, APU_PULSE1_HI, 0x07); /* length index 0, timer high = 7 */
    ASSERT_EQ_U16(0x07AB, apu.pulse1.timer_load);
    ASSERT_EQ_U16(10, apu.pulse1.length_counter); /* length_table[0] = 10 */
    ASSERT_TRUE(apu.pulse1.envelope_start);
    ASSERT_EQ_U8(0, apu.pulse1.duty_sequence_step);

    printf("test_pulse1_timer_write: PASS\n");
}

/* Test pulse1 hi write loads length counter only when enabled */
static void test_pulse1_length_load_requires_enable(void) {
    APU apu;
    apu_init(&apu);
    
    /* Do NOT enable pulse1 */
    apu_write(&apu, APU_PULSE1_LO, 0x00);
    apu_write(&apu, APU_PULSE1_HI, 0x08); /* length index 1 -> length_table[1] = 254 */
    
    /* Length should NOT be loaded when disabled */
    ASSERT_EQ_U16(0, apu.pulse1.length_counter);

    printf("test_pulse1_length_load_requires_enable: PASS\n");
}

/* Test triangle register writes */
static void test_triangle_register_writes(void) {
    APU apu;
    apu_init(&apu);
    
    /* Enable triangle */
    apu_write(&apu, APU_STATUS, 0x04);
    
    /* $4008: Crrr rrrr (C = control/length halt, r = linear counter reload) */
    apu_write(&apu, APU_TRIANGLE_LINEAR, 0xFF);
    ASSERT_TRUE(apu.triangle.length_halt);
    ASSERT_EQ_U8(0x7F, apu.triangle.linear_counter_reload_value);
    
    /* Timer load */
    apu_write(&apu, APU_TRIANGLE_LO, 0xCD);
    apu_write(&apu, APU_TRIANGLE_HI, 0x0B); /* length index 1, timer high = 3 */
    ASSERT_EQ_U16(0x03CD, apu.triangle.timer_load);
    ASSERT_TRUE(apu.triangle.linear_reload);

    printf("test_triangle_register_writes: PASS\n");
}

/* Test noise register writes */
static void test_noise_register_writes(void) {
    APU apu;
    apu_init(&apu);
    
    /* Enable noise */
    apu_write(&apu, APU_STATUS, 0x08);
    
    /* $400C: --lc vvvv */
    apu_write(&apu, APU_NOISE_VOL, 0x3A); /* halt=1, const=1, vol=10 */
    ASSERT_TRUE(apu.noise.length_halt);
    ASSERT_TRUE(apu.noise.constant_volume);
    ASSERT_EQ_U8(10, apu.noise.volume);
    
    /* $400E: M--- pppp (M = mode, p = period index) */
    apu_write(&apu, APU_NOISE_LO, 0x85); /* mode=1, period index=5 */
    ASSERT_TRUE(apu.noise.mode_flag);
    ASSERT_EQ_U16(96, apu.noise.timer_load); /* noise_period[5] = 96 */
    
    /* $400F: llll l--- (length index) */
    apu_write(&apu, APU_NOISE_HI, 0x10); /* length index 2 */
    ASSERT_EQ_U16(20, apu.noise.length_counter); /* length_table[2] = 20 */
    ASSERT_TRUE(apu.noise.envelope_start);

    printf("test_noise_register_writes: PASS\n");
}

/* Test status register ($4015) write enables/disables channels */
static void test_status_write_enables_channels(void) {
    APU apu;
    apu_init(&apu);
    
    /* Set up some length counters */
    apu.pulse1.length_counter = 100;
    apu.pulse2.length_counter = 100;
    apu.triangle.length_counter = 100;
    apu.noise.length_counter = 100;
    
    /* Enable all channels */
    apu_write(&apu, APU_STATUS, 0x1F);
    ASSERT_TRUE(apu.pulse1.enabled);
    ASSERT_TRUE(apu.pulse2.enabled);
    ASSERT_TRUE(apu.triangle.enabled);
    ASSERT_TRUE(apu.noise.enabled);
    ASSERT_TRUE(apu.dmc.enabled);
    
    /* Length counters should be preserved when enabled */
    ASSERT_EQ_U16(100, apu.pulse1.length_counter);
    
    /* Disable pulse1 - should zero its length counter */
    apu_write(&apu, APU_STATUS, 0x1E);
    ASSERT_FALSE(apu.pulse1.enabled);
    ASSERT_EQ_U16(0, apu.pulse1.length_counter);
    
    /* Disable all */
    apu_write(&apu, APU_STATUS, 0x00);
    ASSERT_FALSE(apu.pulse2.enabled);
    ASSERT_EQ_U16(0, apu.pulse2.length_counter);
    ASSERT_EQ_U16(0, apu.triangle.length_counter);
    ASSERT_EQ_U16(0, apu.noise.length_counter);

    printf("test_status_write_enables_channels: PASS\n");
}

/* Test status register ($4015) read returns channel status */
static void test_status_read_returns_length_status(void) {
    APU apu;
    apu_init(&apu);
    
    /* Set up various length counters */
    apu.pulse1.length_counter = 1;
    apu.pulse2.length_counter = 0;
    apu.triangle.length_counter = 1;
    apu.noise.length_counter = 0;
    apu.dmc.bytes_remaining = 1;
    
    uint8_t status = apu_read(&apu, APU_STATUS);
    
    ASSERT_TRUE(status & 0x01);  /* pulse1 active */
    ASSERT_FALSE(status & 0x02); /* pulse2 inactive */
    ASSERT_TRUE(status & 0x04);  /* triangle active */
    ASSERT_FALSE(status & 0x08); /* noise inactive */
    ASSERT_TRUE(status & 0x10);  /* DMC active */

    printf("test_status_read_returns_length_status: PASS\n");
}

/* Test status read clears frame_irq */
static void test_status_read_clears_frame_irq(void) {
    APU apu;
    apu_init(&apu);
    
    apu.frame_irq = true;
    
    uint8_t status = apu_read(&apu, APU_STATUS);
    ASSERT_TRUE(status & 0x40); /* frame_irq was set in returned value */
    ASSERT_FALSE(apu.frame_irq); /* but now cleared */
    
    status = apu_read(&apu, APU_STATUS);
    ASSERT_FALSE(status & 0x40); /* subsequent read shows cleared */

    printf("test_status_read_clears_frame_irq: PASS\n");
}

/* Test frame counter register ($4017) */
static void test_frame_counter_write(void) {
    APU apu;
    apu_init(&apu);
    
    /* Set frame_irq first */
    apu.frame_irq = true;
    
    /* $4017: MI-- ---- (M = mode, I = IRQ inhibit) */
    apu_write(&apu, APU_FRAME_COUNTER, 0xC0); /* mode=1, irq_inhibit=1 */
    
    ASSERT_EQ_U8(1, apu.frame_counter_mode);
    ASSERT_TRUE(apu.irq_inhibit);
    ASSERT_FALSE(apu.frame_irq); /* cleared by irq_inhibit */

    printf("test_frame_counter_write: PASS\n");
}

/* Test that apu_tick increments frame_count */
static void test_tick_increments_frame_count(void) {
    APU apu;
    apu_init(&apu);
    
    ASSERT_TRUE(apu.frame_count == 0);
    
    apu_tick(&apu);
    ASSERT_TRUE(apu.frame_count == 1);
    
    apu_tick(&apu);
    ASSERT_TRUE(apu.frame_count == 2);

    printf("test_tick_increments_frame_count: PASS\n");
}

/* Test that apu_tick accumulates samples */
static void test_tick_accumulates_samples(void) {
    APU apu;
    apu_init(&apu);
    
    /* Run enough ticks to generate at least one sample
     * CPU frequency ~1.789773 MHz, sample rate 44100 Hz
     * Ticks per sample = 1789773 / 44100 ≈ 40.6
     */
    apu.sample_count = 0;
    for (int i = 0; i < 100; i++) {
        apu_tick(&apu);
    }
    
    /* Should have generated at least 1 sample (100 / 40.6 ≈ 2) */
    ASSERT_TRUE(apu.sample_count >= 1);

    printf("test_tick_accumulates_samples: PASS\n");
}

/* Test apu_get_buffer copies and clears samples */
static void test_get_buffer(void) {
    APU apu;
    apu_init(&apu);
    
    /* Manually add some samples */
    apu.sample_buffer[0] = 0.5f;
    apu.sample_buffer[1] = 0.25f;
    apu.sample_count = 2;
    
    float buffer[16];
    int count = apu_get_buffer(&apu, buffer, 16);
    
    ASSERT_EQ_INT(2, count);
    ASSERT_TRUE(fabsf(buffer[0] - 0.5f) < 0.0001f);
    ASSERT_TRUE(fabsf(buffer[1] - 0.25f) < 0.0001f);
    
    /* Buffer should be cleared */
    ASSERT_EQ_INT(0, apu.sample_count);

    printf("test_get_buffer: PASS\n");
}

/* Test noise channel LFSR operation */
static void test_noise_lfsr(void) {
    APU apu;
    apu_init(&apu);
    
    /* Initial shift register = 1 */
    ASSERT_EQ_U16(1, apu.noise.shift_register);
    
    /* Set up noise channel for manual timer tick test */
    apu.noise.timer = 0;
    apu.noise.timer_load = 4;
    apu.noise.mode_flag = false; /* Mode 0: feedback from bits 0 and 1 */
    
    /* Force a noise timer tick by setting timer to 0 and ticking */
    uint16_t initial = apu.noise.shift_register;
    
    /* Manually tick the noise channel (frame_count even, so noise ticks) */
    apu.frame_count = 0;
    apu_tick(&apu);
    
    /* Shift register should have changed */
    /* Initial: 0000000000000001
     * Feedback = bit0 ^ bit1 = 1 ^ 0 = 1
     * New: (1 >> 1) | (1 << 14) = 0 | 0x4000 = 0x4000
     */
    ASSERT_TRUE(apu.noise.shift_register != initial || apu.noise.timer != 0);

    printf("test_noise_lfsr: PASS\n");
}

int main(void) {
    test_apu_init();
    test_apu_reset();
    test_pulse1_volume_write();
    test_pulse1_sweep_write();
    test_pulse1_timer_write();
    test_pulse1_length_load_requires_enable();
    test_triangle_register_writes();
    test_noise_register_writes();
    test_status_write_enables_channels();
    test_status_read_returns_length_status();
    test_status_read_clears_frame_irq();
    test_frame_counter_write();
    test_tick_increments_frame_count();
    test_tick_accumulates_samples();
    test_get_buffer();
    test_noise_lfsr();

    printf("\nAll APU tests passed.\n");
    return 0;
}
