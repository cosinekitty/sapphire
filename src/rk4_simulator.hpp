#pragma once
#include <vector>

namespace RungeKutta
{
    // Simulator is like Integrator, only designed for large or
    // variable-sized states. No return-by-value; instead, the slope
    // vectors k1..k4 are members, and they are updated in place as needed.
    // This makes Simulator less functional-oriented, but eliminates
    // constantly having to construct state_t objects.

    template <typename real_t, typename state_t, typename deriv_proc_t, typename add_func_t, typename mul_func_t>
    class Simulator
    {
    private:
        state_t w;                  // work register
        state_t k1, k2, k3, k4;     // slope vectors

    public:
        deriv_proc_t deriv;
        add_func_t add;
        mul_func_t mul;
        state_t state{};


        explicit Simulator(deriv_proc_t _deriv, add_func_t _add, mul_func_t _mul)
            : deriv(_deriv)
            , add(_add)
            , mul(_mul)
            {}


        void resize(std::size_t itemCount)
        {
            w.resize(itemCount);
            k1.resize(itemCount);
            k2.resize(itemCount);
            k3.resize(itemCount);
            k4.resize(itemCount);
            state.resize(itemCount);
        }


        void step(real_t dt)
        {
            deriv(k1, state);

            mul(w, k1, dt/2);
            add(w, w, state);
            deriv(k2, w);

            mul(w, k2, dt/2);
            add(w, w, state);
            deriv(k3, w);

            mul(w, k3, dt);
            add(w, w, state);
            deriv(k4, w);

            add(w, k2, k3);
            mul(w, w, 2);
            add(w, w, k1);
            add(w, w, k4);
            mul(w, w, dt/6);
            add(state, state, w);
        }
    };


    // Often we need a list of particle states as our system state.
    // If the number of particles is very large or unpredictable,
    // a std::vector can be useful.
    // With that in mind, here are some functor classes for connecting
    // Simulator with vector-based states.

    template <typename item_t>
    struct ListAdd
    {
        using list_t = std::vector<item_t>;

        void operator() (list_t& t, const list_t& a, const list_t& b)
        {
            std::size_t n = a.size();
            if (n == b.size() && n == t.size())
            {
                for (std::size_t i=0; i < n; ++i)
                    t[i] = a[i] + b[i];
            }
        }
    };


    template <typename item_t, typename real_t>
    struct ListMul
    {
        using list_t = std::vector<item_t>;

        void operator() (list_t& t, const list_t& a, real_t k)
        {
            std::size_t n = a.size();
            if (n == t.size())
            {
                for (std::size_t i=0; i < n; ++i)
                    t[i] = k * a[i];
            }
        }
    };


    template <typename real_t, typename item_t, typename deriv_proc_t>
    class ListSimulator
        : public Simulator<real_t, std::vector<item_t>, deriv_proc_t, ListAdd<item_t>, ListMul<item_t, real_t>>
    {
    public:
        using state_t = std::vector<item_t>;

        explicit ListSimulator(deriv_proc_t deriv, std::size_t itemCount)
            : Simulator<real_t, state_t, deriv_proc_t, ListAdd<item_t>, ListMul<item_t, real_t>>(
                deriv, ListAdd<item_t>(), ListMul<item_t, real_t>())
        {
            this->resize(itemCount);
        }
    };
}
