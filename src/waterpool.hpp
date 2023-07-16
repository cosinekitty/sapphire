/*
    waterpool.hpp  -  Don Cross  -  2023-07-12

    Simulation of waves moving on the surface of water.
    Based on ideas from the video
    "How to write a Height-Field Water Simulator with 100 lines of code"
    by Matthias Müller / Ten Minute Physics:
    https://www.youtube.com/watch?v=hswBi5wcqAA

    Uses SIMD instructions to calculate 4 quadrants simultaneously
    for most of the grid; only the edges of each grid quadrant
    need to be broken out into separate arithmetic operations
    to accomodate wrapping around the edges.
*/
#ifndef __COSINEKITTY_WATERPOOL_HPP
#define __COSINEKITTY_WATERPOOL_HPP

#include <memory>
#include <random>
#include <stdexcept>
#include "sapphire_simd.hpp"

namespace Sapphire
{
    struct CellState
    {
        float wet;
        float pos;
        float vel;
        float acc;
        float rnd;

        CellState(float _wet, float _pos, float _vel, float _acc, float _rnd)
            : wet(_wet)
            , pos(_pos)
            , vel(_vel)
            , acc(_acc)
            , rnd(_rnd)
            {}
    };

    class WaterCellSimd
    {
    public:
        // We represent 4 cells in the grid.
        // Each cell is in a corresponding location in each of the 4 quadrants.
        // For example, pos.s[0] is the position in quadrant 0,
        // pos.s[1] is the position in quadrant 1, etc.
        PhysicsVector wet {1.0f};
        PhysicsVector rnd;
        PhysicsVector pos;
        PhysicsVector vel;
        PhysicsVector acc;

        WaterCellSimd()
            {}

        explicit WaterCellSimd(float _rnd)
            : rnd(_rnd)
            {}
    };

    template <int WIDTH, int HEIGHT>
    class WaterPoolSimd
    {
    private:
        static_assert(WIDTH > 0 && WIDTH % 2 == 0, "Width must be a positive even integer.");
        static_assert(HEIGHT > 0 && HEIGHT % 2 == 0, "Height must be a positive even integer.");
        static const int QUADRANT_WIDTH  = WIDTH / 2;
        static const int QUADRANT_HEIGHT = HEIGHT / 2;
        static const int W = QUADRANT_WIDTH  - 1;
        static const int H = QUADRANT_HEIGHT - 1;

        struct buffer_t
        {
            WaterCellSimd array[QUADRANT_WIDTH][QUADRANT_HEIGHT];
        };

        struct coord_t
        {
            const int x;
            const int y;
            const int q;

            coord_t(int i, int j)
                : x(i % QUADRANT_WIDTH)
                , y(j % QUADRANT_HEIGHT)
                , q((i / QUADRANT_WIDTH) + 2*(j / QUADRANT_HEIGHT))
            {
                // Validate inputs
                if (i < 0 || i >= WIDTH)
                    throw std::range_error("bad i");

                if (j < 0 || j >= HEIGHT)
                    throw std::range_error("bad j");

                // Validate outputs
                if (q < 0 || q > 3)
                    throw std::range_error("bad q");
                if (x < 0 || x > W)
                    throw std::range_error("bad x");
                if (y < 0 || y > H)
                    throw std::range_error("bad y");

                // Verify we can recalculate the original coordinates.
                int vi = x + QUADRANT_WIDTH*(q & 1);
                if (vi != i)
                    throw std::range_error("cannot recalc i");

                int vj = y + QUADRANT_HEIGHT*((q >> 1) & 1);
                if (vj != j)
                    throw std::range_error("cannot recalc j");
            }
        };

        buffer_t *buffer = new buffer_t;

        inline float term(float hpos, int q, int x, int y) const
        {
            return buffer->array[x][y].wet[q] * (buffer->array[x][y].pos[q] - hpos);
        }

    public:
        ~WaterPoolSimd()
        {
            delete buffer;
        }

        void initialize()
        {
            std::mt19937 gen;
            std::normal_distribution<float> dist {0.0f, 0.5f};

            for (int i = 0; i < QUADRANT_WIDTH; ++i)
                for (int j = 0; j < QUADRANT_HEIGHT; ++j)
                    buffer->array[i][j] = WaterCellSimd{Clamp(dist(gen), -1.0f, +1.0f)};
        }

        CellState get(int i, int j) const
        {
            coord_t c{i, j};
            const WaterCellSimd& s = buffer->array[c.x][c.y];
            return CellState{s.wet[c.q], s.pos[c.q], s.vel[c.q], s.acc[c.q], s.rnd[c.q]};
        }

        void putWet(int i, int j, float wet)
        {
            coord_t c{i, j};
            buffer->array[c.x][c.y].wet[c.q] = wet;
        }

        void putPos(int i, int j, float pos)
        {
            coord_t c{i, j};
            buffer->array[c.x][c.y].pos[c.q] = pos;
        }

        void putVel(int i, int j, float vel)
        {
            coord_t c{i, j};
            buffer->array[c.x][c.y].vel[c.q] = vel;
        }

        void putRnd(int i, int j, float rnd)
        {
            coord_t c{i, j};
            buffer->array[c.x][c.y].rnd[c.q] = rnd;
        }

        void update(float dt, float halflife, float k, float r)
        {
            const float damp = std::pow(0.5f, dt/halflife);

            // First pass: calculate accelerations.

            for (int i = 1; i < W; ++i)
            {
                // The interior region of each quadrant benefits from SIMD optimizations
                // because all calculations are confined to the same quadrant.
                for (int j = 1; j < H; ++j)
                {
                    WaterCellSimd& h = buffer->array[i][j];
                    h.acc = k * h.wet * (
                        buffer->array[i][j-1].wet*(buffer->array[i][j-1].pos - h.pos) +
                        buffer->array[i][j+1].wet*(buffer->array[i][j+1].pos - h.pos) +
                        buffer->array[i-1][j].wet*(buffer->array[i-1][j].pos - h.pos) +
                        buffer->array[i+1][j].wet*(buffer->array[i+1][j].pos - h.pos)
                    );
                }

                // Calculate accelerations on the top and bottom edges of each quadrant,
                // with `j` increasing downward.
                {
                    // Top edges
                    WaterCellSimd& h = buffer->array[i][0];
                    for (int q = 0; q < 4; ++q)
                        h.acc[q] = k * h.wet[q] * (
                            term(h.pos[q], q^2, i  , H) +
                            term(h.pos[q], q  , i  , 1) +
                            term(h.pos[q], q  , i-1, 0) +
                            term(h.pos[q], q  , i+1, 0)
                        );
                }
                {
                    // Bottom edges
                    WaterCellSimd& h = buffer->array[i][H];
                    for (int q = 0; q < 4; ++q)
                        h.acc[q] = k * h.wet[q] * (
                            term(h.pos[q], q  , i  , H-1) +
                            term(h.pos[q], q^2, i  , 0  ) +
                            term(h.pos[q], q  , i-1, H  ) +
                            term(h.pos[q], q  , i+1, H  )
                        );
                }
            }

            // Calculate accelerations on the left and right edges of each quadrant.
            for (int j = 1; j < H; ++j)
            {
                {
                    // Left edges
                    WaterCellSimd& h = buffer->array[0][j];
                    for (int q = 0; q < 4; ++q)
                        h.acc[q] = k * h.wet[q] * (
                            term(h.pos[q], q  , 0, j-1) +
                            term(h.pos[q], q  , 0, j+1) +
                            term(h.pos[q], q^1, W, j  ) +
                            term(h.pos[q], q  , 1, j  )
                        );
                }
                {
                    // Right edges
                    WaterCellSimd& h = buffer->array[W][j];
                    for (int q = 0; q < 4; ++q)
                        h.acc[q] = k * h.wet[q] * (
                            term(h.pos[q], q  , W  , j-1) +
                            term(h.pos[q], q  , W  , j+1) +
                            term(h.pos[q], q  , W-1, j  ) +
                            term(h.pos[q], q^1, 0  , j  )
                        );
                }
            }

            // Calculate accelerations at each corner of each quadrant (16 total).

            {
                // Upper left corners
                WaterCellSimd& h = buffer->array[0][0];
                for (int q = 0; q < 4; ++q)
                    h.acc[q] = k * h.wet[q] * (
                        term(h.pos[q], q^2, 0, H) +
                        term(h.pos[q], q  , 0, 1) +
                        term(h.pos[q], q^1, W, 0) +
                        term(h.pos[q], q  , 1, 0)
                    );
            }

            {
                // Lower left corners
                WaterCellSimd& h = buffer->array[0][H];
                for (int q = 0; q < 4; ++q)
                    h.acc[q] = k * h.wet[q] * (
                        term(h.pos[q], q  , 0, H-1) +
                        term(h.pos[q], q^2, 0, 0  ) +
                        term(h.pos[q], q^1, W, H  ) +
                        term(h.pos[q], q  , 1, H  )
                    );
            }

            {
                // Upper right corners
                WaterCellSimd& h = buffer->array[W][0];
                for (int q = 0; q < 4; ++q)
                    h.acc[q] = k * h.wet[q] * (
                        term(h.pos[q], q^2, W  , H) +
                        term(h.pos[q], q  , W  , 1) +
                        term(h.pos[q], q  , W-1, 0) +
                        term(h.pos[q], q^1, 0  , 0)
                    );
            }

            {
                // Lower right corners
                WaterCellSimd& h = buffer->array[W][H];
                for (int q = 0; q < 4; ++q)
                    h.acc[q] = k * h.wet[q] * (
                        term(h.pos[q], q  , W  , H-1) +
                        term(h.pos[q], q^2, W  , 0  ) +
                        term(h.pos[q], q  , W-1, H  ) +
                        term(h.pos[q], q^1, 0  , H  )
                    );
            }

            // Second pass: use accelerations to update velocities and positions.
            // We can use SIMD for all cells because these calculations are all local to each cell.
            for (int i = 0; i <= W; ++i)
            {
                for (int j = 0; j <= H; ++j)
                {
                    WaterCellSimd& h = buffer->array[i][j];
                    h.vel = (damp * h.vel) + (dt * (1.0f + r*h.rnd) * h.acc);
                    h.pos += (dt * h.vel);
                }
            }
        }
    };
}

#endif // __COSINEKITTY_WATERPOOL_HPP
