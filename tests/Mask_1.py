import os
import math
import pdb                          # we may want to say pdb.set_trace()
import unittest

import lsst.fw.Core.fwLib as fw
import lsst.mwi.tests as tests
import lsst.mwi.data as mwid
import fwTests

try:
    type(verbose)
except NameError:
    verbose = 0

class MaskTestCase(unittest.TestCase):
    """A test case for Mask (based on Mask_1.cc)"""

    def setUp(self):
        #import rhl; rhl.cmdtrace(11)
        maskImage = fw.ImageViewU(300,400)

        self.testMask = fw.MaskU(fw.MaskIVwPtrT(maskImage))

        for p in ("CR", "BP"):
            self.testMask.addMaskPlane(p)

        self.region = fw.BBox2i(100, 300, 10, 40)
        self.subTestMask = self.testMask.getSubMask(self.region)

        self.pixelList = fw.listPixelCoord()
        for x in range(0, 300):
            for y in range(300, 400, 20):
                self.pixelList.push_back(fw.PixelCoord(x, y))

    def tearDown(self):
        del self.subTestMask
        del self.testMask
        del self.region

    def testPlaneAddition(self):
        """Test mask plane addition"""

        nplane = self.testMask.getNumPlanesUsed()
        for p in ("XCR", "XBP"):
            self.assertEqual(self.testMask.addMaskPlane(p), nplane, "Assigning plane %s" % (p))
            nplane += 1

        for p in range(0,8):
            sp = "P%d" % p
            plane = self.testMask.addMaskPlane(sp)
            #print "Assigned %s to plane %d" % (sp, plane)

        for p in range(0,8):
            sp = "P%d" % p
            self.testMask.removeMaskPlane(sp)

        self.assertEqual(nplane, self.testMask.getNumPlanesUsed(), "Adding and removing planes")

    def testMetaData(self):
        """Test mask plane metaData"""

        metaData = mwid.SupportFactory_createPropertyNode("testMetaData")

        self.testMask.addMaskPlaneMetaData(metaData)
        print "MaskPlane metadata:"
        print metaData.toString("\t");

        print "Printing metadata from Python:"
        d = self.testMask.getMaskPlaneDict()
        for p in d.keys():
            if d[p]:
                print "\t", d[p], p

        newPlane = mwid.DataProperty("Whatever", 5)
        metaData.addProperty(newPlane)

        self.testMask.parseMaskPlaneMetaData(metaData)
        print "After loading metadata: "
        self.testMask.printMaskPlanes()

    def testPlaneOperations(self):
        """Test mask plane operations"""

        planes = lookupPlanes(self.testMask, ["CR", "BP"])
        self.testMask.clearMaskPlane(planes['CR'])

        for p in planes.keys():
            self.testMask.setMaskPlaneValues(planes[p], self.pixelList)

        printMaskPlane(self.testMask, planes['CR'])

        print "\nClearing mask"
        self.testMask.clearMaskPlane(planes['CR'])

        printMaskPlane(self.testMask, planes['CR'])

    def testOrEquals(self):
        """Test |= operator"""

        testMask3 = fw.MaskU(
            fw.MaskIVwPtrT(fw.ImageViewU(self.testMask.getCols(), self.testMask.getRows()))
            )

        testMask3.addMaskPlane("CR")

        self.testMask |= testMask3

        print "Applied |= operator"

    def testPlaneRemoval(self):
        """Test mask plane removal"""

        planes = lookupPlanes(self.testMask, ["CR", "BP"])
        self.testMask.clearMaskPlane(planes['BP'])
        self.testMask.removeMaskPlane("BP")

        self.assertEqual(self.testMask.getMaskPlane("BP"), -1, "Plane BP is removed")

    def testSubmask(self):
        """Test submask methods"""

        planes = lookupPlanes(self.testMask, ["CR", "BP"])
        self.testMask.setMaskPlaneValues(planes['CR'], self.pixelList)

        self.testMask.clearMaskPlane(planes['CR'])

        self.testMask.replaceSubMask(self.region, self.subTestMask)

        printMaskPlane(self.testMask, planes['CR'], range(90, 120), range(295, 350, 5))

    def testMaskPixelBooleanFunc(self):
        """Test MaskPixelBooleanFunc"""
        testCrFuncInstance = fwTests.testCrFuncD(self.testMask)
        testCrFuncInstance.init() # get the latest plane info from testMask
        CR_plane = self.testMask.getMaskPlane("CR")
        self.assertNotEqual(CR_plane, -1)
        
        self.testMask.setMaskPlaneValues(CR_plane, self.pixelList)
        count = self.testMask.countMask(testCrFuncInstance, self.region)
        self.assertEqual(count, 20, "Saw %d pixels with CR set" % count)

        del testCrFuncInstance

        # should generate a vw exception - dims. of region and submask must be =
        self.region.expand(10)
        self.assertRaises(IndexError, self.testMask.replaceSubMask, self.region, self.subTestMask)

#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

def lookupPlanes(mask, planeNames):
    planes = {}
    for p in planeNames:
        try:
            planes[p] = mask.getMaskPlane(p)
            print "%s plane is %d" % (p, planes[p])
        except Exception, e:
            print "No %s plane found: %s" % (p, e)

    return planes

def printMaskPlane(mask, plane,
                   xrange=range(250, 300, 10), yrange=range(300, 400, 20)):
    """Print parts of the specified plane of the mask"""
    
    if True:
        xrange = range(min(xrange), max(xrange), 25)
        yrange = range(min(yrange), max(yrange), 25)

    for x in xrange:
        for y in yrange:
            if False:                   # mask(x,y) confuses swig
                print x, y, mask(x, y), mask(x, y, plane)
            else:
                print x, y, mask(x, y, plane)

#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

def suite():
    """Returns a suite containing all the test cases in this module."""
    tests.init()

    suites = []
    suites += unittest.makeSuite(MaskTestCase)
    suites += unittest.makeSuite(tests.MemoryTestCase)

    return unittest.TestSuite(suites)

def run(exit=False):
    """Run the tests"""
    try:
        tests.run(suite(), exit)        # mwi 1.3
    except:
        tests.run(suite())

if __name__ == "__main__":
    run(True)
