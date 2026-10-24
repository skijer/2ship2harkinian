#include "global.h"

// 2S2H [Unbound] Mtx is float (GBI_FLOAT_MTX); the s16.16 packing loop is gone
void MtxConv_F2L(Mtx* mtx, MtxF* mf) {
    guMtxF2L(mf->mf, mtx);
}

void MtxConv_L2F(MtxF* mtx, Mtx* mf) {
    guMtxL2F(mtx->mf, mf);
}
