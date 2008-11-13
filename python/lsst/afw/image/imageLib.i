// -*- lsst-c++ -*-
%define imageLib_DOCSTRING
"
Basic routines to talk to lsst::afw::image classes
"
%enddef

%feature("autodoc", "1");
%module(package="lsst.afw.image", docstring=imageLib_DOCSTRING) imageLib

// Suppress swig complaints
#pragma SWIG nowarn=314                 // print is a python keyword (--> _print)
#pragma SWIG nowarn=362                 // operator=  ignored

%{
#include <lsst/daf/base.h>
#include <lsst/daf/data.h>
#include <lsst/daf/persistence.h>
#include <lsst/pex/exceptions.h>
#include <lsst/pex/logging/Trace.h>
#include <lsst/pex/policy/Policy.h>
#include <lsst/pex/policy/PolicyFile.h>
#include <lsst/afw/image.h>
%}

%inline %{
namespace boost {
    typedef unsigned short uint16_t;
    namespace mpl { }
}
%}

/************************************************************************************************************/

%include "lsst/p_lsstSwig.i"
%include "lsst/daf/base/persistenceMacros.i"

%pythoncode %{
import lsst.utils

def version(HeadURL = r"$HeadURL$"):
    """Return a version given a HeadURL string. If a different version is setup, return that too"""

    version_svn = lsst.utils.guessSvnVersion(HeadURL)

    try:
        import eups
    except ImportError:
        return version_svn
    else:
        try:
            version_eups = eups.setup("afw")
        except AttributeError:
            return version_svn

    if version_eups == version_svn:
        return version_svn
    else:
        return "%s (setup: %s)" % (version_svn, version_eups)

%}

/******************************************************************************/

%import "lsst/daf/base/baseLib.i"
%import "lsst/pex/policy/policyLib.i"
%import "lsst/daf/persistence/persistenceLib.i"
%import "lsst/daf/data/dataLib.i"

%lsst_exceptions();

/******************************************************************************/

%template(pairIntInt)   std::pair<int, int>;
%template(mapStringInt) std::map<std::string, int>;

/************************************************************************************************************/
// Images, Masks, and MaskedImages
%include "lsst/afw/image/LsstImageTypes.h"

%ignore lsst::afw::image::Filter::operator int;
%include "lsst/afw/image/Filter.h"

%include "image.i"
%include "mask.i"
%include "maskedImage.i"

%template(PointD) lsst::afw::image::Point<double>;
%template(PointI) lsst::afw::image::Point<int>;

%apply double &OUTPUT { double & };
%rename(positionToIndexAndResidual) lsst::afw::image::positionToIndex(double &, double);
%clear double &OUTPUT;

%include "lsst/afw/image/ImageUtils.h"

/************************************************************************************************************/

%{
#include "lsst/afw/image/Wcs.h"
%}

%rename(isValid) operator bool;

SWIG_SHARED_PTR(Wcs, lsst::afw::image::Wcs);

%include "lsst/afw/image/Wcs.h"

/************************************************************************************************************/

%{
#include "lsst/afw/image/Exposure.h"
%}

// Must go Before the %include
%define %exposurePtr(TYPE, PIXEL_TYPE)
SWIG_SHARED_PTR_DERIVED(Exposure##TYPE, lsst::daf::data::LsstBase, lsst::afw::image::Exposure<PIXEL_TYPE>);
%enddef

// Must go After the %include
%define %exposure(TYPE, PIXEL_TYPE)
%template(Exposure##TYPE) lsst::afw::image::Exposure<PIXEL_TYPE>;
%lsst_persistable(lsst::afw::image::Exposure<PIXEL_TYPE>);
%enddef

%exposurePtr(U, boost::uint16_t);
%exposurePtr(I, int);
%exposurePtr(F, float);
%exposurePtr(D, double);

%include "lsst/afw/image/Exposure.h"

%exposure(U, boost::uint16_t);
%exposure(I, int);
%exposure(F, float);
%exposure(D, double);

