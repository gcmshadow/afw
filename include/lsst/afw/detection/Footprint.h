/* 
 * LSST Data Management System
 * Copyright 2008, 2009, 2010 LSST Corporation.
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
 
#if !defined(LSST_DETECTION_FOOTPRINT_H)
#define LSST_DETECTION_FOOTPRINT_H
/**
 * \file
 * \brief Represent a set of pixels of an arbitrary shape and size
 *
 * Footprint is fundamental in astronomical image processing, as it defines what
 * is meant by a Source.
 */
#include <algorithm>
#include <list>
#include <set>
#include <cmath>
#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>
#include "ndarray.h"
#include "lsst/base.h"
#include "lsst/pex/policy/Policy.h"
#include "lsst/afw/image/MaskedImage.h"
#include "lsst/afw/image/Wcs.h"
#include "lsst/afw/detection/Peak.h"
#include "lsst/afw/geom.h"
#include "lsst/afw/geom/ellipses.h"

namespace boost{
namespace serialization{
    class access;
}}

#ifndef SWIG
using boost::serialization::make_nvp;
#endif

namespace lsst {
namespace afw { 
namespace detection {

/*!
 * \brief A range of pixels within one row of an Image
 */
class Span {
public:
    typedef boost::shared_ptr<Span> Ptr;
    typedef boost::shared_ptr<const Span> ConstPtr;

    Span(int y,                         //!< Row that Span's in
         int x0,                        //!< Starting column (inclusive)
         int x1)                        //!< Ending column (inclusive)
        : _y(y), _x0(x0), _x1(x1) {}    
    ~Span() {}

    int getX0() const { return _x0; }         ///< Return the starting x-value
    int getX1() const { return _x1; }         ///< Return the ending x-value
    int getY()  const { return _y; }          ///< Return the y-value
    int getWidth() const { return _x1 - _x0 + 1; } ///< Return the number of pixels

	bool contains(int x) { return (x >= _x0) && (x <= _x1); }
	bool contains(int x, int y) { return (x >= _x0) && (x <= _x1) && (y == _y); }

    std::string toString() const;    

    void shift(int dx, int dy) { _x0 += dx; _x1 += dx; _y += dy; }

	/* Required to make Span "LessThanComparable" so they can be used
	 * in sorting, binary search, etc.
	 * http://www.sgi.com/tech/stl/LessThanComparable.html
	 */
	bool operator<(const Span& b) const;
	
    friend class Footprint;
private:
    Span() {}

    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive & ar, const unsigned int version) {
        ar & make_nvp("y", _y) & make_nvp("x0", _x0) & make_nvp("x1", _x1);
    }
    int _y;                             //!< Row that Span's in
    int _x0;                            //!< Starting column (inclusive)
    int _x1;                            //!< Ending column (inclusive)
};



/************************************************************************************************************/
/*!
 * \brief A set of pixels in an Image
 *
 * A Footprint is a set of pixels, usually but not necessarily contiguous.
 * There are constructors to find Footprints above some threshold in an Image
 * (see FootprintSet), or to create Footprints in the shape of various
 * geometrical figures
 */
class Footprint : public lsst::daf::base::Citizen {
public:
    typedef boost::shared_ptr<Footprint> Ptr;
    typedef boost::shared_ptr<const Footprint> ConstPtr;

    /// The Footprint's Span list
    typedef std::vector<Span::Ptr> SpanList;
    typedef std::vector<Peak::Ptr> PeakList;

    explicit Footprint(int nspan = 0, geom::Box2I const & region=geom::Box2I());
    explicit Footprint(geom::Box2I const & bbox, geom::Box2I const & region=geom::Box2I());
    Footprint(geom::Point2I const & center, double const radius, geom::Box2I const & = geom::Box2I());
    explicit Footprint(geom::ellipses::Ellipse const & ellipse, geom::Box2I const & region=geom::Box2I());

    explicit Footprint(SpanList const & spans, geom::Box2I const & region=geom::Box2I());
    Footprint(Footprint const & other);    
    virtual ~Footprint();

    int getId() const { return _fid; }   //!< Return the Footprint's unique ID
    SpanList& getSpans() { return _spans; } //!< return the Span%s contained in this Footprint
    const SpanList& getSpans() const { return _spans; } //!< return the Span%s contained in this Footprint
    PeakList & getPeaks() { return _peaks; } //!< Return the Peak%s contained in this Footprint
    const PeakList & getPeaks() const { return _peaks; } //!< Return the Peak%s contained in this Footprint
    int getNpix() const { return _area; }     //!< Return the number of pixels in this Footprint
    int getArea() const { return _area; }

    const Span& addSpan(const int y, const int x0, const int x1);
    const Span& addSpan(Span const& span);
    const Span& addSpan(Span const& span, int dx, int dy);

    void shift(int dx, int dy);
    void shift(geom::ExtentI d) {shift(d.getX(), d.getY());}

    /// Return the Footprint's bounding box
    geom::Box2I getBBox() const { return _bbox; }     
    /// Return the corners of the MaskedImage the footprints live in
    geom::Box2I const & getRegion() const { return _region; }

    /// Set the corners of the MaskedImage wherein the footprints dwell
    void setRegion(geom::Box2I const & region) { _region = region; }

    void clipTo(geom::Box2I const & bbox);

    bool contains(geom::Point2I const& pix) const;
    
    void normalize();
    bool isNormalized() const {return _normalized;}

    template<typename PixelT>
    void insertIntoImage(typename lsst::afw::image::Image<PixelT>& idImage, 
                         boost::uint64_t const id,
                         geom::Box2I const& region=geom::Box2I()
    ) const;
    template<typename PixelT>
    void insertIntoImage(typename lsst::afw::image::Image<PixelT>& idImage, 
                         boost::uint64_t const id,
                         bool const overwriteId, long const idMask,
                         typename std::set<boost::uint64_t> *oldIds,
                         geom::Box2I const& region=geom::Box2I()
    ) const;

    Footprint & operator=(Footprint & other);

    template <typename MaskPixelT>
    void intersectMask(
        image::Mask<MaskPixelT> const & mask, 
        MaskPixelT bitmask=~0x0
    );


    /// Transform a footprint from one frame to another via their WCSes
    Footprint::Ptr transform(image::Wcs const& source,
                             image::Wcs const& target,
                             geom::Box2I const& bbox) const;

private:
    friend class boost::serialization::access;
    template <typename Archive>
    void serialize(Archive & ar, const unsigned int version);

    static int id;
    mutable int _fid;                    //!< unique ID
    int _area;                           //!< number of pixels in this Footprint
     
    SpanList _spans;                     //!< the Spans contained in this Footprint
    geom::Box2I _bbox;                   //!< the Footprint's bounding box
    PeakList _peaks;                     //!< the Peaks lying in this footprint
    mutable geom::Box2I _region;         //!< The corners of the MaskedImage the footprints live in
    bool _normalized;                    //!< Are the spans sorted? 
};

Footprint::Ptr growFootprint(Footprint const& foot, int ngrow, bool isotropic=true);
Footprint::Ptr growFootprint(Footprint::Ptr const& foot, int ngrow, bool isotropic=true);

std::vector<lsst::afw::geom::Box2I> footprintToBBoxList(Footprint const& foot);

template<typename ImageT>
typename ImageT::Pixel setImageFromFootprint(ImageT *image,
                                             Footprint const& footprint,
                                             typename ImageT::Pixel const value);
template<typename ImageT>
typename ImageT::Pixel setImageFromFootprintList(ImageT *image,
                                                 CONST_PTR(std::vector<Footprint::Ptr>) footprints,
                                                 typename ImageT::Pixel  const value);
template<typename ImageT>
typename ImageT::Pixel setImageFromFootprintList(ImageT *image,
                                                 std::vector<Footprint::Ptr> const& footprints,
                                                 typename ImageT::Pixel  const value);
template<typename MaskT>
MaskT setMaskFromFootprint(lsst::afw::image::Mask<MaskT> *mask,
                           Footprint const& footprint,
                           MaskT const bitmask);
template<typename MaskT>
MaskT clearMaskFromFootprint(lsst::afw::image::Mask<MaskT> *mask,
                             Footprint const& footprint,
                             MaskT const bitmask);

/************************************************************************************************************/
/**
 * \brief OR bitmask into all the Mask's pixels which are in the set of Footprint%s
 *
 * \return bitmask
 */
template<typename MaskT>
MaskT setMaskFromFootprintList(lsst::afw::image::Mask<MaskT> *mask,
                               std::vector<Footprint::Ptr> const& footprints,
                               MaskT const bitmask);
template<typename MaskT>
MaskT setMaskFromFootprintList(lsst::afw::image::Mask<MaskT> *mask,
                               CONST_PTR(std::vector<Footprint::Ptr>) const & footprints,
                               MaskT const bitmask);
template<typename MaskT>
Footprint::Ptr footprintAndMask(Footprint::Ptr const&  foot,
                                typename image::Mask<MaskT>::Ptr const&  mask,
                                MaskT const bitmask);

/************************************************************************************************************/

class HeavyFootprintCtrl;

/*!
 * \brief A set of pixels in an Image, including those pixels' actual values
 */
template <typename ImagePixelT, typename MaskPixelT=lsst::afw::image::MaskPixel,
          typename VariancePixelT=lsst::afw::image::VariancePixel>
class HeavyFootprint : public Footprint {
public:
    explicit HeavyFootprint(
        Footprint const& foot,
        lsst::afw::image::MaskedImage<ImagePixelT, MaskPixelT, VariancePixelT> const& mimage,
        HeavyFootprintCtrl const* ctrl=NULL
                           );

    void insert(lsst::afw::image::MaskedImage<ImagePixelT, MaskPixelT, VariancePixelT> & mimage) const;
    void insert(lsst::afw::image::Image<ImagePixelT> & image) const;

private:
    ndarray::Array<ImagePixelT, 1, 1> _image;
    ndarray::Array<MaskPixelT, 1, 1> _mask;
    ndarray::Array<VariancePixelT, 1, 1> _variance;
};

template <typename ImagePixelT, typename MaskPixelT, typename VariancePixelT>
HeavyFootprint<ImagePixelT, MaskPixelT, VariancePixelT> makeHeavyFootprint(
    Footprint const& foot,
    lsst::afw::image::MaskedImage<ImagePixelT, MaskPixelT, VariancePixelT> const& img,
    HeavyFootprintCtrl const* ctrl=NULL
                                                                          )    
{
    return HeavyFootprint<ImagePixelT, MaskPixelT, VariancePixelT>(foot, img, ctrl);
}    

}}}

#endif
