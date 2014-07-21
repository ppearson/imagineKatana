#ifndef UTILITIES_H
#define UTILITIES_H

class ImageReader;
class ImageWriter;

class Utilities
{
public:
	Utilities();

	static ImageReader* createImageReaderHDR();
	static ImageReader* createImageReaderEXR();
#if HAVE_TIFF_SUPPORT
	static ImageReader* createImageReaderTIFF();
#endif
	static ImageWriter* createImageWriterEXR();

	static void registerFileReaders();
	static void registerFileWriters();
};

#endif // UTILITIES_H
