#ifndef HANDLE_H
#define HANDLE_H

#include <array>
#include <map>
#include <string>


// window handles
class Handle
{
    public:
        enum Enum {
            NORTH_WEST,
            NORTH,
            NORTH_EAST,
            WEST,
            EAST,
            SOUTH_WEST,
            SOUTH,
            SOUTH_EAST,
            MOVE,
            NONE,
        };

        static constexpr std::array<Enum, 4> EDGES{
                EAST, SOUTH, WEST, NORTH
        };
        static constexpr std::array<Enum, 4> CORNERS{
            SOUTH_EAST, SOUTH_WEST, NORTH_WEST, NORTH_EAST
        };
        static constexpr std::array<Enum, 8> RESIZERS{
            EAST, SOUTH_EAST, SOUTH, SOUTH_WEST,
            WEST, NORTH_WEST, NORTH, NORTH_EAST
        };
        static constexpr std::array<Enum, 5> TOP_RESIZERS{
            EAST, WEST, NORTH_WEST, NORTH, NORTH_EAST
        };
        static constexpr std::array<Enum, 5> BOTTOM_RESIZERS{
            EAST, SOUTH_EAST, SOUTH, SOUTH_WEST, WEST
        };
        static constexpr const std::array<Enum, 9> RESIZE_MOVE{
            EAST, SOUTH_EAST, SOUTH, SOUTH_WEST,
            WEST, NORTH_WEST, NORTH, NORTH_EAST, MOVE
        };
        static constexpr std::array<Enum, 9> ALL = RESIZE_MOVE;

        static std::string handle_to_id(Enum handle);
        static Handle::Enum id_to_handle(const std::string& id);
};

#endif // HANDLE_H
