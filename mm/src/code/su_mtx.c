/**
 * @file su_mtx.c
 * @brief "Fast" functions for constructing RSP-compatible matrices directly
 *
 * The three functions in this file construct scaling, rotation, and translation matrices, and combinations thereof.
 * Vanilla wrote the s16.16 halves by hand; 2S2H builds a float MtxF and copies it (GBI_FLOAT_MTX).
 *
 * We keep the RSP's column-major convention: we are acting on *row vectors* on the *right*.
 *
 * @remark Name inferred from shared Animal Forest functions, meaning of "su" is unclear.
 */

#include "global.h"

/**
 * Constructs a matrix \$f ST \$f, i.e. a scaling \$f S \$f followed by a translation \$f T \$f.
 *
 * The final result is
 *
 * \f[
 *  \begin{bmatrix}
 *      s_x & 0 & 0 & 0 \\
 *      0 & s_y & 0 & 0 \\
 *      0 & 0 & s_z & 0 \\
 *      t_x & t_y & t_z & 1
 *  \end{bmatrix}
 * \f]
 *
 * @remark Original name: "suMtxMakeTS"
 */
void Mtx_SetTranslateScaleMtx(Mtx* mtx, f32 scaleX, f32 scaleY, f32 scaleZ, f32 translateX, f32 translateY,
                              f32 translateZ) {
    MtxF mtxf = {
        { { scaleX, 0, 0, 0 }, { 0, scaleY, 0, 0 }, { 0, 0, scaleZ, 0 }, { translateX, translateY, translateZ, 1 } }
    };
    guMtxF2L(mtxf.mf, mtx);
}

/**
 * Creates a general scale, rotation, translation matrix \$f SRT \$f about a normalised axis.
 *
 * Each row of the rotation is scaled by the matching component of the scale.
 *
 * @warning The sign of the `axisY * sin` term is wrong on elements 20 and 02, which causes the matrix to not be a
 * rotation matrix (and indeed not even necessarily invertible). Kept for vanilla parity.
 *
 * @remark Original name: probably something like "suMtxMakeSRT", although Animal Forest function is a Tait-Bryan
 * rotation rather than axis-angle.
 */
void Mtx_SetTranslationRotationScaleMtx(Mtx* mtx, f32 scaleX, f32 scaleY, f32 scaleZ, s32 angle, f32 axisX, f32 axisY,
                                        f32 axisZ, f32 translateX, f32 translateY, f32 translateZ) {
    f32 sin = Math_SinS(angle);
    f32 cos = Math_CosS(angle);
    f32 yz = axisY * axisZ * (1.0f - cos);
    f32 zx = axisZ * axisX * (1.0f - cos);
    f32 xy = axisX * axisY * (1.0f - cos);
    f32 xx = axisX * axisX;
    f32 yy = axisY * axisY;
    f32 zz = axisZ * axisZ;
    MtxF mtxf = { {
        { (xx + (1.0f - xx) * cos) * scaleX, (xy + axisZ * sin) * scaleX, (zx + axisY * sin) * scaleX, 0.0f },
        { (xy - axisZ * sin) * scaleY, (yy + (1.0f - yy) * cos) * scaleY, (yz + axisX * sin) * scaleY, 0.0f },
        { (zx - axisY * sin) * scaleZ, (yz - axisX * sin) * scaleZ, (zz + (1.0f - zz) * cos) * scaleZ, 0.0f },
        { translateX, translateY, translateZ, 1.0f },
    } };

    guMtxF2L(mtxf.mf, mtx);
}

// Unused
/**
 * Should create an axis-angle rotation matrix; carries the same sign bug as Mtx_SetTranslationRotationScaleMtx().
 *
 * @remark Original name: probably something like "suMtxMakeR" or "suMtxMakeRotateVector"
 */
void Mtx_SetRotationMtx(Mtx* mtx, s32 angle, f32 axisX, f32 axisY, f32 axisZ) {
    Mtx_SetTranslationRotationScaleMtx(mtx, 1.0f, 1.0f, 1.0f, angle, axisX, axisY, axisZ, 0.0f, 0.0f, 0.0f);
}
