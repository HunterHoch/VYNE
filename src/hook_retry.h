#pragma once
#include <MinHook.h>

namespace vyne {
// MinHook uses MEMORY_ALLOC for thread-snapshot enumeration failure as well
// as allocation failure. Retry that status within a fixed worker-thread budget;
// permanent protection/target errors must still fail immediately.
template<class Operation, class Pause>
MH_STATUS retry_hook_snapshot(Operation operation, Pause pause, unsigned& attempts) {
    MH_STATUS status = MH_OK;
    for (attempts = 1; attempts <= 80; ++attempts) {
        status = operation();
        if (status != MH_ERROR_MEMORY_ALLOC || attempts == 80) return status;
        pause(25);
    }
    return status;
}
}
