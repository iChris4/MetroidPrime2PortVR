#ifdef TARGET_PC
#include "port_cannon_repair.h"
#include "Kyoto/Streams/CMemoryInStream.hpp"
#endif
#include "Kyoto/Animation/CSkinRules.hpp"

#include "Kyoto/Animation/CCharLayoutInfo.hpp"
#include "Kyoto/Animation/CPoseAsTransforms_Linear.hpp"
#include "Kyoto/CFactoryMgr.hpp"
#include "Kyoto/Graphics/CModel.hpp"

#include "dolphin/gx.h"
#ifdef TARGET_PC
#include "Kyoto/Graphics/CGX.hpp"
#endif

CSkinRules::CSkinRules(CInputStream& in)
: mBones(in), mMatrixIndices(in), mVertexCount(in.ReadInt32()), mVertexToBone(nullptr) {
  if (mVertexCount > 0) {
    mVertexToBone = rs_new uchar[mVertexCount];
    in.Get(mVertexToBone.get(), mVertexCount);
  }
  CModel::AddToTotal(sizeof(CSkinRules) + mBones.size() * sizeof(CVirtualBone));
}

CSkinRules::~CSkinRules() {
  CModel::RemoveFromTotal(sizeof(CSkinRules) + mBones.size() * sizeof(CVirtualBone));
}

void CSkinRules::BuildAccumulatedTransforms(const CPoseAsTransforms_Linear& pose,
                                            const CCharLayoutInfo& layoutInfo,
                                            CTransform4f* out) const {
  float pointStorage[100][3];
  CVector3f* points = reinterpret_cast< CVector3f* >(pointStorage);
  const CVector3f* offsets = layoutInfo.GetLinearReferenceStanceOffsets().data();
  const int count = pose.GetElements().size();
  const CPoseAsTransforms_Linear::CElementType* elements = pose.GetElements().data();
  for (int i = 0; i < count; ++i) {
    points[i] = elements[i].mOffset - elements[i].mRotation * offsets[i];
  }

  const int boneCount = mBones.size();
  for (int i = 0; i < boneCount; ++i) {
    mBones[i].BuildAccumulatedTransform(pose, points, out[i]);
  }
}

CFactoryFnReturn FSkinRulesFactory(const SObjectTag& tag, CInputStream& in,
                                   const CVParamTransfer& params) {
#ifdef TARGET_PC
  if (const auto* repaired = PortCannonRepair::FindForGame(tag.id)) {
    CMemoryInStream replacement(repaired->data.data(), repaired->data.size());
    return rs_new CSkinRules(replacement);
  }
#endif
  return rs_new CSkinRules(in);
}

void CSkinRules::LoadMatrixBank(int bank) const {
  const int start = bank * 10;
  const int end = start + 10 > mMatrixIndices.size() ? mMatrixIndices.size() : start + 10;
  for (int i = start; i < end; ++i) {
    const int index = mMatrixIndices[i];
    if (index != -1) {
      const int slot = (i - start) * 3;
      GXLoadPosMtxIndx(index, slot);
#ifdef TARGET_PC
      CGX::PortLoadNrmMtxIndx3x3(index, slot);
#else
      GXLoadNrmMtxIndx3x3(index, slot);
#endif
    }
  }
}
