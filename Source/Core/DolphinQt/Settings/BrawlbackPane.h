#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>
#include <QPushButton>

class BrawlbackPane : public QWidget
{
  Q_OBJECT
public:
  explicit BrawlbackPane();

private:
  void CreateWidgets();
  void LayoutWidgets();
  void ConnectWidgets();
  void LoadSettings();
  void SaveSettings();

  //Online settings
  QLabel* m_delay_frames_label = nullptr;
  QSpinBox* m_delay_frames = nullptr;
  QCheckBox* m_force_custom_netplay_port = nullptr;
  QSpinBox* m_custom_netplay_port = nullptr;
  QCheckBox* m_force_lan_ip = nullptr;
  QLineEdit* m_lan_ip = nullptr;

  //Replay settings
  QCheckBox* m_save_replays = nullptr;
  QLabel* m_replay_folder_label = nullptr;
  QLineEdit* m_replays_folder = nullptr;
  QPushButton* m_browse_replays_folder = nullptr;
};
