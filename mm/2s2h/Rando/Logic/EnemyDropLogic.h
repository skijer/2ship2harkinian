#ifndef RANDO_ENEMY_DROP_LOGIC_H
#define RANDO_ENEMY_DROP_LOGIC_H

#include "2s2h/Rando/Rando.h"

namespace Rando {

namespace Logic {

// A drop is awarded by actor id wherever that actor spawns, so under the enemy randomizer each drop check follows the
// seed's placement: it is added where its actor now replaces another drop's actor, and leaves the pool when its actor
// spawns nowhere it can be reached. Called whenever the placement changes.
void RefreshEnemyDropLogic();
bool IsEnemyDropEarnable(RandoCheckId check);

} // namespace Logic

} // namespace Rando

#endif // RANDO_ENEMY_DROP_LOGIC_H
