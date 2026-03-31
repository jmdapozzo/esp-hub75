// SPDX-FileCopyrightText: 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

// @file fm6126a.cpp
// @brief FM6126A/ICN2038S shift driver initialization

// Based on https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA

#include "driver_init.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <initializer_list>

namespace hub75 {

static const char *const TAG = "FM6126A";

#define CLK_PULSE \
  gpio_set_level((gpio_num_t) pins.clk, 1); \
  gpio_set_level((gpio_num_t) pins.clk, 0);

void DriverInit::fm6126a_init(const Hub75Pins &pins, uint16_t pixels_per_row) {
  ESP_LOGI(TAG, "Initializing FM6126A shift driver (pixels_per_row=%d)", pixels_per_row);

  // Control register values
  static constexpr bool REG1[16] = {false, false, false, false, false, true,  true,  true,
                                    true,  true,  true,  false, false, false, false, false};  // Global brightness
  static constexpr bool REG2[16] = {false, false, false, false, false, false, false, false,
                                    false, true,  false, false, false, false, false, false};  // Enable output

  // 1. Configure all pins as GPIO output
  for (uint8_t pin : {pins.r1, pins.r2, pins.g1, pins.g2, pins.b1, pins.b2, pins.clk, pins.lat, pins.oe}) {
    gpio_reset_pin((gpio_num_t) pin);
    gpio_set_direction((gpio_num_t) pin, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t) pin, 0);
  }

  // 2. Disable display (OE high)
  gpio_set_level((gpio_num_t) pins.oe, 1);

  // 3. Send REG1 (latch at pixels_per_row - 12)
  for (int i = 0; i < pixels_per_row; i++) {
    for (uint8_t pin : {pins.r1, pins.r2, pins.g1, pins.g2, pins.b1, pins.b2}) {
      gpio_set_level((gpio_num_t) pin, REG1[i % 16]);
    }
    if (i > pixels_per_row - 12) {
      gpio_set_level((gpio_num_t) pins.lat, 1);
    }
    CLK_PULSE;
  }
  gpio_set_level((gpio_num_t) pins.lat, 0);

  // 4. Send REG2 (latch at pixels_per_row - 13)
  for (int i = 0; i < pixels_per_row; i++) {
    for (uint8_t pin : {pins.r1, pins.r2, pins.g1, pins.g2, pins.b1, pins.b2}) {
      gpio_set_level((gpio_num_t) pin, REG2[i % 16]);
    }
    if (i > pixels_per_row - 13) {
      gpio_set_level((gpio_num_t) pins.lat, 1);
    }
    CLK_PULSE;
  }
  gpio_set_level((gpio_num_t) pins.lat, 0);

  // 5. Blank display data
  for (uint8_t pin : {pins.r1, pins.r2, pins.g1, pins.g2, pins.b1, pins.b2}) {
    gpio_set_level((gpio_num_t) pin, 0);
  }
  for (int i = 0; i < pixels_per_row; i++) {
    CLK_PULSE;
  }

  // 6. Latch and enable display
  gpio_set_level((gpio_num_t) pins.lat, 1);
  CLK_PULSE;
  gpio_set_level((gpio_num_t) pins.lat, 0);
  gpio_set_level((gpio_num_t) pins.oe, 0);
  CLK_PULSE;

  ESP_LOGI(TAG, "FM6126A initialized successfully");
}

// ---------------------------------------------------------------------------
// FM6373 / FM6363 family initialization
//
// These chips use LE-length command encoding: the number of rising DCLK edges
// while LE (LAT) is high determines the command. Observed startup sequence
// from logic-analyzer capture of a working FM6373 controller:
//
//   1. VSYNC  (3 rising CLK edges while LAT=1): synchronise / start frame
//   2. CMD_11 (11 rising CLK edges while LAT=1): enable outputs (EN_OP equiv)
//      followed by ~4 CLK OE-blanking pulses (display disabled during this gap)
//   3. PRE_ACT (14 rising CLK edges while LAT=1): write-enable for registers
//
// All RGB data lines are kept LOW (zero) throughout, so the chip powers up
// with its default register values.  Per-frame operation uses only the
// standard DATA_LATCH (1 CLK) identical to other HUB75 chips — no DMA
// modification is needed.
//
// Reference: FM6363 Programming Guide v0.1 (closest available datasheet).
// ---------------------------------------------------------------------------

static void send_le_command(const Hub75Pins &pins, uint16_t pixels_per_row, int cmd_clks) {
  // Shift in `pixels_per_row` zero bits; assert LAT for the last `cmd_clks`
  // rising edges to encode the command.
  for (int i = 0; i < pixels_per_row; i++) {
    for (uint8_t pin : {pins.r1, pins.r2, pins.g1, pins.g2, pins.b1, pins.b2}) {
      gpio_set_level((gpio_num_t) pin, 0);
    }
    if (i >= pixels_per_row - cmd_clks) {
      gpio_set_level((gpio_num_t) pins.lat, 1);
    }
    CLK_PULSE;
  }
  gpio_set_level((gpio_num_t) pins.lat, 0);
}

void DriverInit::fm6373_init(const Hub75Pins &pins, uint16_t pixels_per_row) {
  ESP_LOGI(TAG, "Initializing FM6373 shift driver (pixels_per_row=%d)", pixels_per_row);

  // 1. Configure all pins as GPIO output; OE=0 (display enabled) for phases 1&2
  for (uint8_t pin : {pins.r1, pins.r2, pins.g1, pins.g2, pins.b1, pins.b2, pins.clk, pins.lat, pins.oe}) {
    gpio_reset_pin((gpio_num_t) pin);
    gpio_set_direction((gpio_num_t) pin, GPIO_MODE_OUTPUT);
    gpio_set_level((gpio_num_t) pin, 0);
  }
  // OE stays LOW (0) during VSYNC and CMD_11 — matches observed CSV behaviour

  // 2. VSYNC (3 rising CLK edges while LAT=1) — OE=0
  send_le_command(pins, pixels_per_row, 3);

  // 3. CMD_11 / EN_OP (11 rising CLK edges while LAT=1) — OE=0
  send_le_command(pins, pixels_per_row, 11);

  // After CMD_11 LAT drops, assert OE=1 (display disabled) for the gap and PRE_ACT
  gpio_set_level((gpio_num_t) pins.oe, 1);

  // 4. OE blanking gap (~4 CLK with OE=1, LAT=0)
  for (int i = 0; i < 4; i++) {
    CLK_PULSE;
  }

  // 5. PRE_ACT (14 rising CLK edges while LAT=1):
  //    OE=1 for the first 8 CLKs, then OE=0 for the last 6 CLKs (as observed in CSV).
  for (int i = 0; i < pixels_per_row; i++) {
    for (uint8_t pin : {pins.r1, pins.r2, pins.g1, pins.g2, pins.b1, pins.b2}) {
      gpio_set_level((gpio_num_t) pin, 0);
    }
    if (i >= pixels_per_row - 14) {
      gpio_set_level((gpio_num_t) pins.lat, 1);
    }
    // OE transitions low 6 CLKs before end of PRE_ACT
    if (i >= pixels_per_row - 6) {
      gpio_set_level((gpio_num_t) pins.oe, 0);
    }
    CLK_PULSE;
  }
  gpio_set_level((gpio_num_t) pins.lat, 0);
  // OE is already 0 here

  // 6. Blank display data (shift in zeros)
  for (int i = 0; i < pixels_per_row; i++) {
    CLK_PULSE;
  }
  gpio_set_level((gpio_num_t) pins.lat, 1);
  CLK_PULSE;
  gpio_set_level((gpio_num_t) pins.lat, 0);
  CLK_PULSE;

  ESP_LOGI(TAG, "FM6373 initialized successfully");
}

void DriverInit::dp3246_init(const Hub75Pins &pins, uint16_t pixels_per_row) {
  // TODO: Port from reference library when hardware available
  ESP_LOGW(TAG, "DP3246 initialization not yet implemented");
}

esp_err_t DriverInit::initialize(const Hub75Config &config) {
  uint16_t pixels_per_row = config.panel_width * config.layout_cols;

  switch (config.shift_driver) {
    case Hub75ShiftDriver::GENERIC:
      // No initialization needed
      return ESP_OK;

    case Hub75ShiftDriver::FM6126A:
    case Hub75ShiftDriver::ICN2038S:
      fm6126a_init(config.pins, pixels_per_row);
      return ESP_OK;

    case Hub75ShiftDriver::DP3246:
      dp3246_init(config.pins, pixels_per_row);
      return ESP_OK;

    case Hub75ShiftDriver::MBI5124:
      ESP_LOGW(TAG, "MBI5124: Ensure clk_phase_inverted is set to true");
      return ESP_OK;

    case Hub75ShiftDriver::FM6373:
      fm6373_init(config.pins, pixels_per_row);
      return ESP_OK;

    case Hub75ShiftDriver::FM6124:
      ESP_LOGW(TAG, "FM6124 initialization not yet implemented");
      return ESP_ERR_NOT_SUPPORTED;

    default:
      ESP_LOGW(TAG, "Unknown shift driver: %d", (int) config.shift_driver);
      return ESP_ERR_NOT_SUPPORTED;
  }
}

}  // namespace hub75
