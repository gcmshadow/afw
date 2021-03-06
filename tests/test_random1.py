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

"""
Tests for the lsst.afw.math.Random Python wrapper

Run with:
   python test_random1.py
or
   pytest test_random1.py

"""

import sys
import time
import unittest

import lsst.pex.exceptions
import lsst.utils.tests
import lsst.geom
import lsst.afw.image as afwImage
import lsst.afw.math as afwMath


def checkRngEquivalence(rng1, rng2):
    for i in range(1000):
        assert rng1.uniform() == rng2.uniform()


def getSeed():
    return int(time.time() * 1000000.0) % 1000000


class RandomTestCase(unittest.TestCase):
    """A test case for lsst.afw.math.Random"""

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def testCreate(self):
        rngs = []
        for name in afwMath.Random.getAlgorithmNames():
            rngs.append(afwMath.Random(name))
        for r in rngs:
            assert afwMath.Random(r.getAlgorithmName()
                                  ).uniform() == r.uniform()
            r2 = afwMath.Random(r.getAlgorithm())
            r2.uniform()
            assert r2.uniform() == r.uniform()

    def testCopy(self):
        """
        Test that the generator returned by deepCopy() produces an
        identical random number sequence to its prototype
        """
        rng1 = afwMath.Random(afwMath.Random.MT19937, getSeed())
        rng2 = rng1.deepCopy()
        checkRngEquivalence(rng1, rng2)


class RandomImageTestCase(unittest.TestCase):
    """A test case for lsst.afw.math.Random applied to Images"""

    def setUp(self):
        self.rand = afwMath.Random()
        self.image = afwImage.ImageF(lsst.geom.Extent2I(1000, 1000))

    def tearDown(self):
        del self.image

    def testState(self):
        for i in range(100):
            self.rand.uniformInt(10000)
        state = self.rand.getState()
        self.assertIsInstance(state, bytes)
        v1 = [self.rand.uniformInt(10000) for i in range(100)]
        self.rand.setState(state)
        v2 = [self.rand.uniformInt(10000) for i in range(100)]
        self.assertEqual(v1, v2)

    def testStateRaisesType(self):
        with self.assertRaises(TypeError):
            self.rand.setState(self.rand)

    @unittest.skipIf(sys.version_info.major < 3, "setState can not distinguish unicode from bytes")
    def testStateRaisesUnicodeType(self):
        with self.assertRaises(TypeError):
            self.rand.setState(u"\u03bc not bytes")

    def testStateRaisesLength(self):
        with self.assertRaises(lsst.pex.exceptions.LengthError):
            self.rand.setState(b"too small")

    def testRandomUniformImage(self):
        afwMath.randomUniformImage(self.image, self.rand)

    def testRandomGaussianImage(self):
        afwMath.randomGaussianImage(self.image, self.rand)

    def testRandomChisqImage(self):
        nu = 10
        afwMath.randomChisqImage(self.image, self.rand, nu)
        stats = afwMath.makeStatistics(
            self.image, afwMath.MEAN | afwMath.VARIANCE)
        if False:
            print("nu = %g.  mean = %g, variance = %g" %
                  (nu, stats.getValue(afwMath.MEAN), stats.getValue(afwMath.VARIANCE)))
        self.assertAlmostEqual(stats.getValue(afwMath.MEAN), nu, 1)
        self.assertAlmostEqual(stats.getValue(afwMath.VARIANCE), 2*nu, 1)

    def testRandomPoissonImage(self):
        mu = 10
        afwMath.randomPoissonImage(self.image, self.rand, mu)
        stats = afwMath.makeStatistics(
            self.image, afwMath.MEAN | afwMath.VARIANCE)
        if False:
            print("mu = %g.  mean = %g, variance = %g" %
                  (mu, stats.getValue(afwMath.MEAN), stats.getValue(afwMath.VARIANCE)))
        self.assertAlmostEqual(stats.getValue(afwMath.MEAN), mu, 1)
        self.assertAlmostEqual(stats.getValue(afwMath.VARIANCE), mu, 1)


class TestMemory(lsst.utils.tests.MemoryTestCase):
    pass


def setup_module(module):
    lsst.utils.tests.init()


if __name__ == "__main__":
    lsst.utils.tests.init()
    unittest.main()
