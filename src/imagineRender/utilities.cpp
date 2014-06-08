#include "utilities.h"

#include "io/file_io_registry.h"

// this stuff is needed as we can't use the static global initialiser trick that Imagine does,
// as that only works with executables, not libraries...

#include "io/image/image_reader_hdr.h"
#include "io/image/image_reader_exr.h"
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

ImageWriter* Utilities::createImageWriterEXR()
{
	return new ImageWriterEXR();
}

void Utilities::registerFileReaders()
{
	FileIORegistry::instance().registerImageReader("hdr", createImageReaderHDR);
	FileIORegistry::instance().registerImageReader("exr", createImageReaderEXR);
}

void Utilities::registerFileWriters()
{
	FileIORegistry::instance().registerImageWriter("exr", createImageWriterEXR);
}
