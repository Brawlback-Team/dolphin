// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include "Common/CommonTypes.h"

constexpr std::array<u8, 520> soup01_tmd = {{
    0x00, 0x01, 0x00, 0x01, 0x5a, 0x35, 0xdb, 0x71, 0x2a, 0xc8, 0x6b, 0x33, 0x1d, 0xd8, 0xf3, 0x8c,
    0xe3, 0x97, 0x28, 0x6d, 0x2e, 0xcc, 0x6f, 0xd5, 0x37, 0xe6, 0xc5, 0xd1, 0x58, 0xe7, 0xd5, 0x9f,
    0xe4, 0x5e, 0x29, 0x16, 0xcf, 0x5c, 0xd1, 0xef, 0xa0, 0x57, 0xb0, 0x29, 0x23, 0xf3, 0xd8, 0xe3,
    0x2c, 0xfb, 0x4b, 0x1d, 0xcb, 0x0b, 0x08, 0x02, 0xb4, 0xf4, 0x1d, 0x22, 0x07, 0x4c, 0xc0, 0xb0,
    0x64, 0xed, 0x5c, 0x98, 0x38, 0x4b, 0x2c, 0x65, 0x22, 0x5e, 0x4f, 0x1d, 0x58, 0x5d, 0x9c, 0x82,
    0x3e, 0x2e, 0x56, 0xa8, 0xc3, 0x67, 0x4d, 0x08, 0x5a, 0x07, 0xc4, 0x60, 0x33, 0x88, 0x2f, 0x49,
    0xb7, 0x20, 0xd2, 0xea, 0x15, 0x40, 0x58, 0x40, 0xc7, 0xea, 0xf9, 0x7f, 0x61, 0xa6, 0x22, 0xc2,
    0x37, 0x95, 0xb7, 0xe9, 0x34, 0x77, 0x65, 0x18, 0xa1, 0x51, 0x81, 0x75, 0xbe, 0x0c, 0xbc, 0x2f,
    0xbd, 0x21, 0x02, 0xd5, 0x8a, 0x16, 0xee, 0xd1, 0x82, 0x90, 0xba, 0x40, 0xa7, 0x1e, 0xe8, 0xac,
    0x23, 0xd6, 0xfe, 0x6d, 0xc7, 0x33, 0xff, 0x24, 0x33, 0xd1, 0x0c, 0x4b, 0xd6, 0xf9, 0xc1, 0x31,
    0x8a, 0xcc, 0xf8, 0xaf, 0xd8, 0x22, 0xad, 0x8d, 0x5b, 0xb5, 0x48, 0x55, 0xe1, 0x79, 0x57, 0x54,
    0xcc, 0x40, 0x45, 0x05, 0x99, 0x9b, 0xbe, 0x17, 0xf0, 0x0c, 0x20, 0x6c, 0x2b, 0xf3, 0x60, 0x11,
    0xe1, 0x38, 0xba, 0x82, 0x07, 0xaf, 0xda, 0x3a, 0x84, 0x44, 0x42, 0xdc, 0x67, 0x4a, 0xce, 0x37,
    0xae, 0x26, 0x50, 0x70, 0x5e, 0xfb, 0xd3, 0xf2, 0xf8, 0xac, 0x8f, 0x68, 0x87, 0x66, 0x77, 0xbb,
    0x63, 0x49, 0x2e, 0x13, 0x5b, 0xb9, 0x8b, 0x40, 0x1b, 0x49, 0x60, 0xd4, 0x2c, 0x0d, 0xd7, 0xeb,
    0x18, 0x34, 0x10, 0x8d, 0xb8, 0xdd, 0x21, 0x37, 0xaf, 0x40, 0x52, 0x07, 0x64, 0x27, 0x05, 0x5a,
    0xce, 0xb2, 0xbb, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x52, 0x6f, 0x6f, 0x74, 0x2d, 0x43, 0x41, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x2d,
    0x43, 0x50, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x38, 0x00, 0x01, 0x00, 0x00,
    0x53, 0x4f, 0x55, 0x50, 0x00, 0x00, 0x00, 0x01, 0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
    0xff, 0x7c, 0x00, 0x00, 0x77, 0x13, 0xe0, 0xac, 0xef, 0xc1, 0x01, 0x51, 0xe7, 0x8b, 0x0b, 0x01,
    0xa2, 0xfb, 0x03, 0xdb, 0x45, 0x8a, 0x0e, 0x18,
}};

constexpr std::array<u8, 108> soup01_tmd_view = {{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x38, 0x00, 0x01, 0x00, 0x00,
    0x53, 0x4f, 0x55, 0x50, 0x00, 0x00, 0x00, 0x01, 0x30, 0x31, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0xff, 0x7c, 0x00, 0x00,
}};

constexpr std::array<u8, 1312> ios59_tmd = {
    {0x00, 0x01, 0x00, 0x01, 0x9f, 0xe6, 0xc6, 0x1a, 0x3b, 0x80, 0xe0, 0x25, 0x3e, 0x3c, 0x48,
     0x0e, 0x46, 0xd0, 0xe0, 0x79, 0x2c, 0x7a, 0xe0, 0xaa, 0xd3, 0x70, 0xcd, 0x15, 0x2d, 0x19,
     0x5a, 0x94, 0xa0, 0x6f, 0x48, 0x34, 0x7c, 0xdf, 0xa0, 0xd1, 0xc9, 0xe1, 0x4b, 0x34, 0xec,
     0x75, 0x84, 0x96, 0x68, 0xbd, 0x38, 0x16, 0x16, 0x26, 0x16, 0x0f, 0x9f, 0x2d, 0x80, 0xd4,
     0x4d, 0x2d, 0xfa, 0x0b, 0x86, 0x7e, 0x14, 0xf9, 0xf3, 0x40, 0x05, 0x77, 0x09, 0x1d, 0xf2,
     0x94, 0xe7, 0x11, 0x69, 0x90, 0x15, 0x81, 0xb1, 0x8e, 0x05, 0x37, 0x08, 0x27, 0xe3, 0x53,
     0xa9, 0x8f, 0x41, 0x89, 0xd8, 0x69, 0xca, 0x31, 0xec, 0x53, 0xda, 0x7b, 0x9a, 0x9d, 0x7f,
     0x59, 0x06, 0x71, 0xcd, 0x32, 0x01, 0xb4, 0x51, 0xd6, 0x9f, 0x81, 0xad, 0x78, 0x27, 0x66,
     0x4a, 0x87, 0x82, 0x84, 0x9e, 0x74, 0xef, 0x74, 0xb3, 0x4a, 0x19, 0xbe, 0xfe, 0xbf, 0x2b,
     0xc4, 0x96, 0xa5, 0x97, 0xaa, 0x93, 0x03, 0x13, 0x7e, 0xac, 0x56, 0x4b, 0xb3, 0xaf, 0x4c,
     0xca, 0x4c, 0xcc, 0xb6, 0xc4, 0x80, 0x97, 0x1b, 0xfb, 0xbf, 0x22, 0x62, 0x09, 0xb2, 0x3e,
     0x71, 0x63, 0x4f, 0x06, 0x68, 0xb9, 0x62, 0x15, 0xc2, 0xf9, 0xc5, 0x3e, 0xd3, 0x41, 0x41,
     0xa3, 0xcc, 0xb9, 0x21, 0x65, 0x63, 0xf0, 0x4b, 0xbf, 0x95, 0x48, 0x16, 0x30, 0xb0, 0x43,
     0x74, 0x65, 0x3b, 0x55, 0x5c, 0x60, 0xfa, 0xd3, 0x5d, 0x7d, 0x92, 0x96, 0xcf, 0x15, 0x91,
     0x2c, 0xf4, 0xbc, 0xb7, 0xb7, 0x29, 0x79, 0x22, 0x34, 0x46, 0xbf, 0x12, 0x0a, 0x8e, 0x90,
     0x0d, 0x37, 0x5c, 0xf7, 0x1e, 0xc1, 0xba, 0xb1, 0x93, 0x51, 0x0d, 0xcd, 0x99, 0x01, 0x25,
     0x79, 0x83, 0x79, 0xc9, 0xf1, 0x72, 0x2c, 0x59, 0xc5, 0x46, 0x1d, 0x4c, 0x26, 0x84, 0x78,
     0xcc, 0x10, 0x89, 0x12, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x6f, 0x6f, 0x74, 0x2d, 0x43, 0x41, 0x30, 0x30, 0x30,
     0x30, 0x30, 0x30, 0x30, 0x31, 0x2d, 0x43, 0x50, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
     0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3b, 0x00,
     0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x21, 0x00, 0x17,
     0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x40, 0x2c, 0x96, 0x97, 0x6d, 0x25, 0x2b, 0x2e, 0xa0, 0xcd, 0xc1,
     0xea, 0x16, 0x57, 0x7f, 0x3d, 0x90, 0x82, 0x59, 0xf1, 0x53, 0x00, 0x00, 0x00, 0x1c, 0x00,
     0x01, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0xc4, 0xd9, 0xc0, 0x24, 0x8b,
     0x6a, 0xaa, 0x6c, 0x52, 0x1b, 0x68, 0x77, 0xb1, 0x16, 0xc4, 0xcb, 0xeb, 0x77, 0xc2, 0xa0,
     0x76, 0x00, 0x00, 0x00, 0x02, 0x00, 0x02, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x82, 0x58, 0x4e, 0x04, 0xe8, 0x8e, 0xc7, 0x25, 0x0d, 0xe8, 0x4a, 0x1e, 0x78, 0x8a, 0xe6,
     0x9f, 0xda, 0xd9, 0x35, 0x13, 0x30, 0xa8, 0x00, 0x00, 0x00, 0x03, 0x00, 0x03, 0x80, 0x01,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3b, 0xf8, 0x3b, 0xc9, 0xe4, 0x16, 0xd3, 0xfa, 0x86,
     0xd0, 0xa0, 0x99, 0xb8, 0x2e, 0xce, 0x0a, 0xa7, 0xef, 0xf8, 0xf5, 0x9a, 0x68, 0x00, 0x00,
     0x00, 0x04, 0x00, 0x04, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5c, 0xb0, 0xd1,
     0x24, 0x72, 0xee, 0xb6, 0x24, 0x04, 0xca, 0x0f, 0x2c, 0xc6, 0x42, 0xf5, 0x23, 0xcd, 0x32,
     0x34, 0x2b, 0xb0, 0x16, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x80, 0x01, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x2a, 0x90, 0xeb, 0x3d, 0xbe, 0xa9, 0xdc, 0xbf, 0x57, 0xe4, 0x46, 0x35,
     0x4a, 0xe2, 0xbd, 0xe1, 0xd8, 0x8c, 0x65, 0x6c, 0x90, 0xa7, 0x00, 0x00, 0x00, 0x06, 0x00,
     0x06, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xa1, 0x38, 0x00, 0xef, 0x2f, 0x8b,
     0xbc, 0xd2, 0x08, 0xeb, 0x5e, 0x64, 0xcd, 0x91, 0x65, 0x54, 0x40, 0x5a, 0xd3, 0x4e, 0xcf,
     0xd5, 0x00, 0x00, 0x00, 0x07, 0x00, 0x07, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
     0xb1, 0xa4, 0xe4, 0x73, 0xfd, 0xa0, 0x35, 0x15, 0x12, 0x40, 0xd6, 0xa6, 0xf5, 0x3c, 0x50,
     0x36, 0x9d, 0x24, 0x14, 0xc3, 0x6f, 0x4d, 0x00, 0x00, 0x00, 0x08, 0x00, 0x08, 0x80, 0x01,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x18, 0xa4, 0x79, 0xa9, 0xf2, 0x15, 0xea, 0x9b, 0x38,
     0xfd, 0x08, 0x5d, 0xa3, 0x27, 0x20, 0xa7, 0xd3, 0x98, 0x18, 0xc8, 0x70, 0x1d, 0x00, 0x00,
     0x00, 0x09, 0x00, 0x09, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0xd5, 0xb0, 0xd1,
     0xf6, 0x0d, 0x86, 0x43, 0xc8, 0xdc, 0xb4, 0x72, 0xd8, 0x41, 0x5a, 0x2e, 0x45, 0x8a, 0x64,
     0xbc, 0x5d, 0x14, 0xdc, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x0a, 0x80, 0x01, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0xa6, 0x4c, 0x1d, 0x18, 0x53, 0x55, 0x81, 0x5b, 0xf6, 0x4c, 0x45, 0x71,
     0x71, 0x35, 0xe6, 0x54, 0x03, 0x3b, 0xab, 0x0c, 0x93, 0xbc, 0x00, 0x00, 0x00, 0x0b, 0x00,
     0x0b, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4a, 0xf0, 0xff, 0x38, 0x0d, 0x01,
     0x88, 0xfd, 0x07, 0x30, 0xba, 0xf8, 0x37, 0xd4, 0x78, 0x3e, 0xa6, 0xa1, 0x8c, 0x84, 0x7b,
     0x27, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x0c, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
     0xf3, 0x30, 0x51, 0x37, 0xd0, 0x39, 0x3c, 0x98, 0x09, 0x7f, 0x0d, 0x59, 0x70, 0x6d, 0x1c,
     0xee, 0xc2, 0x2f, 0x75, 0x48, 0xd6, 0x0c, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x0d, 0x80, 0x01,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0xbc, 0xfc, 0x22, 0x56, 0x63, 0x27, 0x81, 0x43,
     0x44, 0xca, 0xb1, 0xae, 0x1d, 0x71, 0x86, 0xbb, 0xae, 0x55, 0xf4, 0xa8, 0x07, 0x00, 0x00,
     0x00, 0x0e, 0x00, 0x0e, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2d, 0x80, 0xcf,
     0x7c, 0xb9, 0xe9, 0x5d, 0x57, 0x52, 0xad, 0x63, 0x47, 0x48, 0xc0, 0x95, 0x47, 0xfd, 0xdb,
     0x9e, 0x4f, 0xa7, 0x96, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x0f, 0x80, 0x01, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x1c, 0x38, 0xec, 0x38, 0x5c, 0xa1, 0x51, 0x5f, 0x0a, 0x7e, 0x0e, 0x8b,
     0xbd, 0x1d, 0x3d, 0x2c, 0xaa, 0xd4, 0xbf, 0x99, 0x48, 0xf8, 0x00, 0x00, 0x00, 0x10, 0x00,
     0x10, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0xf8, 0x21, 0xa5, 0x05, 0x90,
     0x98, 0xd1, 0x5e, 0x3e, 0x87, 0xbe, 0x4d, 0x34, 0x7d, 0xc8, 0x94, 0x3e, 0xa9, 0xf5, 0xf4,
     0xf4, 0x00, 0x00, 0x00, 0x11, 0x00, 0x11, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x29, 0xc4, 0x20, 0x5e, 0x64, 0xf3, 0x3a, 0x92, 0x5a, 0x79, 0x14, 0xbe, 0xcf, 0xad, 0xe5,
     0x3f, 0xd4, 0x9a, 0x5b, 0x90, 0x45, 0x0c, 0x00, 0x00, 0x00, 0x12, 0x00, 0x12, 0x80, 0x01,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x84, 0x9c, 0x4f, 0x9d, 0xd0, 0x8b, 0x41, 0xee,
     0x64, 0xd5, 0x63, 0x0a, 0x08, 0xb0, 0x9a, 0x64, 0xf1, 0xb5, 0x0a, 0x94, 0xf6, 0x00, 0x00,
     0x00, 0x13, 0x00, 0x13, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2d, 0x0c, 0x73,
     0x34, 0x67, 0x74, 0x45, 0xc8, 0x01, 0x3d, 0x19, 0x32, 0x14, 0x9d, 0x38, 0x7d, 0x42, 0x59,
     0xe1, 0x98, 0x6e, 0xa1, 0x00, 0x00, 0x00, 0x21, 0x00, 0x14, 0x80, 0x01, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x06, 0x97, 0xe0, 0xaa, 0x17, 0x2d, 0xda, 0xc9, 0xa3, 0x94, 0x98, 0x85, 0x9a,
     0x9d, 0xcf, 0x1d, 0xca, 0xa8, 0x27, 0x7c, 0xde, 0xda, 0xdf, 0x00, 0x00, 0x00, 0x22, 0x00,
     0x15, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3f, 0x9c, 0xc8, 0x5f, 0x56, 0x84,
     0x21, 0xbd, 0x95, 0x01, 0xbd, 0x00, 0x09, 0x3e, 0xf0, 0x9d, 0x51, 0xd8, 0x9f, 0x06, 0x44,
     0x11, 0x00, 0x00, 0x00, 0x18, 0x00, 0x16, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
     0x93, 0xd8, 0xfd, 0x77, 0x46, 0xac, 0x47, 0x00, 0xd5, 0x86, 0xa9, 0x32, 0x84, 0xfb, 0xe3,
     0x12, 0xc5, 0x2b, 0x2a, 0xc6, 0x33, 0xd3}};

constexpr std::array<u8, 460> ios59_tmd_view = {{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x3b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x21, 0x00, 0x17, 0x00, 0x00, 0x00, 0x20,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x1c,
    0x00, 0x01, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0xc4, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x02, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0x58, 0x00, 0x00, 0x00, 0x03,
    0x00, 0x03, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3b, 0xf8, 0x00, 0x00, 0x00, 0x04,
    0x00, 0x04, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5c, 0xb0, 0x00, 0x00, 0x00, 0x05,
    0x00, 0x05, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2a, 0x90, 0x00, 0x00, 0x00, 0x06,
    0x00, 0x06, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xa1, 0x38, 0x00, 0x00, 0x00, 0x07,
    0x00, 0x07, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0xb1, 0xa4, 0x00, 0x00, 0x00, 0x08,
    0x00, 0x08, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x18, 0xa4, 0x00, 0x00, 0x00, 0x09,
    0x00, 0x09, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0xd5, 0xb0, 0x00, 0x00, 0x00, 0x0a,
    0x00, 0x0a, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa6, 0x4c, 0x00, 0x00, 0x00, 0x0b,
    0x00, 0x0b, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4a, 0xf0, 0x00, 0x00, 0x00, 0x0c,
    0x00, 0x0c, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xf3, 0x30, 0x00, 0x00, 0x00, 0x0d,
    0x00, 0x0d, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0xbc, 0x00, 0x00, 0x00, 0x0e,
    0x00, 0x0e, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2d, 0x80, 0x00, 0x00, 0x00, 0x0f,
    0x00, 0x0f, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1c, 0x38, 0x00, 0x00, 0x00, 0x10,
    0x00, 0x10, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0xf8, 0x00, 0x00, 0x00, 0x11,
    0x00, 0x11, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x29, 0xc4, 0x00, 0x00, 0x00, 0x12,
    0x00, 0x12, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x84, 0x00, 0x00, 0x00, 0x13,
    0x00, 0x13, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2d, 0x0c, 0x00, 0x00, 0x00, 0x21,
    0x00, 0x14, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x97, 0xe0, 0x00, 0x00, 0x00, 0x22,
    0x00, 0x15, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x3f, 0x9c, 0x00, 0x00, 0x00, 0x18,
    0x00, 0x16, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x93, 0xd8,
}};
