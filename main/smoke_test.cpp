// SPDX-FileCopyrightText: 2025 Stuart Parmenter
// SPDX-License-Identifier: MIT

// @file smoke_test.cpp
// @brief Quick validation smoke test for HUB75 driver
//
// This test demonstrates:
// - Loading configuration from menuconfig
// - Basic driver initialization
// - Simple pixel drawing
// - Color testing with colored squares

#include "hub75.h"
#include "board_config.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>  // NOLINT(misc-header-include-cycle)
#include <freertos/task.h>

static const char *const TAG = "smoke_test";

extern "C" void app_main() {
  ESP_LOGI(TAG, "HUB75 Smoke Test Starting...");
  ESP_LOGI(TAG, "Loading configuration from menuconfig...");

  // Load configuration from menuconfig
  Hub75Config config = getMenuConfigSettings();

  // Print configuration summary
  ESP_LOGI(TAG, "Configuration:");
  ESP_LOGI(TAG, "  Panel: %dx%d pixels (%d-bit, %d Hz min refresh)", config.panel_width, config.panel_height,
           HUB75_BIT_DEPTH, config.min_refresh_rate);
  ESP_LOGI(TAG, "  Layout: %dx%d panels (total %dx%d display)", config.layout_cols, config.layout_rows,
           config.panel_width * config.layout_cols, config.panel_height * config.layout_rows);

  // Print pin configuration
  printPinConfig(config.pins);

  // Create driver instance
  Hub75Driver driver(config);

  // Initialize and start continuous refresh
  if (!driver.begin()) {
    ESP_LOGE(TAG, "Failed to initialize HUB75 driver!");
    return;
  }

  ESP_LOGI(TAG, "Driver initialized successfully");
  ESP_LOGI(TAG, "Display: %dx%d pixels", driver.get_width(), driver.get_height());

  // Clear display
  driver.clear();

  // #ifdef CONFIG_HUB75_DOUBLE_BUFFER
  driver.set_pixel(0, 0, 255, 255, 255);
  driver.set_pixel(0, 8, 0, 0, 255);
  driver.set_pixel(0, 16, 0, 255, 0);
  driver.set_pixel(0, 24, 255, 0, 0);
  driver.set_pixel(16, 0, 0, 0, 255);
  driver.set_pixel(32, 0, 0, 255, 0);
  driver.set_pixel(48, 0, 255, 0, 0);
  driver.flip_buffer();
  // #endif

  ESP_LOGI(TAG, "Smoke test pattern displayed");
  ESP_LOGI(TAG, "Expected output:");
  ESP_LOGI(TAG, "  - White pixel at 0,0");
  ESP_LOGI(TAG, "  - Blue pixel at 0,8");
  ESP_LOGI(TAG, "  - Green pixel at 0,16");
  ESP_LOGI(TAG, "  - Red pixel at 0,24");
  ESP_LOGI(TAG, "  - Blue pixel at 16,0");
  ESP_LOGI(TAG, "  - Green pixel at 32,0");
  ESP_LOGI(TAG, "  - Red pixel at 48,0");

  // Idle loop (display continues refresh via DMA)
  ESP_LOGI(TAG, "Smoke test complete. Display will remain static.");
  while (true) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
