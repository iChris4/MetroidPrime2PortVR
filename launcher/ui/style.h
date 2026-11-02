// SPDX-License-Identifier: MIT
//
// PrimedGun's launcher look, from DolphinQt/MainWindow.cpp ConnectStack, and
// a dark blue and purple Echoes palette. Added
// here: the "not active yet" tag and a disabled button colour, for the Setup
// buttons that wait while the game runs.

#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

namespace PrimedGunLauncher::Style {

inline const QString kPage = QStringLiteral(R"(
    QWidget { background: #0f121a; color: #f1eaf7; font-family: Consolas, monospace; font-size: 12px; }
    QFrame#PrimedGunPanel, QTabWidget::pane { background: #131724; border: 1px solid #30354a; border-radius: 5px; }
    QLabel#PrimedGunTitle, QLabel#PrimedGunSection { color: #c49aff; }
    QLabel#PrimedGunNotice { color: #c49aff; font-size: 14px; font-weight: bold; }
    QLabel#PrimedGunMuted { color: #a298ad; }
    QLabel#PrimedGunTag { color: #83758f; font-style: italic; }
    QLabel#PrimedGunBad { color: #ff5b45; }
    QLabel#PrimedGunGood { color: #38d86f; }
    QPushButton { background: #1d2030; border: 1px solid #1d2030; border-radius: 4px; color: #f1eaf7; padding: 5px 10px; }
    QPushButton:hover { background: #282b40; border-color: #624577; }
    QPushButton:pressed { background: #33283f; border-color: #a979d6; color: #c49aff; }
    QPushButton:disabled { color: #82748e; }
    QTabBar::tab { background: #181c29; color: #f1eaf7; padding: 5px 12px; border-top-left-radius: 4px; border-top-right-radius: 4px; margin-right: 2px; }
    QTabBar::tab:selected { background: #382a47; }
    QTabBar::tab:hover { background: #25273a; }
    QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px; background: #1d2030; }
    QCheckBox::indicator:checked { background: #a46bd6; }
    QRadioButton::indicator { width: 18px; height: 18px; border-radius: 9px; background: #1a1e2c; }
    QRadioButton::indicator:checked { background: #c49aff; }
    QSlider::groove:horizontal { height: 22px; border-radius: 4px; background: #181c29; }
    QSlider::handle:horizontal { width: 12px; margin: 2px 0; border-radius: 5px; background: #a46bd6; }
    QDoubleSpinBox { background: #181c29; border: 1px solid #181c29; color: #f1eaf7; border-radius: 4px; padding: 4px; }
    QScrollArea { border: 1px solid #30354a; border-radius: 5px; background: #131724; }
  )");

inline const QString kGameButton = QStringLiteral(R"(
    QPushButton {
      background-color: #1d2030;
      border: 1px solid #383c52;
      border-radius: 4px;
      color: #f1eaf7;
      min-height: 24px;
      padding: 4px 12px;
    }
    QPushButton:hover {
      background-color: #282b40;
      border-color: #a979d6;
      color: #ffffff;
    }
    QPushButton:pressed {
      background-color: #33283f;
      border-color: #a979d6;
      color: #a979d6;
    }
    QPushButton:disabled {
      background-color: #171a24;
      border-color: #171a24;
      color: #82748e;
    }
  )");

inline const QString kPrimaryGameButton = kGameButton + QStringLiteral(R"(
    QPushButton {
      background-color: #30233d;
      border-color: #674880;
    }
  )");

inline QPalette DarkPalette(QPalette palette) {
  palette.setColor(QPalette::Window, QColor(15, 18, 26));
  palette.setColor(QPalette::WindowText, QColor(235, 224, 244));
  palette.setColor(QPalette::Base, QColor(15, 18, 26));
  palette.setColor(QPalette::AlternateBase, QColor(29, 32, 48));
  palette.setColor(QPalette::PlaceholderText, QColor(162, 152, 173));
  palette.setColor(QPalette::Text, QColor(235, 224, 244));
  palette.setColor(QPalette::Button, QColor(29, 32, 48));
  palette.setColor(QPalette::ButtonText, QColor(241, 234, 247));
  palette.setColor(QPalette::BrightText, QColor(169, 121, 214));
  palette.setColor(QPalette::Highlight, QColor(169, 121, 214));
  palette.setColor(QPalette::HighlightedText, QColor(15, 18, 26));
  palette.setColor(QPalette::Link, QColor(169, 121, 214));
  palette.setColor(QPalette::LinkVisited, QColor(169, 121, 214));
  return palette;
}

} // namespace PrimedGunLauncher::Style
