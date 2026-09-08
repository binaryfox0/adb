#include "spake2_ge.h"
#include "spake2_ge_data.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const uint8_t spake2__n_small_precomp[15 * 2 * 32] = {
    0x20, 0x1b, 0xc5, 0xb3, 0x43, 0x17, 0x71, 0x10, 0x44, 0x1e, 0x73, 0xb3,
    0xae, 0x3f, 0xbf, 0x9f, 0xf5, 0x44, 0xc8, 0x13, 0x8f, 0xd1, 0x01, 0xc2,
    0x8a, 0x1a, 0x6d, 0xea, 0x4d, 0x00, 0x5d, 0x6e, 0x10, 0xe3, 0xdf, 0x0a,
    0xe3, 0x7d, 0x8e, 0x7a, 0x99, 0xb5, 0xfe, 0x74, 0xb4, 0x46, 0x72, 0x10,
    0x3d, 0xbd, 0xdc, 0xbd, 0x06, 0xaf, 0x68, 0x0d, 0x71, 0x32, 0x9a, 0x11,
    0x69, 0x3b, 0xc7, 0x78, 0x93, 0xf1, 0x57, 0x97, 0x6e, 0xf0, 0x6e, 0x45,
    0x37, 0x4a, 0xf4, 0x0b, 0x18, 0x51, 0xf5, 0x4f, 0x67, 0x3c, 0xdc, 0xec,
    0x84, 0xed, 0xd0, 0xeb, 0xca, 0xfb, 0xdb, 0xff, 0x7f, 0xeb, 0xa8, 0x23,
    0x68, 0x87, 0x13, 0x64, 0x6a, 0x10, 0xf7, 0x45, 0xe0, 0x0f, 0x32, 0x21,
    0x59, 0x7c, 0x0e, 0x50, 0xad, 0x56, 0xd7, 0x12, 0x69, 0x7b, 0x58, 0xf8,
    0xb9, 0x3b, 0xa5, 0xbb, 0x4d, 0x1b, 0x87, 0x1c, 0x46, 0xa7, 0x17, 0x9d,
    0x6d, 0x84, 0x45, 0xbe, 0x7f, 0x95, 0xd2, 0x34, 0xcd, 0x89, 0x95, 0xc0,
    0xf0, 0xd3, 0xdf, 0x6e, 0x10, 0x4a, 0xe3, 0x7b, 0xce, 0x7f, 0x40, 0x27,
    0xc7, 0x2b, 0xab, 0x66, 0x03, 0x59, 0xb4, 0x7b, 0xc7, 0xc7, 0xf0, 0x39,
    0x9a, 0x33, 0x35, 0xbf, 0xcc, 0x2f, 0xf3, 0x2e, 0x68, 0x9d, 0x53, 0x5c,
    0x88, 0x52, 0xe3, 0x77, 0x90, 0xa1, 0x27, 0x85, 0xc5, 0x74, 0x7f, 0x23,
    0x0e, 0x93, 0x01, 0x3e, 0xe7, 0x2e, 0x2e, 0x95, 0xf3, 0x0d, 0xc2, 0x25,
    0x25, 0x39, 0x39, 0x3d, 0x6e, 0x8e, 0x89, 0xbd, 0xe8, 0xbb, 0x67, 0x5e,
    0x8c, 0x66, 0x8b, 0x63, 0x28, 0x1e, 0x4e, 0x74, 0x85, 0xa8, 0xaf, 0x0f,
    0x12, 0x5d, 0xb6, 0x8a, 0x83, 0x1a, 0x77, 0x76, 0x5e, 0x62, 0x8a, 0xa7,
    0x3c, 0xb8, 0x05, 0x57, 0x2b, 0xaf, 0x36, 0x2e, 0x10, 0x90, 0xb2, 0x39,
    0xb4, 0x3e, 0x75, 0x6d, 0x3a, 0xa8, 0x31, 0x35, 0xc2, 0x1e, 0x8f, 0xc2,
    0x79, 0x89, 0x35, 0x16, 0x26, 0xd1, 0xc7, 0x0b, 0x04, 0x1f, 0x1d, 0xf9,
    0x9c, 0x05, 0xa6, 0x6b, 0xb5, 0x19, 0x5a, 0x24, 0x6d, 0x91, 0xc5, 0x31,
    0xfd, 0xc5, 0xfa, 0xe7, 0xa6, 0xcb, 0x0e, 0x4b, 0x18, 0x0d, 0x94, 0xc7,
    0xee, 0x1d, 0x46, 0x1f, 0x92, 0xb1, 0xb2, 0x4a, 0x2b, 0x43, 0x37, 0xfe,
    0xc2, 0x15, 0x11, 0x89, 0xef, 0x59, 0x73, 0x3c, 0x06, 0x76, 0x78, 0xcb,
    0xa6, 0x0d, 0x79, 0x5f, 0x28, 0x0b, 0x5b, 0x8c, 0x9e, 0xe4, 0xaa, 0x51,
    0x9a, 0x42, 0x6f, 0x11, 0x50, 0x3d, 0x01, 0xd6, 0x21, 0xc0, 0x99, 0x5e,
    0x1a, 0xe8, 0x81, 0x25, 0x80, 0xeb, 0xed, 0x5d, 0x37, 0x47, 0x30, 0x70,
    0xa0, 0x4e, 0x0b, 0x43, 0x17, 0xbe, 0xb6, 0x47, 0xe7, 0x2a, 0x62, 0x9d,
    0x5d, 0xa6, 0xc5, 0x33, 0x62, 0x9d, 0x56, 0x24, 0x9d, 0x1d, 0xb2, 0x13,
    0xbc, 0x17, 0x66, 0x43, 0xd1, 0x68, 0xd5, 0x3b, 0x17, 0x69, 0x17, 0xa6,
    0x06, 0x9e, 0x12, 0xb8, 0x7c, 0xd5, 0xaf, 0x3e, 0x21, 0x1b, 0x31, 0xeb,
    0x0b, 0xa4, 0x98, 0x1c, 0xf2, 0x6a, 0x5e, 0x7c, 0x9b, 0x45, 0x8f, 0xb2,
    0x12, 0x06, 0xd5, 0x8c, 0x1d, 0xb2, 0xa7, 0x57, 0x5f, 0x2f, 0x4f, 0xdb,
    0x52, 0x99, 0x7c, 0x58, 0x01, 0x5f, 0xf2, 0xa5, 0xf6, 0x51, 0x86, 0x21,
    0x2f, 0x5b, 0x8d, 0x6a, 0xae, 0x83, 0x34, 0x6d, 0x58, 0x4b, 0xef, 0xfe,
    0xbf, 0x73, 0x5d, 0xdb, 0xc4, 0x97, 0x2a, 0x85, 0xf3, 0x6c, 0x46, 0x42,
    0xb3, 0x90, 0xc1, 0x57, 0x97, 0x50, 0x35, 0xb1, 0x9d, 0xb7, 0xc7, 0x3c,
    0x85, 0x6d, 0x6c, 0xfd, 0xce, 0xb0, 0xc9, 0xa2, 0x77, 0xee, 0xc3, 0x6b,
    0x0c, 0x37, 0xfa, 0x30, 0x91, 0xd1, 0x2c, 0xb8, 0x5e, 0x7f, 0x81, 0x5f,
    0x87, 0xfd, 0x18, 0x02, 0x5a, 0x30, 0x4e, 0x62, 0xbc, 0x65, 0xc6, 0xce,
    0x1a, 0xcf, 0x2b, 0xaa, 0x56, 0x3e, 0x4d, 0xcf, 0xba, 0x62, 0x5f, 0x9a,
    0xd0, 0x72, 0xff, 0xef, 0x28, 0xbd, 0xbe, 0xd8, 0x57, 0x3d, 0xf5, 0x57,
    0x7d, 0xe9, 0x71, 0x31, 0xec, 0x98, 0x90, 0x94, 0xd9, 0x54, 0xbf, 0x84,
    0x0b, 0xe3, 0x06, 0x47, 0x19, 0x9a, 0x13, 0x1d, 0xef, 0x9d, 0x13, 0xf3,
    0xdb, 0xc3, 0x5c, 0x72, 0x9e, 0xed, 0x24, 0xaa, 0x64, 0xed, 0xe7, 0x0d,
    0xa0, 0x7c, 0x73, 0xba, 0x9b, 0x86, 0xa7, 0x3b, 0x55, 0xab, 0x58, 0x30,
    0xf1, 0x15, 0x81, 0x83, 0x2f, 0xf9, 0x62, 0x84, 0x98, 0x66, 0xf6, 0x55,
    0x21, 0xd8, 0xf2, 0x25, 0x64, 0x71, 0x4b, 0x12, 0x76, 0x59, 0xc5, 0xaa,
    0x93, 0x67, 0xc3, 0x86, 0x25, 0xab, 0x4e, 0x4b, 0xf6, 0xd8, 0x3f, 0x44,
    0x2e, 0x11, 0xe0, 0xbd, 0x6a, 0xf2, 0x5d, 0xf5, 0xf9, 0x53, 0xea, 0xa4,
    0xc8, 0xd9, 0x50, 0x33, 0x81, 0xd9, 0xa8, 0x2d, 0x91, 0x7d, 0x13, 0x2a,
    0x11, 0xcf, 0xde, 0x3f, 0x0a, 0xd2, 0xbc, 0x33, 0xb2, 0x62, 0x53, 0xea,
    0x77, 0x88, 0x43, 0x66, 0x27, 0x43, 0x85, 0xe9, 0x5f, 0x55, 0xf5, 0x2a,
    0x8a, 0xac, 0xdf, 0xff, 0x9b, 0x4c, 0x96, 0x9c, 0xa5, 0x7a, 0xce, 0xd5,
    0x79, 0x18, 0xf1, 0x0b, 0x58, 0x95, 0x7a, 0xe7, 0xd3, 0x74, 0x65, 0x0b,
    0xa4, 0x64, 0x30, 0xe8, 0x5c, 0xfc, 0x55, 0x56, 0xee, 0x14, 0x14, 0xd3,
    0x45, 0x3b, 0xf8, 0xde, 0x05, 0x3e, 0xb9, 0x3c, 0xd7, 0x6a, 0x52, 0x72,
    0x5b, 0x39, 0x09, 0xbe, 0x82, 0x23, 0x10, 0x4a, 0xb7, 0xc3, 0xdc, 0x4c,
    0x5d, 0xc9, 0xf1, 0x14, 0x83, 0xf9, 0x0b, 0x9b, 0xe9, 0x23, 0x84, 0x6a,
    0xc4, 0x08, 0x3d, 0xda, 0x3d, 0x12, 0x95, 0x87, 0x18, 0xa4, 0x7d, 0x3f,
    0x23, 0xde, 0xd4, 0x1e, 0xa8, 0x47, 0xc3, 0x71, 0xdb, 0xf5, 0x03, 0x6c,
    0x57, 0xe7, 0xa4, 0x43, 0x82, 0x33, 0x7b, 0x62, 0x46, 0x7d, 0xf7, 0x10,
    0x69, 0x18, 0x38, 0x27, 0x9a, 0x6f, 0x38, 0xac, 0xfa, 0x92, 0xc5, 0xae,
    0x66, 0xa6, 0x73, 0x95, 0x15, 0x0e, 0x4c, 0x04, 0xb6, 0xfc, 0xf5, 0xc7,
    0x21, 0x3a, 0x99, 0xdb, 0x0e, 0x36, 0xf0, 0x56, 0xbc, 0x75, 0xf9, 0x87,
    0x9b, 0x11, 0x18, 0x92, 0x64, 0x1a, 0xe7, 0xc7, 0xab, 0x5a, 0xc7, 0x26,
    0x7f, 0x13, 0x98, 0x42, 0x52, 0x43, 0xdb, 0xc8, 0x6d, 0x0b, 0xb7, 0x31,
    0x93, 0x24, 0xd6, 0xe8, 0x24, 0x1f, 0x6f, 0x21, 0xa7, 0x8c, 0xeb, 0xdb,
    0x83, 0xb8, 0x89, 0xe3, 0xc1, 0xd7, 0x69, 0x3b, 0x02, 0x6b, 0x54, 0x0f,
    0x84, 0x2f, 0xb5, 0x5c, 0x17, 0x77, 0xbe, 0xe5, 0x61, 0x0d, 0xc5, 0xdf,
    0x3b, 0xcf, 0x3e, 0x93, 0x4f, 0xf5, 0x89, 0xb9, 0x5a, 0xc5, 0x29, 0x31,
    0xc0, 0xc2, 0xff, 0xe5, 0x3f, 0xa6, 0xac, 0x03, 0xca, 0xf5, 0xff, 0xe0,
    0x36, 0xce, 0xf3, 0xe2, 0xb7, 0x9c, 0x02, 0xe9, 0x9e, 0xd2, 0xbc, 0x87,
    0x2f, 0x3d, 0x9a, 0x1d, 0x8f, 0xc5, 0x72, 0xb8, 0xa2, 0x01, 0xd4, 0x68,
    0xb1, 0x84, 0x16, 0x10, 0xf6, 0xf3, 0x52, 0x25, 0xd9, 0xdc, 0x4c, 0xdd,
    0x0f, 0xd6, 0x4a, 0xcf, 0x60, 0x96, 0x7e, 0xcc, 0x42, 0x0f, 0x64, 0x9d,
    0x72, 0x46, 0x04, 0x07, 0xf2, 0x5b, 0xf4, 0x07, 0xd1, 0xf4, 0x59, 0x71,
};

static const uint8_t spake2__m_small_precomp[15 * 2 * 32] = {
    0xc8, 0xa6, 0x63, 0xc5, 0x97, 0xf1, 0xee, 0x40, 0xab, 0x62, 0x42, 0xee,
    0x25, 0x6f, 0x32, 0x6c, 0x75, 0x2c, 0xa7, 0xd3, 0xbd, 0x32, 0x3b, 0x1e,
    0x11, 0x9c, 0xbd, 0x04, 0xa9, 0x78, 0x6f, 0x45, 0x5a, 0xda, 0x7e, 0x4b,
    0xf6, 0xdd, 0xd9, 0xad, 0xb6, 0x62, 0x6d, 0x32, 0x13, 0x1c, 0x6b, 0x5c,
    0x51, 0xa1, 0xe3, 0x47, 0xa3, 0x47, 0x8f, 0x53, 0xcf, 0xcf, 0x44, 0x1b,
    0x88, 0xee, 0xd1, 0x2e, 0x03, 0x89, 0xaf, 0xc0, 0x61, 0x2d, 0x9e, 0x35,
    0xeb, 0x0e, 0x03, 0xe0, 0xb7, 0xfb, 0xa5, 0xbc, 0x44, 0xbe, 0x0c, 0x89,
    0x0a, 0x0f, 0xd6, 0x59, 0x47, 0x9e, 0xe6, 0x3d, 0x36, 0x9d, 0xff, 0x44,
    0x5e, 0xac, 0xab, 0xe5, 0x3a, 0xd5, 0xb0, 0x35, 0x9f, 0x6d, 0x7f, 0xba,
    0xc0, 0x85, 0x0e, 0xf4, 0x70, 0x3f, 0x13, 0x90, 0x4c, 0x50, 0x1a, 0xee,
    0xc5, 0xeb, 0x69, 0xfe, 0x98, 0x42, 0x87, 0x1d, 0xce, 0x6c, 0x29, 0xaa,
    0x2b, 0x31, 0xc2, 0x38, 0x7b, 0x6b, 0xee, 0x88, 0x0b, 0xba, 0xce, 0xa8,
    0xca, 0x19, 0x60, 0x1b, 0x16, 0xf1, 0x25, 0x1e, 0xcf, 0x63, 0x66, 0x1e,
    0xbb, 0x63, 0xeb, 0x7d, 0xca, 0xd2, 0xb4, 0x23, 0x5a, 0x01, 0x6f, 0x05,
    0xd1, 0xdc, 0x41, 0x73, 0x75, 0xc0, 0xfd, 0x30, 0x91, 0x52, 0x68, 0x96,
    0x45, 0xb3, 0x66, 0x01, 0x3b, 0x53, 0x89, 0x3c, 0x69, 0xbc, 0x6c, 0x69,
    0xe3, 0x51, 0x8f, 0xe3, 0xd2, 0x84, 0xd5, 0x28, 0x66, 0xb5, 0xe6, 0x06,
    0x09, 0xfe, 0x6d, 0xb0, 0x72, 0x16, 0xe0, 0x8a, 0xce, 0x61, 0x65, 0xa9,
    0x21, 0x32, 0x48, 0xdc, 0x7a, 0x1d, 0xe1, 0x38, 0x7f, 0x8c, 0x75, 0x88,
    0x3d, 0x08, 0xa9, 0x4a, 0x6f, 0x3d, 0x9f, 0x7f, 0x3f, 0xbd, 0x57, 0x6b,
    0x19, 0xce, 0x3f, 0x4a, 0xc9, 0xd3, 0xf9, 0x6e, 0x72, 0x7b, 0x5b, 0x74,
    0xea, 0xbe, 0x9c, 0x7a, 0x6d, 0x9c, 0x40, 0x49, 0xe6, 0xfb, 0x2a, 0x1a,
    0x75, 0x70, 0xe5, 0x4e, 0xed, 0x74, 0xe0, 0x75, 0xac, 0xc0, 0xb1, 0x11,
    0x3e, 0xf2, 0xaf, 0x88, 0x4d, 0x66, 0xb6, 0xf6, 0x15, 0x4f, 0x3c, 0x6c,
    0x77, 0xae, 0x47, 0x51, 0x63, 0x9a, 0xfe, 0xe1, 0xb4, 0x1a, 0x12, 0xdf,
    0xe9, 0x54, 0x8d, 0x3b, 0x30, 0x2a, 0x75, 0xe3, 0xe5, 0x29, 0xb1, 0x4c,
    0xb0, 0x7c, 0x6d, 0xb5, 0xae, 0x85, 0xdb, 0x1e, 0x38, 0x55, 0x96, 0xa5,
    0x5b, 0x9f, 0x15, 0x23, 0x28, 0x36, 0xb8, 0xa2, 0x41, 0xb4, 0xd7, 0x19,
    0x91, 0x8d, 0x26, 0x3e, 0xca, 0x9c, 0x05, 0x7a, 0x2b, 0x60, 0x45, 0x86,
    0x8b, 0xee, 0x64, 0x6f, 0x5c, 0x09, 0x4d, 0x4b, 0x5a, 0x7f, 0xb0, 0xc3,
    0x26, 0x9d, 0x8b, 0xb8, 0x83, 0x69, 0xcf, 0x16, 0x72, 0x62, 0x3e, 0x5e,
    0x53, 0x4f, 0x9c, 0x73, 0x76, 0xfc, 0x19, 0xef, 0xa0, 0x74, 0x3a, 0x11,
    0x1e, 0xd0, 0x4d, 0xb7, 0x87, 0xa1, 0xd6, 0x87, 0x6c, 0x0e, 0x6c, 0x8c,
    0xe9, 0xa0, 0x44, 0xc4, 0x72, 0x3e, 0x73, 0x17, 0x13, 0xd1, 0x4e, 0x3d,
    0x8e, 0x1d, 0x5a, 0x8b, 0x75, 0xcb, 0x59, 0x2c, 0x47, 0x87, 0x15, 0x41,
    0xfe, 0x08, 0xe9, 0xa6, 0x97, 0x17, 0x08, 0x26, 0x6a, 0xb5, 0xbb, 0x73,
    0xaa, 0xb8, 0x5b, 0x65, 0x65, 0x5b, 0x30, 0x9e, 0x62, 0x59, 0x02, 0xf8,
    0xb8, 0x0f, 0x32, 0x10, 0xc1, 0x36, 0x08, 0x52, 0x98, 0x4a, 0x1e, 0xf0,
    0xab, 0x21, 0x5e, 0xde, 0x16, 0x0c, 0xda, 0x09, 0x99, 0x6b, 0x9e, 0xc0,
    0x90, 0xa5, 0x5a, 0xcc, 0xb0, 0xb7, 0xbb, 0xd2, 0x8b, 0x5f, 0xd3, 0x3b,
    0x3e, 0x8c, 0xa5, 0x71, 0x66, 0x06, 0xe3, 0x28, 0xd4, 0xf8, 0x3f, 0xe5,
    0x27, 0xdf, 0xfe, 0x0f, 0x09, 0xb2, 0x8a, 0x09, 0x5a, 0x23, 0x61, 0x0d,
    0x2d, 0xf5, 0x44, 0xf1, 0x5c, 0xf8, 0x82, 0x4e, 0xdc, 0x78, 0x7a, 0xab,
    0xc3, 0x57, 0x91, 0xaf, 0x65, 0x6e, 0x71, 0xf1, 0x44, 0xbf, 0xed, 0x43,
    0x50, 0xb4, 0x67, 0x48, 0xef, 0x5a, 0x10, 0x46, 0x81, 0xb4, 0x0c, 0xc8,
    0x48, 0xed, 0x99, 0x7a, 0x45, 0xa5, 0x92, 0xc3, 0x69, 0xd6, 0xd7, 0x8a,
    0x20, 0x1b, 0xeb, 0x8f, 0xb2, 0xff, 0xec, 0x6d, 0x76, 0x04, 0xf8, 0xc2,
    0x58, 0x9b, 0xf2, 0x20, 0x53, 0xc4, 0x74, 0x91, 0x19, 0xdd, 0x2d, 0x12,
    0x53, 0xc7, 0x6e, 0xd0, 0x02, 0x51, 0x3c, 0xa6, 0x7d, 0x80, 0x75, 0x6b,
    0x1d, 0xdf, 0xf8, 0x6a, 0x52, 0xbb, 0x81, 0xf8, 0x30, 0x45, 0xef, 0x51,
    0x85, 0x36, 0xbe, 0x8e, 0xcf, 0x0b, 0x9a, 0x46, 0xe8, 0x3f, 0x99, 0xfd,
    0xf7, 0xd9, 0x3e, 0x84, 0xe5, 0xe3, 0x37, 0xcf, 0x98, 0x7f, 0xeb, 0x5e,
    0x5a, 0x53, 0x77, 0x1c, 0x20, 0xdc, 0xf1, 0x20, 0x99, 0xec, 0x60, 0x40,
    0x93, 0xef, 0x5c, 0x1c, 0x81, 0xe2, 0xa5, 0xad, 0x2a, 0xc2, 0xdb, 0x6b,
    0xc1, 0x7e, 0x8f, 0xa9, 0x23, 0x5b, 0xd9, 0x0d, 0xfe, 0xa0, 0xac, 0x11,
    0x28, 0xba, 0x8e, 0x92, 0x07, 0x2d, 0x07, 0x40, 0x83, 0x14, 0x4c, 0x35,
    0x8d, 0xd0, 0x11, 0xff, 0x98, 0xdb, 0x00, 0x30, 0x6f, 0x65, 0xb6, 0xa0,
    0x7f, 0x9c, 0x08, 0xb8, 0xce, 0xb3, 0xa8, 0x42, 0xd3, 0x84, 0x45, 0xe1,
    0xe3, 0x8f, 0xa6, 0x89, 0x21, 0xd7, 0x74, 0x02, 0x4d, 0x64, 0xdf, 0x54,
    0x15, 0x9e, 0xba, 0x12, 0x49, 0x09, 0x41, 0xf6, 0x10, 0x24, 0xa1, 0x84,
    0x15, 0xfd, 0x68, 0x6a, 0x57, 0x66, 0xb3, 0x6d, 0x4c, 0xea, 0xbf, 0xbc,
    0x60, 0x3f, 0x52, 0x1c, 0x44, 0x1b, 0xc0, 0x4a, 0x25, 0xe3, 0xd9, 0x4c,
    0x9a, 0x74, 0xad, 0xfc, 0x9e, 0x8d, 0x0b, 0x18, 0x66, 0x24, 0xd1, 0x06,
    0xac, 0x68, 0xc1, 0xae, 0x14, 0xce, 0xb1, 0xf3, 0x86, 0x9f, 0x87, 0x11,
    0xd7, 0x9f, 0x30, 0x92, 0xdb, 0xec, 0x0b, 0x4a, 0xe8, 0xf6, 0x53, 0x36,
    0x68, 0x12, 0x11, 0x5e, 0xe0, 0x34, 0xa4, 0xff, 0x00, 0x0a, 0x26, 0xb8,
    0x62, 0x79, 0x9c, 0x0c, 0xd5, 0xe5, 0xf5, 0x1c, 0x1a, 0x16, 0x84, 0x4d,
    0x8e, 0x5d, 0x31, 0x7e, 0xf7, 0xe2, 0xd3, 0xa1, 0x41, 0x90, 0x61, 0x5d,
    0x04, 0xb2, 0x9a, 0x18, 0x9e, 0x54, 0xfb, 0xd1, 0x61, 0x95, 0x1b, 0x08,
    0xca, 0x7c, 0x49, 0x44, 0x74, 0x1d, 0x2f, 0xca, 0xc4, 0x7a, 0xe1, 0x8b,
    0x2f, 0xbb, 0x96, 0xee, 0x19, 0x8a, 0x5d, 0xfb, 0x3e, 0x82, 0xe7, 0x15,
    0xdb, 0x29, 0x14, 0xee, 0xc9, 0x4d, 0x9a, 0xfb, 0x9f, 0x8a, 0xbb, 0x17,
    0x37, 0x1b, 0x6e, 0x28, 0x6c, 0xf9, 0xff, 0xb5, 0xb5, 0x8b, 0x9d, 0x88,
    0x20, 0x08, 0x10, 0xd7, 0xca, 0x58, 0xf6, 0xe1, 0x32, 0x91, 0x6f, 0x36,
    0xc0, 0xad, 0xc1, 0x57, 0x5d, 0x76, 0x31, 0x43, 0xf3, 0xdd, 0xec, 0xf1,
    0xa9, 0x79, 0xe9, 0xe9, 0x85, 0xd7, 0x91, 0xc7, 0x31, 0x62, 0x3c, 0xd2,
    0x90, 0x2c, 0x9c, 0xa4, 0x56, 0x37, 0x7b, 0xbe, 0x40, 0x58, 0xc0, 0x81,
    0x83, 0x22, 0xe8, 0x13, 0x79, 0x18, 0xdb, 0x3a, 0x1b, 0x31, 0x0d, 0x00,
    0x6c, 0x22, 0x62, 0x75, 0x70, 0xd8, 0x96, 0x59, 0x99, 0x44, 0x79, 0x71,
    0xa6, 0x76, 0x81, 0x28, 0xb2, 0x65, 0xe8, 0x47, 0x14, 0xc6, 0x39, 0x06,
};

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Adapt these two names if your fe header uses different names.
 *
 * Expected semantics:
 *
 *     spake2__fe_0(f)
 *     spake2__fe_1(f)
 *     spake2__fe_mul(r, a, b)
 *     spake2__fe_invert(r, a)
 *     spake2__fe_tobytes(out, a)
 */
#include "spake2_fe.h"

#define TEST_OK 1
#define TEST_FAIL 0

static int g_failed = 0;


/* ------------------------------------------------------------------------- */
/* helpers                                                                   */
/* ------------------------------------------------------------------------- */

static void dump_hex(const char *name, const uint8_t *v, size_t len)
{
    size_t i;

    printf("%s = ", name);

    for (i = 0; i < len; ++i) {
        printf("%02x", v[i]);
    }

    printf("\n");
}

static int bytes_equal(
        const uint8_t *a,
        const uint8_t *b,
        size_t len)
{
    return memcmp(a, b, len) == 0;
}

static void fail_bytes(
        const char *name,
        const uint8_t *got,
        const uint8_t *expected,
        size_t len)
{
    printf("FAIL: %s\n", name);

    dump_hex("got     ", got, len);
    dump_hex("expected", expected, len);

    ++g_failed;
}

static void expect_bytes(
        const char *name,
        const uint8_t *got,
        const uint8_t *expected,
        size_t len)
{
    if (!bytes_equal(got, expected, len)) {
        fail_bytes(name, got, expected, len);
        return;
    }

    printf("  %-40s OK\n", name);
}

static void expect_true(
        const char *name,
        int condition)
{
    if (!condition) {
        printf("  %-40s FAIL\n", name);
        ++g_failed;
        return;
    }

    printf("  %-40s OK\n", name);
}


/*
 * Little-endian scalar.
 */
static void scalar_zero(uint8_t s[32])
{
    memset(s, 0, 32);
}

static void scalar_one(uint8_t s[32])
{
    memset(s, 0, 32);
    s[0] = 1;
}

static void scalar_set_u64(
        uint8_t s[32],
        uint64_t v)
{
    unsigned int i;

    memset(s, 0, 32);

    for (i = 0; i < 8; ++i) {
        s[i] = (uint8_t)v;
        v >>= 8;
    }
}


/*
 * Canonical compressed Ed25519 identity.
 */
static const uint8_t kIdentity[32] = {
    0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};


/*
 * Ed25519 base point:
 *
 *   5866666666666666666666666666666666666666666666666666666666666666
 *
 * y = 4/5, x is positive.
 */
static const uint8_t kBasepoint[32] = {
    0x58, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66,
    0x66, 0x66, 0x66, 0x66
};


/* ------------------------------------------------------------------------- */
/* point encoding                                                            */
/* ------------------------------------------------------------------------- */

static int test_frombytes_tobytes(void)
{
    spake2__ge_p3 p;
    spake2__ge_p2 p2;
    uint8_t encoded[32];

    printf("\n[ frombytes / tobytes ]\n");

    if (!spake2__ge_frombytes_vartime(&p, kBasepoint)) {
        printf("FAIL: ge_frombytes_vartime(basepoint)\n");
        ++g_failed;
        return TEST_FAIL;
    }

    printf("  %-40s OK\n", "ge_frombytes_vartime(basepoint)");

    spake2__ge_p3_tobytes(encoded, &p);
    expect_bytes(
            "ge_p3_tobytes(basepoint)",
            encoded,
            kBasepoint,
            sizeof(kBasepoint));

    spake2__ge_p3_to_p2(&p2, &p);

    spake2__ge_tobytes(encoded, &p2);
    expect_bytes(
            "ge_tobytes(basepoint)",
            encoded,
            kBasepoint,
            sizeof(kBasepoint));

    return TEST_OK;
}


static int test_identity_encoding(void)
{
    spake2__ge_p3 p;
    spake2__ge_p2 p2;
    uint8_t encoded[32];

    printf("\n[ identity encoding ]\n");

    spake2__ge_p3_0(&p);

    spake2__ge_p3_tobytes(encoded, &p);

    expect_bytes(
            "p3 identity encoding",
            encoded,
            kIdentity,
            sizeof(kIdentity));

    spake2__ge_p3_to_p2(&p2, &p);

    spake2__ge_tobytes(encoded, &p2);

    expect_bytes(
            "p2 identity encoding",
            encoded,
            kIdentity,
            sizeof(kIdentity));

    return TEST_OK;
}


static int test_invalid_point(void)
{
    spake2__ge_p3 p;
    uint8_t invalid[32];

    printf("\n[ invalid point decoding ]\n");

    /*
     * y = p is non-canonical.
     *
     * p = 2^255 - 19
     */
    memset(invalid, 0xff, sizeof(invalid));
    invalid[0] = 0xed;
    invalid[31] = 0x7f;

    expect_true(
            "reject canonical-invalid field element",
            spake2__ge_frombytes_vartime(&p, invalid) == 0);

    return TEST_OK;
}


/* ------------------------------------------------------------------------- */
/* conversions                                                               */
/* ------------------------------------------------------------------------- */

static int test_conversions(void)
{
    spake2__ge_p3 p;
    spake2__ge_p2 p2a;
    spake2__ge_p2 p2b;
    uint8_t a[32];
    uint8_t b[32];

    printf("\n[ conversions ]\n");

    if (!spake2__ge_frombytes_vartime(&p, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        ++g_failed;
        return TEST_FAIL;
    }

    spake2__ge_p3_to_p2(&p2a, &p);

    spake2__ge_tobytes(a, &p2a);

    /*
     * Reconstruct the same projective point through p1p1 -> p2.
     */
    {
        spake2__ge_p1p1 tmp;

        spake2__ge_p3_dbl(&tmp, &p);
        spake2__ge_p1p1_to_p2(&p2b, &tmp);
        spake2__ge_tobytes(b, &p2b);

        /*
         * This is 2P, so compare through another independent path later.
         */
    }

    expect_bytes(
            "p3_to_p2 keeps encoding",
            a,
            kBasepoint,
            32);

    return TEST_OK;
}


/* ------------------------------------------------------------------------- */
/* cached point + add/sub                                                    */
/* ------------------------------------------------------------------------- */

static int test_add_sub(void)
{
    spake2__ge_p3 p;
    spake2__ge_p3 zero;
    spake2__ge_p3 r3;
    spake2__ge_p1p1 r;
    spake2__ge_cached cached;
    uint8_t got[32];

    printf("\n[ add / sub ]\n");

    if (!spake2__ge_frombytes_vartime(&p, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        ++g_failed;
        return TEST_FAIL;
    }

    spake2__ge_p3_0(&zero);

    /*
     * P + 0 = P
     */
    spake2__ge_p3_to_cached(&cached, &zero);
    spake2__ge_add(&r, &p, &cached);
    spake2__ge_p1p1_to_p3(&r3, &r);
    spake2__ge_p3_tobytes(got, &r3);

    expect_bytes(
            "P + 0 = P",
            got,
            kBasepoint,
            32);

    /*
     * P - 0 = P
     */
    spake2__ge_sub(&r, &p, &cached);
    spake2__ge_p1p1_to_p3(&r3, &r);
    spake2__ge_p3_tobytes(got, &r3);

    expect_bytes(
            "P - 0 = P",
            got,
            kBasepoint,
            32);

    /*
     * P - P = 0
     */
    spake2__ge_p3_to_cached(&cached, &p);
    spake2__ge_sub(&r, &p, &cached);
    spake2__ge_p1p1_to_p3(&r3, &r);
    spake2__ge_p3_tobytes(got, &r3);

    expect_bytes(
            "P - P = 0",
            got,
            kIdentity,
            32);

    return TEST_OK;
}


/* ------------------------------------------------------------------------- */
/* doubling                                                                  */
/* ------------------------------------------------------------------------- */

static int test_doubling(void)
{
    spake2__ge_p3 p;
    spake2__ge_p1p1 r3;
    spake2__ge_p1p1 r2;
    spake2__ge_p3 p3_a;
    spake2__ge_p3 p3_b;
    spake2__ge_p2 p2;
    spake2__ge_p2 p2b;
    spake2__ge_cached cached;
    uint8_t a[32];
    uint8_t b[32];

    printf("\n[ doubling ]\n");

    if (!spake2__ge_frombytes_vartime(&p, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        ++g_failed;
        return TEST_FAIL;
    }

    /*
     * ge_p3_dbl()
     */
    spake2__ge_p3_dbl(&r3, &p);
    spake2__ge_p1p1_to_p3(&p3_a, &r3);
    spake2__ge_p3_tobytes(a, &p3_a);

    /*
     * ge_p2_dbl()
     */
    spake2__ge_p3_to_p2(&p2, &p);
    spake2__ge_p2_dbl(&r2, &p2);
    spake2__ge_p1p1_to_p2(&p2b, &r2);
    spake2__ge_tobytes(b, &p2b);

    expect_bytes(
            "p3_dbl == p2_dbl",
            a,
            b,
            32);

    /*
     * Also compare against P + P.
     */
    spake2__ge_p3_to_cached(&cached, &p);
    spake2__ge_add(&r3, &p, &cached);
    spake2__ge_p1p1_to_p3(&p3_b, &r3);
    spake2__ge_p3_tobytes(b, &p3_b);

    expect_bytes(
            "p3_dbl == P + P",
            a,
            b,
            32);

    return TEST_OK;
}


/* ------------------------------------------------------------------------- */
/* scalar multiplication                                                      */
/* ------------------------------------------------------------------------- */

static void point_from_scalar_base(
        spake2__ge_p3 *p,
        uint64_t scalar)
{
    uint8_t s[32];

    scalar_set_u64(s, scalar);
    spake2__ge_scalarmult_base(p, s);
}


static void encode_p3(
        const spake2__ge_p3 *p,
        uint8_t out[32])
{
    spake2__ge_p3_tobytes(out, p);
}


static void encode_p2(
        const spake2__ge_p2 *p,
        uint8_t out[32])
{
    spake2__ge_tobytes(out, p);
}


static int test_scalarmult_base(void)
{
    spake2__ge_p3 p1;
    spake2__ge_p3 p2;
    spake2__ge_p3 p3;
    spake2__ge_p3 zero;
    spake2__ge_p1p1 dbl;
    spake2__ge_p3 dbl3;
    uint8_t scalar[32];
    uint8_t got[32];
    uint8_t expected[32];

    printf("\n[ scalarmult_base ]\n");

    /*
     * 0B = 0
     */
    scalar_zero(scalar);
    spake2__ge_scalarmult_base(&zero, scalar);

    encode_p3(&zero, got);

    expect_bytes(
            "0 * B",
            got,
            kIdentity,
            32);

    /*
     * 1B = B
     */
    scalar_one(scalar);
    spake2__ge_scalarmult_base(&p1, scalar);

    encode_p3(&p1, got);

    expect_bytes(
            "1 * B",
            got,
            kBasepoint,
            32);

    /*
     * 2B = doubling(B)
     */
    scalar_set_u64(scalar, 2);
    spake2__ge_scalarmult_base(&p2, scalar);

    spake2__ge_p3_dbl(&dbl, &p1);
    spake2__ge_p1p1_to_p3(&dbl3, &dbl);

    encode_p3(&p2, got);
    encode_p3(&dbl3, expected);

    expect_bytes(
            "2 * B == dbl(B)",
            got,
            expected,
            32);

    /*
     * 3B = 2B + B
     */
    scalar_set_u64(scalar, 3);
    spake2__ge_scalarmult_base(&p3, scalar);

    {
        spake2__ge_cached cached;
        spake2__ge_p1p1 tmp;

        spake2__ge_p3_to_cached(&cached, &p2);
        spake2__ge_add(&tmp, &p1, &cached);
        spake2__ge_p1p1_to_p3(&dbl3, &tmp);
    }

    encode_p3(&p3, got);
    encode_p3(&dbl3, expected);

    expect_bytes(
            "3 * B == 2B + B",
            got,
            expected,
            32);

    /*
     * Make sure a non-trivial scalar works.
     */
    scalar_set_u64(scalar, 0x123456789ULL);
    spake2__ge_scalarmult_base(&p1, scalar);

    /*
     * Repeat the exact same operation and require deterministic result.
     */
    spake2__ge_scalarmult_base(&p2, scalar);

    encode_p3(&p1, got);
    encode_p3(&p2, expected);

    expect_bytes(
            "scalarmult_base deterministic",
            got,
            expected,
            32);

    return TEST_OK;
}


/* ------------------------------------------------------------------------- */
/* generic scalar multiplication                                              */
/* ------------------------------------------------------------------------- */

static int test_scalarmult(void)
{
    spake2__ge_p3 B;
    spake2__ge_p3 A;
    spake2__ge_p3 base_result;
    spake2__ge_p2 generic_result;

    uint8_t scalar[32];
    uint8_t got[32];
    uint8_t expected[32];

    printf("\n[ scalarmult ]\n");

    if (!spake2__ge_frombytes_vartime(&B, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        ++g_failed;
        return TEST_FAIL;
    }

    /*
     * [1]B
     */
    scalar_one(scalar);

    spake2__ge_scalarmult(&generic_result, scalar, &B);
    spake2__ge_tobytes(got, &generic_result);

    expect_bytes(
            "generic [1]B",
            got,
            kBasepoint,
            32);

    /*
     * [2]B should equal scalarmult_base(2).
     */
    scalar_set_u64(scalar, 2);

    spake2__ge_scalarmult(&generic_result, scalar, &B);
    spake2__ge_tobytes(expected, &generic_result);

    spake2__ge_scalarmult_base(&base_result, scalar);
    spake2__ge_p3_tobytes(got, &base_result);

    expect_bytes(
            "generic [2]B == base [2]B",
            expected,
            got,
            32);

    /*
     * Generic scalar multiplication of an arbitrary point:
     *
     * A = 3B
     * [5]A == [15]B
     */
    scalar_set_u64(scalar, 3);
    spake2__ge_scalarmult_base(&A, scalar);

    scalar_set_u64(scalar, 5);
    spake2__ge_scalarmult(&generic_result, scalar, &A);
    spake2__ge_tobytes(expected, &generic_result);

    scalar_set_u64(scalar, 15);
    spake2__ge_scalarmult_base(&base_result, scalar);
    spake2__ge_p3_tobytes(got, &base_result);

    expect_bytes(
            "[5](3B) == 15B",
            expected,
            got,
            32);

    return TEST_OK;
}


/* ------------------------------------------------------------------------- */
/* double scalar multiplication                                               */
/* ------------------------------------------------------------------------- */

static int test_double_scalarmult(void)
{
    spake2__ge_p3 A;
    spake2__ge_p3 B;
    spake2__ge_p3 A_mul;
    spake2__ge_p3 B_mul;
    spake2__ge_p3 A_plus_B;
    spake2__ge_p2 result;

    spake2__ge_cached cached;
    spake2__ge_p1p1 tmp;

    uint8_t a[32];
    uint8_t b[32];
    uint8_t expected[32];
    uint8_t got[32];

    printf("\n[ double_scalarmult_vartime ]\n");

    if (!spake2__ge_frombytes_vartime(&B, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        ++g_failed;
        return TEST_FAIL;
    }

    /*
     * A = 7B
     */
    scalar_set_u64(a, 7);
    spake2__ge_scalarmult_base(&A, a);

    /*
     * Calculate:
     *
     *     [3]A + [5]B
     *
     * independently.
     */
    scalar_set_u64(a, 3);
    spake2__ge_scalarmult(&result, a, &A);
    spake2__ge_tobytes(got, &result);

    spake2__ge_p3_tobytes(expected, &A);

    /*
     * A_mul = [3]A
     */
    scalar_set_u64(a, 3);
    spake2__ge_scalarmult(&result, a, &A);
    spake2__ge_tobytes(expected, &result);

    /*
     * B_mul = [5]B
     */
    scalar_set_u64(b, 5);
    spake2__ge_scalarmult_base(&B_mul, b);

    /*
     * Convert [3]A from p2 -> p3.
     */
    {
        spake2__ge_p3 tmp3;

        spake2__ge_tobytes(got, &result);

        if (!spake2__ge_frombytes_vartime(&tmp3, got)) {
            printf("FAIL: decode scalar multiplication result\n");
            ++g_failed;
            return TEST_FAIL;
        }

        A_mul = tmp3;
    }

    /*
     * [3]A + [5]B
     */
    spake2__ge_p3_to_cached(&cached, &B_mul);
    spake2__ge_add(&tmp, &A_mul, &cached);
    spake2__ge_p1p1_to_p3(&A_plus_B, &tmp);

    spake2__ge_p3_tobytes(expected, &A_plus_B);

    /*
     * Now double-scalar multiplication.
     */
    scalar_set_u64(a, 3);
    scalar_set_u64(b, 5);

    spake2__ge_double_scalarmult_vartime(
            &result,
            a,
            &A,
            b);

    spake2__ge_tobytes(got, &result);

    expect_bytes(
            "[3]A + [5]B",
            got,
            expected,
            32);

    return TEST_OK;
}


/* ------------------------------------------------------------------------- */
/* ge_madd / ge_msub                                                          */
/* ------------------------------------------------------------------------- */

static int test_madd_msub_identity(void)
{
    spake2__ge_p3 p;
    spake2__ge_p3 madd_result;
    spake2__ge_p3 msub_result;
    spake2__ge_p1p1 r;
    spake2__ge_precomp identity;
    uint8_t got[32];

    printf("\n[ madd / msub ]\n");

    if (!spake2__ge_frombytes_vartime(&p, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        ++g_failed;
        return TEST_FAIL;
    }

    /*
     * ge_precomp identity:
     *
     *   y + x = 1
     *   y - x = 1
     *   2dxy  = 0
     *
     * This is the identity element in the precomputed representation.
     */
    spake2__fe_1(identity.yplusx);
    spake2__fe_1(identity.yminusx);
    spake2__fe_0(identity.xy2d);

    /*
     * P + identity = P
     */
    spake2__ge_madd(&r, &p, &identity);
    spake2__ge_p1p1_to_p3(&madd_result, &r);

    spake2__ge_p3_tobytes(got, &madd_result);

    expect_bytes(
            "ge_madd(P, identity) = P",
            got,
            kBasepoint,
            32);

    /*
     * P - identity = P
     */
    spake2__ge_msub(&r, &p, &identity);
    spake2__ge_p1p1_to_p3(&msub_result, &r);

    spake2__ge_p3_tobytes(got, &msub_result);

    expect_bytes(
            "ge_msub(P, identity) = P",
            got,
            kBasepoint,
            32);

    return TEST_OK;
}


/* ------------------------------------------------------------------------- */
/* p1p1 -> p2 / p3                                                           */
/* ------------------------------------------------------------------------- */

static int test_p1p1_conversions(void)
{
    spake2__ge_p3 p;
    spake2__ge_p1p1 p1p1;
    spake2__ge_p2 p2;
    spake2__ge_p3 p3;
    uint8_t a[32];
    uint8_t b[32];

    printf("\n[ p1p1 conversions ]\n");

    if (!spake2__ge_frombytes_vartime(&p, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        ++g_failed;
        return TEST_FAIL;
    }

    spake2__ge_p3_dbl(&p1p1, &p);

    spake2__ge_p1p1_to_p2(&p2, &p1p1);
    spake2__ge_tobytes(a, &p2);

    spake2__ge_p1p1_to_p3(&p3, &p1p1);
    spake2__ge_p3_tobytes(b, &p3);

    expect_bytes(
            "p1p1_to_p2 == p1p1_to_p3",
            a,
            b,
            32);

    return TEST_OK;
}


/* ------------------------------------------------------------------------- */
/* p3 / p2 initialization                                                     */
/* ------------------------------------------------------------------------- */

static int test_initializers(void)
{
    spake2__ge_p2 p2;
    spake2__ge_p3 p3;
    uint8_t a[32];
    uint8_t b[32];

    printf("\n[ initializers ]\n");

    spake2__ge_p2_0(&p2);
    spake2__ge_tobytes(a, &p2);

    expect_bytes(
            "ge_p2_0",
            a,
            kIdentity,
            32);

    spake2__ge_p3_0(&p3);
    spake2__ge_p3_tobytes(b, &p3);

    expect_bytes(
            "ge_p3_0",
            b,
            kIdentity,
            32);

    return TEST_OK;
}


/* ------------------------------------------------------------------------- */
/* p3 -> cached                                                               */
/* ------------------------------------------------------------------------- */

static int test_cached(void)
{
    spake2__ge_p3 p;
    spake2__ge_p1p1 r;
    spake2__ge_p3 result;
    spake2__ge_cached cached;

    uint8_t got[32];

    printf("\n[ p3_to_cached ]\n");

    if (!spake2__ge_frombytes_vartime(&p, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        ++g_failed;
        return TEST_FAIL;
    }

    /*
     * Convert to cached and add it back.
     */
    spake2__ge_p3_to_cached(&cached, &p);
    spake2__ge_add(&r, &p, &cached);
    spake2__ge_p1p1_to_p3(&result, &r);

    spake2__ge_p3_tobytes(got, &result);

    /*
     * P + P = 2P.
     */
    {
        spake2__ge_p3 expected;
        uint8_t expected_bytes[32];
        uint8_t scalar[32];

        scalar_set_u64(scalar, 2);
        spake2__ge_scalarmult_base(&expected, scalar);
        spake2__ge_p3_tobytes(expected_bytes, &expected);

        expect_bytes(
                "cached P + P = 2P",
                got,
                expected_bytes,
                32);
    }

    return TEST_OK;
}


/* ------------------------------------------------------------------------- */
/* small-precomp                                                             */
/* ------------------------------------------------------------------------- */

/*
 * Build the format expected by ge_scalarmult_small_precomp():
 *
 *     [x1 || y1 || x2 || y2 || ... || x15 || y15]
 *
 * where point i = i * B.
 *
 * BoringSSL itself expands exactly this representation into ge_precomp:
 *
 *   yplusx = y + x
 *   yminusx = y - x
 *   xy2d = 2*d*x*y
 *
 * See:
 *   crypto/curve25519/curve25519.c
 */
static int point_to_affine_xy(
        const spake2__ge_p3 *p,
        uint8_t x[32],
        uint8_t y[32])
{
    spake2__fe_t zinv;
    spake2__fe_t xa;
    spake2__fe_t ya;

    /*
     * x = X / Z
     * y = Y / Z
     */
    spake2__fe_invert(zinv, p->Z);

    spake2__fe_mul(xa, p->X, zinv);
    spake2__fe_mul(ya, p->Y, zinv);

    spake2__fe_tobytes(x, xa);
    spake2__fe_tobytes(y, ya);

    return TEST_OK;
}


static int build_basepoint_small_table(
        uint8_t table[15 * 2 * 32])
{
    spake2__ge_p3 point;
    spake2__ge_p3 base;
    spake2__ge_p1p1 tmp;
    spake2__ge_cached cached;

    unsigned int i;
    uint8_t x[32];
    uint8_t y[32];

    if (!spake2__ge_frombytes_vartime(&base, kBasepoint)) {
        return TEST_FAIL;
    }

    spake2__ge_p3_0(&point);

    for (unsigned int i = 1; i <= 15; ++i) {
        /*
         * point = point + B
         */
        spake2__ge_p3_to_cached(&cached, &base);
        spake2__ge_add(&tmp, &point, &cached);
        spake2__ge_p1p1_to_p3(&point, &tmp);

        if (!point_to_affine_xy(&point, x, y)) {
            return TEST_FAIL;
        }

        memcpy(
                &table[(i - 1) * 64],
                x,
                32);

        memcpy(
                &table[(i - 1) * 64 + 32],
                y,
                32);
    }

    return TEST_OK;
}


static int test_scalarmult_small_precomp(void)
{
    uint8_t table[15 * 2 * 32];
    uint8_t scalar[32];
    uint8_t got[32];
    uint8_t expected[32];

    spake2__ge_p3 p;
    spake2__ge_p3 expected_p;

    uint64_t values[] = {
        0,
        1,
        2,
        3,
        7,
        15,
        16,
        17,
        31,
        32,
        255,
        256,
        0x12345678ULL,
        0x123456789abcdefULL
    };

    size_t i;

    printf("\n[ scalarmult_small_precomp ]\n");

    if (!build_basepoint_small_table(table)) {
        printf("FAIL: build small precomp table\n");
        ++g_failed;
        return TEST_FAIL;
    }

    for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        scalar_set_u64(scalar, values[i]);

        /*
         * Test the function under investigation.
         */
        spake2__ge_scalarmult_small_precomp(
                &p,
                scalar,
                table);

        spake2__ge_p3_tobytes(got, &p);

        /*
         * Independent reference: ordinary basepoint multiplication.
         */
        spake2__ge_scalarmult_base(
                &expected_p,
                scalar);

        spake2__ge_p3_tobytes(expected, &expected_p);

        if (!bytes_equal(got, expected, 32)) {
            char name[64];

            snprintf(
                    name,
                    sizeof(name),
                    "small_precomp scalar=%llu",
                    (unsigned long long)values[i]);

            fail_bytes(name, got, expected, 32);
        } else {
            printf(
                    "  small_precomp scalar=%llu OK\n",
                    (unsigned long long)values[i]);
        }
    }

    return TEST_OK;
}


/* ------------------------------------------------------------------------- */
/* randomized-ish consistency tests                                         */
/* ------------------------------------------------------------------------- */

static int test_scalar_consistency(void)
{
    uint64_t values_a[] = {
        1, 2, 3, 5, 7, 11, 17,
        31, 63, 127, 255, 256,
        1000, 10000, 0x123456789ULL
    };

    size_t i;
    uint8_t scalar[32];

    spake2__ge_p3 base_result;
    spake2__ge_p3 generic_point;
    spake2__ge_p2 generic_result;

    uint8_t a[32];
    uint8_t b[32];

    printf("\n[ scalar consistency ]\n");

    for (i = 0; i < sizeof(values_a) / sizeof(values_a[0]); ++i) {
        scalar_set_u64(scalar, values_a[i]);

        /*
         * Base multiplication.
         */
        spake2__ge_scalarmult_base(
                &base_result,
                scalar);

        spake2__ge_p3_tobytes(a, &base_result);

        /*
         * First reconstruct B from bytes.
         */
        if (!spake2__ge_frombytes_vartime(&generic_point, kBasepoint)) {
            printf("FAIL: basepoint decode\n");
            ++g_failed;
            return TEST_FAIL;
        }

        /*
         * Generic multiplication.
         */
        spake2__ge_scalarmult(
                &generic_result,
                scalar,
                &generic_point);

        spake2__ge_tobytes(b, &generic_result);

        if (!bytes_equal(a, b, 32)) {
            printf(
                    "FAIL: scalar consistency scalar=%llu\n",
                    (unsigned long long)values_a[i]);

            dump_hex("base", a, 32);
            dump_hex("generic", b, 32);

            ++g_failed;
        } else {
            printf(
                    "  scalar=%llu OK\n",
                    (unsigned long long)values_a[i]);
        }
    }

    return TEST_OK;
}
static int test_cached_from_p1p1(void)
{
    spake2__ge_p3 B;
    spake2__ge_p3 expected;
    spake2__ge_p3 result;

    spake2__ge_p2 B_p2;
    spake2__ge_p1p1 dbl;
    spake2__ge_cached cached;
    spake2__ge_p1p1 sum;

    uint8_t scalar[32];
    uint8_t got[32];
    uint8_t want[32];

    printf("\n[ p1p1_to_cached exact ]\n");

    if (!spake2__ge_frombytes_vartime(&B, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        return 0;
    }

    /*
     * This is exactly how ge_scalarmult() constructs Ai[2]:
     *
     *     Ai_p2[1] = B
     *     Ai[2]    = 2B
     */

    spake2__ge_p3_to_p2(&B_p2, &B);

    spake2__ge_p2_dbl(&dbl, &B_p2);

    spake2__ge_p1p1_to_cached(&cached, &dbl);

    /*
     * Now calculate:
     *
     *     B + Ai[2]
     *       = B + 2B
     *       = 3B
     */
    spake2__ge_add(&sum, &B, &cached);
    spake2__ge_p1p1_to_p3(&result, &sum);

    spake2__ge_p3_tobytes(got, &result);

    /*
     * Independent reference: 3B.
     */
    scalar_set_u64(scalar, 3);

    spake2__ge_scalarmult_base(
            &expected,
            scalar);

    spake2__ge_p3_tobytes(want, &expected);

    expect_bytes(
            "cached(p2_dbl(B)) + B = 3B",
            got,
            want,
            32);

    return 1;
}
static int test_p2_dbl_cached_path(void)
{
    spake2__ge_p3 B;
    spake2__ge_p3 p3_result;
    spake2__ge_p3 p2_result;
    spake2__ge_p2 B_p2;

    spake2__ge_p1p1 p2_dbl;
    spake2__ge_p1p1 p3_dbl;

    spake2__ge_cached cached;
    spake2__ge_p1p1 sum;

    uint8_t got[32];
    uint8_t want[32];

    printf("\n[ p2_dbl cached path ]\n");

    if (!spake2__ge_frombytes_vartime(&B, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        return 0;
    }

    /*
     * p2 path:
     *
     * B -> P2 -> p2_dbl -> P1P1 -> P3
     */
    spake2__ge_p3_to_p2(&B_p2, &B);
    spake2__ge_p2_dbl(&p2_dbl, &B_p2);
    spake2__ge_p1p1_to_p3(&p2_result, &p2_dbl);

    /*
     * p3 path:
     *
     * B -> p3_dbl -> P1P1 -> P3
     */
    spake2__ge_p3_dbl(&p3_dbl, &B);
    spake2__ge_p1p1_to_p3(&p3_result, &p3_dbl);

    spake2__ge_p3_tobytes(got, &p2_result);
    spake2__ge_p3_tobytes(want, &p3_result);

    expect_bytes(
            "p2_dbl(B) == p3_dbl(B)",
            got,
            want,
            32);

    /*
     * Now test the cached representation produced directly
     * from p2_dbl.
     */
    spake2__ge_p1p1_to_cached(&cached, &p2_dbl);

    spake2__ge_add(&sum, &B, &cached);
    spake2__ge_p1p1_to_p3(&p2_result, &sum);

    /*
     * Compare against the same operation using p3_dbl -> cached.
     */
    spake2__ge_p1p1_to_cached(&cached, &p3_dbl);

    spake2__ge_add(&sum, &B, &cached);
    spake2__ge_p1p1_to_p3(&p3_result, &sum);

    spake2__ge_p3_tobytes(got, &p2_result);
    spake2__ge_p3_tobytes(want, &p3_result);

    expect_bytes(
            "cached(p2_dbl(B)) == cached(p3_dbl(B))",
            got,
            want,
            32);

    return 1;
}
/* ------------------------------------------------------------------------- */
/* test runner                                                               */
/* ------------------------------------------------------------------------- */
static int test_add_2b_b(void)
{
    spake2__ge_p3 B;
    spake2__ge_p3 two_b;
    spake2__ge_p3 three_b_add;
    spake2__ge_p3 three_b_repeat;

    spake2__ge_p1p1 dbl;
    spake2__ge_p1p1 sum;

    spake2__ge_cached cached_b;
    spake2__ge_cached cached_2b;

    uint8_t got[32];
    uint8_t want[32];

    printf("\n[ B + 2B ]\n");

    if (!spake2__ge_frombytes_vartime(&B, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        return 0;
    }

    /*
     * Calculate 2B.
     */
    spake2__ge_p3_dbl(&dbl, &B);
    spake2__ge_p1p1_to_p3(&two_b, &dbl);

    /*
     * Calculate B + 2B using ge_add().
     */
    spake2__ge_p3_to_cached(&cached_2b, &two_b);

    spake2__ge_add(&sum, &B, &cached_2b);
    spake2__ge_p1p1_to_p3(&three_b_add, &sum);

    /*
     * Calculate 3B using repeated addition:
     *
     *     B + B = 2B
     *     2B + B = 3B
     */
    spake2__ge_p3_to_cached(&cached_b, &B);

    spake2__ge_add(&sum, &B, &cached_b);
    spake2__ge_p1p1_to_p3(&two_b, &sum);

    spake2__ge_p3_to_cached(&cached_b, &B);

    spake2__ge_add(&sum, &two_b, &cached_b);
    spake2__ge_p1p1_to_p3(&three_b_repeat, &sum);

    spake2__ge_p3_tobytes(got, &three_b_add);
    spake2__ge_p3_tobytes(want, &three_b_repeat);

    expect_bytes(
            "B + 2B == (B + B) + B",
            got,
            want,
            32);

    return 1;
}

static int test_basepoint_scalar_3(void)
{
    spake2__ge_p3 B;
    spake2__ge_p3 expected;
    spake2__ge_p3 two_b;
    spake2__ge_p3 three_b;

    spake2__ge_p1p1 tmp;
    spake2__ge_cached cached;

    uint8_t scalar[32];
    uint8_t got[32];
    uint8_t want[32];

    printf("\n[ basepoint scalar 3 ]\n");

    if (!spake2__ge_frombytes_vartime(&B, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        return 0;
    }

    /*
     * Independent calculation:
     *
     *     2B = B + B
     *     3B = 2B + B
     */
    spake2__ge_p3_to_cached(&cached, &B);

    spake2__ge_add(&tmp, &B, &cached);
    spake2__ge_p1p1_to_p3(&two_b, &tmp);

    spake2__ge_p3_to_cached(&cached, &B);

    spake2__ge_add(&tmp, &two_b, &cached);
    spake2__ge_p1p1_to_p3(&three_b, &tmp);

    /*
     * Basepoint scalar multiplication.
     */
    scalar_set_u64(scalar, 3);

    spake2__ge_scalarmult_base(&expected, scalar);

    spake2__ge_p3_tobytes(got, &three_b);
    spake2__ge_p3_tobytes(want, &expected);

    expect_bytes(
            "3B by addition == scalarmult_base(3)",
            got,
            want,
            32);

    return 1;
}
static void test_point_add_n(
        spake2__ge_p3 *r,
        const spake2__ge_p3 *p,
        unsigned n)
{
    spake2__ge_p3 acc;
    spake2__ge_cached cached;
    spake2__ge_p1p1 tmp;
    unsigned i;

    spake2__ge_p3_0(&acc);
    spake2__ge_p3_to_cached(&cached, p);

    for (i = 0; i < n; ++i) {
        spake2__ge_add(&tmp, &acc, &cached);
        spake2__ge_p1p1_to_p3(&acc, &tmp);
    }

    *r = acc;
}
static int test_double_scalarmult_independent(void)
{
    spake2__ge_p3 B;
    spake2__ge_p3 A;
    spake2__ge_p3 expected;

    spake2__ge_p2 result;

    uint8_t a[32];
    uint8_t b[32];

    uint8_t got[32];
    uint8_t want[32];

    printf("\n[ double_scalarmult independent ]\n");

    if (!spake2__ge_frombytes_vartime(&B, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        return 0;
    }

    test_point_add_n(&A, &B, 7);
    test_point_add_n(&expected, &B, 26);

    scalar_set_u64(a, 3);
    scalar_set_u64(b, 5);

    spake2__ge_double_scalarmult_vartime(
            &result,
            a,
            &A,
            b);

    spake2__ge_tobytes(got, &result);
    spake2__ge_p3_tobytes(want, &expected);

    expect_bytes(
            "[3](7B) + [5]B = 26B",
            got,
            want,
            32);

    return 1;
}
static int test_scalarmult_small(void)
{
    spake2__ge_p3 B;
    spake2__ge_p3 expected;

    spake2__ge_p2 result;

    uint8_t scalar[32];
    uint8_t got[32];
    uint8_t want[32];

    char name[64];

    unsigned i;

    printf("\n[ scalarmult small independent ]\n");

    if (!spake2__ge_frombytes_vartime(&B, kBasepoint)) {
        printf("FAIL: basepoint decode\n");
        return 0;
    }

    for (i = 0; i <= 16; ++i) {
        scalar_set_u64(scalar, i);

        /*
         * Reference:
         *
         *     iB = B + B + ... + B
         */
        test_point_add_n(&expected, &B, i);

        /*
         * Implementation under test.
         */
        spake2__ge_scalarmult(
                &result,
                scalar,
                &B);

        spake2__ge_tobytes(got, &result);
        spake2__ge_p3_tobytes(want, &expected);

        snprintf(
                name,
                sizeof(name),
                "generic [%u]B",
                i);

        expect_bytes(
                name,
                got,
                want,
                32);
    }

    return 1;
}
int main(void)
{
    int rc;

    printf("SPAKE2 group tests\n");

    rc = test_initializers();
    if (!rc) {
        return 1;
    }

    rc = test_identity_encoding();
    if (!rc) {
        return 1;
    }

    rc = test_frombytes_tobytes();
    if (!rc) {
        return 1;
    }

    rc = test_invalid_point();
    if (!rc) {
        return 1;
    }

    rc = test_conversions();
    if (!rc) {
        return 1;
    }

    rc = test_cached();
    if (!rc) {
        return 1;
    }

    rc = test_add_sub();
    if (!rc) {
        return 1;
    }

    rc = test_doubling();
    if (!rc) {
        return 1;
    }

    rc = test_p1p1_conversions();
    if (!rc) {
        return 1;
    }
    
    rc = test_scalarmult_small();
    if (!rc) {
        return 1;
    }
    rc = test_double_scalarmult_independent();
    if (!rc) {
        return 1;
    }
    
    rc = test_add_2b_b();
    if (!rc) {
        return 1;
    }
    rc = test_p2_dbl_cached_path();
    if (!rc) {
        return 1;
    }
    rc = test_cached_from_p1p1();
    if (!rc) {
        return 1;
    }

    rc = test_basepoint_scalar_3();
    if (!rc) {
        return 1;
    }
    rc = test_scalarmult_base();
    if (!rc) {
        return 1;
    }

    rc = test_scalarmult();
    if (!rc) {
        return 1;
    }

    rc = test_double_scalarmult();
    if (!rc) {
        return 1;
    }

    rc = test_madd_msub_identity();
    if (!rc) {
        return 1;
    }

    rc = test_scalarmult_small_precomp();
    if (!rc) {
        return 1;
    }

    rc = test_scalar_consistency();
    if (!rc) {
        return 1;
    }

    printf("\n");

    if (g_failed != 0) {
        printf("FAILED: %d test(s)\n", g_failed);
        return 1;
    }

    printf("ALL TESTS PASSED\n");
    return 0;
}
