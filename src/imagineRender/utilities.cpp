#include "utilities.h"

#include "io/file_io_registry.h"

// this stuff is needed as we can't use the static global initialiser trick that Imagine does,
// as that only works with executables, not libraries...

#include "colour/colour_space.h"

#include "io/image/image_reader_hdr.h"
#include "io/image/image_reader_exr.h"
#if HAVE_TIFF_SUPPORT
#include "io/image/image_reader_tiff.h"
#endif
#include "io/image/image_writer_exr.h"

Utilities::Utilities()
{
}

ImageReader* Utilities::createImageReaderHDR()
{
	return new ImageReaderHDR();
}

ImageReader* Utilities::createImageReaderEXR()
{
	return new ImageReaderEXR();
}

#if HAVE_TIFF_SUPPORT
ImageReader* Utilities::createImageReaderTIFF()
{
	return new ImageReaderTIFF();
}
#endif

ImageWriter* Utilities::createImageWriterEXR()
{
	return new ImageWriterEXR();
}

void Utilities::registerFileReaders()
{
	// needed for LUTs for 8-bit images
	ColourSpace::initLUTs();

	// register the file readers manually...
	FileIORegistry::instance().registerImageReader("hdr", createImageReaderHDR);
	FileIORegistry::instance().registerImageReader("exr", createImageReaderEXR);
#if HAVE_TIFF_SUPPORT
	FileIORegistry::instance().registerImageReaderMultipleExtensions("tif;tiff;tex;tx", createImageReaderTIFF);
#endif
}

void Utilities::registerFileWriters()
{
	FileIORegistry::instance().registerImageWriter("exr", createImageWriterEXR);
}
