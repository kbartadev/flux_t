#pragma once

// MIT License
//
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

#ifndef FLUX_T_PULSE_T_HPP
#define FLUX_T_PULSE_T_HPP

namespace flux_t
{

/**
 * @file pulse_t.hpp
 * @brief Pulse marker and concept documentation.
 *
 * Pulses are immutable signals that flow through the flux_t system.
 * Users typically define concrete payload types and use them as pulses.
 */

/**
 * @brief Marker type representing the pulse concept.
 *
 * This type is intentionally empty. It documents the concept and can be
 * used in templates or static assertions to indicate pulse semantics.
 */
struct pulse_t
{
    // Intentionally empty: user payload types carry data.
};

} // namespace flux_t

#endif // FLUX_T_PULSE_T_HPP
