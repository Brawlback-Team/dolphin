// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "Common/CommonTypes.h"

class PointerWrap;

namespace HSP
{
class IHSPDevice;
enum class HSPDeviceType : int;

void Init();
void Shutdown();

u64 Read(u32 address);
void Write(u32 address, u64 value);

void DoState(PointerWrap& p);

void RemoveDevice();
void AddDevice(std::unique_ptr<IHSPDevice> device);
void AddDevice(HSPDeviceType device);
}  // namespace HSP
