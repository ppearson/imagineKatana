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
	static ImageWriter* createImageWriterEXR();

	static void registerFileReaders();
	static void registerFileWriters();
};

#endif // UTILITIES_H
