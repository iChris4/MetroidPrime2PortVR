#ifndef _CPARTICLEGEN
#define _CPARTICLEGEN

#include "Kyoto/Graphics/CColor.hpp"
#include "Kyoto/Graphics/CLight.hpp"
#include "Kyoto/Math/CAABox.hpp"
#include "Kyoto/Math/CTransform4f.hpp"
#include "Kyoto/Math/CVector3f.hpp"
#include "Kyoto/Particles/CWarp.hpp"

#include "rstl/list.hpp"
#include "rstl/optional_object.hpp"
#include "rstl/pair.hpp"

class CWarp;

class CParticleGen {
public:
  CParticleGen() : mDrawFlags(0) {}
  virtual ~CParticleGen() = 0;
  virtual const bool Update(double) = 0;
  virtual void Render() = 0;
  virtual void SetOrientation(const CTransform4f& orientation) = 0;
  virtual void SetTranslation(const CVector3f& translation) = 0;
  virtual void SetGlobalOrientation(const CTransform4f& orientation) = 0;
  virtual void SetGlobalTranslation(const CVector3f& translation) = 0;
  virtual void SetGlobalScale(const CVector3f& scale) = 0;
  virtual void SetLocalScale(const CVector3f& scale) = 0;
  virtual void SetParticleEmission(bool emission) = 0;
  virtual void SetModulationColor(const CColor& col) = 0;
  virtual void SetGeneratorRate(float rate) {}
#ifdef TARGET_PC
  // Scales where new particles (or bolt end points) spawn in the system's local axes, without
  // scaling the particles themselves: HUD effects use it to cover a view wider than 4:3. `origin`
  // is the parent's translation: a child placed off it (SSPO/SEPO) has its offset spread too.
  virtual void PortSetSpawnSpread(const CVector3f& spread, const CVector3f& origin) {}
  // PortVr: a visor effect's generator (CHUDBillboardEffect), placed in the game camera's space,
  // drawn between PortVr::VrBeginVisorDraw and VrEndVisorDraw (vr/vr_view.h).
  void PortSetVisor(bool visor) { mPortVisor = visor; }
  bool PortIsVisor() const { return mPortVisor; }
#endif
  // Names of the draw-flag methods are hypotheses based on the sDrawFlags/sDrawMask Wii exports.
  virtual void SetDrawFlags(uint flags) { mDrawFlags = flags; }
  virtual const CTransform4f& GetOrientation() const = 0;
  virtual const CVector3f& GetTranslation() const = 0;
  virtual const CTransform4f& GetGlobalOrientation() const = 0;
  virtual const CVector3f& GetGlobalTranslation() const = 0;
  virtual const CVector3f& GetGlobalScale() const = 0;
  virtual bool GetParticleEmission() const = 0;
  virtual const CColor& GetModulationColor() const = 0;
  virtual float GetGeneratorRate() const { return 1.f; }
  virtual int GetEmitterTime() const = 0;
  virtual uint GetDrawFlags() const { return mDrawFlags; }
  virtual bool ShouldDraw() const { return (GetDrawFlags() & sDrawMask) == sDrawFlags; }
  virtual int GetSystemCount() = 0;
  virtual bool IsSystemDeletable() = 0;
  virtual rstl::optional_object< CAABox > GetBounds() = 0;
  virtual int GetParticleCount() = 0;
  virtual bool SystemHasLight() = 0;
  virtual CLight GetLight() = 0;
  virtual void DestroyParticles() = 0;
  virtual void AddModifier(CWarp*);
  virtual uint Get4CharId() const = 0;

  static uint sDrawFlags;
  static uint sDrawMask;

protected:
  rstl::list< CWarp* > mModifiersList;

private:
  uint mDrawFlags;
#ifdef TARGET_PC
  bool mPortVisor = false;
#endif
};

inline CParticleGen::~CParticleGen() {}

#endif // _CPARTICLEGEN
