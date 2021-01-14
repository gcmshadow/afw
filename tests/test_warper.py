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
"""Basic test of Warp (the warping algorithm is thoroughly tested in lsst.afw.math)
"""
import os
import unittest

import lsst.utils
import lsst.utils.tests
import lsst.geom
import lsst.afw.geom as afwGeom
import lsst.afw.image as afwImage
import lsst.afw.math as afwMath
import lsst.pex.exceptions as pexExcept
from lsst.log import Log

# Change the level to Log.DEBUG to see debug messages
Log.getLogger("afw.image.Mask").setLevel(Log.INFO)
Log.getLogger("TRACE3.afw.math.warp").setLevel(Log.INFO)
Log.getLogger("TRACE4.afw.math.warp").setLevel(Log.INFO)

try:
    afwdataDir = lsst.utils.getPackageDir("afwdata")
except pexExcept.NotFoundError:
    afwdataDir = None
    dataDir = None
else:
    dataDir = os.path.join(afwdataDir, "data")
    originalExposureName = "medexp.fits"
    originalExposurePath = os.path.join(dataDir, originalExposureName)
    subExposureName = "medsub.fits"
    subExposurePath = os.path.join(dataDir, originalExposureName)
    originalFullExposureName = os.path.join(
        "CFHT", "D4", "cal-53535-i-797722_1.fits")
    originalFullExposurePath = os.path.join(dataDir, originalFullExposureName)


class WarpExposureTestCase(lsst.utils.tests.TestCase):
    """Test case for Warp
    """

    def testMatchSwarpLanczos2Exposure(self):
        """Test that warpExposure matches swarp using a lanczos2 warping kernel.
        """
        self.compareToSwarp("lanczos2")

    def testMatchSwarpLanczos2SubExposure(self):
        """Test that warpExposure matches swarp using a lanczos2 warping kernel with a subexposure
        """
        for useDeepCopy in (False, True):
            self.compareToSwarp("lanczos2", useSubregion=True,
                                useDeepCopy=useDeepCopy)

    @unittest.skipIf(dataDir is None, "afwdata not setup")
    def testBBox(self):
        """Test that the default bounding box includes all warped pixels
        """
        kernelName = "lanczos2"
        warper = afwMath.Warper(kernelName)
        originalExposure, swarpedImage, swarpedWcs = self.getSwarpedImage(
            kernelName=kernelName, useSubregion=True, useDeepCopy=False)

        originalFilterLabel = afwImage.FilterLabel(band="i")
        originalPhotoCalib = afwImage.PhotoCalib(1.0e5, 1.0e3)
        originalExposure.setFilterLabel(originalFilterLabel)
        originalExposure.setPhotoCalib(originalPhotoCalib)

        warpedExposure1 = warper.warpExposure(
            destWcs=swarpedWcs, srcExposure=originalExposure)
        # the default size must include all good pixels, so growing the bbox
        # should not add any
        warpedExposure2 = warper.warpExposure(
            destWcs=swarpedWcs, srcExposure=originalExposure, border=1)
        # a bit of excess border is allowed, but surely not as much as 10 (in
        # fact it is approx. 5)
        warpedExposure3 = warper.warpExposure(
            destWcs=swarpedWcs, srcExposure=originalExposure, border=-10)
        # assert that warpedExposure and warpedExposure2 have the same number of non-no_data pixels
        # and that warpedExposure3 has fewer
        noDataBitMask = afwImage.Mask.getPlaneBitMask("NO_DATA")
        mask1Arr = warpedExposure1.getMaskedImage().getMask().getArray()
        mask2Arr = warpedExposure2.getMaskedImage().getMask().getArray()
        mask3Arr = warpedExposure3.getMaskedImage().getMask().getArray()
        nGood1 = (mask1Arr & noDataBitMask == 0).sum()
        nGood2 = (mask2Arr & noDataBitMask == 0).sum()
        nGood3 = (mask3Arr & noDataBitMask == 0).sum()
        self.assertEqual(nGood1, nGood2)
        self.assertLess(nGood3, nGood1)

        self.assertEqual(warpedExposure1.getFilterLabel().bandLabel,
                         originalFilterLabel.bandLabel)
        self.assertEqual(warpedExposure1.getPhotoCalib(), originalPhotoCalib)

    @unittest.skipIf(dataDir is None, "afwdata not setup")
    def testDestBBox(self):
        """Test that the destBBox argument works
        """
        kernelName = "lanczos2"
        warper = afwMath.Warper(kernelName)
        originalExposure, swarpedImage, swarpedWcs = self.getSwarpedImage(
            kernelName=kernelName, useSubregion=True, useDeepCopy=False)

        bbox = lsst.geom.Box2I(lsst.geom.Point2I(100, 25), lsst.geom.Extent2I(3, 7))
        warpedExposure = warper.warpExposure(
            destWcs=swarpedWcs,
            srcExposure=originalExposure,
            destBBox=bbox,
            # should be ignored
            border=-2,
            # should be ignored
            maxBBox=lsst.geom.Box2I(lsst.geom.Point2I(1, 2),
                                    lsst.geom.Extent2I(8, 9)),
        )
        self.assertEqual(bbox, warpedExposure.getBBox(afwImage.PARENT))

    @unittest.skipIf(dataDir is None, "afwdata not setup")
    def getSwarpedImage(self, kernelName, useSubregion=False, useDeepCopy=False):
        """
        Inputs:
        - kernelName: name of kernel in the form used by afwImage.makeKernel
        - useSubregion: if True then the original source exposure (from which the usual
            test exposure was extracted) is read and the correct subregion extracted
        - useDeepCopy: if True then the copy of the subimage is a deep copy,
            else it is a shallow copy; ignored if useSubregion is False

        Returns:
        - originalExposure
        - swarpedImage
        - swarpedWcs
        """
        if useSubregion:
            originalFullExposure = afwImage.ExposureF(originalExposurePath)
            # "medsub" is a subregion of med starting at 0-indexed pixel (40, 150) of size 145 x 200
            bbox = lsst.geom.Box2I(lsst.geom.Point2I(40, 150),
                                   lsst.geom.Extent2I(145, 200))
            originalExposure = afwImage.ExposureF(
                originalFullExposure, bbox, afwImage.LOCAL, useDeepCopy)
            swarpedImageName = f"medsubswarp1{kernelName}.fits"
        else:
            originalExposure = afwImage.ExposureF(originalExposurePath)
            swarpedImageName = f"medswarp1{kernelName}.fits"

        swarpedImagePath = os.path.join(dataDir, swarpedImageName)
        swarpedDecoratedImage = afwImage.DecoratedImageF(swarpedImagePath)
        swarpedImage = swarpedDecoratedImage.getImage()
        swarpedMetadata = swarpedDecoratedImage.getMetadata()
        swarpedWcs = afwGeom.makeSkyWcs(swarpedMetadata)
        return (originalExposure, swarpedImage, swarpedWcs)

    @unittest.skipIf(dataDir is None, "afwdata not setup")
    def compareToSwarp(self, kernelName,
                       useSubregion=False, useDeepCopy=False,
                       interpLength=10, cacheSize=100000,
                       rtol=4e-05, atol=1e-2):
        """Compare warpExposure to swarp for given warping kernel.

        Note that swarp only warps the image plane, so only test that plane.

        Inputs:
        - kernelName: name of kernel in the form used by afwImage.makeKernel
        - useSubregion: if True then the original source exposure (from which the usual
            test exposure was extracted) is read and the correct subregion extracted
        - useDeepCopy: if True then the copy of the subimage is a deep copy,
            else it is a shallow copy; ignored if useSubregion is False
        - interpLength: interpLength argument for lsst.afw.math.warpExposure
        - cacheSize: cacheSize argument for lsst.afw.math.SeparableKernel.computeCache;
            0 disables the cache
            10000 gives some speed improvement but less accurate results (atol must be increased)
            100000 gives better accuracy but no speed improvement in this test
        - rtol: relative tolerance as used by numpy.allclose
        - atol: absolute tolerance as used by numpy.allclose
        """
        warper = afwMath.Warper(kernelName)

        originalExposure, swarpedImage, swarpedWcs = self.getSwarpedImage(
            kernelName=kernelName, useSubregion=useSubregion, useDeepCopy=useDeepCopy)
        maxBBox = lsst.geom.Box2I(
            lsst.geom.Point2I(swarpedImage.getX0(), swarpedImage.getY0()),
            lsst.geom.Extent2I(swarpedImage.getWidth(), swarpedImage.getHeight()))

        # warning: this test assumes that the swarped image is smaller than it needs to be
        # to hold all of the warped pixels
        afwWarpedExposure = warper.warpExposure(
            destWcs=swarpedWcs,
            srcExposure=originalExposure,
            maxBBox=maxBBox,
        )
        afwWarpedMaskedImage = afwWarpedExposure.getMaskedImage()

        afwWarpedMask = afwWarpedMaskedImage.getMask()
        noDataBitMask = afwImage.Mask.getPlaneBitMask("NO_DATA")
        noDataMask = afwWarpedMask.getArray() & noDataBitMask

        msg = "afw and swarp %s-warped %s (ignoring bad pixels)"
        self.assertImagesAlmostEqual(afwWarpedMaskedImage.getImage(), swarpedImage,
                                     skipMask=noDataMask, rtol=rtol, atol=atol, msg=msg)


class MemoryTester(lsst.utils.tests.MemoryTestCase):
    pass


def setup_module(module):
    lsst.utils.tests.init()


if __name__ == "__main__":
    lsst.utils.tests.init()
    unittest.main()
