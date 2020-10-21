#!/usr/bin/env python
#
# LSST Data Management System
# Copyright 2008, 2009, 2010 LSST Corporation.
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
import sys
import matplotlib.pyplot as plt
    
import lsst.afw.cameraGeom.utils as cameraGeomUtils
from lsst.log import Log

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='Show the layout of CCDs in a camera.',
                                     epilog='The corresponding obs-package must be setup (e.g. obs_decam '
                                     'if you want to see DECam)'
                                     )
    parser.add_argument(
        'mapper', help="Name of camera (e.g. decam)", default=None)
    parser.add_argument('--outputFile', type=str,
                        help="File to write plot to", default=None)
    parser.add_argument('--ids', action="store_true",
                        help="Use CCD's IDs, not names")

    args = parser.parse_args()

    #
    # Import the obs package and lookup the mapper
    #
    obsPackageName = f"lsst.obs.{args.mapper}"  # guess the package

    try:
        __import__(obsPackageName)
    except Exception:
        print(f"Unable to import {obsPackageName} -- is it setup?", file=sys.stderr)
        sys.exit(1)

    # __import__ returns the top-level module, so look ours up
    obsPackage = sys.modules[obsPackageName]

    # Guess the name too.
    mapperName = f"{args.mapper[0].title()}{args.mapper[1:]}Mapper"
    try:
        mapper = getattr(obsPackage, mapperName)
    except AttributeError:
        print("Unable to find mapper {mapperName} in {obsPackageName}", file=sys.stderr)
        sys.exit(1)
    #
    # Control verbosity from butler
    #
    log = Log.getLogger("CameraMapper")
    log.setLevel(Log.FATAL)
    #
    # And finally find the camera
    #
    camera = mapper().camera

    if not args.outputFile:
        plt.interactive(True)

    cameraGeomUtils.plotFocalPlane(camera, useIds=args.ids,
                                   showFig=not args.outputFile, savePath=args.outputFile)

    if not args.outputFile:
        print("Hit any key to exit", end=' ')
        input()

    sys.exit(0)
