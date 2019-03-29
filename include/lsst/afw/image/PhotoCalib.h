/*
 * LSST Data Management System
 * Copyright 2017 LSST Corporation.
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
 * see <http://www.lsstcorp.org/LegalNotices/>.
 */

#ifndef LSST_AFW_IMAGE_PHOTOCALIB_H
#define LSST_AFW_IMAGE_PHOTOCALIB_H

/**
 * @file
 * @brief Implementation of the Photometric Calibration class.
 * @ingroup afw
 */

#include <cmath>  // For quiet_nan in the deprecated block

#include "boost/format.hpp"

#include "lsst/afw/math/BoundedField.h"
#include "lsst/afw/math/ChebyshevBoundedField.h"
#include "lsst/geom/Point.h"
#include "lsst/geom/Box.h"
#include "lsst/afw/table/Source.h"
#include "lsst/afw/table/io/Persistable.h"
#include "lsst/afw/image/MaskedImage.h"
#include "lsst/daf/base/PropertyList.h"
#include "lsst/pex/exceptions/Exception.h"
#include "lsst/utils/Magnitude.h"

namespace lsst {
namespace afw {
namespace image {

/// A value and its error.
struct Measurement {
    Measurement(double value, double error) : value(value), error(error) {}
    double const value;
    double const error;
};

/**
 * Raise lsst::pex::exceptions::InvalidParameterError if value is not >=0.
 *
 * Used for checking the calibration mean/error in the constructor.
 *
 * @param value Value that should be positive.
 * @param name Text to prepend to error message.
 *
 * @throws lsst::pex::exceptions::InvalidParameterError if value < 0
 */
inline void assertNonNegative(double value, std::string const &name) {
    if (value < 0) {
        throw LSST_EXCEPT(lsst::pex::exceptions::InvalidParameterError,
                          (boost::format("%s must be positive: %.3g") % name % value).str());
    }
}

/**
 * @class PhotoCalib
 *
 * @brief The photometric calibration of an exposure.
 *
 * A PhotoCalib is a BoundedField (a function with a specified domain) that converts from post-ISR
 * counts-on-chip (ADU) to flux and magnitude. It is defined such that a calibration of 1 means one count
 * is equal to one nanojansky (nJy, 10^-35 W/m^2/Hz in SI units). The nJy was chosen because it represents
 * a linear flux unit with values in a convenient range (e.g. LSST's single image depth of 24.5 is 575 nJy).
 * See more detailed discussion in: https://pstn-001.lsst.io/
 *
 * PhotoCalib is immutable.
 *
 * The spatially varying flux calibration has units of nJy/ADU, and is defined such that,
 * at a position (x,y) in the domain of the boundedField calibration and for a given measured source instFlux:
 * @f[
 *     instFlux*calibration(x,y) = flux [nJy]
 * @f]
 * while the errors (constant on the domain) are defined as:
 * @f[
 *     sqrt((instFluxErr/instFlux)^2 + (calibrationErr/calibration)^2)*flux = fluxErr [nJy]
 * @f]
 * This implies that the conversions from instFlux and instFlux error to magnitude and magnitude error
 * are as follows:
 * @f[
 *     -2.5*log_{10}(instFlux*calibration(x,y)*1e-9/referenceFlux) = magnitude
 * @f]
 *
 * where referenceFlux is the AB Magnitude reference flux from Oke & Gunn 1983 (first equation),
 * @f[
 *     referenceFlux = 1e23 * 10^(48.6/-2.5)
 * @f]
 * and
 * @f[
 *     2.5/log(10)*sqrt((instFluxErr/instFlux)^2 + (calibrationErr/calibration)^2) = magnitudeErr
 * @f]
 * Note that this is independent of referenceFlux.
 */
class PhotoCalib : public table::io::PersistableFacade<PhotoCalib>, public table::io::Persistable {
public:
    // no move or copy
    PhotoCalib(PhotoCalib const &) = delete;
    PhotoCalib(PhotoCalib &&) = delete;
    PhotoCalib &operator=(PhotoCalib const &) = delete;
    PhotoCalib &operator=(PhotoCalib &&) = delete;

    ~PhotoCalib() override = default;

    /**
     * Create a empty, zeroed calibration.
     */
    PhotoCalib() : PhotoCalib(0) {}

    /**
     * Create a non-spatially-varying calibration.
     *
     * @param[in]  calibrationMean The spatially-constant calibration (must be non-negative).
     * @param[in]  calibrationErr  The error on the calibration (must be non-negative).
     * @param[in]  bbox            The bounding box on which this PhotoCalib is valid. If not specified,
     *                             this PhotoCalib is valid at any point (i.e. an empty bbox).
     */
    explicit PhotoCalib(double calibrationMean, double calibrationErr = 0,
                        lsst::geom::Box2I const &bbox = lsst::geom::Box2I())
            : _calibrationMean(calibrationMean), _calibrationErr(calibrationErr), _isConstant(true) {
        assertNonNegative(_calibrationMean, "Calibration mean");
        assertNonNegative(_calibrationErr, "Calibration error");
        ndarray::Array<double, 2, 2> coeffs = ndarray::allocate(ndarray::makeVector(1, 1));
        coeffs[0][0] = calibrationMean;
        _calibration = std::make_shared<afw::math::ChebyshevBoundedField>(
                afw::math::ChebyshevBoundedField(bbox, coeffs));
    }

    /**
     * Create a spatially-varying calibration.
     *
     * @param[in]  calibration    The spatially varying photometric calibration (must have non-negative mean).
     * @param[in]  calibrationErr The error on the calibration (must be non-negative).
     */
    PhotoCalib(std::shared_ptr<afw::math::BoundedField> calibration, double calibrationErr = 0)
            : _calibration(calibration),
              _calibrationMean(computeCalibrationMean(calibration)),
              _calibrationErr(calibrationErr),
              _isConstant(false) {
        assertNonNegative(_calibrationMean, "Calibration (computed via BoundedField.mean()) mean");
        assertNonNegative(_calibrationErr, "Calibration error");
    }

    /**
     * Create a calibration with a pre-computed mean. Primarily for de-persistence.
     *
     * @param[in]  calibrationMean The mean of the calibration() over its bounding box (must be non-negative).
     * @param[in]  calibrationErr  The error on the calibration (must be non-negative).
     * @param[in]  calibration     The spatially varying photometric calibration.
     * @param[in]  isConstant      Is this PhotoCalib spatially constant?
     */
    PhotoCalib(double calibrationMean, double calibrationErr,
               std::shared_ptr<afw::math::BoundedField> calibration, bool isConstant)
            : _calibration(calibration),
              _calibrationMean(calibrationMean),
              _calibrationErr(calibrationErr),
              _isConstant(isConstant) {
        assertNonNegative(_calibrationMean, "Calibration mean");
        assertNonNegative(_calibrationErr, "Calibration error");
    }

    /**
     * Convert instFlux in ADU to nJy at a point in the BoundedField.
     *
     * If passed point, use the exact calculation at that point, otherwise, use the mean scaling factor.
     *
     * @param[in]  instFlux The source instFlux in ADU.
     * @param[in]  point    The point that instFlux is measured at.
     *
     * @returns    The flux in nJy.
     */
    double instFluxToNanojansky(double instFlux, lsst::geom::Point<double, 2> const &point) const;

    /// @overload instFluxToNanojansky(double, lsst::geom::Point<double, 2> const &) const;
    double instFluxToNanojansky(double instFlux) const;

    /**
     * Convert instFlux and error in instFlux (ADU) to nJy and nJy error.
     *
     * If passed point, use the exact calculation at that point, otherwise, use the mean scaling factor.
     *
     * @param[in]  instFlux     The source fluxinstFlux in ADU.
     * @param[in]  instFluxErr  The instFlux error.
     * @param[in]  point        The point that instFlux is measured at.
     *
     * @returns    The flux in nJy and error.
     */
    Measurement instFluxToNanojansky(double instFlux, double instFluxErr,
                                     lsst::geom::Point<double, 2> const &point) const;

    /// @overload Measurement instFluxToNanojansky(double, double, lsst::geom::Point<double, 2> const &) const
    Measurement instFluxToNanojansky(double instFlux, double instFluxErr) const;

    /**
     * Convert `sourceRecord[instFluxField_instFlux]` (ADU) at location
     * `(sourceRecord.get("x"), sourceRecord.get("y"))` (pixels) to flux and flux error (in nJy).
     *
     * @param[in]  sourceRecord  The source record to get instFlux and position from.
     * @param[in]  instFluxField The instFlux field: Keys of the form "*_instFlux" and "*_instFluxErr" must
     *                           exist. For example: instFluxField = "PsfFlux" -> "PsfFlux_instFlux",
     *                           "PsfFlux_instFluxErr"
     *
     * @returns    The flux in nJy and error for this source.
     */
    Measurement instFluxToNanojansky(const afw::table::SourceRecord &sourceRecord,
                                     std::string const &instFluxField) const;

    /**
     * Convert `sourceCatalog[instFluxField_instFlux]` (ADU) at locations
     * `(sourceCatalog.get("x"), sourceCatalog.get("y"))` (pixels) to nJy.
     *
     * @param[in]  sourceCatalog  The source catalog to get instFlux and position from.
     * @param[in]  instFluxField  The instFlux field: Keys of the form "*_instFlux" and "*_instFluxErr" must
     *                            exist. For example: instFluxField = "PsfFlux" -> "PsfFlux_instFlux",
     *                            "PsfFlux_instFluxErr"
     *
     * @returns    The flux in nJy and error for this source.
     */
    ndarray::Array<double, 2, 2> instFluxToNanojansky(afw::table::SourceCatalog const &sourceCatalog,
                                                      std::string const &instFluxField) const;

    /**
     * Convert `sourceCatalog[instFluxField_instFlux]` (ADU) at locations
     * `(sourceCatalog.get("x"), sourceCatalog.get("y"))` (pixels) to nJy
     * and write the results back to `sourceCatalog[outField_mag]`.
     *
     * @param[in]  sourceCatalog  The source catalog to get instFlux and position from.
     * @param[in]  instFluxField  The instFlux field: Keys of the form "*_instFlux" and "*_instFluxErr" must
     *                            exist. For example: instFluxField = "PsfFlux" -> "PsfFlux_instFlux",
     *                            "PsfFlux_instFluxErr"
     * @param[in]  outField       The field to write the nJy and magnitude errors to.
     *                            Keys of the form "*_instFlux" and "*_instFluxErr" must exist in the schema.
     *
     * @warning Not implemented yet: See DM-10155.
     */
    void instFluxToNanojansky(afw::table::SourceCatalog &sourceCatalog, std::string const &instFluxField,
                              std::string const &outField) const;

    /**
     * Convert instFlux in ADU to AB magnitude.
     *
     * If passed point, use the exact calculation at that point, otherwise, use the mean scaling factor.
     *
     * @param[in]  instFlux The source instFlux in ADU.
     * @param[in]  point    The point that instFlux is measured at.
     *
     * @returns    The AB magnitude.
     */
    double instFluxToMagnitude(double instFlux, lsst::geom::Point<double, 2> const &point) const;

    /// @overload instFluxToMagnitude(double, lsst::geom::Point<double, 2> const &) const;
    double instFluxToMagnitude(double instFlux) const;

    /**
     * Convert instFlux and error in instFlux (ADU) to AB magnitude and magnitude error.
     *
     * If passed point, use the exact calculation at that point, otherwise, use the mean scaling factor.
     *
     * @param[in]  instFlux     The source instFlux in ADU.
     * @param[in]  instFluxErr  The instFlux error (standard deviation).
     * @param[in]  point        The point that instFlux is measured at.
     *
     * @returns    The AB magnitude and error.
     */
    Measurement instFluxToMagnitude(double instFlux, double instFluxErr,
                                    lsst::geom::Point<double, 2> const &point) const;

    /// @overload instFluxToMagnitude(double, double, lsst::geom::Point<double, 2> const &) const;
    Measurement instFluxToMagnitude(double instFlux, double instFluxErr) const;

    /**
     * Convert `sourceRecord[instFluxField_instFlux]` (ADU) at location
     * `(sourceRecord.get("x"), sourceRecord.get("y"))` (pixels) to AB magnitude.
     *
     * @param[in]  sourceRecord  The source record to get instFlux and position from.
     * @param[in]  instFluxField The instFlux field: Keys of the form "*_instFlux" and "*_instFluxErr" must
     *                           exist. For example: instFluxField = "PsfFlux" -> "PsfFlux_instFlux",
     *                           "PsfFlux_instFluxErr"
     *
     * @returns    The magnitude and magnitude error for this source.
     */
    Measurement instFluxToMagnitude(afw::table::SourceRecord const &sourceRecord,
                                    std::string const &instFluxField) const;

    /**
     * Convert `sourceCatalog[instFluxField_instFlux]` (ADU) at locations
     * `(sourceCatalog.get("x"), sourceCatalog.get("y"))` (pixels) to AB magnitudes.
     *
     * @param[in]  sourceCatalog  The source catalog to get instFlux and position from.
     * @param[in]  instFluxField  The instFlux field: Keys of the form "*_instFlux" and "*_instFluxErr" must
     *                            exist. For example: instFluxField = "PsfFlux" -> "PsfFlux_instFlux",
     *                            "PsfFlux_instFluxErr"
     *
     * @returns    The magnitudes and magnitude errors for the sources.
     */
    ndarray::Array<double, 2, 2> instFluxToMagnitude(afw::table::SourceCatalog const &sourceCatalog,
                                                     std::string const &instFluxField) const;

    /**
     * Convert instFluxes in a catalog to AB magnitudes and write back into the catalog.
     *
     * Convert `sourceCatalog[instFluxField_instFlux]` (ADU) at
     * locations `(sourceCatalog.get("x"), sourceCatalog.get("y"))` (pixels) to AB magnitudes
     * and write the results back to `sourceCatalog[outField_mag]`.
     *
     * @param[in]  sourceCatalog  The source catalog to get instFlux and position from.
     * @param[in]  instFluxField  The instFlux field: Keys of the form "*_instFlux" and "*_instFluxErr" must
     *                            exist. For example: instFluxField = "PsfFlux" -> "PsfFlux_instFlux",
     *                            "PsfFlux_instFluxErr"
     * @param[in]  outField       The field to write the magnitudes and magnitude errors to.
     *                            Keys of the form "*_instFlux", "*_instFluxErr", *_mag", and "*_magErr"
     *                            must exist in the schema.
     *
     * @warning Not implemented yet: See DM-10155.
     */
    void instFluxToMagnitude(afw::table::SourceCatalog &sourceCatalog, std::string const &instFluxField,
                             std::string const &outField) const;

    /**
     * Return a flux calibrated image, with pixel values in nJy.
     *
     * Mask pixels are propogated directly from the input image.
     *
     * @param maskedImage The masked image to calibrate.
     * @param includeScaleUncertainty Include the uncertainty on the calibration in the resulting variance?
     *
     * @return The calibrated masked image.
     */
    MaskedImage<float> calibrateImage(MaskedImage<float> const &maskedImage,
                                      bool includeScaleUncertainty = true) const;

    /**
     * Convert AB magnitude to instFlux (ADU).
     *
     * If passed point, use the exact calculation at that point, otherwise, use the mean scaling factor.
     *
     * Useful for inserting fake sources into an image.
     *
     * @param[in]  magnitude  The AB magnitude to convert.
     * @param[in]  point      The position that magnitude is to be converted at.
     *
     * @returns    Source instFlux in ADU.
     */
    double magnitudeToInstFlux(double magnitude, lsst::geom::Point<double, 2> const &point) const;
    /// @overload magnitudeToInstFlux(double, lsst::geom::Point<double, 2> const &) const;
    double magnitudeToInstFlux(double magnitude) const;

    /**
     * Get the mean photometric calibration.
     *
     * This value is defined, for instFlux at (x,y), such that:
     * @f[
     *   instFlux*computeScaledCalibration()(x,y)*getCalibrationMean() = instFluxToNanojansky(instFlux, (x,y))
     * @f]
     *
     * @see PhotoCalib::computeScaledCalibration(), getCalibrationErr(), getInstFluxAtZeroMagnitude()
     *
     * @returns     The spatial mean of this calibration.
     */
    double getCalibrationMean() const { return _calibrationMean; }

    /**
     * Get the mean photometric calibration error.
     *
     * This value is defined such that for some instFluxErr, instFlux, and flux:
     * @f[
     *     sqrt((instFluxErr/instFlux)^2 + (calibrationErr/calibration(x,y))^2)*flux = fluxErr [nJy]
     * @f]
     *
     * @see PhotoCalib::computeScaledCalibration(), getCalibrationMean()
     *
     * @returns    The calibration error.
     */
    double getCalibrationErr() const { return _calibrationErr; }

    /**
     * Get the magnitude zero point (the instrumental flux corresponding to 0 magnitude).
     *
     * This value is defined such that:
     * @f[
     *   instFluxToMagnitude(getInstFluxAtZeroMagnitude()) == 0
     * @f]
     *
     * @see PhotoCalib::computeScaledCalibration(), getCalibrationMean()
     *
     * @returns     The instFlux magnitude zero point.
     */
    double getInstFluxAtZeroMagnitude() const { return utils::referenceFlux / _calibrationMean; }

    /**
     * Calculates the spatially-variable calibration, normalized by the mean in the valid domain.
     *
     * This value is defined, for instFlux at (x,y), such that:
     * @f[
     *   instFlux*computeScaledCalibration()(x,y)*getCalibrationMean() = instFluxToNanojansky(instFlux,
     * (x,y))
     * @f]
     *
     * @see PhotoCalib::getCalibrationMean()
     *
     * @returns    The normalized spatially-variable calibration.
     */
    std::shared_ptr<afw::math::BoundedField> computeScaledCalibration() const;

    /**
     * Calculates the scaling between this PhotoCalib and another PhotoCalib.
     *
     * The BoundedFields of these PhotoCalibs must have the same BBoxes (or one or both must be empty).
     *
     * With:
     *   - c = instFlux at position (x,y)
     *   - this = this PhotoCalib
     *   - other = other PhotoCalib
     *   - return = BoundedField returned by this method
     * the return value from this method is defined as:
     * @f[
     *   this.instFluxToNanojansky(c, (x,y))*return(x, y) = other.instFluxToNanojansky(c, (x,y))
     * @f]
     *
     * @param[in]  other  The PhotoCalib to scale to.
     *
     * @returns    The BoundedField as defined above.
     *
     * @warning Not implemented yet: See DM-10154.
     */
    std::shared_ptr<afw::math::BoundedField> computeScalingTo(std::shared_ptr<PhotoCalib> other) const;

    /// Two PhotoCalibs are equal if their component bounded fields and calibrationErr are equal.
    bool operator==(PhotoCalib const &rhs) const;

    /// @copydoc operator==
    bool operator!=(PhotoCalib const &rhs) const { return !(*this == rhs); }

    bool isPersistable() const noexcept override { return true; }

    friend std::ostream &operator<<(std::ostream &os, PhotoCalib const &photoCalib);

    /* Backwards compatibility with old Calib object */

    /// No-op: for backwards compatibility with Calib.
    [[deprecated("No-op: PhotoCalib never throws on negative instFlux. Will remove after v18.")]] static void
    setThrowOnNegativeFlux(bool raiseException) noexcept {
        ;  // do nothing!
    }
    /// No-op: for backwards compatibility with Calib (always returns false).
    [[deprecated("No-op: PhotoCalib never throws on negative instFlux. Will remove after v18.")]] static bool
    getThrowOnNegativeFlux() noexcept {
        return false;
    }

    /** @copydoc instFluxToMagnitude(double)const
     * Deprecated: For backwards compatibility with Calib.
     */
    [
            [deprecated("For backwards compatibility with Calib; use `instFluxToMagnitude` instead. To be "
                        "removed after v18.")]] double
    getMagnitude(double instFlux) const {
        return instFluxToMagnitude(instFlux);
    }

    /** @copydoc instFluxToMagnitude(double,double)const
     * Deprecated: For backwards compatibility with Calib.
     */
    [
            [deprecated("For backwards compatibility with Calib; use `instFluxToMagnitude` instead. To be "
                        "removed after v18.")]] ndarray::Array<double, 1>
    getMagnitude(ndarray::Array<double const, 1> const &instFlux) const;
    [
            [deprecated("For backwards compatibility with Calib; use `instFluxToMagnitude` instead. To be "
                        "removed after v18.")]] std::pair<double, double>
    getMagnitude(double instFlux, double instFluxErr) const {
        auto result = instFluxToMagnitude(instFlux, instFluxErr);
        return std::make_pair<const double &, const double &>(result.value, result.error);
    };
    [[deprecated(
            "For backwards compatibility with Calib; use `instFluxToMagnitude` instead. To be "
            "removed after v18.")]] std::pair<ndarray::Array<double, 1>, ndarray::Array<double, 1>>
    getMagnitude(ndarray::Array<double const, 1> const &instFlux,
                 ndarray::Array<double const, 1> const &instFluxErr) const;
    /**
     * @copydoc magnitudeToInstFlux(double)const
     * Deprecated: For backwards compatibility with Calib.
     */
    [[deprecated(
            "For backwards compatibility with Calib; use `magnitudeToInstFlux` instead. To be removed "
            "after v18.")]] double
    getFlux(double magnitude) const {
        return magnitudeToInstFlux(magnitude);
    }
    /**
     * @copydoc getInstFluxAtZeroMagnitude()
     * Deprecated: For backwards compatibility with Calib.
     */
    [[deprecated(
            "For backwards compatibility with Calib: use `getCalibrationMean`, `getCalibrationErr`, or "
            "`getInstFluxAtZeroMagnitude. To be removed after v18.")]] std::pair<double, double>
    getFluxMag0() const {
        return std::make_pair<double, double>(getInstFluxAtZeroMagnitude(),
                                              std::numeric_limits<double>::quiet_NaN());
    }
    /// Invalid for PhotoCalib: this only exists to provide the user an informative error message.
    [[deprecated(
            "PhotoCalib is immutable: create a new one with the calibration factor and calibration error,"
            " or create it like an old Calib object with makePhotoCalibFromCalibZeroPoint.")]] void
    setFluxMag0(double, double = 0) const {
        std::string msg =
                "PhotoCalib is immutable: create a new `PhotoCalib` with the calibration"
                " factor and error, or create it like an old Calib object with "
                "`makePhotoCalibFromCalibZeroPoint`.";
        throw LSST_EXCEPT(pex::exceptions::RuntimeError, msg);
    }

protected:
    std::string getPersistenceName() const override;

    void write(OutputArchiveHandle &handle) const override;

private:
    std::shared_ptr<afw::math::BoundedField> _calibration;

    // The "mean" calibration, defined as the geometric mean of _calibration evaluated over _calibration's
    // bbox. Computed on instantiation as a convinience. Also, the actual calibration for a spatially-constant
    // calibration.
    double _calibrationMean;

    // The standard deviation of this PhotoCalib.
    double _calibrationErr;

    // Is this spatially-constant? Used to short-circuit getting centroids.
    bool _isConstant;

    /**
     * Return the calibration evaluated at a point.
     *
     * Helper function to manage constant vs. non-constant PhotoCalibs
     */
    double evaluate(lsst::geom::Point<double, 2> const &point) const;

    /// Returns the spatially-constant calibration (for setting _calibrationMean)
    double computeCalibrationMean(std::shared_ptr<afw::math::BoundedField> calibration) const;

    /// Helpers for converting arrays of instFlux
    void instFluxToNanojanskyArray(afw::table::SourceCatalog const &sourceCatalog,
                                   std::string const &instFluxField,
                                   ndarray::Array<double, 2, 2> result) const;
    void instFluxToMagnitudeArray(afw::table::SourceCatalog const &sourceCatalog,
                                  std::string const &instFluxField,
                                  ndarray::Array<double, 2, 2> result) const;
};

/**
 * Construct a PhotoCalib from FITS FLUXMAG0/FLUXMAG0ERR keywords.
 *
 * This provides backwards compatibility with the obsoleted Calib object that PhotoCalib replaced.
 * It should not be used outside of reading old Exposures written before PhotoCalib existed.
 *
 * @param metadata FITS header metadata containing FLUXMAG0 and FLUXMAG0ERR keys.
 * @param strip Strip FLUXMAG0 and FLUXMAG0ERR from `metadata`?
 *
 * @returns Pointer to the constructed PhotoCalib, or nullptr if FLUXMAG0 is not in the metadata.
 */
std::shared_ptr<PhotoCalib> makePhotoCalibFromMetadata(daf::base::PropertySet &metadata, bool strip = false);

/**
 * Construct a PhotoCalib from the deprecated `Calib`-style instFluxMag0/instFluxMag0Err values.
 *
 * This provides backwards compatibility with the obsoleted Calib object that PhotoCalib replaced.
 * It should not be used outside of tests that compare with old persisted Calib objects.
 *
 * @param instFluxMag0 The instrumental flux at zero magnitude. If 0, the resulting `PhotoCalib` will have
 *                     infinite calibrationMean and non-finite (inf or NaN) calibrationErr.
 * @param instFluxMag0Err The instrumental flux at zero magnitude error. If 0, the resulting `PhotoCalib` will
 *                        have 0 calibrationErr.
 *
 * @returns Pointer to the constructed PhotoCalib.
 */
std::shared_ptr<PhotoCalib> makePhotoCalibFromCalibZeroPoint(double instFluxMag0, double instFluxMag0Err);

}  // namespace image
}  // namespace afw
}  // namespace lsst

#endif  // LSST_AFW_IMAGE_PHOTOCALIB_H
