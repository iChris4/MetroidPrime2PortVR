// SPDX-License-Identifier: MIT
//
// The launcher's update check (launcher/ui/update_release.cpp): version order, which release is
// offered, trusted downloads only, checksum manifests. From PrimedGun v2's launcher tests.
// With a file argument (a saved GitHub releases-API reply), checks it against the live schema too.
#include "update_release.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstdio>
#include <cstdlib>

using namespace PrimedGunLauncher::Updates;

namespace {

void Check(bool ok, const char* why) {
  if (!ok) {
    std::fprintf(stderr, "updater: %s\n", why);
    std::abort();
  }
}

QJsonObject Make(const QString& version, bool preview = true, const QString& suffix = "Windows-x64.zip") {
  const QString name = "PrimedGunEchoes-v" + version + "-" + suffix;
  return {{"tag_name", "v" + version},
          {"prerelease", preview},
          {"assets", QJsonArray{QJsonObject{
                         {"name", name},
                         {"state", "uploaded"},
                         {"size", 1234},
                         {"digest", "sha256:" + QString(64, 'a')},
                         {"browser_download_url",
                          "https://github.com/iChris4/MetroidPrime2PortVR/releases/download/v" + version + "/" + name},
                     }}}};
}

} // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Check(CompareVersions("v1.0.0-alpha.10", "1.0.0-alpha.9") > 0, "numeric alpha ordering");
  Check(CompareVersions("1.0.0", "1.0.0-rc.99") > 0, "stable newer than rc");
  Check(CompareVersions("1.0.0-rc.1", "1.0.0-beta.99") > 0, "rc newer than beta");
  Check(CompareVersions("1.0.0-beta.1", "1.0.0-alpha.99") > 0, "beta newer than alpha");
  Check(CompareVersions("1.1.0-alpha.1", "1.0.9") > 0, "major/minor before channel");
  Check(CompareVersions("v1.0.0-alpha.2", "1.0.0-alpha.2") == 0, "ignore v prefix");
  Check(!ValidVersion("v1.0.0-alpha.02") && !ValidVersion("garbage"), "invalid tags rejected");

  QJsonArray releases{Make("1.0.0-alpha.3"), Make("1.0.0-alpha.10"), Make("1.0.0-alpha.4"),
                      Make("1.0.0-alpha.11", true, "Windows-x64-symbols.zip")};
  Check(Select(releases, "1.0.0-alpha.2", true).version == "1.0.0-alpha.10",
        "highest ready platform release, not API order or symbols");
  Check(!Select(releases, "1.0.0-alpha.2", false).valid(), "stable channel excludes previews");
  Check(!Select(releases, "1.0.0-alpha.10", true).valid(), "never downgrade or reinstall");
  releases.append(Make("1.0.0", false));
  Check(Select(releases, "1.0.0-alpha.2", false).version == "1.0.0", "stable channel accepts final");

  QJsonObject draft = Make("2.0.0", false);
  draft["draft"] = true;
  Check(!Select({draft}, "1.0.0", true).valid(), "draft ignored");
  // Prime 1's package name in this repository is not this launcher's.
  QJsonObject other = Make("2.0.0", false);
  QJsonArray otherAssets = other["assets"].toArray();
  QJsonObject otherAsset = otherAssets[0].toObject();
  otherAsset["name"] = "PrimedGun-v2.0.0-Windows-x64.zip";
  otherAssets[0] = otherAsset;
  other["assets"] = otherAssets;
  Check(!Select({other}, "1.0.0", true).valid(), "another product's package ignored");
  QJsonObject bad = Make("2.0.0", false);
  QJsonArray assets = bad["assets"].toArray();
  QJsonObject asset = assets[0].toObject();
  asset["browser_download_url"] = "https://example.com/bad.zip";
  assets[0] = asset;
  bad["assets"] = assets;
  Check(!Select({bad}, "1.0.0", true).valid(), "foreign download rejected");

  const QByteArray digest = QString(64, 'a').toUtf8();
  Check(Checksum(digest + "  file.zip\r\n", "file.zip") == QString(64, 'a'), "exact checksum match");
  Check(Checksum(digest + " *file.zip\n", "file.zip") == QString(64, 'a'), "binary checksum syntax");
  Check(Checksum(digest + "  other.zip\n", "file.zip").isEmpty(), "wrong asset checksum rejected");
  Check(Checksum(digest + "  file.zip\n" + digest + "  file.zip\n", "file.zip").isEmpty(),
        "duplicate checksum rejected");

  if (argc > 1) {
    QFile file(QString::fromLocal8Bit(argv[1]));
    Check(file.open(QIODevice::ReadOnly), "live release file readable");
    const Release r = Select(QJsonDocument::fromJson(file.readAll()).array(), "0.9.0", true);
    Check(r.valid() && !r.digest.isEmpty(), "published GitHub schema/asset/digest compatible");
    std::printf("live: %s %s\n", qPrintable(r.version), qPrintable(r.url));
  }
  std::puts("Update version, release selection and checksum checks passed.");
  return 0;
}
