#include "SetExitList.h"

namespace SOH {
uint32_t* SetExitList::GetPointer() {
    return exits.data();
}

size_t SetExitList::GetPointerSize() {
    return exits.size() * sizeof(uint32_t);
}
} // namespace SOH
