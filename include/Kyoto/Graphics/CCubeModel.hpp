#ifndef _CCUBEMODEL
#define _CCUBEMODEL

#include "types.h"

#include "Kyoto/Graphics/CCubeMaterial.hpp"
#include "Kyoto/Graphics/CCubeSurface.hpp"
#include "Kyoto/Math/CAABox.hpp"
#include "Kyoto/TToken.hpp"

#include "rstl/vector.hpp"

class CCubeSurface;
class CModelFlags;
class CStopwatch;
class CTexture;
class CTransform4f;
class CVector3f;
class IObjectStore;

enum ESurfaceSelection {
  kSS_Unsorted,
  kSS_Sorted,
  kSS_All,
};

class CCubeModel {
public:
  class ModelInstance {
  public:
    ModelInstance(rstl::vector< void* >& surfaces, const void* materialData, const void* positions,
                  const void* normals, const void* colors, const void* uvs, const void* packedTexCoords
#ifdef TARGET_PC
                  , uint positionsSize = 0, uint normalsSize = 0, uint colorsSize = 0,
                  uint texCoordsSize = 0, uint packedTexCoordsSize = 0
#endif
                  )
    : mSurfacePtrs(surfaces)
    , mMaterialData(materialData)
    , mPositions(positions)
    , mNormals(normals)
    , mColors(colors)
    , mTexCoords(uvs)
    , mPackedTexCoords(packedTexCoords)
#ifdef TARGET_PC
    , mPositionsSize(positionsSize)
    , mNormalsSize(normalsSize)
    , mColorsSize(colorsSize)
    , mTexCoordsSize(texCoordsSize)
    , mPackedTexCoordsSize(packedTexCoordsSize)
#endif
    {}

    rstl::vector< void* >& Surfaces() { return mSurfacePtrs; }
    const rstl::vector< void* >& Surfaces() const { return mSurfacePtrs; }
    const void* GetMaterialPointer() const { return mMaterialData; }
    void SetMaterialPointer(const void* data) { mMaterialData = data; }
    const void* GetVertexPointer() const { return mPositions; }
    const void* GetNormalPointer() const { return mNormals; }
    const void* GetColorPointer() const { return mColors; }
    const void* GetTCPointer() const { return mTexCoords; }
    const void* GetPackedTCPointer() const { return mPackedTexCoords; }
#ifdef TARGET_PC
    uint GetVertexSize() const { return mPositionsSize; }
    uint GetNormalSize() const { return mNormalsSize; }
    uint GetColorSize() const { return mColorsSize; }
    uint GetTCSize() const { return mTexCoordsSize; }
    uint GetPackedTCSize() const { return mPackedTexCoordsSize; }
#endif

  private:
    rstl::vector< void* >& mSurfacePtrs;
    const void* mMaterialData;
    const void* mPositions;
    const void* mNormals;
    const void* mColors;
    const void* mTexCoords;
    const void* mPackedTexCoords;
#ifdef TARGET_PC
    uint mPositionsSize;
    uint mNormalsSize;
    uint mColorsSize;
    uint mTexCoordsSize;
    uint mPackedTexCoordsSize;
#endif
  };

  // Echoes method names below are inferred from Prime and call sites.
  CCubeModel(rstl::vector< void* >* surfaces, rstl::vector< TCachedToken< CTexture > >* textures,
             const void* materialData, const void* positions, const void* normals,
             const void* colors, const void* uvs, const void* compressedUvs, const CAABox& bounds,
             uchar visorFlags, bool texturesLoaded, uint idx
#ifdef TARGET_PC
             , uint positionsSize = 0, uint normalsSize = 0, uint colorsSize = 0,
             uint texCoordsSize = 0, uint packedTexCoordsSize = 0
#endif
             );
#ifdef TARGET_PC
  // Port: unregisters the model from PortDrawDebug (whose labels are keyed by address).
  ~CCubeModel();

  // Port: what a "[model] ..." refusal line names - the CMDL asset id, or the area and world model
  // index. Set (thread-local) around a construction by the code that knows it.
  static const uint kPortNoAsset = 0xffffffffu;
  static void PortSetBuildContext(uint assetId, int areaId, int modelIndex);
  static void PortClearBuildContext() { PortSetBuildContext(kPortNoAsset, -1, -1); }
  static uint PortBuildAssetId();
  uint PortAssetId() const { return mPortAssetId; }
  // A material that draws nothing: big-endian zeros except a flag bit DrawSurface always skips.
  static const uchar* PortNeutralMaterial();
  // Whether a model made from now on keeps its vertex arrays (and its display lists) on the GPU
  // (GXPortRetainResident) while it exists; main.cpp sets it once Aurora has the room for them.
  static void PortSetResident(bool arrays, bool displayLists);
  // After writing into this model's arrays or display lists (the drawn button ring moves a box's
  // frame line and empties its quad): the GPU's copies of what it retained are taken again, so the
  // next draw sees the writes (GXPortUpdateResident). Nothing when it retained nothing.
  void PortUpdateResidentData() const;
#endif

  static void SetRenderModelBlack(bool v);
#ifdef TARGET_PC
  // Port: read by PortDrawDebug (a black draw sets no texture coordinates up).
  static bool IsRenderingModelBlack();
#endif
  static void SetModelWireframe(bool v);
  static void DisableShadowMaps();
  static void EnableShadowMaps(const CTexture*, const CTransform4f&, unsigned char, unsigned char);
  static void SetNewPlayerPositionAndTime(const CVector3f& pos, const CStopwatch& stopwatch);
  static void SetDrawingOccluders(bool);
  static void MakeTexturesFromMats(const void* data,
                                   rstl::vector< TCachedToken< CTexture > >& textures,
                                   IObjectStore& store, bool cache);

  void UnlockTextures() const;
  void RemapMaterialData(const void* data, rstl::vector< TCachedToken< CTexture > >* textures);
  bool TryLockTextures() const;
  void DrawNormal(ESurfaceSelection which) const;
  void DrawFlat(int flags) const;
  void Draw(const CModelFlags& flags) const;
  void Draw(u64 mask, const CModelFlags& flags) const;
  void DrawNormal(const CModelFlags& flags) const;
  void DrawAlpha(const CModelFlags& flags) const;
  void SetArraysCurrent() const;
  void SetStaticArraysCurrent() const;
  void DrawSurface(const CCubeSurface& surface, const CModelFlags& flags) const;
  void DrawSurfaceWireframe(const CCubeSurface& surface) const;
  void DrawSurfaceFlat(const CCubeSurface& surface) const; // Guessed name.
  void DrawSurfaces(const CModelFlags& flags) const;
  void DrawSurfaces(u64 mask, const CModelFlags& flags) const;
  void DrawNormalSurfaces(const CModelFlags& flags) const;
  void DrawAlphaSurfaces(const CModelFlags& flags) const;

  void SetUsingPackedLightmaps(bool v) const;
  static bool IsUsingPackedLightmaps() { return sUsingPackedLightmaps; }

  const ModelInstance& GetModelInstance() const { return mInstance; }
  const void* GetPositions() const { return mInstance.GetVertexPointer(); }
  const void* GetNormals() const { return mInstance.GetNormalPointer(); }
  bool AreTexturesLoaded() const { return !mLoadTextures; }
  uchar GetModelFlags() const { return mVisorFlags; }
  int GetModelIndex() const { return mIdx; }

  const rstl::vector< TCachedToken< CTexture > >& GetTextures() const { return *mTextures; }
  const rstl::vector< TCachedToken< CTexture > >* GetTexturesPtr() const { return mTextures; }
  const CAABox& GetBoundingBox() const { return mBounds; }
  const CCubeSurface& GetNormalSurfaces() const { return mFirstUnsorted; }
  const CCubeSurface& GetAlphaSurfaces() const { return mFirstSorted; }

  // Guessed name; the target passes a surface handle by value for material lookup.
  CCubeMaterial GetMaterial(CCubeSurface surface) const {
    return GetMaterialByIndex(surface.GetMaterialIndex());
  }

  CCubeMaterial GetMaterialByIndex(int index) const {
#ifdef TARGET_PC
    if (static_cast< uint >(index) >= mPortMaterialCount) {
      return CCubeMaterial(PortRefuseMaterial(index));
    }
#endif
    const uint offset = index != 0 ? CBasics::SwapBytes(mMaterialOffsets[index - 1]) : 0;
    return CCubeMaterial(mMaterialData + offset);
  }

private:
  void CacheMaterialData(); // Guessed name; caches the offsets and material payload.

  ModelInstance mInstance;
  rstl::vector< TCachedToken< CTexture > >* mTextures;
  CAABox mBounds;
  CCubeSurface mFirstUnsorted;
  CCubeSurface mFirstSorted;
  mutable bool mLoadTextures : 1;
  bool mVisible : 1;
  uchar x40_ : 6;
  uchar mVisorFlags;
  int mIdx;
  const uint* mMaterialOffsets;
  const uchar* mMaterialData;
#ifdef TARGET_PC
  // Bounds for the material reads: a surface's material index comes straight from disc data.
  const uchar* PortRefuseMaterial(int index) const;
  void PortCheckSurfaces();
  void PortDescribe(char* out, int len) const;
  uint mPortMaterialCount;
  uint mPortAssetId;
  int mPortAreaId;
  int mPortModelIndex;
  mutable bool mPortMaterialRefused;
  // Retains (or releases) every non-empty array and display list; see PortSetResident.
  void PortSetResidentData(bool retain);
  enum { kPortResidentArrays = 1, kPortResidentDisplayLists = 2 };
  uchar mPortResident; // what this model retained (kPortResident*), released by its destructor
  rstl::vector< const void* > mPortResidentLists; // the display lists it retained
#endif

  static bool sUsingPackedLightmaps;
};
CHECK_SIZEOF(CCubeModel, 0x50)

#endif // _CCUBEMODEL
