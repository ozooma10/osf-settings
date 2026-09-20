#pragma once

#include "RE/B/BSInputEventUser.h"

namespace OSFSettings
{
    class BSInputEventUserStandalone : public RE::BSInputEventUser
    {
    protected:
        BSInputEventUserStandalone();
        ~BSInputEventUserStandalone() override;

        BSInputEventUserStandalone(const BSInputEventUserStandalone&) = delete;
        BSInputEventUserStandalone& operator=(const BSInputEventUserStandalone&) = delete;
    };
    static_assert(sizeof(BSInputEventUserStandalone) == sizeof(RE::BSInputEventUser));
}
