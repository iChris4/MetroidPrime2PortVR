// MP1's port_storage_report.cpp, with MP2's folders (no mods or Remastered import here): the user
// folder, the cache and, on Android, the app's internal storage.
#include "port_storage_report.h"
#include "port_utf8_path.h"

#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

#if defined(__ANDROID__)
#include <SDL3/SDL.h>
#endif

namespace fs = std::filesystem;

namespace PortStorageReport {
namespace {

// The folders come as UTF-8; a narrow path is the ANSI code page on Windows.
using PortUtf8::FromUtf8;
using PortUtf8::ToUtf8;

struct Totals {
  uint64_t bytes = 0;
  uint64_t files = 0;
};

// Hard-linked files count once per inode.
struct Walker {
  std::set< std::pair< uint64_t, uint64_t > > seen;
  uint64_t bytes = 0;

  void Add(const fs::path& file, const fs::directory_entry& entry, Totals& totals) {
    std::error_code ec;
#if !defined(_WIN32)
    (void)entry;
    struct stat st {};
    if (::lstat(file.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
      return;
    }
    if (!seen.insert({static_cast< uint64_t >(st.st_dev), static_cast< uint64_t >(st.st_ino)}).second) {
      return;
    }
    const uint64_t size = static_cast< uint64_t >(st.st_blocks) * 512u;
#else
    (void)file;
    const uint64_t size = entry.file_size(ec);
    if (ec) {
      return;
    }
#endif
    totals.bytes += size;
    totals.files += 1;
    bytes += size;
  }

  Totals Measure(const fs::path& path) {
    Totals totals;
    std::error_code ec;
    const fs::file_status status = fs::symlink_status(path, ec);
    if (ec || fs::is_symlink(status)) {
      return totals;
    }
    if (!fs::is_directory(status)) {
      Add(path, fs::directory_entry(path, ec), totals);
      return totals;
    }
    fs::recursive_directory_iterator it(path, fs::directory_options::skip_permission_denied, ec);
    for (const fs::recursive_directory_iterator end; !ec && it != end; it.increment(ec)) {
      std::error_code entryError;
      if (it->is_symlink(entryError) || !it->is_regular_file(entryError)) {
        continue;
      }
      Add(it->path(), *it, totals);
    }
    return totals;
  }

  // One line per direct child of `root`.
  void ReportChildren(const fs::path& root) {
    std::error_code ec;
    std::vector< fs::path > children;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
      children.push_back(it->path());
    }
    if (ec) {
      std::fprintf(stderr, "port: storage: cannot list %s: %s\n", ToUtf8(root).c_str(),
                   ec.message().c_str());
    }
    for (const fs::path& child : children) {
      const Totals totals = Measure(child);
      std::fprintf(stderr, "port: storage: %s %llu MiB (%llu files)\n", ToUtf8(child).c_str(),
                   static_cast< unsigned long long >(totals.bytes >> 20),
                   static_cast< unsigned long long >(totals.files));
    }
  }
};

bool Inside(const fs::path& path, const fs::path& parent) {
  auto p = path.begin();
  for (auto q = parent.begin(); q != parent.end(); ++q, ++p) {
    if (p == path.end() || *p != *q) {
      return false;
    }
  }
  return true;
}

void Run(const std::vector< std::string >& rootTexts) {
  std::vector< fs::path > roots;
  const auto addRoot = [&roots](const fs::path& path) {
    std::error_code ec;
    fs::path root = fs::weakly_canonical(path, ec);
    // A trailing separator would make the root differ from the same path without one.
    if (!root.empty() && root.filename().empty()) {
      root = root.parent_path();
    }
    if (!ec && !root.empty()) {
      roots.push_back(root);
    }
  };
#if defined(__ANDROID__)
  if (const char* storage = SDL_GetAndroidInternalStoragePath()) {
    addRoot(fs::path(storage));
  }
#endif
  for (const std::string& text : rootTexts) {
    if (!text.empty()) {
      addRoot(FromUtf8(text));
    }
  }
  Walker walker;
  std::vector< fs::path > listed;
  for (const fs::path& root : roots) {
    bool covered = false;
    for (const fs::path& earlier : listed) {
      covered = covered || Inside(root, earlier);
    }
    if (!covered) {
      walker.ReportChildren(root);
      listed.push_back(root);
    }
  }
  std::fprintf(stderr, "port: storage: total %llu MiB\n",
               static_cast< unsigned long long >(walker.bytes >> 20));
}

} // namespace

void StartLogging(std::vector< std::string > roots) {
  try {
    std::thread([roots = std::move(roots)] {
      try {
        Run(roots);
      } catch (const std::exception& e) {
        std::fprintf(stderr, "port: storage: report failed: %s\n", e.what());
      } catch (...) {
        std::fprintf(stderr, "port: storage: report failed\n");
      }
    }).detach();
  } catch (...) {
  }
}

} // namespace PortStorageReport
