#include "keyboardscanner.h"

using std::string;


KeyboardScanner::~KeyboardScanner()
{

}

void KeyboardScanner::update_layer(LayoutItemPtr& layout, const string& layer_id, bool force_update)
{
    (void)layout;
    (void)layer_id;
    (void)force_update;
}
