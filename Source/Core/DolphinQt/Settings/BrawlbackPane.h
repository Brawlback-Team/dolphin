#pragma once

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QWidget>

class BrawlbackPane : public QWidget
{
  Q_OBJECT
public:
  explicit BrawlbackPane();

private:
  void CreateWidgets();
  void ConnectWidgets();
  void LoadSettings();
  void SaveSettings();

  QLabel* m_delay_frames_label = nullptr;
  QSpinBox* m_delay_frames = nullptr;
  QCheckBox* m_force_custom_netplay_port = nullptr;
  QSpinBox* m_custom_netplay_port = nullptr;
  QCheckBox* m_force_lan_ip = nullptr;
  QLineEdit* m_lan_ip = nullptr;
};
