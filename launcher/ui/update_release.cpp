// SPDX-License-Identifier: MIT
//
// From PrimedGun v2's launcher (Nobbie's verified GitHub updates), for PrimedGun Echoes.

#include "update_release.h"

#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>

namespace PrimedGunLauncher::Updates {
namespace {

const QRegularExpression kVersionPattern(QStringLiteral(
    R"(^v?(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-(alpha|beta|rc)\.(0|[1-9][0-9]*))?$)"));

QString Canonical(QString version) {
  if (version.startsWith('v')) {
    version.remove(0, 1);
  }
  return version;
}

QString ReleasesPrefix() {
  return QStringLiteral("https://github.com/%1/releases/").arg(QLatin1String(kRepo));
}

} // namespace

bool ValidVersion(const QString& text) { return kVersionPattern.match(text).hasMatch(); }

int CompareVersions(QString a, QString b) {
  const auto x = kVersionPattern.match(a);
  const auto y = kVersionPattern.match(b);
  if (!x.hasMatch() || !y.hasMatch()) {
    return 0;
  }
  for (int i : {1, 2, 3, 4, 5}) {
    const QString left = x.captured(i);
    const QString right = y.captured(i);
    if (i == 4) {
      // No stage (a stable release) sorts after every preview stage.
      const QStringList stages{QStringLiteral("alpha"), QStringLiteral("beta"), QStringLiteral("rc"), QString()};
      const int l = stages.indexOf(left);
      const int r = stages.indexOf(right);
      if (l != r) {
        return l < r ? -1 : 1;
      }
    } else {
      // Numbers without leading zeros: the longer is the larger.
      if (left.size() != right.size()) {
        return left.size() < right.size() ? -1 : 1;
      }
      const int c = QString::compare(left, right);
      if (c != 0) {
        return c < 0 ? -1 : 1;
      }
    }
  }
  return 0;
}

bool TrustedAsset(const QString& url) { return url.startsWith(ReleasesPrefix() + QStringLiteral("download/")); }

Release Select(const QJsonArray& releases, const QString& current, bool previews) {
  static const QRegularExpression kHex64(QStringLiteral("^[0-9a-fA-F]{64}$"));
  Release best;
  for (const auto& value : releases) {
    const QJsonObject release = value.toObject();
    const QString tag = release[QStringLiteral("tag_name")].toString();
    if (release[QStringLiteral("draft")].toBool() || !ValidVersion(tag) || CompareVersions(tag, current) <= 0 ||
        (!previews && (release[QStringLiteral("prerelease")].toBool() || tag.contains('-')))) {
      continue;
    }
    if (best.valid() && CompareVersions(tag, best.version) <= 0) {
      continue;
    }
    Release candidate;
    candidate.version = Canonical(tag);
    candidate.notes = release[QStringLiteral("body")].toString();
    candidate.page = ReleasesPrefix() + QStringLiteral("tag/") + tag;
    const QString expected = QStringLiteral("PrimedGunEchoes-v%1-Windows-x64.zip").arg(candidate.version);
    for (const auto& assetValue : release[QStringLiteral("assets")].toArray()) {
      const QJsonObject asset = assetValue.toObject();
      const QString url = asset[QStringLiteral("browser_download_url")].toString();
      if (!TrustedAsset(url) || asset[QStringLiteral("state")].toString() != QStringLiteral("uploaded")) {
        continue;
      }
      const QString name = asset[QStringLiteral("name")].toString();
      if (name == QStringLiteral("SHA256SUMS.txt")) {
        candidate.checksumUrl = url;
      }
      if (name == expected) {
        candidate.name = expected;
        candidate.url = url;
        candidate.size = asset[QStringLiteral("size")].toVariant().toLongLong();
        candidate.digest = asset[QStringLiteral("digest")].toString();
        if (candidate.digest.startsWith(QStringLiteral("sha256:"))) {
          candidate.digest.remove(0, 7);
        } else {
          candidate.digest.clear();
        }
        if (!kHex64.match(candidate.digest).hasMatch()) {
          candidate.digest.clear();
        }
      }
    }
    if (candidate.valid() && candidate.size > 0 && candidate.size <= 2147483648LL) {
      best = candidate;
    }
  }
  return best;
}

QString Checksum(const QByteArray& text, const QString& name) {
  static const QRegularExpression kLine(QStringLiteral(R"(^([0-9a-fA-F]{64})[ \t]+\*?(.+)$)"));
  QString found;
  for (const QString& line : QString::fromUtf8(text).split('\n')) {
    const auto match = kLine.match(line.trimmed());
    if (match.hasMatch() && match.captured(2) == name) {
      if (!found.isEmpty()) {
        return {}; // listed twice: ambiguous
      }
      found = match.captured(1).toLower();
    }
  }
  return found;
}

} // namespace PrimedGunLauncher::Updates
