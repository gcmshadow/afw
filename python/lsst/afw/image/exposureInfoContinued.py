#
# LSST Data Management System
# Copyright 2008-2017 LSST/AURA.
#
# This product includes software developed by the
# LSST Project (http://www.lsst.org/).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the LSST License Statement and
# the GNU General Public License along with this program.  If not,
# see <http://www.lsstcorp.org/LegalNotices/>.
#

from lsst.utils import continueClass
from .exposureInfo import ExposureInfo

__all__ = []  # import this module only for its side effects


@continueClass  # noqa: F811
class ExposureInfo:  # noqa: F811
    KEY_SUMMARY = 'SUMMARY'

    def getSummary(self):
        return self.getComponent('SUMMARY')

    def setSummary(self, summary):
        self.setComponent('SUMMARY', summary)

    def hasSummary(self):
        return self.hasComponent('SUMMARY')
