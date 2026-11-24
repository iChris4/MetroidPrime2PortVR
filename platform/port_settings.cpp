#include "port_utf8_path.h"
#include "port_strings.h"
#include "port_settings.h"

#include "port_env.h"

#include <atomic>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace {

std::string sPath; // empty: no file
std::atomic< bool > sInitialized{false};

using port::ParseBool;
using port::Trim;

} // namespace

namespace PortSettings {

void Initialize(const std::string& userDirectory) {
  sPath.clear();
  if (!userDirectory.empty())
    sPath = PortUtf8::ToUtf8(PortUtf8::FromUtf8(userDirectory) / "settings.cfg");
  sInitialized.store(true, std::memory_order_release);
}

bool Initialized() { return sInitialized.load(std::memory_order_acquire); }

const std::string& FilePath() { return sPath; }

bool ReadFile(std::string& text) {
  if (sPath.empty())
    return false;
  std::ifstream in(PortUtf8::FromUtf8(sPath), std::ios::binary);
  if (!in.is_open())
    return false;
  text.assign(std::istreambuf_iterator< char >(in), std::istreambuf_iterator< char >());
  return true;
}

bool WriteFile(const std::string& text) {
  if (sPath.empty())
    return false;
  const std::string tmp = sPath + ".tmp";
  {
    std::ofstream out(PortUtf8::FromUtf8(tmp), std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
      std::fprintf(stderr, "port: could not write settings to %s\n", tmp.c_str());
      return false;
    }
    out << text;
    out.flush();
    if (!out) {
      std::fprintf(stderr, "port: could not write settings to %s (write failed)\n", tmp.c_str());
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::rename(tmp, sPath, ec);
  if (ec) {
    std::fprintf(stderr, "port: could not replace %s: %s\n", sPath.c_str(), ec.message().c_str());
    std::filesystem::remove(tmp, ec);
    return false;
  }
  return true;
}

bool LogFileEnabled() {
  if (port::EnvString("MP2_LOG_FILE") != nullptr)
    return port::EnvFlag("MP2_LOG_FILE", true);
  std::string text;
  if (!ReadFile(text))
    return true;
  bool enabled = true;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    const size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line[first] == '#')
      continue;
    const size_t separator = line.find('=');
    if (separator == std::string::npos)
      continue;
    if (Trim(line.substr(0, separator)) == "log_file")
      enabled = ParseBool(Trim(line.substr(separator + 1)));
  }
  return enabled;
}

} // namespace PortSettings
