#include "globalkeylistener.h"

#include "signalling.h"

using std::string;
using std::vector;


std::unique_ptr<GlobalKeyListener> GlobalKeyListener::make(const ContextBase& context)
{
    return std::make_unique<GlobalKeyListener>(context);
}

GlobalKeyListener::GlobalKeyListener(const ContextBase& context) :
    Super(context)
{

}

GlobalKeyListener::~GlobalKeyListener()
{
}

