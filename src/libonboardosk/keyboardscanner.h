#ifndef KEYBOARDSCANNER_H
#define KEYBOARDSCANNER_H

#include <string>

#include "layoutdecls.h"
#include "onboardoskglobals.h"

class KeyboardScanner : public ContextBase
{
    public:
        KeyboardScanner(const ContextBase& context) :
            ContextBase(context)
        {}
        virtual ~KeyboardScanner();

        void update_layer(LayoutItemPtr& layout, const std::string& layer_id, bool force_update=false);
};

#endif // KEYBOARDSCANNER_H
