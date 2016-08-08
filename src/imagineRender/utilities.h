#ifndef UTILITIES_H
#define UTILITIES_H

namespace Imagine
{
	class ImageReader;
	class ImageWriter;
}

class Utilities
{
public:
	Utilities();

	static Imagine::ImageReader* createImageReaderHDR();
	static Imagine::ImageReader* createImageReaderEXR();
	static Imagine::ImageReader* createImageReaderTXT();
#if HAVE_TIFF_SUPPORT
	static Imagine::ImageReader* createImageReaderTIFF();
#endif
	static Imagine::ImageWriter* createImageWriterEXR();

	static void registerFileReaders();
	static void registerFileWriters();
};

#endif // UTILITIES_H
