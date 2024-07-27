
#include "tools/cairocontext.h"

#include "drawingcontext.h"
#include "inputsequence.h"
#include "layoutscrolledpanel.h"
#include "timer.h"


LayoutScrolledPanel::LayoutScrolledPanel(const ContextBase& context) :
    Super(context),
    m_step_timer(std::make_unique<Timer>(context)),
    m_cancel_timer(std::make_unique<Timer>(context))
{
    stop_scrolling();
}

LayoutScrolledPanel::~LayoutScrolledPanel()
{
}

void LayoutScrolledPanel::dump(std::ostream& out) const
{
    out << "LayoutScrolledPanel(";
    Super::dump(out);
    out << ")";
}

void LayoutScrolledPanel::set_scroll_rect(const Rect& rect)
{
    m_scroll_rect = rect;
    set_damage(get_visible_scrolled_rect());
}

void LayoutScrolledPanel::set_scroll_offset(const Offset& offset)
{
    stop_scrolling();

    Rect r = get_visible_scrolled_rect();

    m_scroll_offset = {std::max(r.w - m_scroll_rect.w, offset.x),
                       std::max(r.h - m_scroll_rect.h, offset.y)};
    update_contents();
}

const Offset& LayoutScrolledPanel::get_scroll_offset()
{
    return m_scroll_offset;
}

void LayoutScrolledPanel::stop_scrolling()
{
    m_last_step_time.set_none();
    m_target_offset = {NONE, NONE};
    m_acceleration = {0, 0};
    m_velocity = {0, 0};
    m_dampening = {0, 0};
    m_step_timer->stop();
}

void LayoutScrolledPanel::lock_x_axis(bool lock)
{
    m_lock_x_axis = lock;
}

void LayoutScrolledPanel::lock_y_axis(bool lock)
{
    m_lock_y_axis = lock;
}

void LayoutScrolledPanel::set_damage(const Rect& scrolled_log_rect)
{
    this->on_damage(scrolled_log_rect);
}

bool LayoutScrolledPanel::on_input_sequence_begin(const InputSequencePtr& sequence)
{
    if (sequence->primary)
    {
        sequence->active_item = getptr();
        drag_initiate(sequence);
    }
    return false;
}

bool LayoutScrolledPanel::on_input_sequence_update(const InputSequencePtr& sequence)
{
    if (is_drag_initiated() &&
        !is_drag_cancelled())
    {
        drag_update(sequence);

        if (is_drag_active())
        {
            sequence->cancel_key_action = true;
            sequence->active_item = getptr();
        }
    }

    return false;
}

bool LayoutScrolledPanel::on_input_sequence_end(const InputSequencePtr& sequence)
{
    if (is_drag_active())
    {
        drag_end();
        sequence->active_item = nullptr;
    }
    else if (is_drag_initiated() &&
             !is_drag_cancelled())
    {
        drag_cancel(sequence);
    }

    return false;
}

void LayoutScrolledPanel::drag_initiate(const InputSequencePtr& sequence)
{
    Point point = sequence->point;
    m_drag_active = false;
    m_drag_cancelled = false;
    m_drag_begin_point = point;
    m_drag_begin_scroll_offset = m_scroll_offset;
    m_drag_begin_time = Clock::now();
    m_dampening = {20, 20};

    Point log_point = m_scrolled_context->canvas_to_log(point);
    if (is_background_at(log_point))
        drag_activate();
    else
        m_cancel_timer->start(std::chrono::milliseconds(350),
                              [=]{return drag_cancel(sequence);});
}

void LayoutScrolledPanel::drag_activate()
{
    m_drag_active = true;
    m_cancel_timer->stop();
    start_animation();
}

bool LayoutScrolledPanel::drag_cancel(const InputSequencePtr& sequence)
{
    // Save sequence, as the only reference left in the timer lambda
    // is being released when the timer is stopped in drag_end().
    InputSequencePtr sequence_ = sequence;

    m_drag_cancelled = true;
    drag_end();

    sequence_->active_item = nullptr;
    if (this->sequence_begin_retry_func)
        this->sequence_begin_retry_func(sequence_);

    return false;
}

void LayoutScrolledPanel::drag_update(const InputSequencePtr& sequence)
{
    using namespace std::chrono;
    Point point = sequence->point;
    Offset delta = point - m_drag_begin_point.value;

    // initiate scrolling?
    if (!is_drag_active())
    {
        double dt = duration_cast<duration<double>>(Clock::now() - m_drag_begin_time).count();
        double d_thresh = 12.0;
        double v_thresh = 100.0;

        double d = std::abs(delta.x);
        double vx = d / dt;
        if (d > d_thresh && vx > v_thresh)
        {
            if (m_lock_x_axis)
                drag_cancel(sequence);
            else
                drag_activate();
        }
        else
        {
            d = std::abs(delta.y);
            double vy = d / dt;
            if (d > d_thresh && vy > v_thresh)
            {
                if (m_lock_y_axis)
                    drag_cancel(sequence);
                else
                    drag_activate();
            }
        }
    }

    if (is_drag_active())
    {
        m_target_offset = m_drag_begin_scroll_offset +
                          get_context()->scale_canvas_to_log(delta);
    }
}

void LayoutScrolledPanel::drag_end()
{
    m_drag_active = false;
    m_drag_begin_point.set_none();
    m_drag_begin_scroll_offset = {};
    m_target_offset = {NONE, NONE};
    m_dampening = {3, 3};
    m_cancel_timer->stop();
}

bool LayoutScrolledPanel::is_drag_initiated()
{
    return !m_drag_begin_point.is_none();
}

bool LayoutScrolledPanel::is_drag_active()
{
    return m_drag_active;
}

bool LayoutScrolledPanel::is_drag_cancelled()
{
    return m_drag_cancelled;
}

void LayoutScrolledPanel::start_animation()
{
    m_step_timer->start(1.0 / 60.0, [this]
    {
        return step_scroll_position();
    });
}

void LayoutScrolledPanel::stop_animation()
{
    m_step_timer->stop();
    m_last_step_time.set_none();
}

bool LayoutScrolledPanel::step_scroll_position()
{
    using namespace std::chrono;

    Vec2 force = {0.0, 0.0};
    const auto& context = get_context();

    TimePoint t = Clock::now();
    if (!m_last_step_time.is_none())
    {
        double dt = std::min(duration_cast<duration<double>>(t - m_last_step_time.value).count(),
                             0.1);  // stay in stable range
        double mass = 0.002;
        double limit_dampening = 15;
        double limit_force_scale = 0.25;

        Rect canvas_rect = get_canvas_rect();
        Rect scroll_rect = context->log_to_canvas_rect(m_scroll_rect);
        Offset scroll_offset = context->scale_log_to_canvas(m_scroll_offset);
        Offset target_offset = {
            m_target_offset.x == NONE ? NONE :
            context->scale_log_to_canvas_x(m_target_offset.x),
            m_target_offset.y == NONE ? NONE :
            context->scale_log_to_canvas_y(m_target_offset.y)
        };

        double tleft = canvas_rect.left() - scroll_rect.left();
        double dleft = scroll_offset.x - tleft;

        double tright = canvas_rect.right() - scroll_rect.right();
        double dright = scroll_offset.x - tright;

        double ttop = canvas_rect.top() - scroll_rect.top();
        double dtop = scroll_offset.y - ttop;

        double tbottom = canvas_rect.bottom() - scroll_rect.bottom();
        double dbottom = scroll_offset.y - tbottom;

        // left limit
        if (dleft > 0)
        {
            force.x -= dleft * limit_force_scale;
            m_dampening.x = limit_dampening;
            if (!is_drag_active() &&
                target_offset.x == NONE)
            {
                target_offset.x = tleft;   // snap to edge
            }
        }

        // right limit
        else if (dright < 0)
        {
            force.x -= dright * limit_force_scale;
            m_dampening.x = limit_dampening;
            if (!is_drag_active() &&
                target_offset.x == NONE)
            {
                target_offset.x = tright;
            }
        }

        // top limit
        if (dtop > 0)
        {
            force.y -= dtop * limit_force_scale;
            m_dampening.y = limit_dampening;
            if (!is_drag_active() &&
                target_offset.y == NONE)
            {
                target_offset.y = ttop;
            }
        }

        // bottom limit
        else
            if (dbottom < 0)
            {
                force.y -= dbottom * limit_force_scale;
                m_dampening.y = limit_dampening;
                if (!is_drag_active() &&
                    target_offset.y == NONE)
                {
                    target_offset.y = tbottom;
                }
            }

        if (target_offset.y != NONE)
            force.x += target_offset.x - scroll_offset.x;
        if (target_offset.y != NONE)
            force.y += target_offset.y - scroll_offset.y;

        m_acceleration.x = force.x / mass;
        m_acceleration.y = force.y / mass;
        m_velocity.x += m_acceleration.x * dt;
        m_velocity.y += m_acceleration.y * dt;
        m_velocity.x *= std::exp(dt * -m_dampening.x);
        m_velocity.y *= std::exp(dt * -m_dampening.y);

        if (!m_lock_x_axis)
        {
            scroll_offset.x += m_velocity.x * dt;
            if (0 && this->is_drag_active() &&
                target_offset.x != NONE)
            {
                scroll_offset.x = target_offset.x;
            }
        }

        if (!m_lock_y_axis)
        {
            scroll_offset.y += m_velocity.y * dt;
            if (0 && this->is_drag_active() &&
                target_offset.y != NONE)
            {
                scroll_offset.y = target_offset.y;
            }
        }

        m_scroll_offset = context->scale_canvas_to_log(scroll_offset);

        m_target_offset.x = target_offset.x == NONE ? NONE :
                                                      context->scale_canvas_to_log_x(target_offset.x);
        m_target_offset.y = target_offset.y == NONE ? NONE :
                                                      context->scale_canvas_to_log_y(target_offset.y);

        idle_call<LayoutScrolledPanel>(this, [](LayoutScrolledPanel* this_) {
            this_->update_contents_on_scroll();
            return 0;  // one-shot
        });
    }

    m_last_step_time = t;

    // stop updates when movement has died down
    if (!is_drag_initiated())
    {
        double eps = 0.5;
        double velocity2 = (m_velocity.x * m_velocity.x +
                            m_velocity.y * m_velocity.y);
        if (force.x < eps &&
            force.y < eps &&
            velocity2 < eps * eps)
        {
            stop_animation();
        }
    }

    return true;
}

void LayoutScrolledPanel::update_contents_on_scroll()
{
    // time.sleep(0.1)
    update_contents();
    on_scroll_offset_changed();
}

void LayoutScrolledPanel::update_contents()
{
    do_fit_inside_canvas(get_canvas_border_rect());
    set_damage(get_visible_scrolled_rect());
}

Rect LayoutScrolledPanel::get_visible_scrolled_rect()
{
    Rect rect = get_rect();
    rect.x -= m_scroll_offset.x;
    rect.y -= m_scroll_offset.y;
    return rect;
}

void LayoutScrolledPanel::do_fit_inside_canvas(const Rect& canvas_border_rect)
{
    Super::do_fit_inside_canvas(canvas_border_rect);

    // Setup children's transformations, take care of the border.
    if (get_border_rect().empty())
    {
        // Clear all item's transformations if there are no visible items.
        for (auto item : get_children())
            item->get_context()->canvas_rect = {};
    }
    else
    {
        auto context = std::make_unique<LayoutContext>();
        context->log_rect = get_border_rect();
        context->canvas_rect = get_canvas_rect();  // exclude border
        context->canvas_rect.x +=
                get_context()->scale_log_to_canvas_x(m_scroll_offset.x);
        context->canvas_rect.y +=
                get_context()->scale_log_to_canvas_y(m_scroll_offset.y);

        for (auto item : get_children())
        {
            Rect rect = context->log_to_canvas_rect(item->get_context()->log_rect);
            item->do_fit_inside_canvas(rect);
        }

        m_scrolled_context = std::move(context);
    }
}

void LayoutScrolledPanel::draw_item(DrawingContext& dc)
{
    auto cc = dc.get_cc();
    cc->save();
    cc->set_source_rgba(get_fill_color());
    cc->rectangle(get_canvas_rect());
    cc->fill();
    cc->restore();
}

