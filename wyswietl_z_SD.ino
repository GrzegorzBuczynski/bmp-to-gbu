// MCUFRIEND UNO shields have microSD on pins 10, 11, 12, 13
// The official <SD.h> library only works on the hardware SPI pins
// e.g. 11, 12, 13 on a Uno
// e.g. 50, 51, 52 on a Mega2560
// e.g. 74, 75, 76 on a Due
//
// if you are not using a UNO,  you must use Software SPI:
//
// install v1.0.1 of the <SdFat.h> library with the Arduino Library Manager.
// edit the src/SdFatConfig.h file to #define ENABLE_SOFTWARE_SPI_CLASS 1
//
// copy all your BMP files to the root directory on the microSD with your PC
// (or another directory)

// #include "convert24to16.h"
// #include <stdstring.h>
#include <SPI.h> // f.k. for Arduino-1.5.2
#define USE_SDFAT
#include <SdFat.h>			 // Use the SdFat library
SdFatSoftSpi<12, 11, 13> SD; // Bit-Bang on the Shield pins

#include <Adafruit_GFX.h> // Hardware-specific library
#include <MCUFRIEND_kbv.h>
MCUFRIEND_kbv tft;

#define SD_CS 10
#define NAMEMATCH "" // "" matches any name
// #define NAMEMATCH "tiger"    // *tiger*.bmp
#define PALETTEDEPTH 8 // support 256-colour Palette

char namebuf[32] = "/"; // BMP files in root directory
// char namebuf[32] = "/bitmaps/";  //BMP directory e.g. files in /bitmaps/*.bmp

File root;
int pathlen;

void setup()
{
	uint16_t ID;
	Serial.begin(9600);
	Serial.print("Show BMP files on TFT with ID:0x");
	ID = tft.readID();
	Serial.println(ID, HEX);
	if (ID == 0x0D3D3)
		ID = 0x9481;
	tft.begin(ID);
	tft.fillScreen(0x001F);
	tft.setTextColor(0xFFFF, 0x0000);
	bool good = SD.begin(SD_CS);
	if (!good)
	{
		Serial.print(F("cannot start SD"));
		while (1)
			;
	}
	root = SD.open(namebuf);
	pathlen = strlen(namebuf);
	convertBmpToGbu();
}

void loop()
{
	char *nm = namebuf + pathlen;
	File f = root.openNextFile();
	uint8_t ret;
	uint32_t start;
	if (f != NULL)
	{
#ifdef USE_SDFAT
		f.getName(nm, 32 - pathlen);
#else
		strcpy(nm, (char *)f.name());
#endif
		f.close();
		strlwr(nm);
		if (strstr(nm, ".gbu") != NULL && strstr(nm, NAMEMATCH) != NULL)
		{
			Serial.print(namebuf);
			Serial.print(F(" - "));
			tft.fillScreen(0);
			start = millis();
			ret = showGBU(namebuf, 5, 5);
			switch (ret)
			{
			case 0:
				Serial.print(millis() - start);
				Serial.println(F("ms"));
				delay(5000);
				break;
			case 1:
				Serial.println(F("bad position"));
				break;
			case 2:
				Serial.println(F("bad BMP ID"));
				break;
			case 3:
				Serial.println(F("wrong number of planes"));
				break;
			case 4:
				Serial.println(F("unsupported BMP format"));
				break;
			case 5:
				Serial.println(F("unsupported palette"));
				break;
			default:
				Serial.println(F("unknown"));
				break;
			}
		}
	}
	else
		root.rewindDirectory();
}

#define BMPIMAGEOFFSET 3

#define BUFFPIXEL 20

uint8_t read8(File &f)
{
	uint8_t result; // read little-endian
	f.read(&result, sizeof(result));
	return result;
}

uint16_t read16(File &f)
{
	uint16_t result; // read little-endian
	f.read(&result, sizeof(result));
	return result;
}

uint32_t read32(File &f)
{
	uint32_t result;
	f.read(&result, sizeof(result));
	return result;
}

uint8_t showGBU(char *nm, int x, int y)
{
	// napisz funkcje która spróbuje znaleść pliki o rozszerzeniu .bmp skonwertuje zawartość bitmapy z 3 bajtów na 2 bajty (565) następnie zapisze w plik na karcie sd z rozszerzeniem .gbu a następnie usunie orginalny plik .bmp, w nagłówku pliku .gbu zamieść takie wartości uint8_t bmpWidth, bmpHeight0, bmpDepth, bmpOffset, wyskość i szerokość pobierz z orginalnego pliku. Głębokość ustaw na 16, w wartośći offetu zapisz offet gdzie znajdują się dane uwzględniając że obecnym nagłówku jest wolnych 10 bajtów. dane mają być zapisane od lewej do prawej i od góry do dołu  Użyj #include <SdFat.h>

	uint16_t bmpWidth = 0, bmpHeight = 0, bmpDepth = 0, bmpOffset = 0;

	File file = SD.open(nm);

	// Read the header
	bmpHeight = read16(file);
	bmpWidth = read16(file);
	read16(file);
	bmpDepth = read16(file);
	bmpOffset = read16(file);
	read16(file);
	read16(file);
	read16(file);
	read16(file);
	read16(file);

	int w = bmpWidth;
	int h = bmpHeight;
	if ((x + w) >= tft.width()) // Crop area to be loaded
		w = tft.width() - x;
	if ((y + h) >= tft.height()) //
		h = tft.height() - y;

	tft.setAddrWindow(x, y, x + w - 1, y + h - 1);
	bool first = true;
	// Read the pixel data
	for (int row = 0; row < h; row++)
	{
		uint16_t sdbuffer[w];
		file.read(sdbuffer, sizeof(sdbuffer));
		int16_t col = 0;
		tft.pushColors(&sdbuffer[col], 1, first);
		first = false;
		col++;
		for (; col < w; col++)
		{
			tft.pushColors(&sdbuffer[col], 1, first);
		}
	}

	// Close the file
	file.close();
}

void convertBmpToGbu()
{
	File root = SD.open("/");
	if (!root)
	{
		Serial.println(F("Error opening root /"));
		return;
	}

	while (true)
	{
		// Open the next file
		File bmpFile = root.openNextFile();
		if (!bmpFile)
		{
			Serial.println(F("No more files"));
			break; // Exit the loop when there are no more files
		}

		char bmpFileName[31];
		bmpFile.getName(bmpFileName, 31);
		if (strstr(bmpFileName, ".bmp") != NULL && strstr(bmpFileName, NAMEMATCH) != NULL)
		{
			Serial.println(F("Converting BMP to GBU: "));
			// Read the BMP header
			// Read the BMP header
			uint8_t bmpHeader[54];
			bmpFile.read(bmpHeader, sizeof(bmpHeader));

			// Extract the BMP width, height, and depth from the header
			uint16_t bmpWidth = *(uint16_t *)(bmpHeader + 22);
			uint16_t bmpHeight = *(uint16_t *)(bmpHeader + 18);
			uint16_t bmpDepth = *(uint8_t *)(bmpHeader + 28);

			// Calculate the offset where the pixel data starts
			uint32_t bmpOffset = *(uint32_t *)(bmpHeader + 10);

			// Calculate the total number of pixels
			uint16_t totalPixels = bmpWidth * bmpHeight;

			// Calculate the number of blocks of 200 pixels
			uint16_t numBlocks = (totalPixels + 199) / 200; // This rounds up to the nearest block
			uint16_t currentBlock = 0;

			String gbuFileName(bmpFileName);
			gbuFileName.replace(".bmp", ".gbu");

			// Open the GBU file for writing
			File gbuFile = SD.open(gbuFileName.c_str(), FILE_WRITE);
			if (!gbuFile)
			{
				Serial.println(F("Error opening GBU file"));
				return;
			}
			uint16_t gbuOffset = 22;
			uint16_t gbuColorDepht = 16;

			// Write the GBU file
			// Write the GBU header
			uint16_t gbuHeader[10] = {
				(uint16_t)bmpWidth,
				(uint16_t)bmpHeight,
				0,
				gbuColorDepht,
				gbuOffset, // offset to the first pixel data
				0,
				0,
				0,
				0,
				0};
			gbuFile.write(gbuHeader, sizeof(gbuHeader));

			for (int row = bmpHeight - 1; row >= 0; row--)
			{
				// Calculate the offset to the pixel data for this row
				int offset = row * bmpWidth * 3;

				// Create a buffer to hold the converted pixel data
				uint16_t *bmpData = new uint16_t[bmpWidth];

				// Seek to the pixel data for this row
				bmpFile.seek(bmpOffset + offset);

				// Convert the pixel data from 24-bit to 16-bit
				for (int i = 0; i < bmpWidth; i++) // little-endian (bgr)
				{
					uint8_t rgb[3];
					bmpFile.read(rgb, 3);
					uint8_t r = rgb[0];
					uint8_t g = rgb[1];
					uint8_t b = rgb[2];
					uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);

					// bmpData[i] = (r << 11) | (g << 5) | b;
					gbuFile.write((uint8_t *)&color, sizeof(color));
				}

				// for (int i = 0; i < bmpWidth; i++) // little-endian (bgr)
				// {
				// 	uint8_t rgb[3];
				// 	bmpFile.read(rgb, 3);

				// 	uint16_t r = rgb[0] >> 3;
				// 	uint16_t g = rgb[1] >> 2;
				// 	uint16_t b = rgb[2] >> 3;

				// 	bmpData[i] = (r << 11) | (g << 5) | b;
				// }
				// for (int i = 0; i < bmpWidth; i++) // big-endian (rgb)
				// {
				// 	uint8_t rgb[3];
				// 	bmpFile.read(rgb, 3);

				// 	uint16_t b = rgb[0] >> 3;
				// 	uint16_t g = rgb[1] >> 2;
				// 	uint16_t r = rgb[2] >> 3;

				// 	bmpData[i] = (r << 11) | (g << 5) | b;
				// }

				// Write the converted pixel data to the GBU file

				delete[] bmpData;
			}

			bmpFile.close();
			// Close the GBU file
			gbuFile.close();

			// Delete the original BMP file
			SD.remove(bmpFileName);

			// Free the memory allocated for the buffers
			Serial.println(F("Conversion done!"));
		}
	}
}
