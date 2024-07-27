#ifndef NONEABLES_H
#define NONEABLES_H

#include <ostream>
#include <type_traits>

// None-able double
template <class T>
class Noneable
{
    public:
        Noneable<T>() = default;
        Noneable<T>(const T& val)
        {
            value = val;
            none = false;
        }

        Noneable<T>& operator=(const Noneable<T>& val)
        {
            value = val.value;
            none = val.none;
            return *this;
        }
        Noneable<T>& operator=(const T& val)
        {
            value = val;
            none = false;
            return *this;
        }

        bool operator==(const Noneable<T>& other) const
        {
            return this->none == other.none &&
                   (this->none == true || this->value == other.value);
        }

        bool operator==(T val) const
        {
            return value == val && !none;
        }

        bool operator!=(T val) const
        {
            return !operator==(val);
        }

        // cast to bool only for bool
        template <typename U = T,
                  typename = typename std::enable_if<std::is_same<U, bool>::value >::type >
        operator bool() const
        {
            return value && !none;
        }

        // cast to value type not for bool
        template <typename U = T,
                  typename = typename std::enable_if<!std::is_same<U, bool>::value >::type >
        operator T&()
        {
            return value;
        }

        template <typename U = T,
                  typename = typename std::enable_if<!std::is_same<U, bool>::value >::type >
        operator const T&() const
        {
            return value;
        }

        bool is_none() const
        {
            return none;
        }

        void set_none()
        {
            none = true;
        }

    public:
        T value{};
        bool none{true};

};

template<class T>
bool operator<(const Noneable<T>& n, const T& val)
{ return n.value < val; }
template<class T>
bool operator<(const T& val, const Noneable<T>& n)
{ return val < n.value; }

template<class T>
bool operator<=(const Noneable<T>& n, const T& val)
{ return n.value <= val; }
template<class T>
bool operator<=(const T& val, const Noneable<T>& n)
{ return val <= n.value; }

template<class T>
bool operator>(const Noneable<T>& n, const T& val)
{ return n.value > val; }
template<class T>
bool operator>(const T& val, const Noneable<T>& n)
{ return val > n.value; }

template<class T>
bool operator>=(const Noneable<T>& n, const T& val)
{ return n.value >= val; }
template<class T>
bool operator>=(const T& val, const Noneable<T>& n)
{ return val >= n.value; }


template<class T>
std::ostream& operator<<(std::ostream& s, const Noneable<T>& n){
    if (n.is_none())
        s << "None";
    else
        s << n.value;
    return s;
}

#endif // NONEABLES_H
