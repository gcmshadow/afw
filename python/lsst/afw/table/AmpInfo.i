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

/*
 * Wrappers for IdFactory, AmpInfoTable and AmpInfoRecord (and, via Base.i, their dependencies).  Also
 * includes SortedCatalog.i and instantiates AmpInfoCatalog.
 *
 * This file does not include Source-, or Exposure- Record/Table/Catalog, or the matching functions.
 */

%include "lsst/afw/table/Base.i"

%{
#include "lsst/afw/table/AmpInfo.h"
%}

// =============== IdFactory =======================================================================

%shared_ptr(lsst::afw::table::IdFactory);
%ignore lsst::afw::table::IdFactory::operator=;

%include "lsst/afw/table/IdFactory.h"

// =============== AmpInfoTable and AmpInfoRecord =============================================================

%shared_ptr(lsst::afw::table::AmpInfoTable)
%shared_ptr(lsst::afw::table::AmpInfoRecord)

%include "lsst/afw/table/AmpInfo.h"

%template(AmpInfoColumnView) lsst::afw::table::ColumnViewT<lsst::afw::table::AmpInfoRecord>;

%addCastMethod(lsst::afw::table::AmpInfoTable, lsst::afw::table::BaseTable)
%addCastMethod(lsst::afw::table::AmpInfoRecord, lsst::afw::table::BaseRecord)

// =============== Catalogs =================================================================================

%include "lsst/afw/table/SortedCatalog.i"

namespace lsst { namespace afw { namespace table {

%declareSortedCatalog(SortedCatalogT, AmpInfo)

}}} // namespace lsst::afw::table