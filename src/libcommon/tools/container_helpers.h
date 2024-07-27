#ifndef TOOLS_CONTAINER_H
#define TOOLS_CONTAINER_H

#include <array>
#include <climits>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <memory>
#include <numeric>   // iota

template <typename V, typename... T>
constexpr auto array_of(T&&... t) -> std::array <V, sizeof...(T)>
{
    return {{ std::forward<T>(t)... }};
}

template <typename T, size_t N>
bool contains(const std::array<T, N>& a, const T& e)
{
    return std::find(a.begin(), a.end(), e) != a.end();
}

template <typename T>
bool contains(const std::vector<T>& v, const T& e)
{
    return std::find(v.begin(), v.end(), e) != v.end();
}
// one more to make strings literals work as keys
inline bool contains(const std::vector<std::string>& v, const std::string& e)
{
    return std::find(v.begin(), v.end(), e) != v.end();
}

template <typename TKey, typename TValue>
bool contains(const std::vector<std::pair<TKey, TValue>>& v, const TKey& key)
{
    auto it = std::find_if(v.begin(), v.end(),
        [&](const std::pair<TKey, TValue>& e){return e.first == key;});
    return it != v.end();
}

template <typename T>
bool contains(const std::set<T>& s, const T& e)
{
    return s.count(e) != 0;
}

template <typename TKey, typename TValue>
bool contains(const std::map<TKey, TValue>& m, const TKey& e)
{
    return m.count(e) != 0;
}
// one more to make strings literals work as keys
template <typename TValue>
bool contains(const std::map<std::string, TValue>& m, const std::string& e)
{
    return m.count(e) != 0;
}

constexpr const size_t NPOS (static_cast<size_t>(-1));

template <typename T, size_t N>
size_t find_index(const std::array<T, N>& v, const T& e)
{
    auto it = std::find(v.begin(), v.end(), e);
    if (it == v.end())
        return NPOS;
    return std::distance(v.begin(), it);
}

template <size_t N>
size_t find_index(const std::array<const char*, N>& v, const std::string& e)
{
    auto it = std::find(v.begin(), v.end(), e);
    if (it == v.end())
        return NPOS;
    return std::distance(v.begin(), it);
}

template <typename T>
bool find_index(const std::vector<T>& v, const T& e, size_t& index)
{
    auto it = std::find(v.begin(), v.end(), e);
    if (it == v.end())
        return false;
    index = std::distance(v.begin(), it);
}

template <typename T>
size_t find_index(const std::vector<T>& v, const T& e)
{
    auto it = std::find(v.begin(), v.end(), e);
    if (it == v.end())
        return NPOS;
    return std::distance(v.begin(), it);
}

template <typename TKey, typename TValue>
typename std::vector<std::pair<TKey, TValue>>::iterator find_key(std::vector<std::pair<TKey, TValue>>& v, const TKey& key)
{
    auto it = std::find_if(v.begin(), v.end(),
        [&](const std::pair<TKey, TValue>& e){return e.first == key;});
    if (it != v.end())
        return it;
    return v.end();
}

// update set from the contents of another map, like Python's dict.update()
template <typename T>
void update_set(std::set<T>& set1, const std::set<T>& set2)
{
    for (const auto& e : set2)
        set1.emplace(e);
}

// update map from the contents of another map, like Python's dict.update()
template <class TKey, class TValue>
void update_map(std::map<TKey, TValue>& map1, const std::map<TKey, TValue>& map2)
{
    for (const auto& it : map2)
        map1[it.first] = it.second;
}

template <typename TKey, typename TValue>
const TValue get_value_default(const std::map<TKey, TValue>& m, const TKey& key,
                        const TValue& default_value={})
{
    auto it = m.find(key);
    if (it == m.end())
        return default_value;
    return it.second;
}
// one more to make char strings work as keys
template <typename TValue>
const TValue get_value_default(const std::map<std::string, TValue>& m, const std::string& key,
                        const TValue& default_value={})
{
    auto it = m.find(key);
    if (it == m.end())
        return default_value;
    return it->second;
}

template <typename TKey, typename TValue>
const TValue get_value(const std::map<TKey, TValue>& m, const TKey& key)
{
    auto it = m.find(key);
    if (it == m.end())
        return {};
    return it->second;
}

// one more to make char strings work as keys
template <typename TValue>
const TValue get_value(const std::map<std::string, TValue>& m, const std::string& key)
{
    auto it = m.find(key);
    if (it == m.end())
        return {};
    return it->second;
}

// get_value that returns true if the key was available
template <typename TValue>
bool get_value(const std::map<std::string, TValue>& m, const std::string& key,
               TValue& value_out)
{
    auto it = m.find(key);
    if (it == m.end())
        return false;
    value_out = it->second;
    return true;
}
template <typename TKey, typename TValue>
bool get_value(const std::map<TKey, TValue>& m, const TKey& key,
               TValue& value_out)
{
    auto it = m.find(key);
    if (it == m.end())
        return false;
    value_out = it->second;
    return true;
}

template <typename TKey, typename TValue>
std::vector<TValue> get_values(const std::map<TKey, TValue>& m)
{
    std::vector<TValue> v;
    for (const auto& it : m)
        v.emplace_back(it.second);
    return v;
}

template <typename TKey, typename TValue>
std::vector<TValue> get_values(const std::vector<std::pair<TKey, TValue>>& v)
{
    std::vector<TValue> result;
    for (const auto& e : v)
        result.emplace_back(e.first);
    return result;
}

template <typename TKey, typename TValue>
std::vector<TKey> get_keys(const std::vector<std::pair<TKey, TValue>>& v)
{
    std::vector<TKey> result;
    for (const auto& e : v)
        result.emplace_back(e.first);
    return result;
}

template <typename T>
std::vector<T> get_keys(const std::set<T>& s)
{
    std::vector<T> v;
    for (const auto& e : s)
        v.emplace_back(e);
    return v;
}

template <typename TKey, typename TValue>
std::vector<TKey> get_keys(const std::map<TKey, TValue>& m)
{
    std::vector<TKey> v;
    for (const auto& it : m)
        v.emplace_back(it.first);
    return v;
}

// Return a reference to the value at key <key>. If it doesn't
// exist create it with <default_value>.
// (like python's dict.setdefault)
template <typename TKey, typename TValue>
TValue& set_default(std::vector<std::pair<TKey, TValue>>& v, const TKey& key, const TValue& default_value={})
{
    auto it = std::find_if(v.begin(), v.end(),
        [&](const std::pair<TKey, TValue>& e){return e.first == key;});
    if (it != v.end())
        return it->second;
    v.emplace_back(key, default_value);
    return v.back().second;
}

// Return a reference to the value at key <key>. If it doesn't
// exist create it with <default_value>.
// (like python's dict.setdefault)
template <typename TKey, typename TValue>
TValue& set_default(std::map<TKey, TValue>& m, const TKey& key, const TValue& default_value={})
{
    auto it = m.find(key);
    if (it == m.end())
        it = m.emplace(key, default_value).first;
    return it->second;
}

template <typename T>
void remove(std::vector<T>& v, const T& e)
{
    auto it = std::find(v.begin(), v.end(), e);
    if (it != v.end())
        v.erase(it);
}
inline void remove(std::vector<std::string>& v, const char* e)
{
    auto it = std::find(v.begin(), v.end(), e);
    if (it != v.end())
        v.erase(it);
}

template <typename TKey, typename TValue>
void remove(std::map<TKey, TValue>& m, const TKey& key)
{
    auto it = m.find(key);
    if (it != m.end())
        m.erase(it);
}
// one more to make char strings work as keys
template <typename TValue>
void remove(std::map<std::string, TValue>& m, const std::string& key)
{
    auto it = m.find(key);
    if (it != m.end())
        m.erase(it);
}

template <typename T, typename F>
void remove_any(std::vector<T>& v, const F& func)
{
    v.erase(std::remove_if(v.begin(), v.end(), func), v.end());
}

// extend vector with vector
// append vector to vector
template <typename T>
void extend(std::vector<T>& a, const std::vector<T>& b)
{
    a.insert(a.end(), b.begin(), b.end());
}
inline void extend(std::vector<std::string>& a, const std::vector<const char*>& b)
{
    a.insert(a.end(), b.begin(), b.end());
}


// add vector to vector
// relies on copy elision for efficiency
template <typename T>
std::vector<T> concat(std::vector<T>& a, const std::vector<T>& b)
{
    std::vector<T>{a};
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

// add vector to vector with operator
// relies on copy elision for efficiency
template <typename T>
std::vector<T>& operator+=(std::vector<T> &a, const std::vector<T> &b)
{
    a.insert(a.end(), b.begin(), b.end());
    return a;
}
template <typename T>
std::vector<T> operator+(const std::vector<T> &a, const std::vector<T> &b)
{
    std::vector<T> result;
    result.reserve(a.size() + b.size());
    result.insert(result.end(), a.begin(), a.end());
    result.insert(result.end(), b.begin(), b.end());
    return result;
}

template <typename T>
std::vector<T> slice(const std::vector<T>& v, int begin, int end=INT_MAX, int step=1)
{
    std::vector<T> result;
    int n = v.size();
    int b = begin >= 0 ? begin : n + begin;
    int e =   end >= 0 ? end   : n + end;

    if (step >= 0)
    {
        b = std::max(b, 0);
        e = std::min(e, n);
        for (int i=b; i<e; i+=step)
            result.emplace_back(v[i]);
    }
    else
    {
        b = std::max(b, n-1);
        e = std::min(b, 0);
        for (int i=b; i>=e; i+=step)
            result.emplace_back(v[i]);
    }
    return result;
}

template <typename T>
std::vector<T> union_sort_inplace(std::vector<T>& a, std::vector<T>& b)
{
    std::vector<T> result(a.size() + b.size());
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    auto it = std::set_union(a.begin(), a.end(), b.begin(), b.end(), result.begin());
    result.resize(it - result.begin());
    return result;
}

template <typename T>
std::vector<T> difference_sort_inplace(std::vector<T>& a, std::vector<T>& b)
{
    std::vector<T> result(a.size() + b.size());
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    auto it = std::set_difference(a.begin(), a.end(), b.begin(), b.end(), result.begin());
    result.resize(it - result.begin());
    return result;
}

template <typename T>
std::vector<T> difference(const std::vector<T>& a_, const std::vector<T>& b_)
{
    std::vector<T> a = a_;
    std::vector<T> b = b_;
    std::vector<T> result(a.size() + b.size());

    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    auto it = std::set_difference(a.begin(), a.end(), b.begin(), b.end(), result.begin());
    result.resize(it - result.begin());
    return result;
}

template <typename T>
struct reverse_helper {T& iterable;};
template <typename T>
auto begin(reverse_helper<T> w) {return rbegin(w.iterable);}
template <typename T>
auto end(reverse_helper<T> w) {return rend(w.iterable);}

template <typename T>
reverse_helper<T> reversed(T&& iterable) {return {iterable};}

#endif // TOOLS_CONTAINER_H
