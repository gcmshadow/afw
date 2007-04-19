// -*- lsst-c++ -*-
#include "lsst/fw/MaskedImage.h"
#include "lsst/fw/Trace.h"
#include <typeinfo>

using namespace lsst;

template <typename ImagePixelT, typename MaskPixelT> class testPixProcFunc : public PixelProcessingFunc<ImagePixelT, MaskPixelT> {
public:
    typedef typename PixelChannelType<ImagePixelT>::type ImageChannelT;
    typedef typename PixelChannelType<MaskPixelT>::type MaskChannelT;
    typedef PixelLocator<ImagePixelT> ImageIteratorT;
    typedef PixelLocator<MaskPixelT> MaskIteratorT;
     
    testPixProcFunc(MaskedImage<ImagePixelT, MaskPixelT>& m) : PixelProcessingFunc<ImagePixelT, MaskPixelT>(m), initCount(0) {}
    
    void init() {
        PixelProcessingFunc<ImagePixelT, MaskPixelT>::_maskPtr->getPlaneBitMask("CR", bitsCR);
        testCount = 0;
        initCount++;
    }
        
    void operator ()(ImageIteratorT &i,MaskIteratorT &m ) { 
        //  In general, do something to the pixel values
//         ImageIteratorT j = i;
//         if (++testCount < 10) {
//             std::cout << *i << " " << *m << std::endl;
//             *j = 1;
//             int dx = 1;
//             int dy = 0;
//             if (initCount <2) *(j.advance(dx,dy)) = 2*testCount;
//             std::cout << "modified: " << *j << std::endl;
//          }
        if (*i > 15000) {
            MaskPixelT mPix = *m;
            MaskChannelT mPixChannel = mPix[0];
            mPixChannel |= bitsCR;
            mPix[0] = mPixChannel;
            *m = mPix;
            testCount++;
        }
     }

    int getCount() { return testCount; }

private:
    MaskChannelT bitsCR;
    int testCount;
    int initCount;
};




int main(int argc, char**argv)
{
    using namespace lsst::fw::Trace;

    setDestination(std::cout);

    setVerbosity(".", 0);

     typedef PixelGray<uint8> MaskPixelType;
     typedef PixelGray<float32> ImagePixelType;

     MaskedImage<ImagePixelType,MaskPixelType > testMaskedImage1;
     testMaskedImage1.readFits(argv[1]);
     testMaskedImage1.getMask()->addMaskPlane("CR");
     
     testPixProcFunc<ImagePixelType, MaskPixelType> fooFunc(testMaskedImage1);   // should be a way to automatically convey template types
                                                                                 // from testMaskedImage1 to fooFunc
     fooFunc.init();

     testMaskedImage1.processPixels(fooFunc);

     fooFunc.init();
     testMaskedImage1.processPixels(fooFunc);
     std::cout << fooFunc.getCount() << " mask pixels were set" << std::endl;

}
