
#include "tools/container_helpers.h"
#include "handle.h"


constexpr std::array<Handle::Enum, 4> Handle::EDGES;
constexpr std::array<Handle::Enum, 4> Handle::CORNERS;
constexpr std::array<Handle::Enum, 8> Handle::RESIZERS;
constexpr std::array<Handle::Enum, 5> Handle::TOP_RESIZERS;
constexpr std::array<Handle::Enum, 5> Handle::BOTTOM_RESIZERS;
constexpr const std::array<Handle::Enum, 9> Handle::RESIZE_MOVE;
constexpr std::array<Handle::Enum, 9> Handle::ALL;

std::string Handle::handle_to_id(Handle::Enum handle)
{
    static std::map<Handle::Enum, std::string> m = {
        {EAST,       "E"},
        {SOUTH_WEST, "SW"},
        {SOUTH,      "S"},
        {SOUTH_EAST, "SE"},
        {WEST,       "W"},
        {NORTH_WEST, "NW"},
        {NORTH,      "N"},
        {NORTH_EAST, "NE"},
        {MOVE,       "M"}
    };
    return get_value(m, handle);
}

Handle::Enum Handle::id_to_handle(const std::string& id)
{
    static std::map<std::string, Handle::Enum> m = {
        {"E",  EAST},
        {"SW", SOUTH_WEST},
        {"S",  SOUTH},
        {"SE", SOUTH_EAST},
        {"W",  WEST},
        {"NW", NORTH_WEST},
        {"N",  NORTH},
        {"NE", NORTH_EAST},
        {"M",  MOVE}
    };
    return get_value_default(m, id, Handle::NONE);
}
