#ifndef TOOLS_STREAM_H
#define TOOLS_STREAM_H

#include <array>
#include <map>
#include <ostream>
#include <set>
#include <optional>
#include <sstream>
#include <vector>


template<class TFirst, class TSecond>
std::ostream& operator<< (std::ostream& out, const std::pair<TFirst, TSecond>& p)
{
    out << "<" << p.first << ", " << p.second << ">";
    return out;
}

template <typename T>
std::ostream& operator<< (std::ostream& out, const std::vector<T>& v) {
    out << '{';
    if (!v.empty())
    {
        auto sep = "";
        for(auto& x : v)
        {
            out << sep << x;
            sep = ", ";
        }
    }
    out << '}';
    return out;
}

template <typename T>
std::ostream& operator<< (std::ostream&& out, const std::vector<T>& v) {
    out << '{';
    if (!v.empty())
    {
        auto sep = "";
        for(auto& x : v)
        {
            out << sep << x;
            sep = ", ";
        }
    }
    out << '}';
    return out;
}

template <typename T, std::size_t N>
std::ostream& operator<< (std::ostream& out, const std::array<T, N>& v) {
    out << '{';
    if (!v.empty())
    {
        auto sep = "";
        for(auto& x : v)
        {
            out << sep << x;
            sep = ", ";
        }
    }
    out << '}';
    return out;
}

template <typename TKey>
std::ostream& operator<< (std::ostream& out, const std::set<TKey>& s) {
    out << '{';
    if (!s.empty())
    {
        auto sep = "";
        for(auto& k : s)
        {
            out << sep << k;
            sep = ", ";
        }
    }
    out << '}';
    return out;
}

template <typename TKey, typename TValue>
std::ostream& operator<< (std::ostream& out, const std::map<TKey, TValue>& m) {
    out << '{';
    if (!m.empty())
    {
        auto sep = "";
        for(auto& it : m)
        {
            out << sep << it.first << ": " << it.second;
            sep = ", ";
        }
    }
    out << '}';
    return out;
}

template <typename T>
std::string to_string(const std::vector<T>& v)
{
    std::stringstream ss;
    ss << v;
    return ss.str();
}

template <typename TKey, typename TValue>
std::string to_string(const std::map<TKey, TValue>& m)
{
    std::stringstream ss;
    ss << m;
    return ss.str();
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::optional<T>& o) {
    out << "{";
    if (o)
        out << o.value();
    out << "}";
    return out;
}

#endif // TOOLS_STREAM_H
