#include <QGridLayout>

#include "Common/Config/Config.h"
#include "Core/ConfigManager.h"
#include "DolphinQt/Settings/BrawlbackPane.h"

BrawlbackPane::BrawlbackPane()
{
  CreateWidgets();
  LoadSettings();
  ConnectWidgets();
}
void BrawlbackPane::CreateWidgets()
{
  m_delay_frames_label = new QLabel(tr("Delay Frames"));
  m_delay_frames = new QSpinBox();
  m_delay_frames->setMinimum(1);
  m_delay_frames->setMaximum(7);

  m_force_custom_netplay_port = new QCheckBox(tr("Force Netplay Port"));
  m_custom_netplay_port = new QSpinBox();
  m_custom_netplay_port->setMinimum(1);
  m_custom_netplay_port->setMaximum(65535);

  m_force_lan_ip = new QCheckBox(tr("Force LAN IP"));
  m_lan_ip = new QLineEdit();
  {
    auto size_policy = m_lan_ip->sizePolicy();
    size_policy.setRetainSizeWhenHidden(true);
    m_lan_ip->setSizePolicy(size_policy);
  }

  auto grid = new QGridLayout(this);
  grid->addWidget(m_delay_frames_label, 0, 0);
  grid->addWidget(m_delay_frames, 0, 1);
  grid->addWidget(m_force_custom_netplay_port, 1, 0);
  grid->addWidget(m_custom_netplay_port, 1, 1);
  grid->addWidget(m_force_lan_ip, 2, 0);
  grid->addWidget(m_lan_ip, 2, 1);

  // Fix spin box sizes so they don't stretch to column size
  m_delay_frames->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_custom_netplay_port->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  auto hSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
  grid->addItem(hSpacer, 0, grid->columnCount());
  auto vSpacer = new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
  grid->addItem(vSpacer, grid->rowCount(), 0);
}
void BrawlbackPane::ConnectWidgets()
{
  connect(m_delay_frames, qOverload<int>(&QSpinBox::valueChanged), this,
          &BrawlbackPane::SaveSettings);
  connect(m_force_custom_netplay_port, &QCheckBox::stateChanged, this, [this]() {
    m_custom_netplay_port->setVisible(m_force_custom_netplay_port->isChecked());
    SaveSettings();
  });
  connect(m_custom_netplay_port, qOverload<int>(&QSpinBox::valueChanged), this,
          &BrawlbackPane::SaveSettings);
  connect(m_force_lan_ip, &QCheckBox::stateChanged, this, [this]() {
    m_lan_ip->setVisible(m_force_lan_ip->isChecked());
    SaveSettings();
  });
  connect(m_lan_ip, &QLineEdit::editingFinished, this, &BrawlbackPane::SaveSettings);
}

void BrawlbackPane::LoadSettings()
{
  const SConfig& params = SConfig::GetInstance();

  m_delay_frames->setValue(params.m_delayFrames);

  m_force_custom_netplay_port->setChecked(params.m_slippiForceNetplayPort);
  m_custom_netplay_port->setValue(params.m_slippiNetplayPort);
  m_custom_netplay_port->setVisible(params.m_slippiForceNetplayPort);

  m_force_lan_ip->setChecked(params.m_slippiForceLanIp);
  m_lan_ip->setText(QString::fromStdString(params.m_slippiLanIp));
  m_lan_ip->setVisible(params.m_slippiForceLanIp);
}
void BrawlbackPane::SaveSettings()
{
  Config::ConfigChangeCallbackGuard config_guard;

  SConfig& params = SConfig::GetInstance();

  params.m_delayFrames = m_delay_frames->value();
  params.m_slippiForceNetplayPort = m_force_custom_netplay_port->isChecked();
  params.m_slippiNetplayPort = m_custom_netplay_port->value();
  params.m_slippiForceLanIp = m_force_lan_ip->isChecked();
  params.m_slippiLanIp = m_lan_ip->text().toStdString();
}
