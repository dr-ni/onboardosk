#ifndef CLICKGENERATOR_H
#define CLICKGENERATOR_H

#include "onboardoskglobals.h"


class ClickGenerator : public ContextBase
{
    public:
        using Super = ContextBase;

        ClickGenerator(const ContextBase& context) :
            Super (context)
        {}

        void end_mapped_click() {}  // TODO
};

#endif // CLICKGENERATOR_H
