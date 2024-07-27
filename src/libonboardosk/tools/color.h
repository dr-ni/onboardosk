#ifndef COLOR_H
#define COLOR_H

#include <ostream>
#include <cmath>

template <class T>
class THLS;

template <class T>
class TRGBA
{
    public:
        TRGBA() {}
        TRGBA(T r_, T g_, T b_, T a_) :
            r(r_), g(g_), b(b_), a(a_)
        {}

        TRGBA<T> average(TRGBA<T> other) const
        {
            return {(r+other.r) / 2,
                    (g+other.g) / 2,
                    (b+other.b) / 2,
                    (a+other.a) / 2};
        }

        // Make color brighter by amount a [-1.0...1.0]
        TRGBA<T> brighten(T amount) const
        {
            THLS<T> hls = to_hls();
            hls.l += amount;
            if (hls.l > 1.0)
                hls.l = 1.0;
            if (hls.l < 0.0)
                hls.l = 0.0;
            return hls.to_rgb();
        }

        // convert RGB to HLS colorspace
        THLS<T> to_hls() const
        {
            THLS<T> hls;
            hls.a = a;
            T max_comp = std::max(std::max(r, g), b);
            T min_comp = std::min(std::min(r, g), b);
            T sum = max_comp + min_comp;
            T dif = max_comp - min_comp;

            hls.l = sum / 2.0;

            if (min_comp == max_comp)
            {
                hls.h = hls.s = 0.0;
                return hls;
            }

            if (hls.l <= 0.5)
                hls.s = dif / sum;
            else
                hls.s = (max_comp-min_comp) / (2.0-dif);
            T rc = (max_comp-r) / dif;
            T gc = (max_comp-g) / dif;
            T bc = (max_comp-b) / dif;
            if (r == max_comp)
                hls.h = bc - gc;
            else if (g == max_comp)
                hls.h = 2.0 + rc - bc;
            else
                hls.h = 4.0 + gc - rc;
            hls.h = std::fmod((hls.h / 6.0), 1.0);
            return hls;
        }

    public:
        T r{};
        T g{};
        T b{};
        T a{1.0};
};
typedef TRGBA<double> RGBA;

template<class T>
std::ostream& operator<<(std::ostream& s, const TRGBA<T>& rgba){
    s << "RGBA(" << rgba.r << ", " << rgba.g << ", " << rgba.b << ", " << rgba.a << ")";
    return s;
}


template <class T>
class THLS
{
    public:
        THLS() {}
        THLS(T h_, T l_, T s_, T a_) :
            h(h_), l(l_), s(s_), a(a_)
        {}

        TRGBA<T> to_rgb() const
        {
            TRGBA<T> rgba;
            rgba.a = a;
            if (s == 0.0)
                rgba.r = rgba.b = rgba.g = l;
            else
            {
                T q = (l <= 0.5) ? l * (1.0 + s) : l + s - (l * s);
                T p = 2.0 * l - q;
                rgba.r = hue_to_component(p, q, h+1.0/3.0);
                rgba.g = hue_to_component(p, q, h);
                rgba.b = hue_to_component(p, q, h-1.0/3.0);
            }
            return rgba;
        }


    private:
        T hue_to_component(T m1, T m2, T hue) const
        {
            hue = std::fmod(hue, 1.0);
            if (hue < 0.0)
                hue += 1.0;

            if (hue < 1.0/6.0)
                return m1 + (m2-m1) * hue * 6.0;
            if (hue < 0.5)
                return m2;
            if (hue < 2.0/3.0)
                return m1 + (m2 - m1) * (2.0/3.0 - hue) * 6.0;
            return m1;
        }

    public:
        T h{};
        T l{};
        T s{};
        T a{1.0};
};
typedef THLS<double> HLS;


#endif // COLOR_H
