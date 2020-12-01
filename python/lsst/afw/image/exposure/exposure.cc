/*
 * LSST Data Management System
 * Copyright 2008-2017 AURA/LSST.
 *
 * This product includes software developed by the
 * LSST Project (http://www.lsst.org/).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the LSST License Statement and
 * the GNU General Public License along with this program.  If not,
 * see <https://www.lsstcorp.org/LegalNotices/>.
 */

#include "pybind11/pybind11.h"

#include "lsst/afw/cameraGeom/Detector.h"
#include "lsst/afw/geom/SkyWcs.h"
#include "lsst/afw/image/PhotoCalib.h"
#include "lsst/afw/image/Filter.h"
#include "lsst/afw/image/FilterLabel.h"
#include "lsst/afw/image/Exposure.h"
#include "lsst/afw/detection/Psf.h"

namespace py = pybind11;
using namespace pybind11::literals;

namespace lsst {
namespace afw {
namespace image {
namespace {

template <typename PixelT>
using PyExposure = py::class_<Exposure<PixelT>, std::shared_ptr<Exposure<PixelT>>>;

/*
Declare a constructor that takes an Exposure of FromPixelT and returns an Exposure cast to ToPixelT

The mask and variance must be of the standard types.

@param[in] cls  The pybind11 class to which add the constructor
*/
template <typename FromPixelT, typename ToPixelT>
void declareCastConstructor(PyExposure<ToPixelT> &cls) {
    cls.def(py::init<Exposure<FromPixelT> const &, bool const>(), "src"_a, "deep"_a);
}

template <typename PixelT>  // only the image type varies; mask and variance are fixed to default tparams
PyExposure<PixelT> declareExposure(py::module &mod, const std::string &suffix) {
    using ExposureT = Exposure<PixelT>;
    using MaskedImageT = typename ExposureT::MaskedImageT;

    PyExposure<PixelT> cls(mod, ("Exposure" + suffix).c_str());

    mod.def("makeExposure", &makeExposure<PixelT, MaskPixel, VariancePixel>, "maskedImage"_a,
            "wcs"_a = std::shared_ptr<geom::SkyWcs const>());

    /* Constructors */
    cls.def(py::init<unsigned int, unsigned int, std::shared_ptr<geom::SkyWcs const>>(), "width"_a,
            "height"_a, "wcs"_a = std::shared_ptr<geom::SkyWcs const>());
    cls.def(py::init<lsst::geom::Extent2I const &, std::shared_ptr<geom::SkyWcs const>>(),
            "dimensions"_a = lsst::geom::Extent2I(), "wcs"_a = std::shared_ptr<geom::SkyWcs const>());
    cls.def(py::init<lsst::geom::Box2I const &, std::shared_ptr<geom::SkyWcs const>>(), "bbox"_a,
            "wcs"_a = std::shared_ptr<geom::SkyWcs const>());
    cls.def(py::init<MaskedImageT &, std::shared_ptr<geom::SkyWcs const>>(), "maskedImage"_a,
            "wcs"_a = std::shared_ptr<geom::SkyWcs const>());
    cls.def(py::init<MaskedImageT &, std::shared_ptr<ExposureInfo>>(), "maskedImage"_a, "exposureInfo"_a);
    cls.def(py::init<std::string const &, lsst::geom::Box2I const &, ImageOrigin, bool, bool>(), "fileName"_a,
            "bbox"_a = lsst::geom::Box2I(), "origin"_a = PARENT, "conformMasks"_a = false,
            "allowUnsafe"_a = false);
    cls.def(py::init<fits::MemFileManager &, lsst::geom::Box2I const &, ImageOrigin, bool, bool>(),
            "manager"_a, "bbox"_a = lsst::geom::Box2I(), "origin"_a = PARENT, "conformMasks"_a = false,
            "allowUnsafe"_a = false);
    cls.def(py::init<ExposureT const &, bool>(), "other"_a, "deep"_a = false);
    cls.def(py::init<ExposureT const &, lsst::geom::Box2I const &, ImageOrigin, bool>(), "other"_a, "bbox"_a,
            "origin"_a = PARENT, "deep"_a = false);

    /* Members */
    cls.def("getMaskedImage", (MaskedImageT(ExposureT::*)()) & ExposureT::getMaskedImage);
    cls.def("setMaskedImage", &ExposureT::setMaskedImage, "maskedImage"_a);
    cls.def_property("maskedImage", (MaskedImageT(ExposureT::*)()) & ExposureT::getMaskedImage,
                     &ExposureT::setMaskedImage);
    cls.def("getMetadata", &ExposureT::getMetadata);
    cls.def("setMetadata", &ExposureT::setMetadata, "metadata"_a);
    cls.def("getWidth", &ExposureT::getWidth);
    cls.def("getHeight", &ExposureT::getHeight);
    cls.def("getDimensions", &ExposureT::getDimensions);
    cls.def("getX0", &ExposureT::getX0);
    cls.def("getY0", &ExposureT::getY0);
    cls.def("getXY0", &ExposureT::getXY0);
    cls.def("setXY0", &ExposureT::setXY0, "xy0"_a);
    cls.def("getBBox", &ExposureT::getBBox, "origin"_a = PARENT);
    cls.def("getWcs", (std::shared_ptr<geom::SkyWcs>(ExposureT::*)()) & ExposureT::getWcs);
    cls.def("setWcs", &ExposureT::setWcs, "wcs"_a);
    cls.def("hasWcs", &ExposureT::hasWcs);
    cls.def("getDetector", &ExposureT::getDetector);
    cls.def("setDetector", &ExposureT::setDetector, "detector"_a);
    cls.def("getFilter", &ExposureT::getFilter);
    cls.def("setFilter", &ExposureT::setFilter, "filter"_a);
    cls.def("getFilterLabel", &ExposureT::getFilterLabel);
    cls.def("setFilterLabel", &ExposureT::setFilterLabel, "filterLabel"_a);

    cls.def("getPhotoCalib", &ExposureT::getPhotoCalib);
    cls.def("setPhotoCalib", &ExposureT::setPhotoCalib, "photoCalib"_a);
    cls.def("getPsf", (std::shared_ptr<detection::Psf>(ExposureT::*)()) & ExposureT::getPsf);
    cls.def("setPsf", &ExposureT::setPsf, "psf"_a);
    cls.def("hasPsf", &ExposureT::hasPsf);
    cls.def("getInfo", (std::shared_ptr<ExposureInfo>(ExposureT::*)()) & ExposureT::getInfo);
    cls.def("setInfo", &ExposureT::setInfo, "exposureInfo"_a);

    cls.def("subset", &ExposureT::subset, "bbox"_a, "origin"_a = PARENT);

    cls.def("writeFits", (void (ExposureT::*)(std::string const &) const) & ExposureT::writeFits);
    cls.def("writeFits", (void (ExposureT::*)(fits::MemFileManager &) const) & ExposureT::writeFits);
    cls.def("writeFits", [](ExposureT &self, fits::Fits &fits) { self.writeFits(fits); });

    cls.def("writeFits",
            [](ExposureT &self, std::string const &filename, fits::ImageWriteOptions const &imageOptions,
               fits::ImageWriteOptions const &maskOptions, fits::ImageWriteOptions const &varianceOptions) {
                self.writeFits(filename, imageOptions, maskOptions, varianceOptions);
            },
            "filename"_a, "imageOptions"_a, "maskOptions"_a, "varianceOptions"_a);
    cls.def("writeFits",
            [](ExposureT &self, fits::MemFileManager &manager, fits::ImageWriteOptions const &imageOptions,
               fits::ImageWriteOptions const &maskOptions, fits::ImageWriteOptions const &varianceOptions) {
                self.writeFits(manager, imageOptions, maskOptions, varianceOptions);
            },
            "manager"_a, "imageOptions"_a, "maskOptions"_a, "varianceOptions"_a);
    cls.def("writeFits",
            [](ExposureT &self, fits::Fits &fits, fits::ImageWriteOptions const &imageOptions,
               fits::ImageWriteOptions const &maskOptions, fits::ImageWriteOptions const &varianceOptions) {
                self.writeFits(fits, imageOptions, maskOptions, varianceOptions);
            },
            "fits"_a, "imageOptions"_a, "maskOptions"_a, "varianceOptions"_a);

    cls.def_static("readFits", (ExposureT(*)(std::string const &))ExposureT::readFits);
    cls.def_static("readFits", (ExposureT(*)(fits::MemFileManager &))ExposureT::readFits);

    cls.def("getCutout", &ExposureT::getCutout, "center"_a, "size"_a);

    return cls;
}

PYBIND11_MODULE(exposure, mod) {
    py::module::import("lsst.afw.image.exposureInfo");
    py::module::import("lsst.afw.image.maskedImage");

    auto clsExposureF = declareExposure<float>(mod, "F");
    auto clsExposureD = declareExposure<double>(mod, "D");
    declareExposure<int>(mod, "I");
    declareExposure<std::uint16_t>(mod, "U");
    declareExposure<std::uint64_t>(mod, "L");

    // Declare constructors for casting all exposure types to to float and double
    // (the only two types of casts that Python supports)
    declareCastConstructor<int, float>(clsExposureF);
    declareCastConstructor<int, double>(clsExposureD);

    declareCastConstructor<float, double>(clsExposureD);

    declareCastConstructor<double, float>(clsExposureF);

    declareCastConstructor<std::uint16_t, float>(clsExposureF);
    declareCastConstructor<std::uint16_t, double>(clsExposureD);

    declareCastConstructor<std::uint64_t, float>(clsExposureF);
    declareCastConstructor<std::uint64_t, double>(clsExposureD);
}
}  // namespace
}  // namespace image
}  // namespace afw
}  // namespace lsst
