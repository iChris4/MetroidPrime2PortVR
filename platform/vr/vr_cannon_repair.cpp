// SPDX-License-Identifier: GPL-3.0-or-later
#include "port_cannon_repair.h"
#include "vr/vr_settings.h"
#include "Kyoto/CResFactory.hpp"
#include "Kyoto/Streams/CMemoryInStream.hpp"
#include "Kyoto/Streams/CLZOInputStream.hpp"

namespace PortCannonRepair {
const Resource* FindForGame(uint32_t id) {
  if (!IsResource(id) || !PortVr::GetVrSettings().enabled || gpResourceFactory == nullptr)
    return nullptr;
  auto& loader = gpResourceFactory->GetResLoader();
  return Find(id, [&loader](uint32_t asset, std::vector<uint8_t>& bytes) {
    const SObjectTag tag(loader.GetResourceTypeById(asset), asset);
    if (!loader.ResourceExists(tag)) return false;
    const bool compressed = loader.GetResourceCompression(tag) == CResLoader::kCompressionType_Compressed;
    char* buffer = nullptr;
    int size = 0;
    loader.LoadMemResourceSync(tag, &buffer, &size);
    rstl::auto_ptr<CInputStream> input(rs_new CMemoryInStream(buffer, size, CMemoryInStream::kOS_Owned));
    if (compressed) {
      const uint length = input->ReadInt32();
      if (length > 16 * 1024 * 1024) return false;
      CLZOInputStream decoded(input, size - 4, length);
      bytes.resize(length);
      return decoded.ReadBytes(bytes.data(), bytes.size()) == bytes.size();
    }
    bytes.resize(size);
    return input->ReadBytes(bytes.data(), bytes.size()) == bytes.size();
  });
}
} // namespace PortCannonRepair
