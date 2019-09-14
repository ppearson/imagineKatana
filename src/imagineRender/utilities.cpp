/*
 ImagineKatana
 Copyright 2014-2019 Peter Pearson.

 Licensed under the Apache License, Version 2.0 (the "License");
 You may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 ---------
*/

#include "utilities.h"

#include "io/file_io_registry.h"

// this stuff is needed as we can't use the static global initialiser trick that Imagine does,
// as that only works with executables, not libraries...

#include "colour/colour_space.h"

#include "io/image/image_reader_hdr.h"
#include "io/image/image_reader_exr.h"
#include "io/image/image_reader_test.h"
#if HAVE_TIFF_SUPPORT
#include "io/image/image_reader_tiff.h"
#endif
#include "io/image/image_writer_exr.h"

using namespace Imagine;

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

ImageReader* Utilities::createImageReaderTXT()
{
	return new ImageReaderTXT();
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
	FileIORegistry::instance().registerImageReader("txt", createImageReaderTXT);
#if HAVE_TIFF_SUPPORT
	FileIORegistry::instance().registerImageReaderMultipleExtensions("tif;tiff;tex;tx", createImageReaderTIFF);
#endif
}

void Utilities::registerFileWriters()
{
	FileIORegistry::instance().registerImageWriter("exr", createImageWriterEXR);
}
