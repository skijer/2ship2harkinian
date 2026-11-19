#include "global.h"

void MtxConv_F2L(Mtx* mtx, MtxF* mf) {
    guMtxF2L(mf->mf, mtx);
}

void MtxConv_L2F(MtxF* mtx, Mtx* mf) {
    guMtxL2F(mtx->mf, mf);
}
