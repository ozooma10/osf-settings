#include "BSInputEventUserStandalone.h"

namespace OSFSettings
{
    BSInputEventUserStandalone::BSInputEventUserStandalone()
    {
        using func_t = RE::BSInputEventUser* (*)(RE::BSInputEventUser*);
        static REL::Relocation<func_t> construct{ RE::ID::BSInputEventUser::ctor };
        construct(this);
    }

    BSInputEventUserStandalone::~BSInputEventUserStandalone()
    {
        using func_t = void* (*)(RE::BSInputEventUser*, std::uint32_t);
        static REL::Relocation<func_t> destroy{ RE::ID::BSInputEventUser::ScalarDeletingDestructor };
        destroy(this, 0);  // Run the scalar deleting destructor without freeing storage.
    }
}
