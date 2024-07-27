
#include <algorithm>
#include <memory>
#include "tools/logger.h"

#include "keyboard.h"
#include "layoutview.h"
#include "onboardosk.h"
#include "onboardoskcallbacks.h"
#include "onboardoskglobals.h"
#include "textrendererpangocairo.h"
#include "wordsuggestions.h"


Logger* ContextBase::logger()
{
    if (m_globals && m_globals->m_logger)
        return m_globals->m_logger.get();
    return Logger::get_default().get();
}

const Logger* ContextBase::logger() const
{
    if (m_globals && m_globals->m_logger)
        return m_globals->m_logger.get();
    return Logger::get_default().get();
}

OnboardOskContext* ContextBase::get_cinstance()
{
    return m_globals->m_instance;
}

KeyboardKeyLogic* ContextBase::get_key_logic() const
{
    auto keyboard = get_keyboard();
    return keyboard->get_key_logic();
}

WPEngine* ContextBase::get_wp_engine() const
{
    if (auto ws = get_word_suggestions())
        return ws->WordSuggestions::get_wp_engine();
    return {};
}

TextContext* ContextBase::get_text_context() const
{
    if (auto ws = get_word_suggestions())
        return ws->WordSuggestions::get_text_context();
    return {};
}

LayoutViews& ContextBase::get_keyboard_layout_views()
{
    return m_globals->m_layout_views;
}

std::vector<LayoutView*> ContextBase::get_layout_views()
{
    std::vector<std::string> vv;
    for(auto& view : get_toplevel_views())
        vv.emplace_back(view->name);

    std::vector<LayoutView*> results;

    for(auto& view : get_toplevel_views())
    {
        view->for_each([&](const ViewBasePtr& v)
        {
            LayoutView* lv = dynamic_cast<LayoutView*>(v.get());
            if (lv)
                results.emplace_back(lv);
        });
    }
    return results;
}

WordSuggestions* ContextBase::get_word_suggestions() const
{
    auto keyboard = get_keyboard();
    return keyboard->get_word_suggestions();
}

void ContextBase::clear_text_renderers()
{
    m_globals->m_text_renderers.clear();
}

TextRendererPangoCairo* ContextBase::get_text_renderer(
        TextRendererSlot slot,
        FontSizeInt font_size_int)
{
    TextRendererPangoCairo* tr{};
    TextRenderers& trs = get_text_renderers();
    auto it = trs.find({slot, font_size_int});
    if (it == trs.end())
    {
        auto callbacks = get_global_callbacks();
        auto view = get_first_toplevel_view();
        if (callbacks->view_get_pango_context && view)
        {
            PangoContext* pc = callbacks->view_get_pango_context(view);
            auto trptr = std::make_unique<TextRendererPangoCairo>(pc);
            tr = trptr.get();
            trs[{slot, font_size_int}] = std::move(trptr);
        }
    }
    else
        tr = it->second.get();

    return tr;
}

bool ContextBase::is_composited()
{
    auto callbacks = get_global_callbacks();
    if (callbacks->is_composited && callbacks->is_composited())
        return true;
    return false;
}

bool ContextBase::is_toplevel_view(ViewBase* view)
{
    for (auto& v : get_toplevel_views())
        if (v == view)
            return true;
    return false;
}

bool ContextBase::is_leaf_view(ViewBase* view)
{
    for (auto& v : get_keyboard_layout_views())
        if (v.get() == view)
            return true;
    return false;
}

ViewBase* ContextBase::get_first_toplevel_view()
{
    auto& toplevel_views = get_toplevel_views();
    if (!toplevel_views.empty())
        return toplevel_views[0];
    return {};
}

void ContextBase::add_toplevel_view(ViewBase* view)
{
    auto& views = m_globals->m_toplevel_views;
    views.emplace_back(view);

    // sort by view type, i.e. move popups to the end
    std::stable_sort(views.begin(), views.end(),
                     [](const ViewBase* a, const ViewBase* b)
                     {return a->get_view_type() < b->get_view_type();});
}

void ContextBase::remove_toplevel_view(ViewBase* view)
{
    auto& views = m_globals->m_toplevel_views;
    remove(views, view);
}
