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

__all__ = ["writeFootprintAsDefects"]

from . import footprintToBBoxList


def writeFootprintAsDefects(fd, foot):
    """
    Write foot as a set of Defects to fd

    Given a detection footprint, convert it to a BBoxList and write the output to the file object fd.
    Does not return anything.

    Parameters
    ----------
    fd : `typing.TextIO`
    foot : `lsst.afw.detection.Footprint`

    Returns
    -------

    See Also
    --------
    lsst.afw.detection.footprintToBBoxList
    """

    bboxes = footprintToBBoxList(foot)
    for bbox in bboxes:
        print("""\
Defects: {
    x0:     %4d                         # Starting column
    width:  %4d                         # number of columns
    y0:     %4d                         # Starting row
    height: %4d                         # number of rows
}""" % (bbox.getMinX(), bbox.getWidth(), bbox.getMinY(), bbox.getHeight()), file=fd)
