// SPDX-License-Identifier: MIT
//
// Setup > Check for updates. From PrimedGun v2's launcher (Nobbie's verified GitHub updates), for
// PrimedGun Echoes: asks GitHub's releases API for a newer release with a Windows package, shows
// its notes, downloads the package with Windows' own curl.exe, checks its SHA-256 (the GitHub
// asset digest, else the release's SHA256SUMS.txt) and hands it to update-windows.ps1, which waits
// for the launcher and the game to close, replaces the installed files (never the user folder),
// keeps the old ones for a rollback and starts the new launcher. Nothing is installed unasked.

#include "update_dialog.h"

#include "tab_context.h"
#include "update_release.h"

#include <QApplication>
#include <QCheckBox>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLabel>
#include <QPointer>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>

#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace PrimedGunLauncher {
namespace {

constexpr qint64 kReleaseListMax = 8 * 1024 * 1024;
constexpr qint64 kChecksumsMax = 1024 * 1024;

QString ReleasesPage() {
  return QStringLiteral("https://github.com/%1/releases").arg(QLatin1String(Updates::kRepo));
}

class UpdateDialog final : public QDialog {
public:
  explicit UpdateDialog(TabContext& context) : QDialog(context.window), m_ctx(context) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("PrimedGun Echoes updates"));
    resize(620, 500);
    m_work = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/PrimedGunEchoes-update-" +
             QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDir().mkpath(m_work);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Installed: %1").arg(QStringLiteral(PRIMEDGUN_LAUNCHER_VERSION))));
    m_previews = new QCheckBox(tr("Include alpha, beta and release candidate versions"));
    // Previews are offered by default while a preview is installed.
    QSettings prefs(m_ctx.paths.launcherIni, QSettings::IniFormat);
    m_previews->setChecked(
        prefs.value("updates/include_previews", QStringLiteral(PRIMEDGUN_LAUNCHER_VERSION).contains('-')).toBool());
    layout->addWidget(m_previews);
    m_check = new QPushButton(tr("Check for updates"));
    layout->addWidget(m_check);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    m_progress = new QProgressBar;
    m_progress->hide();
    layout->addWidget(m_progress);
    m_notes = new QTextBrowser;
    m_notes->setOpenExternalLinks(false);
    layout->addWidget(m_notes, 1);
    m_action = new QPushButton(tr("Download update"));
    m_action->setEnabled(false);
    layout->addWidget(m_action);
    auto* page = new QPushButton(tr("Open GitHub releases"));
    layout->addWidget(page);

    connect(page, &QPushButton::clicked, this,
            [this] { QDesktopServices::openUrl(QUrl(m_release.valid() ? m_release.page : ReleasesPage())); });
    connect(m_check, &QPushButton::clicked, this, [this] { Check(); });
    connect(m_action, &QPushButton::clicked, this, [this] {
      if (m_verified) {
        Install();
      } else {
        StartDownload();
      }
    });
    QTimer::singleShot(0, this, [this] { Check(); });
  }

  ~UpdateDialog() override {
    if (m_transfer != nullptr) {
      m_transfer->kill();
      m_transfer->waitForFinished(1000);
    }
  }

private:
  void Error(const QString& message) {
    m_status->setText(message);
    m_check->setEnabled(true);
    m_previews->setEnabled(true);
    m_action->setEnabled(m_release.valid());
    m_progress->hide();
  }

  // Downloads `url` to `output` with curl.exe (https only, at most `maximum` bytes), then `done`.
  void Fetch(const QString& url, const QString& output, qint64 maximum, std::function<void()> done) {
    m_check->setEnabled(false);
    m_previews->setEnabled(false);
    m_action->setEnabled(false);
    m_progress->setRange(0, 0);
    m_progress->show();
#ifdef Q_OS_WIN
    const QString curl = QDir(qEnvironmentVariable("SystemRoot", "C:/Windows")).filePath("System32/curl.exe");
#else
    const QString curl = QStringLiteral("curl");
#endif
    auto* process = new QProcess(this);
    m_transfer = process;
    process->setProgram(curl);
    process->setArguments({"--fail", "--location", "--proto", "=https", "--proto-redir", "=https",
                           "--connect-timeout", "15", "--max-time", maximum > kReleaseListMax ? "900" : "45",
                           "--max-filesize", QString::number(maximum), "--silent", "--show-error", "--user-agent",
                           "PrimedGunEchoes-Updater/" PRIMEDGUN_LAUNCHER_VERSION, "--header",
                           "Accept: application/vnd.github+json", "--output", output, "--url", url});
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
      if (error == QProcess::FailedToStart) {
        m_transfer = nullptr;
        process->deleteLater();
        Error(tr("Could not start the system download client: %1").arg(process->errorString()));
      }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process, output, maximum, done](int code, QProcess::ExitStatus state) {
              m_transfer = nullptr;
              const QString detail = QString::fromUtf8(process->readAllStandardError()).left(600);
              process->deleteLater();
              if (code != 0 || state != QProcess::NormalExit || QFileInfo(output).size() > maximum) {
                Error(tr("Update download failed. Check your connection and try again. %1").arg(detail));
                return;
              }
              m_progress->hide();
              done();
            });
    process->start();
  }

  void Offer() {
    m_check->setEnabled(true);
    m_previews->setEnabled(true);
    m_progress->hide();
    if (!m_release.valid()) {
      m_status->setText(tr("You are up to date for the selected release channel."));
      return;
    }
    m_status->setText(tr("PrimedGun Echoes %1 is available (%2 MB).")
                          .arg(m_release.version)
                          .arg(m_release.size / 1048576.0, 0, 'f', 1));
    m_notes->setPlainText(m_release.notes);
    m_action->setText(tr("Download update"));
    m_action->setEnabled(true);
  }

  void Check() {
    m_release = {};
    m_verified = false;
    m_action->setEnabled(false);
    m_notes->clear();
    m_status->setText(tr("Checking GitHub releases..."));
    QSettings prefs(m_ctx.paths.launcherIni, QSettings::IniFormat);
    prefs.setValue("updates/include_previews", m_previews->isChecked());
    const QString list = m_work + "/releases.json";
    Fetch(QStringLiteral("https://api.github.com/repos/%1/releases?per_page=100").arg(QLatin1String(Updates::kRepo)),
          list, kReleaseListMax, [this, list] {
            QFile file(list);
            if (!file.open(QIODevice::ReadOnly)) {
              Error(tr("Could not read release information."));
              return;
            }
            QJsonParseError error;
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !doc.isArray()) {
              Error(tr("GitHub returned invalid release information."));
              return;
            }
            m_release = Updates::Select(doc.array(), QStringLiteral(PRIMEDGUN_LAUNCHER_VERSION),
                                        m_previews->isChecked());
            Offer();
          });
  }

  void Download() {
    m_status->setText(tr("Downloading %1...").arg(m_release.name));
    m_archive = m_work + "/update.zip";
    Fetch(m_release.url, m_archive, m_release.size, [this] {
      m_status->setText(tr("Verifying SHA-256 checksum..."));
      QFile file(m_archive);
      QCryptographicHash hash(QCryptographicHash::Sha256);
      if (!file.open(QIODevice::ReadOnly) || file.size() != m_release.size || !hash.addData(&file) ||
          QString::fromLatin1(hash.result().toHex()) != m_release.digest.toLower()) {
        Error(tr("Update verification failed. Nothing has been installed. Check for updates to retry."));
        m_action->setEnabled(false);
        return;
      }
      m_verified = true;
      m_check->setEnabled(true);
      m_previews->setEnabled(true);
      m_status->setText(tr("Download verified. Close the game before installing. Your user folder will be kept."));
      m_action->setText(tr("Install and restart launcher"));
      m_action->setEnabled(true);
    });
  }

  void StartDownload() {
    if (!m_release.digest.isEmpty()) {
      Download();
      return;
    }
    if (m_release.checksumUrl.isEmpty()) {
      Error(tr("This release has no SHA-256 checksum. Open its release page to update by hand."));
      m_action->setEnabled(false);
      return;
    }
    m_status->setText(tr("Downloading checksums..."));
    const QString sums = m_work + "/checksums.txt";
    Fetch(m_release.checksumUrl, sums, kChecksumsMax, [this, sums] {
      QFile file(sums);
      if (file.open(QIODevice::ReadOnly)) {
        m_release.digest = Updates::Checksum(file.readAll(), m_release.name);
      }
      if (m_release.digest.isEmpty()) {
        Error(tr("No unambiguous checksum was found for this Windows package."));
        m_action->setEnabled(false);
        return;
      }
      Download();
    });
  }

  void Install() {
#ifdef Q_OS_WIN
    if (!m_ctx.prepareUpdate || !m_ctx.prepareUpdate()) {
      return;
    }
    QFile script(":/update-windows.ps1");
    if (!script.copy(m_work + "/apply-update.ps1")) {
      Error(tr("Could not prepare the update helper. Check for updates to retry."));
      return;
    }
    QProcess updater;
    updater.setProgram(QDir(qEnvironmentVariable("SystemRoot", "C:/Windows"))
                           .filePath("System32/WindowsPowerShell/v1.0/powershell.exe"));
    updater.setArguments({"-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File",
                          m_work + "/apply-update.ps1", "-Archive", m_archive, "-Destination", m_ctx.paths.exeFolder,
                          "-LauncherPid", QString::number(QCoreApplication::applicationPid()), "-ExpectedHash",
                          m_release.digest});
    updater.setWorkingDirectory(m_work);
    updater.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* args) { args->flags |= CREATE_NO_WINDOW; });
    if (!updater.startDetached()) {
      Error(tr("Could not start the updater. Your installation has not changed."));
      return;
    }
    QApplication::quit();
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_work));
#endif
  }

  TabContext& m_ctx;
  QLabel* m_status = nullptr;
  QCheckBox* m_previews = nullptr;
  QPushButton* m_check = nullptr;
  QPushButton* m_action = nullptr;
  QTextBrowser* m_notes = nullptr;
  QProgressBar* m_progress = nullptr;
  QProcess* m_transfer = nullptr;
  Updates::Release m_release;
  QString m_work;
  QString m_archive;
  bool m_verified = false;
};

} // namespace

void CheckForUpdates(TabContext& ctx) {
  static QPointer<UpdateDialog> current;
  if (current) {
    current->raise();
    current->activateWindow();
    return;
  }
  current = new UpdateDialog(ctx);
  current->show();
}

} // namespace PrimedGunLauncher
