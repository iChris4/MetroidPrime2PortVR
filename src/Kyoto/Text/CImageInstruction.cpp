#include "Kyoto/Text/CImageInstruction.hpp"

#include "Kyoto/Math/CVector2i.hpp"
#include "Kyoto/Text/CFontRenderState.hpp"
#include "Kyoto/Text/CLineInstruction.hpp"
#include "Kyoto/Text/CTextRenderBuffer.hpp"

int CFontImageDef::CalculateHeight() const {
  return GetHeight() - (GetHeight() - CalculateBaseline());
}

void CImageInstruction::Invoke(CFontRenderState& state, CTextRenderBuffer* buf) const {
  if (mImage.IsLoaded() && !mImage.GetImages().empty()) {
    if (state.GetBlock()->GetTextDirection() == kTD_Horizontal) {
      if (buf) {
#ifdef TARGET_PC
        // Retail sits an image's baseline 5/6 down it, which suited the GameCube
        // art; the port's prompt icons sit at the quad's centre and rode about a
        // third of their own height above the text. A baseline 2/3 down centres
        // them on the capitals (measured on the hint memo, both devices).
        // Port: the retail statement stays whole below; a local y changes MWCC's code.
        int y = state.GetY() + state.GetLine()->GetBaseline() - mImage.GetHeight() * 2 / 3;
        buf->AddImage(CVector2i(state.GetX(), y), mImage);
#else
        buf->AddImage(CVector2i(state.GetX(), state.GetY() + state.GetLine()->GetBaseline() -
                                                  mImage.CalculateBaseline()),
                      mImage);
#endif
      }
      state.SetX(state.GetX() + mImage.GetWidth());
    } else {
      int offset = (state.GetLine()->GetWidth() - mImage.GetWidth()) / 2;
      if (buf) {
        buf->AddImage(CVector2i(offset + state.GetX(), state.GetY()), mImage);
      }
      state.SetY(state.GetY() + mImage.CalculateHeight());
    }
  }
}

void CImageInstruction::GetAssets(rstl::vector< CToken >& assetsOut) const {
  for (int i = 0; i < mImage.GetImages().size(); ++i) {
    assetsOut.push_back_unsafe(mImage.GetImages()[i]);
  }
}
