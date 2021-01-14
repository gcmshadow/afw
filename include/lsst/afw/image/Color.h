// -*- lsst-c++ -*-
/*
 * Capture the colour of an object
 */

#ifndef LSST_AFW_IMAGE_COLOR_H
#define LSST_AFW_IMAGE_COLOR_H

#include <cmath>
#include <limits>
#include "lsst/afw/image/Filter.h"

namespace lsst {
namespace afw {
namespace image {

/**
 * Describe the colour of a source
 *
 * We need a concept of colour more general than "g - r" in order to calculate e.g. atmospheric dispersion
 * or a source's PSF
 *
 * @note This is very much just a place holder until we work out what we need.  A full SED may be required,
 * in which case a constructor from an SED name might be appropriate, or a couple of colours, or ...
 */
class Color final {
public:
    explicit Color(double g_r = std::numeric_limits<double>::quiet_NaN()) : _g_r(g_r) {}

    Color(Color const &) = default;
    Color(Color &&) = default;
    Color &operator=(Color const &) = default;
    Color &operator=(Color &&) = default;
    ~Color() noexcept = default;

    /// Whether the color is the special value that indicates that it is unspecified.
    bool isIndeterminate() const noexcept { return std::isnan(_g_r); }

    //@{
    /**
     *  Equality comparison for colors
     *
     *  Just a placeholder like everything else, but we explicitly let indeterminate colors compare
     *  as equal.
     *
     *  In the future, we'll probably want some way of doing fuzzy comparisons on colors, but then
     *  we'd have to define some kind of "color difference" matric, and it's not worthwhile doing
     *  that yet.
     */
    bool operator==(Color const &other) const noexcept {
        return (isIndeterminate() && other.isIndeterminate()) || other._g_r == _g_r;
    }
    bool operator!=(Color const &other) const noexcept { return !operator==(other); }
    //@}

    /// Return a hash of this object.
    std::size_t hash_value() const noexcept { return isIndeterminate() ? 42 : std::hash<double>()(_g_r); }

    /** Return the effective wavelength for this object in the given filter
     */
    [
            [deprecated("Removed with no replacement (but see lsst::afw::image::TransmissionCurve). Will be "
                        "removed after v23.")]] double
    getLambdaEff(Filter const &  ///< The filter in question
                 ) const {
        return 1000 * _g_r;
    }

private:
    double _g_r;
};
}  // namespace image
}  // namespace afw
}  // namespace lsst

namespace std {
template <>
struct hash<lsst::afw::image::Color> {
    using argument_type = lsst::afw::image::Color;
    using result_type = size_t;
    size_t operator()(argument_type const &obj) const noexcept { return obj.hash_value(); }
};
}  // namespace std

#endif
