/*
 * Firmware for the Portenta X8 STM32H747AIIX/Cortex-M7 core.
 * Copyright (C) 2022 Arduino (http://www.arduino.cc/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SYSTEM_H
#define SYSTEM_H

/**************************************************************************************
 * INCLUDE
 **************************************************************************************/

#include <inttypes.h>
#include <stdbool.h>

/**************************************************************************************
 * DEFINE
 **************************************************************************************/

#define SPI_DMA_BUFFER_SIZE   64 * 1024

__attribute__((packed, aligned(4))) struct subpacket {
  __attribute__((packed, aligned(4))) struct {
    uint8_t peripheral;
    uint8_t opcode;
    uint16_t size;
  } header;
  uint8_t raw_data;
};

__attribute__((packed, aligned(4))) struct complete_packet {
  __attribute__((packed, aligned(4))) struct {
    uint16_t size;
    uint16_t checksum;
  } header;
  struct subpacket data;
  // ... other subpackets will follow
};

#define max(a, b)                                                              \
  ({                                                                           \
    __typeof__(a) _a = (a);                                                    \
    __typeof__(b) _b = (b);                                                    \
    _a > _b ? _a : _b;                                                         \
  })

/**************************************************************************************
 * FUNCTION DECLARATION
 **************************************************************************************/

void system_init();

void dma_init();

void enqueue_packet(uint8_t peripheral, uint8_t opcode, uint16_t size, void* data);

void dispatchPacket(uint8_t peripheral, uint8_t opcode, uint16_t size, uint8_t* data);

struct complete_packet* get_dma_packet();

int get_dma_packet_size();

void dma_handle_data();

void register_peripheral_callback(uint8_t peripheral,/* uint8_t opcode,*/ void* func);

#endif //SYSTEM_H