# This file is part of afw.
#
# Developed for the LSST Data Management System.
# This product includes software developed by the LSST Project
# (https://www.lsst.org).
# See the COPYRIGHT file at the top-level directory of this distribution
# for details of code ownership.
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
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
"""This file only exists to deprecate the Filter and FilterProperty classes.
"""
import warnings

from lsst.utils.deprecated import deprecate_pybind11
from lsst.utils import continueClass

from . import Filter, FilterProperty


@continueClass
class Filter:
    # NOTE: Using `deprecate_pybind11` causes the `AUTO` and `UNKNOWN` static
    # members to become `pybind11_builtins.pybind11_static_property` instead
    # of `int`, which breaks any uses of them in the python layer, so we
    # deprecate this way instead.
    _init = Filter.__init__

    def __init__(self, *args, **kwargs):
        warnings.warn("Replaced by FilterLabel. Will be removed after v23.",
                      FutureWarning, stacklevel=2)
        self._init(*args, **kwargs)


FilterProperty = deprecate_pybind11(
    FilterProperty,
    reason=("Removed with no replacement (but see lsst.afw.image.TransmissionCurve)."
            "Will be removed after v23."),
    version="v22.0")
