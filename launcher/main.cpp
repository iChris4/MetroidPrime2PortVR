// SPDX-License-Identifier: GPL-3.0-or-later
//
// PrimedGunEchoes.exe: the PrimedGun launcher for the native Metroid Prime 2: Echoes port.

#include "ui/launcher_window.h"
#include "ui/style.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QApplication::setApplicationName(QStringLiteral("PrimedGun Echoes"));
  QApplication::setApplicationVersion(QStringLiteral(PRIMEDGUN_LAUNCHER_VERSION));
  QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
  QApplication::setPalette(PrimedGunLauncher::Style::DarkPalette(QApplication::palette()));
  QApplication::setWindowIcon(QIcon(QStringLiteral(":/PrimedGun.png")));

  PrimedGunLauncher::LauncherWindow window;
  window.show();
  return QApplication::exec();
}
