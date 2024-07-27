#ifndef ONBOARDOSKVIEW_H
#define ONBOARDOSKVIEW_H

typedef enum   // = mutter/common.h: MetaCursor
{
    ONBOARD_OSK_CURSOR_NONE = 0,
    ONBOARD_OSK_CURSOR_DEFAULT,
    ONBOARD_OSK_CURSOR_NORTH_RESIZE,
    ONBOARD_OSK_CURSOR_SOUTH_RESIZE,
    ONBOARD_OSK_CURSOR_WEST_RESIZE,
    ONBOARD_OSK_CURSOR_EAST_RESIZE,
    ONBOARD_OSK_CURSOR_SE_RESIZE,
    ONBOARD_OSK_CURSOR_SW_RESIZE,
    ONBOARD_OSK_CURSOR_NE_RESIZE,
    ONBOARD_OSK_CURSOR_NW_RESIZE,
    ONBOARD_OSK_CURSOR_MOVE_OR_RESIZE_WINDOW,
} OnboardOskCursorType;


typedef enum
{
    ONBOARD_OSK_VIEW_TYPE_NONE,           // error, forgot to initialize
    ONBOARD_OSK_VIEW_TYPE_KEYBOARD,
    ONBOARD_OSK_VIEW_TYPE_LAYOUT_POPUP,   // interactive
    ONBOARD_OSK_VIEW_TYPE_LABEL_POPUP,    // non-interactive
} OnboardOskViewType;

struct _OnboardOskView
{
    struct _OnboardOskContext* ooc;
    OnboardOskViewType view_type;
    const char* name;   // for looking up actors from JavaScript

    void* user_data;
};
typedef struct _OnboardOskView OnboardOskView;

struct _OnboardOskViewGeometry
{
    double x;
    double y;
    double w;
    double h;
};
typedef struct _OnboardOskViewGeometry OnboardOskViewGeometry;

#endif // ONBOARDOSKVIEW_H
