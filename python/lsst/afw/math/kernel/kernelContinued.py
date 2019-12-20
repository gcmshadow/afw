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

from lsst.utils.deprecated import deprecate_pybind11
from .kernel import Kernel

__all__ = []  # import this module only for its side effects


Kernel.getCtrX = deprecate_pybind11(
    Kernel.getCtrX,
    reason='Use `getCtr` instead. To be removed after 20.0.0.')
Kernel.getCtrY = deprecate_pybind11(
    Kernel.getCtrY,
    reason='Use `getCtr` instead. To be removed after 20.0.0.')
Kernel.setCtrX = deprecate_pybind11(
    Kernel.setCtrX,
    reason='Use `setCtr` instead. To be removed after 20.0.0.')
Kernel.setCtrY = deprecate_pybind11(
    Kernel.setCtrY,
    reason='Use `setCtr` instead. To be removed after 20.0.0.')
