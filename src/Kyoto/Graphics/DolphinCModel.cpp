#ifdef TARGET_PC
#include "port_cannon_repair.h"
#endif
#include "Kyoto/Graphics/CModel.hpp"

#include "Kyoto/Alloc/CMemory.hpp"
#include "Kyoto/CFrameDelayedKiller.hpp"
#include "Kyoto/CSimplePool.hpp"
#include "Kyoto/CVParamTransfer.hpp"
#include "Kyoto/Graphics/CCubeMaterial.hpp"
#include "Kyoto/Graphics/CCubeModel.hpp"
#include "Kyoto/Graphics/CGX.hpp"
#include "Kyoto/Graphics/CGraphics.hpp"
#include "Kyoto/Graphics/CModelFlags.hpp"
#include "Kyoto/Graphics/DolphinGPUMemory.hpp"
#include "Kyoto/Math/CAABox.hpp"
#include "Kyoto/Math/CTransform4f.hpp"

#include <dolphin/gx.h>
#include <dolphin/os.h>
#ifdef TARGET_PC
#include "Kyoto/Graphics/PortDrawDebug.hpp"

#include <stdio.h>
#include <string.h>
#endif

static bool sIsTextureTimeoutEnabled = true;
uint CModel::sTotalMemory = 0;
CModel::SShader* CModel::sThisFrameList = nullptr;
CModel::SShader* CModel::sOneFrameList = nullptr;
CModel::SShader* CModel::sTwoFrameList = nullptr;
static uint sFrameCounter = 0;

namespace {
// GX matrix arrays must be 32-byte aligned.
struct SSkinMatrices {
  CTransform4f mModelView;
  float mNormal[3][3];
} ATTRIBUTE_ALIGN(32);

class CSkinMatricesGuard {
public:
  CSkinMatricesGuard(void* matrices) : mActive(matrices != nullptr) {}
  ~CSkinMatricesGuard() {
    if (mActive) {
      GPUMemory::ReleaseAllocation();
      GXSetCurrentMtx(GX_PNMTX0);
    }
  }

private:
  bool mActive;
};
} // namespace

#ifdef TARGET_PC
static uchar* MemoryFromPartData(uchar*& dataCur, int*& secSizeCur, uint* outSize = nullptr) {
  const uint size = CBasics::SwapBytes(*secSizeCur);
  if (outSize != nullptr) {
    *outSize = size;
  }
  uchar* ret = size != 0 ? dataCur : nullptr;
  dataCur += size;
  secSizeCur++;
  return ret;
}

namespace {
inline uint PortReadWord(const uchar* data, unsigned long long offset) {
  return CBasics::SwapBytes(*reinterpret_cast< const uint* >(data + offset));
}

// Checks every count and size the CModel constructor trusts against the resource's real length.
// Writes what was out of range to `why` and returns false for a resource it must not parse.
bool PortCheckModelData(const uchar* data, int length, char* why, int whyLen) {
  if (data == nullptr || length < 0x2c) {
    snprintf(why, whyLen, "%d bytes, shorter than the 0x2c-byte header", length);
    return false;
  }
  const uint flags = PortReadWord(data, 8);
  const int sectionCount = static_cast< int >(PortReadWord(data, 0x24));
  const int numMatSets = static_cast< int >(PortReadWord(data, 0x28));
  if (sectionCount < 1 || sectionCount > (length - 0x2c) / 4) {
    snprintf(why, whyLen, "section count %d does not fit %d bytes", sectionCount, length);
    return false;
  }
  const int sectionsBeforeSurfaces = numMatSets + 4 + static_cast< int >((flags >> 2) & 1);
  if (numMatSets < 1 || sectionsBeforeSurfaces + 1 > sectionCount) {
    snprintf(why, whyLen, "%d material sets in %d sections", numMatSets, sectionCount);
    return false;
  }
  const unsigned long long header = (0x2cull + sectionCount * 4ull + 31) & ~31ull;
  unsigned long long total = header;
  for (int i = 0; i < sectionCount; ++i) {
    total += PortReadWord(data, 0x2c + i * 4ull);
  }
  if (total > static_cast< unsigned long long >(length)) {
    snprintf(why, whyLen, "%d sections claim %llu bytes, the resource has %d", sectionCount, total,
             length);
    return false;
  }

  unsigned long long offset = header;
  for (int i = 0; i < numMatSets; ++i) {
    const uint size = PortReadWord(data, 0x2c + i * 4ull);
    const uint textureCount = size >= 8 ? PortReadWord(data, offset) : 0;
    if (size < 8 || 8ull + textureCount * 4ull > size) {
      snprintf(why, whyLen, "material set %d (%u bytes) claims %u textures", i, size, textureCount);
      return false;
    }
    const uint materialCount = PortReadWord(data, offset + 4 + textureCount * 4ull);
    if ((2ull + textureCount + materialCount) * 4ull > size) {
      snprintf(why, whyLen, "material set %d (%u bytes) claims %u materials", i, size,
               materialCount);
      return false;
    }
    offset += size;
  }
  for (int i = numMatSets; i < sectionsBeforeSurfaces; ++i) {
    offset += PortReadWord(data, 0x2c + i * 4ull);
  }

  const uint countSize = PortReadWord(data, 0x2c + sectionsBeforeSurfaces * 4ull);
  const uint surfaceCount = countSize >= 4 ? PortReadWord(data, offset) : 0;
  const int surfaceSections = sectionCount - sectionsBeforeSurfaces - 1;
  if (countSize < 4 || surfaceCount > static_cast< uint >(surfaceSections)) {
    snprintf(why, whyLen, "surface count %u (section of %u bytes) with %d sections left",
             surfaceCount, countSize, surfaceSections);
    return false;
  }
  offset += countSize;
  for (uint i = 0; i < surfaceCount; ++i) {
    const uint size = PortReadWord(data, 0x2c + (sectionsBeforeSurfaces + 1 + i) * 4ull);
    if (size < 0x30) {
      snprintf(why, whyLen, "surface %u is %u bytes, shorter than its 0x30-byte header", i, size);
      return false;
    }
    const uint extraSize = PortReadWord(data, offset + 0x1c);
    const uint displayListSize = PortReadWord(data, offset + 0x10) & 0x7fffffff;
    // The header (0x48 with the bounds that a non-zero extra size brings), then the display list.
    unsigned long long needed = ((0x4full + extraSize) & ~31ull) + displayListSize;
    if (needed < (extraSize != 0 ? 0x48ull : 0x30ull)) {
      needed = extraSize != 0 ? 0x48ull : 0x30ull;
    }
    if (needed > size) {
      snprintf(why, whyLen,
               "surface %u (%u bytes) claims extra 0x%x + display list 0x%x (needs %llu bytes)", i,
               size, extraSize, displayListSize, needed);
      return false;
    }
    offset += size;
  }
  return true;
}

// A CMDL with one empty material set, no vertex data and no surfaces: what a refused resource
// becomes, so its CModel exists and draws nothing.
const int kPortEmptyModelSize = 0x80;
void PortWriteEmptyModel(uchar* data) {
  memset(data, 0, kPortEmptyModelSize);
  uint* words = reinterpret_cast< uint* >(data);
  words[0x24 / 4] = CBasics::SwapBytes(6u); // sections: matset, 4 vertex arrays, surface count
  words[0x28 / 4] = CBasics::SwapBytes(1u); // one material set
  words[0x2c / 4] = CBasics::SwapBytes(8u); // matset: 0 textures, 0 materials
  words[0x40 / 4] = CBasics::SwapBytes(4u); // surface count: 0 (at 0x68)
}
} // namespace
#else
static uchar* MemoryFromPartData(uchar*& dataCur, int*& secSizeCur) {
  uchar* ret = *secSizeCur != 0 ? dataCur : nullptr;
  dataCur += *secSizeCur;
  secSizeCur++;
  return ret;
}
#endif

CModel::SShader::SShader(uchar* data, CModel* owner)
: mData(data), mOwner(owner), mPrev(nullptr), mNext(nullptr) {}

CModel::SShader::SShader(const SShader& other)
: mTextures(other.mTextures)
, mData(other.mData)
, mOwner(other.mOwner)
, mPrev(nullptr)
, mNext(nullptr) {}

CModel::SShader::~SShader() { RemoveFromList(); }

void CModel::SShader::MoveToThisFrameList() {
  if (sThisFrameList == this) {
    return;
  }
  RemoveFromList();

  SShader* head = sThisFrameList;
  if (head != nullptr) {
    mNext = head;
    head->mPrev = this;
  }

  sThisFrameList = this;
}

void CModel::SShader::RemoveFromList() {
  if (mPrev != nullptr) {
    mPrev->mNext = mNext;
  } else if (this == sThisFrameList) {
    sThisFrameList = mNext;
  } else if (this == sOneFrameList) {
    sOneFrameList = mNext;
  } else if (this == sTwoFrameList) {
    sTwoFrameList = mNext;
  }

  if (mNext != nullptr) {
    mNext->mPrev = mPrev;
  }

  mPrev = nullptr;
  mNext = nullptr;
}

void CModel::SShader::UnlockTextures() {
  rstl::vector< TCachedToken< CTexture > >::iterator it = mTextures.begin();
  rstl::vector< TCachedToken< CTexture > >::iterator end = mTextures.end();
  for (; it != end; ++it) {
    it->Unlock();
  }
}

CModel::CModel(const rstl::auto_ptr< uchar >& data, int length, IObjectStore& store)
: mData(data.release())
, mDataLen(length)
, mModelInstance(nullptr)
, mLastFrame(CGraphics::GetFrameCounter() - 2)
, mCurrentMatxIdx(0)
, mInitialTextureLocksReleased(false)
, mHasSkinMatrices(false) {
  uchar* dataPtr = data.get();
#ifdef TARGET_PC
  char why[160];
  if (!PortCheckModelData(dataPtr, length, why, sizeof(why))) {
    fprintf(stderr, "[res] CMDL 0x%08X: %s; drawing nothing\n", CCubeModel::PortBuildAssetId(),
            why);
    if (dataPtr == nullptr || length < kPortEmptyModelSize) {
      mData = rs_new uchar[kPortEmptyModelSize];
      dataPtr = mData.get();
      length = kPortEmptyModelSize;
      mDataLen = length;
    }
    PortWriteEmptyModel(dataPtr);
  }
  mHasSkinMatrices = CBasics::SwapBytes(*reinterpret_cast< const uint* >(dataPtr + 8)) & 1;
  const int sectionCount = CBasics::SwapBytes(*reinterpret_cast< const int* >(dataPtr + 0x24));
  const uint flags = CBasics::SwapBytes(*reinterpret_cast< const uint* >(dataPtr + 8));
  const uint visorFlags = (flags >> 1) & 1;
  const uint hasShortUvs = (flags >> 2) & 1;
  const int numMatSets = CBasics::SwapBytes(*reinterpret_cast< const int* >(dataPtr + 0x28));
#else
  mHasSkinMatrices = *reinterpret_cast< const uint* >(dataPtr + 8) & 1;
  const int sectionCount = *reinterpret_cast< const int* >(dataPtr + 0x24);
  const uint flags = *reinterpret_cast< const uint* >(dataPtr + 8);
  const uint visorFlags = (flags >> 1) & 1;
  const uint hasShortUvs = (flags >> 2) & 1;
  const int numMatSets = *reinterpret_cast< const int* >(dataPtr + 0x28);
#endif

  uchar* dataCur = dataPtr + ((0x2c + sectionCount * 4 + 31) & ~31);
  int* secSizeCur = reinterpret_cast< int* >(dataPtr + 0x2c);
  mMatSets.reserve(numMatSets);
  for (int i = 0; i < numMatSets; ++i) {
    mMatSets.push_back_unsafe(SShader(MemoryFromPartData(dataCur, secSizeCur), this));
    SShader& shader = mMatSets.back();
    CCubeModel::MakeTexturesFromMats(shader.mData, shader.mTextures, store, true);
    for (int j = 0; j < shader.mTextures.size(); ++j) {
      shader.mTextures[j].Lock();
    }
    mDataLen += shader.mTextures.size() * 12;
  }

#ifdef TARGET_PC
  uint positionsSize = 0;
  uint normalsSize = 0;
  uint colorsSize = 0;
  uint floatUvsSize = 0;
  uint shortUvsSize = 0;
  const void* positions = MemoryFromPartData(dataCur, secSizeCur, &positionsSize);
  const void* normals = MemoryFromPartData(dataCur, secSizeCur, &normalsSize);
  const void* vtxColors = MemoryFromPartData(dataCur, secSizeCur, &colorsSize);
  const void* floatUvs = MemoryFromPartData(dataCur, secSizeCur, &floatUvsSize);
  const void* shortUvs = nullptr;
  if (hasShortUvs) {
    shortUvs = MemoryFromPartData(dataCur, secSizeCur, &shortUvsSize);
  }

  const uint surfaceCount =
      CBasics::SwapBytes(*reinterpret_cast< uint* >(MemoryFromPartData(dataCur, secSizeCur)));
  mSurfaces.reserve(surfaceCount);
  for (uint i = 0; i < surfaceCount; ++i) {
    uchar* surface = MemoryFromPartData(dataCur, secSizeCur);
    CCubeSurface::ConvertSurfaceHeader(surface);
    mSurfaces.push_back_unsafe(surface);
  }

  const float* boundsData = reinterpret_cast< const float* >(dataPtr + 0xc);
  const CAABox aabb(CBasics::SwapBytes(boundsData[0]), CBasics::SwapBytes(boundsData[1]),
                    CBasics::SwapBytes(boundsData[2]), CBasics::SwapBytes(boundsData[3]),
                    CBasics::SwapBytes(boundsData[4]), CBasics::SwapBytes(boundsData[5]));
  mModelInstance = rs_new CCubeModel(&mSurfaces, &mMatSets.front().mTextures,
                                     mMatSets.front().mData, positions, normals, vtxColors,
                                     floatUvs, shortUvs, aabb, visorFlags ? 1 : 0, true, -1,
                                     positionsSize, normalsSize, colorsSize, floatUvsSize,
                                     shortUvsSize);
#else
  const void* positions = MemoryFromPartData(dataCur, secSizeCur);
  const void* normals = MemoryFromPartData(dataCur, secSizeCur);
  const void* vtxColors = MemoryFromPartData(dataCur, secSizeCur);
  const void* floatUvs = MemoryFromPartData(dataCur, secSizeCur);
  const void* shortUvs = nullptr;
  if (hasShortUvs) {
    shortUvs = MemoryFromPartData(dataCur, secSizeCur);
  }

  const uint surfaceCount = *reinterpret_cast< uint* >(MemoryFromPartData(dataCur, secSizeCur));
  mSurfaces.reserve(surfaceCount);
  for (uint i = 0; i < surfaceCount; ++i) {
    mSurfaces.push_back_unsafe(MemoryFromPartData(dataCur, secSizeCur));
  }

  mModelInstance = rs_new CCubeModel(
      &mSurfaces, &mMatSets.front().mTextures, mMatSets.front().mData, positions, normals,
      vtxColors, floatUvs, shortUvs, *reinterpret_cast< const CAABox* >(dataPtr + 0xc),
      visorFlags ? 1 : 0, true, -1);
#endif
  mDataLen += mSurfaces.size() * 4;
  AddToTotal(mDataLen);
  DCFlushRange(mData.get(), length);
}

CModel::~CModel() {
  RemoveFromTotal(mDataLen);
  const int frame = CGraphics::GetFrameCounter();
  if (mLastFrame == frame) {
    CFrameDelayedKiller::ScheduleDeletion(CFrameDelayedKiller::kWhichFrame_NextFrame,
                                          mData.release());
  } else if (mLastFrame == frame - 1) {
    CFrameDelayedKiller::ScheduleDeletion(CFrameDelayedKiller::kWhichFrame_ThisFrame,
                                          mData.release());
  }
}

void CModel::PreDrawModel(const CModelFlags& flags) const {
  CCubeMaterial::ResetCachedMaterials();
  UpdateLastFrame();
  VerifyCurrentShader(flags.GetShaderSet());
}

void* CModel::SetupSkinMatrices() const {
  if (!mHasSkinMatrices) {
    return nullptr;
  }

  SSkinMatrices* matrices =
      static_cast< SSkinMatrices* >(GPUMemory::EnsureAllocation(sizeof(SSkinMatrices)));
  matrices->mModelView = CGraphics::GetGXModelView();
  float (*normal)[3] = matrices->mNormal;
  const Mtx& invXpose = CGraphics::GetGXModelViewInvXpose();
  for (int i = 0; i < 3; ++i) {
    normal[i][0] = invXpose[i][0];
    normal[i][1] = invXpose[i][1];
    normal[i][2] = invXpose[i][2];
  }
  DCFlushRange(matrices, 0x54);
  CGX::SetArray(GX_POS_MTX_ARRAY, &matrices->mModelView, sizeof(SSkinMatrices));
  CGX::SetArray(GX_NRM_MTX_ARRAY, normal, sizeof(SSkinMatrices));
  for (int i = 1; i < 10; ++i) {
    GXLoadPosMtxIndx(0, i * 3);
#ifdef TARGET_PC
    CGX::PortLoadNrmMtxIndx3x3(0, i * 3);
#else
    GXLoadNrmMtxIndx3x3(0, i * 3);
#endif
  }
  return matrices;
}

void CModel::Draw(const CModelFlags& flags) const {
  PreDrawModel(flags);
  if (flags.GetOtherFlags() & CModelFlags::kF_DrawNormal) {
    mModelInstance->DrawNormal(kSS_All);
  }
  CSkinMatricesGuard guard(SetupSkinMatrices());
  mModelInstance->Draw(flags);
}

void CModel::Draw(u64 mask, const CModelFlags& flags) const {
  PreDrawModel(flags);
  if (flags.GetOtherFlags() & CModelFlags::kF_DrawNormal) {
    mModelInstance->DrawNormal(kSS_All);
  }
  CSkinMatricesGuard guard(SetupSkinMatrices());
  mModelInstance->Draw(mask, flags);
}

void CModel::DrawUnsortedParts(const CModelFlags& flags) const {
  PreDrawModel(flags);
  if (flags.GetOtherFlags() & CModelFlags::kF_DrawNormal) {
    mModelInstance->DrawNormal(kSS_Unsorted);
  }
  CSkinMatricesGuard guard(SetupSkinMatrices());
  mModelInstance->DrawNormal(flags);
}

void CModel::DrawSortedParts(const CModelFlags& flags) const {
  PreDrawModel(flags);
  if (flags.GetOtherFlags() & CModelFlags::kF_DrawNormal) {
    mModelInstance->DrawNormal(kSS_Sorted);
  }
  CSkinMatricesGuard guard(SetupSkinMatrices());
  mModelInstance->DrawAlpha(flags);
}

void CModel::DolphinDrawFlat(EDrawFlatFlags flags) const {
  CCubeMaterial::ResetCachedMaterials();
  UpdateLastFrame();
  CSkinMatricesGuard guard(SetupSkinMatrices());
  mModelInstance->DrawFlat(flags);
}

void CModel::VerifyCurrentShader(int shader) const {
  if (shader >= mMatSets.size()) {
    shader = 0;
  }
  SShader& material = mMatSets[shader];
  if (shader != mCurrentMatxIdx) {
    mModelInstance->RemapMaterialData(material.mData, &material.mTextures);
    mCurrentMatxIdx = shader;
  }
  if (mInitialTextureLocksReleased) {
    material.MoveToThisFrameList();
  }
}

CFactoryFnReturn FModelFactory(const SObjectTag& tag, const rstl::auto_ptr< uchar >& ptr, int len,
                               const CVParamTransfer& xfer) {
  rstl::rc_ptr< IVParamObj > obj = xfer.GetObj();
  CSimplePool* pool = static_cast< TObjOwnerParam< CSimplePool* >* >(obj.GetPtr())->GetData();
  GXInvalidateVtxCache();
#ifdef TARGET_PC
  CCubeModel::PortSetBuildContext(tag.id, -1, -1);
  CModel* model;
  if (const auto* repaired = PortCannonRepair::FindForGame(tag.id)) {
    rstl::auto_ptr<uchar> data(static_cast<uchar*>(CMemory::Alloc(
        repaired->data.size(), IAllocator::kHI_RoundUpLen)));
    memcpy(data.get(), repaired->data.data(), repaired->data.size());
    model = rs_new CModel(data, repaired->data.size(), *pool);
  } else {
    model = rs_new CModel(ptr, len, *pool);
  }
  CCubeModel::PortClearBuildContext();
  PortDrawDebug::LabelAsset(model->GetModelInstance(), tag.id);
  return model;
#else
  return rs_new CModel(ptr, len, *pool);
#endif
}

const float* CModel::GetPositions() const {
  return static_cast< const float* >(mModelInstance->GetPositions());
}

const float* CModel::GetNormals() const {
  return static_cast< const float* >(mModelInstance->GetNormals());
}

void CModel::Touch(int shader) const {
  UpdateLastFrame();
  VerifyCurrentShader(shader);
  mModelInstance->TryLockTextures();
}

bool CModel::IsLoaded(int shader) const {
  VerifyCurrentShader(shader);

  const rstl::vector< TCachedToken< CTexture > >& textures = mModelInstance->GetTextures();
  for (rstl::vector< TCachedToken< CTexture > >::const_iterator it = textures.begin();
       it != textures.end(); ++it) {
    if (!it->IsLoaded()) {
      return false;
    }
  }
  return true;
}

void CModel::FrameDone() {
  ++sFrameCounter;
  if (!sIsTextureTimeoutEnabled) {
    return;
  }

  for (SShader* shader = sTwoFrameList; shader != nullptr;) {
    SShader* next = shader->mNext;
    CCubeModel* instance = shader->mOwner->mModelInstance.get();
    if (instance->GetTexturesPtr() == &shader->mTextures) {
      instance->UnlockTextures();
    } else {
      shader->UnlockTextures();
    }
    shader->mNext = nullptr;
    shader->mPrev = nullptr;
    shader = next;
  }

  sTwoFrameList = sOneFrameList;
  sOneFrameList = sThisFrameList;
  sThisFrameList = nullptr;
}

void CModel::DisableTextureTimeout() { sIsTextureTimeoutEnabled = false; }

void CModel::EnableTextureTimeout() { sIsTextureTimeoutEnabled = true; }

uint CModel::GetDataSize() const { return mDataLen; }

rstl::auto_ptr< uchar > CModel::GetData() { return rstl::auto_ptr< uchar >(mData.get()); }

namespace {
#ifdef TARGET_PC
template < typename T >
inline void RemapPointer(T*& pointer, uintptr_t offset) {
  if (pointer != nullptr) {
    pointer = reinterpret_cast< T* >(reinterpret_cast< uintptr_t >(pointer) + offset);
  }
}
#else
inline void RemapPointer(const void*& pointer, uintptr_t offset) {
  if (pointer != nullptr) {
    pointer = reinterpret_cast< void* >(reinterpret_cast< uintptr_t >(pointer) + offset);
  }
}

template < typename T >
inline void RemapPointer(T*& pointer, uintptr_t offset) {
  RemapPointer(reinterpret_cast< const void*& >(pointer), offset);
}
#endif
} // namespace

void CModel::RemapData(uchar* data) {
  uintptr_t offset =
      reinterpret_cast< uintptr_t >(data) - reinterpret_cast< uintptr_t >(mData.release());
  mData = data;
  for (int i = 0; i < mMatSets.size(); ++i) {
    RemapPointer(mMatSets[i].mData, offset);
  }

  const CCubeModel::ModelInstance& instance = mModelInstance->GetModelInstance();
  const uchar* positions = static_cast< const uchar* >(instance.GetVertexPointer());
  const uchar* normals = static_cast< const uchar* >(instance.GetNormalPointer());
  const uchar* colors = static_cast< const uchar* >(instance.GetColorPointer());
  const uchar* uvs = static_cast< const uchar* >(instance.GetTCPointer());
  const uchar* packedUvs = static_cast< const uchar* >(instance.GetPackedTCPointer());
  const CAABox bounds = mModelInstance->GetBoundingBox();
  uchar flags = mModelInstance->GetModelFlags();
  bool texturesLoaded = mModelInstance->AreTexturesLoaded();
  const int index = mModelInstance->GetModelIndex();
  RemapPointer(positions, offset);
  RemapPointer(normals, offset);
  RemapPointer(colors, offset);
  RemapPointer(uvs, offset);
  RemapPointer(packedUvs, offset);
  for (int i = 0; i < mSurfaces.size(); ++i) {
    RemapPointer(mSurfaces[i], offset);
  }

#ifdef TARGET_PC
  CCubeModel::PortSetBuildContext(mModelInstance->PortAssetId(), -1, -1);
  CCubeModel* remapped = rs_new CCubeModel(&mSurfaces, &mMatSets.front().mTextures,
                                           mMatSets.front().mData, positions, normals, colors, uvs,
                                           packedUvs, bounds, flags, texturesLoaded, index,
                                           instance.GetVertexSize(), instance.GetNormalSize(),
                                           instance.GetColorSize(), instance.GetTCSize(),
                                           instance.GetPackedTCSize());
  CCubeModel::PortClearBuildContext();
  PortDrawDebug::CopyLabel(mModelInstance.get(), remapped);
  mModelInstance = remapped;
#else
  mModelInstance =
      rs_new CCubeModel(&mSurfaces, &mMatSets.front().mTextures, mMatSets.front().mData, positions,
                        normals, colors, uvs, packedUvs, bounds, flags, texturesLoaded, index);
#endif
  UpdateLastFrame();
}

void CModel::UpdateLastFrame() const { mLastFrame = CGraphics::GetFrameCounter(); }

bool CModel::IsDefinitelyOpaque() const {
  return mModelInstance.get() != nullptr && !mModelInstance->GetAlphaSurfaces().IsValid();
}

const CAABox& CModel::GetAABB() const { return mModelInstance->GetBoundingBox(); }

void CModel::UnlockTextures() {
  if (mInitialTextureLocksReleased) {
    return;
  }

  mInitialTextureLocksReleased = true;
  for (int i = 0; i < mMatSets.size(); ++i) {
#ifdef TARGET_PC
    // Host fix: the instance's current set needs CCubeModel::UnlockTextures (as in FrameDone) so it
    // also clears mLoadTextures. Releasing it through the shader nulls the cached textures while
    // TryLockTextures still reports them locked, so the next draw binds null CTextures (a crash on
    // the host; GC reads low memory). Seen when CPlayer builds a transition beam CModelData for a
    // model already drawn and calls LockTextures (CTexture::Load from CPlayer::Render).
    if (mModelInstance.get() != nullptr &&
        mModelInstance->GetTexturesPtr() == &mMatSets[i].mTextures) {
      mModelInstance->UnlockTextures();
      continue;
    }
#endif
    mMatSets[i].UnlockTextures();
  }
#ifdef TARGET_PC
  // The loop just nulled the cached textures of the instance's current set too. If the model was
  // drawn before its first CModelData::LockTextures (CPlayer's morph-transition beam model is),
  // the instance still says its textures are locked, so the next draw skips TryLockTextures and
  // CCubeMaterial::SetCurrent loads a null CTexture (tablet SIGSEGV while morphing). Clear that
  // flag the way CModel::FrameDone does; a second Unlock of the same tokens is a no-op.
  if (mModelInstance.get() != nullptr) {
    mModelInstance->UnlockTextures();
  }
#endif
}
