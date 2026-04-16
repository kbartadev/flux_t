#pragma once

// MIT License
// Copyright (c) 2026 Kristof Barta
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <atomic>
#include <vector>
#include "flux_substrate.hpp"
#include "flux_wave.hpp"

namespace flux_t
{

    template<typename wave_type>
    class lens_t
    {
    public:
        explicit lens_t(substrate_t& parent)
            : parent_(parent),
            accepting_(true)
        {
        }

        lens_t(const lens_t&) = delete;
        lens_t& operator=(const lens_t&) = delete;

        substrate_t& parent() noexcept { return parent_; }

        template<typename prism_type>
        void bind(prism_type* prism, vein_t* vein)
        {
            bindings_.push_back({ prism, vein });
        }

        void emit(const wave_type& w) const
        {
            if (!accepting_.load(std::memory_order_acquire))
            {
                return;
            }

            for (auto& b : bindings_)
            {
                b.vein->submit([p = b.prism, &w]() {
                    p->refract(w);
                    });
            }
        }

        void set_accepting(bool v) noexcept
        {
            accepting_.store(v, std::memory_order_release);
        }

    private:
        struct binding_t
        {
            void* prism;
            vein_t* vein;
        };

        substrate_t& parent_;
        std::vector<binding_t> bindings_;
        mutable std::atomic<bool> accepting_;
    };

} // namespace flux_t
