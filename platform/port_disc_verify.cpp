#include "port_disc_verify.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>

namespace PortDiscVerify {

namespace {

std::mutex sMutex;
std::mutex sThreadMutex; // Start and Shutdown
std::string sImage;
Result sResult;
std::thread sThread;
std::atomic< bool > sCancel{false};

// Reads len bytes from the image's current position; fewer means the end or a failure.
size_t ReadFull(std::FILE* file, uint8_t* buf, size_t len) {
  size_t total = 0;
  while (total < len) {
    const size_t n = std::fread(buf + total, 1, len - total, file);
    if (n == 0) {
      break;
    }
    total += n;
  }
  return total;
}

void Fail(const std::string& error) {
  std::lock_guard< std::mutex > lock(sMutex);
  sResult.state = State::Error;
  sResult.error = error;
}

void Run(std::string path) {
  const auto started = std::chrono::steady_clock::now();
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    return Fail("the disc image can't be opened");
  }

  std::vector< uint8_t > buf(kChunkSize);
  size_t got = ReadFull(file, buf.data(), buf.size());
  if (got < 0x440) {
    std::fclose(file);
    return Fail("the disc image is too short");
  }
  // A container (RVZ, WIA, WBFS, CISO) hides the disc behind a compressed stream; the check reads
  // plain images, which is what the port itself mounts from an .iso or .gcm.
  const bool plain =
      std::memcmp(buf.data(), "G2ME01", 6) == 0 || std::memcmp(buf.data(), "G2MP01", 6) == 0 ||
      std::memcmp(buf.data(), "R3ME01", 6) == 0 || std::memcmp(buf.data(), "R3MP01", 6) == 0;
  if (!plain) {
    std::fclose(file);
    return Fail("this is not a plain GameCube disc image (an .iso or .gcm); the check does not "
                "read compressed images");
  }
  // The revision comes from the header, not the hash, so a damaged image is still compared with
  // the right one.
  const Revision* revision = FindRevision(reinterpret_cast< const char* >(buf.data()), buf[7]);
  const bool known = revision != nullptr && buf[6] == 0;
  if (!known) {
    revision = nullptr;
  }
  {
    std::lock_guard< std::mutex > lock(sMutex);
    sResult.known = known;
    sResult.revision = known ? revision->name : "unknown disc";
    sResult.bytesTotal = known ? revision->size : 0;
    sResult.chunks = known ? static_cast< uint32_t >(revision->crc.size()) : 0;
  }

  Sha1Stream sha;
  std::vector< BadChunk > bad;
  uint64_t done = 0;
  uint32_t index = 0;
  bool cancelled = false;
  for (;;) {
    if (sCancel.load()) {
      cancelled = true;
      break;
    }
    if (got == 0) {
      break;
    }
    if (known) {
      CompareChunk(*revision, index, buf.data(), got, bad);
    }
    sha.Update(buf.data(), got);
    done += got;
    ++index;
    {
      std::lock_guard< std::mutex > lock(sMutex);
      sResult.bytesDone = done;
    }
    if (got < kChunkSize || (known && done >= revision->size)) {
      break;
    }
    got = ReadFull(file, buf.data(), buf.size());
  }
  // A short image: the chunks that were never read are bad.
  if (known && !cancelled) {
    for (uint32_t i = index; i < revision->crc.size(); ++i) {
      bad.push_back({i, false, true});
    }
  }

  // Name the files under the bad chunks from the disc's own FST.
  std::vector< std::string > files;
  if (!bad.empty() && !cancelled) {
    uint8_t header[0x430];
    std::rewind(file);
    if (ReadFull(file, header, sizeof(header)) == sizeof(header)) {
      const uint32_t fstOffset = (header[0x424] << 24) | (header[0x425] << 16) |
                                 (header[0x426] << 8) | header[0x427];
      const uint32_t fstSize =
          (header[0x428] << 24) | (header[0x429] << 16) | (header[0x42a] << 8) | header[0x42b];
      std::vector< uint8_t > fst(fstSize);
      if (fstSize != 0 && fstSize <= (16u << 20) &&
          std::fseek(file, static_cast< long >(fstOffset), SEEK_SET) == 0 &&
          ReadFull(file, fst.data(), fst.size()) == fst.size()) {
        const std::vector< FstFile > all = ParseFst(fst.data(), fst.size());
        for (const BadChunk& b : bad) {
          for (std::string& name : FilesOverlapping(all, uint64_t(b.index) * kChunkSize,
                                                    uint64_t(b.index + 1) * kChunkSize)) {
            bool seen = false;
            for (const std::string& have : files) {
              seen = seen || have == name;
            }
            if (!seen) {
              files.push_back(std::move(name));
            }
          }
        }
      }
    }
  }
  std::fclose(file);

  Result final;
  {
    std::lock_guard< std::mutex > lock(sMutex);
    final = sResult;
  }
  final.state = cancelled ? State::Cancelled : State::Done;
  final.bytesDone = done;
  final.sha1 = sha.FinalHex();
  final.sha1Matches = known && final.sha1 == revision->sha1;
  final.bad = bad;
  for (const BadChunk& b : bad) {
    final.zeroed += b.zeroed;
  }
  final.files = files;
  final.seconds = std::chrono::duration< double >(std::chrono::steady_clock::now() - started).count();
  std::fprintf(stderr, "metroid_prime2_port: %s (%.1f s)\n", Summary(final).c_str(),
               final.seconds);
  std::lock_guard< std::mutex > lock(sMutex);
  sResult = final;
}

} // namespace

void SetImagePath(const std::string& path) {
  std::lock_guard< std::mutex > lock(sMutex);
  sImage = path;
}

std::string ImagePath() {
  std::lock_guard< std::mutex > lock(sMutex);
  return sImage;
}

bool Start() {
  std::lock_guard< std::mutex > threadLock(sThreadMutex);
  std::string path;
  {
    std::lock_guard< std::mutex > lock(sMutex);
    if (sResult.state == State::Running || sImage.empty()) {
      return false;
    }
    path = sImage;
  }
  if (sThread.joinable()) {
    sThread.join();
  }
  sCancel = false;
  {
    std::lock_guard< std::mutex > lock(sMutex);
    sResult = Result{};
    sResult.state = State::Running;
  }
  sThread = std::thread([path] { Run(path); });
  return true;
}

void Cancel() { sCancel = true; }

void Shutdown() {
  Cancel();
  std::lock_guard< std::mutex > threadLock(sThreadMutex);
  if (sThread.joinable()) {
    sThread.join();
  }
}

Result Snapshot() {
  std::lock_guard< std::mutex > lock(sMutex);
  return sResult;
}

} // namespace PortDiscVerify
