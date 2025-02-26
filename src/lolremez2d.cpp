//
//  LolRemez — Remez algorithm implementation
//
//  Copyright © 2005—2020 Sam Hocevar <sam@hocevar.net>
//
//  This program is free software. It comes without any warranty, to
//  the extent permitted by applicable law. You can redistribute it
//  and/or modify it under the terms of the Do What the Fuck You Want
//  to Public License, Version 2, as published by the WTFPL Task Force.
//  See http://www.wtfpl.net/ for more details.
//

#if HAVE_CONFIG_H
#   include "config.h"
#endif

#include <iostream>
#include <iomanip>
#include <array>

#include <lol/real>

#include "matrix.h"

using namespace lol;

// FIXME: use rvec2 when possible
struct real2
{
    real x, y;
};

real f(real const &x, real const &y)
{
    static real const one = real::R_1();

    //return exp(sin(x+x+x)+cos(y-one/4));
    real f = (x + one) / 2;
    real d = (y + one) / 2;
    return sin((one - f) * acos(d)) / sqrt(one - d * d);
}

struct solver
{
    solver(int grid_size, int iters)
      : m_grid_size(grid_size),
        m_iters(iters),
        m_ek(iters, iters)
    {
        for (int i = 0; i <= grid_size; ++i)
            m_coeff.push_back(cheb(i, grid_size));
    }

    void step()
    {
        /* Find a new good pivot */
        real2 best_pivot { 0, 0 };
        real best_val(0);

        for (auto const &y : m_coeff)
        for (auto const &x : m_coeff)
        {
            real res = eval_ek(x, y);
            if (fabs(res) >= fabs(best_val))
            {
                best_pivot = { x, y };
                best_val = res;
            }
        }

        /* Compute d_k = 1/e_{k-1}(x,y) */
        real dk = real::R_1() / best_val;

        /* Compute e_{k-1}(x_k,y) as a vector of f(x_i,y) components
         * and e_{k-1}(x,y_k) as a vector of f(x,y_i) components */
        std::vector<real> ek_x, ek_y;
        ek_x.resize(m_pivots.size() + 1);
        ek_y.resize(m_pivots.size() + 1);
        for (size_t i = 0; i < m_pivots.size(); ++i)
            for (size_t j = 0; j < m_pivots.size(); ++j)
                if (m_ek[j][i])
                {
                    ek_x[j] += m_ek[j][i] * eval_f(m_pivots[i].x, best_pivot.y);
                    ek_y[i] += m_ek[j][i] * eval_f(best_pivot.x, m_pivots[j].y);
                }
        ek_x[m_pivots.size()] = 1; /* implicit f */
        ek_y[m_pivots.size()] = 1; /* implicit f */

        /* Compute new fk */
        for (size_t i = 0; i < m_pivots.size() + 1; ++i)
            for (size_t j = 0; j < m_pivots.size() + 1; ++j)
                m_ek[j][i] -= ek_y[i] * ek_x[j] * dk;

        /* Register new pivot */
        m_pivots.push_back(best_pivot);
    }

    real eval_f(real const &x, real const &y)
    {
        static std::vector<std::array<real, 3>> cache;

        for (auto const &v : cache)
            if (x == v[0] && y == v[1])
                return v[2];

        cache.push_back({x, y, f(x, y)});
        return cache.back()[2];
    }

    real eval_ek(real const &x, real const &y)
    {
        /* Evaluate e_k at x,y: first, the implicit f part */
        real ret = eval_f(x, y);

        /* Then, the f(x_i,y)*f(x,y_j) parts */
        for (size_t i = 0; i < m_pivots.size(); ++i)
            for (size_t j = 0; j < m_pivots.size(); ++j)
                if (m_ek[j][i])
                    ret += m_ek[j][i] * eval_f(m_pivots[i].x, y) * eval_f(x, m_pivots[j].y);

        return ret;
    }

    void dump_gnuplot()
    {
        std::cout << "f(x,y)=sin((1-x)/2*acos((1+y)/2))/sqrt(1-((y+1)/2)**2)\n";
        std::cout << "e0(x,y)=f(x,y)\n";
        //printf("f0(x,y)=0\n");

        std::cout << std::setprecision(20);
        for (int n = 0; n < (int)m_pivots.size(); ++n)
        {
            std::cout << 'x' << (n+1) << '=' << m_pivots[n].x << '\n';
            std::cout << 'y' << (n+1) << '=' << m_pivots[n].y << '\n';
            std::cout << 'd' << (n+1) << "=e" << n << "(x" << (n+1) << ",y" << (n+1) << ")\n";
            std::cout << 'e' << (n+1) << "(x,y)=e" << n << "(x,y)-e" << n
                      << "(x" << (n+1) << ",y)*e" << n << "(x,y" << (n+1)
                      << ")/d" << (n+1) << '\n';
            //printf("f%d(x,y)=f%d(x,y)+e%d(x%d,y)*e%d(x,y%d)/d%d\n", n+1, n, n, n+1, n, n+1, n+1);
        }

        std::cout << "splot [-1:1][-1:1] e" << m_pivots.size() << "(x,y)\n";
    }

private:
    real cheb(int i, int n)
    {
        return -cos(real::R_PI() * i / n) * real("0.999999999999999");
    }

    int m_grid_size;
    int m_iters;

    /*
     * Our “meta-function” structure. It has a matrix of coefficients that
     * contains the contribution of f(x_i,y)f(x,y_j) to this function. Since
     * this function is the error function, it has an implicit f(x,y) added
     * to it because of the algorithm’s setup.
     *
     * Using this kind of storage, we can add meta-functions together, and
     * evaluate them at a given point or along a given x or y line.
     */
    array2d<real> m_ek;

    std::vector<real> m_coeff;
    std::vector<real2> m_pivots;
};

int main()
{
    int const grid_size = 33;
    int const iters = 6;

    /* Create solver and iterate */
    solver s(grid_size, iters);
    for (int n = 0; n < iters; ++n)
        s.step();

    /* Dump gnuplot data about the solver */
    s.dump_gnuplot();

    return EXIT_SUCCESS;
}

