// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <string_view>

#include <QWidget>

#include "Common/EnumMap.h"
#include "Core/HW/EXI/EXI.h"

class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QString;
class QVBoxLayout;

class GameCubePane : public QWidget
{
  Q_OBJECT
public:
  explicit GameCubePane();

  static std::string GetOpenGBARom(std::string_view title);

private:
  void CreateWidgets();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  void OnEmulationStateChanged();

  void UpdateButton(ExpansionInterface::Slot slot);
  void OnConfigPressed(ExpansionInterface::Slot slot);

  void BrowseMemcard(ExpansionInterface::Slot slot);
  bool SetMemcard(ExpansionInterface::Slot slot, const QString& filename);
  void BrowseGCIFolder(ExpansionInterface::Slot slot);
  bool SetGCIFolder(ExpansionInterface::Slot slot, const QString& path);
  void BrowseAGPRom(ExpansionInterface::Slot slot);
  void SetAGPRom(ExpansionInterface::Slot slot, const QString& filename);
  void BrowseGBABios();
  void BrowseGBARom(size_t index);
  void SaveRomPathChanged();
  void BrowseGBASaves();

  QCheckBox* m_skip_main_menu;
  QComboBox* m_language_combo;

  enum
  {
    SLOT_A_INDEX,
    SLOT_SP1_INDEX,
    SLOT_COUNT
  };

  QPushButton* m_slot_buttons[SLOT_COUNT];
  QComboBox* m_slot_combos[SLOT_COUNT];

  QCheckBox* m_gba_threads;
  QCheckBox* m_gba_save_rom_path;
  QPushButton* m_gba_browse_bios;
  QLineEdit* m_gba_bios_edit;
  std::array<QPushButton*, 4> m_gba_browse_roms;
  std::array<QLineEdit*, 4> m_gba_rom_edits;
  QPushButton* m_gba_browse_saves;
  QLineEdit* m_gba_saves_edit;
};
