// SPDX-License-Identifier: MIT
//
// The launcher's update check: which GitHub release to offer and how to check its download. From
// PrimedGun v2's launcher (Nobbie's verified GitHub updates, iChris4/MetroidPrimePortVR), for
// PrimedGun Echoes' releases and Windows package.
#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QString>

namespace PrimedGunLauncher::Updates {

// The GitHub repository whose releases the launcher offers.
inline constexpr const char* kRepo = "iChris4/MetroidPrime2PortVR";

struct Release {
  QString version;     // "1.0.0-alpha.3", no leading v
  QString notes;       // the release body
  QString page;        // the release's page on GitHub
  QString name;        // the Windows package's file name
  QString url;         // its download
  QString digest;      // its SHA-256 from the GitHub asset digest, lower-case hex; empty when absent
  QString checksumUrl; // the release's SHA256SUMS.txt, for a package without a digest
  qint64 size = 0;
  bool valid() const { return !url.isEmpty(); }
};

// Semantic versions with an optional alpha/beta/rc stage ("v" prefix allowed): <0, 0, >0 like
// strcmp. A stable version is newer than every preview of it. 0 when either is not a version.
int CompareVersions(QString a, QString b);
bool ValidVersion(const QString& text);
// Only this repository's release downloads are fetched.
bool TrustedAsset(const QString& url);
// The newest release after `current` with an uploaded Windows package, out of a GitHub
// releases-API reply; previews (alpha, beta, rc) only when `previews` is set. Drafts are skipped.
Release Select(const QJsonArray& releases, const QString& current, bool previews);
// `name`'s SHA-256 in a sha256sum-style manifest, lower-case; empty when absent or listed twice.
QString Checksum(const QByteArray& text, const QString& name);

} // namespace PrimedGunLauncher::Updates
