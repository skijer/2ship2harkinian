#ifndef MIC_OCARINA_H
#define MIC_OCARINA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Recognizes a hummed song by relative pitch and hands it to the game's native
// recognition flow. Opens the capture device on first call and closes it once
// the ocarina is put away.
void MicOcarina_Update(void);

#ifdef __cplusplus
}
#endif

#endif // MIC_OCARINA_H
