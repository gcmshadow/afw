#!/usr/bin/env python
# -*- python -*-
"""
Tests for Coord

Run with:
   python Coord.py
or
   python
   >>> import Coord
   >>> Coord.run()
"""

import sys, os
import unittest
import lsst.afw.geom             as geom
import lsst.afw.coord.coordLib   as coord
import lsst.utils.tests          as utilsTests
import eups

# todo: see if we can give an Fk5 and an ecliptic at a different epoch and get the right answer
# todo: make sure ICrs stuff works

######################################
# main body of code
######################################
class CoordTestCase(unittest.TestCase):

    def setUp(self):
        # define some arbitrary values
        self.ra, self.raKnown  = "10:00:00.00", 10.0
        self.dec, self.decKnown = "-02:30:00.00", -2.5
        self.l = 100.0
        self.b = 30.0
        
    def testFormat(self):
        """Test formatting"""
        equ = coord.Fk5Coord(self.ra, self.dec)
        self.assertAlmostEqual(equ.getRaHrs(), self.raKnown)
        self.assertAlmostEqual(equ.getDecDeg(), self.decKnown)

        print "Format: %s  %s" % (equ.getRaStr(), self.ra)
        self.assertEqual(equ.getRaStr(), self.ra)


    def testFactory(self):
        """Test the Factory function makeCoord()"""

        # make a (eg galactic) coord with the constructor, and with the factory
        # ... require an internal coordinate conversion to equatorial and see if they agree.
        # They should only agree if they both converted from the same type, thus we got it right.
        
        coordList = [
            [coord.Fk5Coord, coord.FK5],
            [coord.IcrsCoord, coord.ICRS],
            [coord.GalacticCoord, coord.GALACTIC],
            [coord.EclipticCoord, coord.ECLIPTIC],
            # we can't factory an AltAz ... Observatory must be specified.
            # [coord.AltAzCoord, coord.ALTAZ]  
            ]
        
        for constructor, enum in coordList:
            con = constructor(self.l, self.b)
            factories = []
            factories.append(coord.makeCoord(enum, self.l, self.b))
            factories.append(coord.makeCoord(enum, geom.makePointD(self.l, self.b), coord.DEGREES))
            factories.append(coord.makeCoord(enum, con.getLongitudeStr(), con.getLatitudeStr()))

            print "Factory: "
            for fac in factories:
                self.assertAlmostEqual(con.getRaDeg(), fac.getRaDeg())
                self.assertAlmostEqual(con.getDecDeg(), fac.getDecDeg())
                print " tried ", fac.getRaDeg(), "(expected ", con.getRaDeg(), ")"
                
                
    def testPointD(self):
        """Test the getPoint2D() method"""
        equ = coord.Fk5Coord(self.ra, self.dec)

        # make sure we get what we asked for
        pDeg = equ.getPoint2D(coord.DEGREES)
        self.assertAlmostEqual(equ.getRaDeg(), pDeg.getX())
        self.assertAlmostEqual(equ.getDecDeg(), pDeg.getY())

        pRad = equ.getPoint2D(coord.RADIANS)
        self.assertAlmostEqual(equ.getRaRad(), pRad.getX())
        self.assertAlmostEqual(equ.getDecRad(), pRad.getY())

        pHrs = equ.getPoint2D(coord.HOURS)
        self.assertAlmostEqual(equ.getRaHrs(), pHrs.getX())
        self.assertAlmostEqual(equ.getDecDeg(), pHrs.getY())

        # make sure we construct with the type we ask for
        equ1 = coord.Fk5Coord(pDeg, coord.DEGREES)
        self.assertAlmostEqual(equ1.getRaRad(), equ.getRaRad())

        equ2 = coord.Fk5Coord(pRad, coord.RADIANS)
        self.assertAlmostEqual(equ2.getRaRad(), equ.getRaRad())

        equ3 = coord.Fk5Coord(pHrs, coord.HOURS)
        self.assertAlmostEqual(equ1.getRaRad(), equ.getRaRad())

        
    def testNames(self):
        """Test the names of the Coords (Useful with Point2D form)"""
        
        radec1, known1 = coord.Coord(self.ra, self.dec).getCoordNames(), ["RA", "Dec"]
        radec3, known3 = coord.Fk5Coord(self.ra, self.dec).getCoordNames(), ["RA", "Dec"]
        radec4, known4 = coord.IcrsCoord(self.ra, self.dec).getCoordNames(), ["RA", "Dec"]
        lb, known5     = coord.GalacticCoord(self.ra, self.dec).getCoordNames(), ["L", "B"]
        lambet, known6 = coord.EclipticCoord(self.ra, self.dec).getCoordNames(), ["Lambda", "Beta"]
        altaz, known7  = coord.AltAzCoord(self.ra, self.dec, 2000.0,
                                          coord.Observatory(0,0,0)).getCoordNames(), ["Az", "Alt"]

        pairs = [ [radec1, known1],
                  [radec3, known3],
                  [radec4, known4],
                  [lb,     known5],
                  [lambet, known6],
                  [altaz,  known7], ]
                  
        for pair, known in (pairs):
            self.assertEqual(pair[0], known[0])
            self.assertEqual(pair[1], known[1])
            
        
    def testEcliptic(self):
        """Verify Ecliptic Coordinate Transforms""" 
       
        # Pollux
        alpha, delta = "07:45:18.946", "28:01:34.26"
        # known ecliptic coords (example from Meeus, Astro algorithms, pg 95)
        lamb, beta = 113.215629, 6.684170
        
        polluxEqu = coord.Fk5Coord(alpha, delta)
        polluxEcl = polluxEqu.toEcliptic()
        print "Ecliptic (Pollux): ", polluxEcl.getLambdaDeg(), polluxEcl.getBetaDeg(), lamb, beta

        # verify to precision of known values
        self.assertAlmostEqual(polluxEcl.getLambdaDeg(), lamb, 6)
        self.assertAlmostEqual(polluxEcl.getBetaDeg(), beta, 6)

        # make sure it transforms back (machine precision)
        self.assertAlmostEqual(polluxEcl.toFk5().getRaDeg(), polluxEqu.getRaDeg(), 13)
        self.assertAlmostEqual(polluxEcl.toFk5().getDecDeg(), polluxEqu.getDecDeg(), 13)


    def testGalactic(self):
        """Verify Galactic coordinate transforms"""

        # Sagittarius A (very nearly the galactic center)

        sagAKnownEqu = coord.Fk5Coord("17:45:40.04","-29:00:28.1")
        sagAKnownGal = coord.GalacticCoord(359.94432, -0.04619)
        
        sagAGal = sagAKnownEqu.toGalactic()
        print "Galactic (Sag-A):  (transformed) %.5f %.5f   (known) %.5f %.5f\n" % (sagAGal.getLDeg(),
                                                                                    sagAGal.getBDeg(),
                                                                                    sagAKnownGal.getLDeg(),
                                                                                    sagAKnownGal.getBDeg())
        
        # verify ... to 4 places, the accuracy of the galactic pole in Fk5
        self.assertAlmostEqual(sagAGal.getLDeg(), sagAKnownGal.getLDeg(), 4)
        self.assertAlmostEqual(sagAGal.getBDeg(), sagAKnownGal.getBDeg(), 4)

        # make sure it transforms back ... to machine precision
        self.assertAlmostEqual(sagAGal.toFk5().getRaDeg(), sagAKnownEqu.getRaDeg(), 14)
        self.assertAlmostEqual(sagAGal.toFk5().getDecDeg(), sagAKnownEqu.getDecDeg(), 14)
        
        
    def testAltAz(self):
        """Verify Altitude/Azimuth coordinate transforms"""
        
        # sedna (from jpl) for 2010-03-03 00:00 UT
        ra, dec = "03:26:42.61",  "+06:32:07.1"
        az, alt = 231.5947, 44.3375
        obs = coord.Observatory(40.384, 74.659, 100.0) # peyton
        obsDate = coord.Date(2010, 3, 3, 0, 0, 0)
        sedna = coord.Fk5Coord(ra, dec, obsDate.getEpoch())
        altaz = sedna.toAltAz(obs, obsDate)
        print "AltAz (Sedna): ", altaz.getAltitudeDeg(), altaz.getAzimuthDeg(), alt, az

        # precision is low as we don't account for as much as jpl (abberation, nutation, etc)
        self.assertAlmostEqual(altaz.getAltitudeDeg(), alt, 1)
        self.assertAlmostEqual(altaz.getAzimuthDeg(), az, 1)


    def testPrecess(self):
        """Test precession calculations in different coordinate systems"""
        
        ### Fk5 ###
        
        # example 21.b Meeus, pg 135, for Alpha Persei ... with proper motion
        alpha0, delta0 = "2:44:11.986", "49:13:42.48"
        # proper motions per year
        dAlphaS, dDeltaAS = 0.03425, -0.0895
        dAlphaDeg, dDeltaDeg = dAlphaS*15/3600.0, dDeltaAS/3600.0

        # get for 2028, Nov 13.19
        epoch = coord.Date(2028, 11, 13, 4, 33, 36.0).getEpoch()

        # the known final answer
        # - actually 41.547214, 49.348483 (suspect precision error in Meeus)
        alphaKnown, deltaKnown = 41.547236, 49.348488

        alphaPer0 = coord.Fk5Coord(alpha0, delta0)
        alphaDeg = alphaPer0.getRaDeg() + dAlphaDeg*(epoch - 2000.0)
        deltaDeg = alphaPer0.getDecDeg() + dDeltaDeg*(epoch - 2000.0)

        alphaPer = coord.Fk5Coord(alphaDeg, deltaDeg).precess(epoch)

        print "Precession (Alpha-Per): %.6f %.6f   (known) %.6f %.6f" % (alphaPer.getRaDeg(),
                                                                         alphaPer.getDecDeg(),
                                                                         alphaKnown, deltaKnown)
        # precision 6 (with 1 digit fudged in the 'known' answers)
        self.assertAlmostEqual(alphaPer.getRaDeg(), alphaKnown, 6)
        self.assertAlmostEqual(alphaPer.getDecDeg(), deltaKnown, 6)
        
        ### Galactic ###
        
        # make sure Galactic doesn't change
        gal = coord.GalacticCoord(self.l, self.b, 2000.0)
        epochNew = 2010.0
        galNew = gal.precess(epochNew)
        print "Precession (Galactic, 2000): %.6f %.6f   (%.1f) %.6f %.6f" % (self.l, self.b, epochNew,
                                                                             galNew.getLDeg(),
                                                                             galNew.getBDeg())
        # machine precision
        self.assertAlmostEqual(self.l, galNew.getLDeg())
        self.assertAlmostEqual(self.b, galNew.getBDeg())

        ### Ecliptic ###
        
        # test for ecliptic with venus (example from meeus, pg 137)
        lambFk5, betaFk5 = 149.48194, 1.76549
        
        # known values for -214, June 30.0
        # they're actually 118.704, 1.615, but I suspect discrepancy is a rounding error in Meeus
        # -- we use double precision, he carries 7 places only.
        lambNew, betaNew = 118.704, 1.606 
        venusFk5 = coord.EclipticCoord(lambFk5, betaFk5, 2000.0)
        venusNew = venusFk5.precess(coord.Date(-214, 6, 30, 0, 0, 0).getEpoch())
        print "Precession (Ecliptic, Venus): %.4f %.4f  (known) %.4f %.4f" % (venusNew.getLambdaDeg(),
                                                                              venusNew.getBetaDeg(),
                                                                              lambNew, betaNew)
        # 3 places precision (accuracy of our controls)
        self.assertAlmostEqual(venusNew.getLambdaDeg(), lambNew, 3)
        self.assertAlmostEqual(venusNew.getBetaDeg(), betaNew, 3)


    def testAngularSeparation(self):
        """Test measure of angular separation between two coords"""

        # test from Meeus, pg 110
        spica = coord.Fk5Coord(201.2983, -11.1614)
        arcturus = coord.Fk5Coord(213.9154, 19.1825)
        knownDeg = 32.7930
        
        deg = spica.angularSeparation(arcturus)

        print "Separation (Spica/Arcturus): %.6f (known) %.6f" % (deg, knownDeg)
        # verify to precision of known
        self.assertAlmostEqual(deg, knownDeg, 4)
        
        # verify small angles ... along a constant ra, add an arcsec to spica dec
        epsilonDeg = 1.0/3600.0
        spicaPlus = coord.Fk5Coord(spica.getRaDeg(), spica.getDecDeg() + epsilonDeg)
        deg = spicaPlus.angularSeparation(spica)

        print "Separation (Spica+epsilon): %.8f  (known) %.8f" % (deg, epsilonDeg)
        # machine precision
        self.assertAlmostEqual(deg, epsilonDeg)

        
#################################################################
# Test suite boiler plate
#################################################################
def suite():
    """Returns a suite containing all the test cases in this module."""

    utilsTests.init()

    suites = []
    suites += unittest.makeSuite(CoordTestCase)
    suites += unittest.makeSuite(utilsTests.MemoryTestCase)
    return unittest.TestSuite(suites)

def run(exit = False):
    """Run the tests"""
    utilsTests.run(suite(), exit)

if __name__ == "__main__":
    run(True)
