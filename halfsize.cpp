#define _CRT_SECURE_NO_WARNINGS

#pragma warning(push)
#pragma warning(disable:4530)
#include <array>
#include <type_traits>
#include <vector>
#pragma warning(pop)

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#if ! defined(_WIN32)
#error program may not behave correctly on this platform
// for example, it assumes little-endian Byte order and `pragma pack`
#endif

namespace
{
	////////////////////////////////////////////////////////////////////////////////
	// basic types

	typedef std::uint8_t Byte;
	typedef std::uint16_t Word;

	////////////////////////////////////////////////////////////////////////////////
	// Error handling

	enum class ExitStatus
	{
		ok = 0,
		reserved1 = 1,
		reserved2 = 2,
		badArgs,
		badInputFile,
		badOutputFile,
		badInputFormat,
		unsupportedInputFormat,
		size,
	};

	char const * const errorMessages[] =
	{
		nullptr,
		nullptr,
		nullptr,
		"usage: halfsize.exe <input.tga> <output.tga>",
		"failed to open input file",
		"failed to open output file",
		"failed to read input file",
		"unsupported input format"
	};
	static_assert(std::extent<decltype(errorMessages)>::value <= int(ExitStatus::size), "too many error messages");
	static_assert(std::extent<decltype(errorMessages)>::value >= int(ExitStatus::size), "too few error messages");

	// terminate program with given exit status
	void fail(ExitStatus exitStatus)
	{
		assert(exitStatus != ExitStatus::ok);

		auto errorMessage = errorMessages[static_cast<int>(exitStatus)];
		assert(errorMessage);

		std::fputs(errorMessage, stderr);
		std::exit(static_cast<int>(exitStatus));
	}

	// fail iff condition is not satisfied
	void enforce(bool condition, ExitStatus exitStatus)
	{
		if (!condition)
		{
			fail(exitStatus);
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	// FILE helpers

	// seek inFile forward a given number of Bytes
	void skip(std::FILE * inFile, long numBytes)
	{
		auto pos = std::ftell(inFile);
		std::fseek(inFile, pos + numBytes, SEEK_SET);
	}

	template <typename T>
	void readObjects(FILE * inFile, T * objects, std::size_t numObjects)
	{
		auto readCount = std::fread(reinterpret_cast<void *>(objects), sizeof(T), numObjects, inFile);

		if (readCount != numObjects)
		{
			fail(ExitStatus::badInputFormat);
		}
	}

	template <typename T>
	T readObject(FILE * inFile)
	{
		T object;
		readObjects(inFile, &object, 1);
		return object;
	}

	template <typename T>
	void writeObjects(FILE * outFile, T const * objects, std::size_t numObjects)
	{
		auto writeCount = std::fwrite(reinterpret_cast<void const *>(objects), sizeof(T), numObjects, outFile);

		if (writeCount != numObjects)
		{
			fail(ExitStatus::badOutputFile);
		}
	}

	template <typename T>
	void writeObject(FILE * outFile, T const & object)
	{
		writeObjects(outFile, &object, 1);
	}

	////////////////////////////////////////////////////////////////////////////////
	// TGA

#pragma pack(push)
#pragma pack(1)
	struct Header
	{
		// 1) length of the image ID field
		Byte idLength;
		
		// 2) whether a color map is included
		enum class ColorMapType : Byte
		{
			none = 0
		} colorMapType;
		
		// 3) compression and color types
		enum class ImageType : Byte
		{
			uncompressedTrueColorImage = 2,
			uncompressedGrayScaleImage = 3,
		} type;

		// 4) describes the color map
		struct ColorMapSpecification
		{
			Word offset;
			Word size;
			Byte bpp;
		} colorMapSpecification;

		// 5) image dimensions and format
		struct Specification
		{
			Word xOrigin;
			Word yOrigin;
			Word width;
			Word height;
			Byte bpp;
			struct
			{
				Byte attributeBits : 4;
				Byte reserved : 1;
				Byte direction : 1;
				Byte interleave : 2;
			} descriptor;
		} specification;
	};

	static_assert(sizeof(Header::ColorMapSpecification) == 5, "Header::Specification does not match TGA format");
	static_assert(sizeof(Header::Specification) == 10, "Header::Specification does not match TGA format");
	static_assert(sizeof(Header) == 18, "Header does not match TGA format");
#pragma pack(pop)

	// represents grey-scale/true-color TGA pixel
	template <int numComponents>
	class Pixel : public std::array<Byte, numComponents> {};

	// used to accumulate Pixel values
	template <int numComponents>
	class Accumulator : public std::array<Word, numComponents> {};

	// convenient store for row of pixels which have 1-Byte color components;
	// note: these stub classes should be replaced with using directives
	template <int numComponents>
	class Row : public std::vector <Pixel<numComponents>> 
	{
	public:
		Row(std::size_t numElements) : std::vector<Pixel<numComponents>>(numElements) { }
	};

	// scrutinize header for errors and compatability with converter
	void inspect(Header header)
	{
		enforce(header.colorMapType == Header::ColorMapType::none, ExitStatus::unsupportedInputFormat);
		enforce(header.colorMapSpecification.offset == 0, ExitStatus::badInputFormat);
		enforce(header.colorMapSpecification.size == 0, ExitStatus::badInputFormat);
		enforce(header.colorMapSpecification.bpp == 0, ExitStatus::badInputFormat);
		enforce(header.specification.width > 0, ExitStatus::badInputFormat);
		enforce(header.specification.height > 0, ExitStatus::badInputFormat);
		enforce(header.specification.bpp >= 8, ExitStatus::unsupportedInputFormat);
		enforce(header.specification.bpp <= 32, ExitStatus::unsupportedInputFormat);
		enforce((header.specification.bpp % 8) == 0, ExitStatus::unsupportedInputFormat);
		enforce(header.specification.descriptor.attributeBits == 0
			|| header.specification.descriptor.attributeBits == 8, ExitStatus::unsupportedInputFormat);
		enforce(header.specification.descriptor.reserved == 0, ExitStatus::badInputFormat);
		enforce(header.specification.descriptor.interleave == 0, ExitStatus::unsupportedInputFormat);
	}

	template <int numComponents>
	void readRow(
		FILE * inFile,
		Row<numComponents> & row,
		int inWidth)
	{
		readObjects(inFile, row.data(), inWidth);

		// account for odd column by writing value twice
		row.back() = row[inWidth - 1];
	}

	template <int numComponents>
	void writeRow(
		FILE * outFile,
		Row<numComponents> const & row)
	{
		writeObjects(outFile, row.data(), row.size());
	}

	////////////////////////////////////////////////////////////////////////////////
	// conversion

	template <int numComponents>
	void convert(
		Row<numComponents> const & inRows0,
		Row<numComponents> const & inRows1,
		Row<numComponents> & outRow)
	{
		assert(inRows0.size() == inRows1.size());
		assert(inRows0.size() == outRow.size() * 2);

		auto inPixelIterator0 = std::begin(inRows0);
		auto inPixelIterator1 = std::begin(inRows1);
		auto outPixelIterator = std::begin(outRow);

		while (inPixelIterator0 != std::end(inRows0))
		{
			// accumulates component values from the four input pixels
			Accumulator<numComponents> accumulator;

			// initialize accumulator with a .5 value;
			// facilitates round-to-nearest behavior
			auto const accumulatedUnit = 4;
			auto const accumulatedHalf = accumulatedUnit / 2;
			std::fill(std::begin(accumulator), std::end(accumulator), accumulatedHalf);

			auto accumulate = [&](Pixel<numComponents> pixel)
			{
				for (auto componentIndex = 0; componentIndex != numComponents; ++componentIndex)
				{
					accumulator[componentIndex] += pixel[componentIndex];
				}
			};

			accumulate(*inPixelIterator0++);
			accumulate(*inPixelIterator0++);
			accumulate(*inPixelIterator1++);
			accumulate(*inPixelIterator1++);

			Pixel<numComponents> outPixel;
			for (auto componentIndex = 0; componentIndex != numComponents; ++componentIndex)
			{
				outPixel[componentIndex] = accumulator[componentIndex] >> 2;
			}

			*outPixelIterator++ = outPixel;
		}

		assert(inPixelIterator0 == std::end(inRows0));
		assert(inPixelIterator1 == std::end(inRows1));
		assert(outPixelIterator == std::end(outRow));
	}

	template <int numComponents>
	void convert(
		FILE * inFile,
		FILE * outFile,
		Header::Specification inSpecification,
		Header::Specification outSpecification)
	{
		typedef Row<numComponents> Row;

		auto inWidthComplete = inSpecification.width;
		auto inWidthRup = (inWidthComplete + 1) & (~1u);

		auto outColumnsRup = (inSpecification.width + 1) >> 1;
		auto outRowsComplete = inSpecification.height >> 1;

		Row inRow0(inWidthRup), inRow1(inWidthRup), outRow(outColumnsRup);
		assert((reinterpret_cast<char const *>(&inRow0.back()) - reinterpret_cast<char const *>(&inRow0.front())) == (inWidthRup - 1) * numComponents);
		assert((reinterpret_cast<char const *>(&inRow1.back()) - reinterpret_cast<char const *>(&inRow1.front())) == (inWidthRup - 1) * numComponents);
		assert((reinterpret_cast<char const *>(&outRow.back()) - reinterpret_cast<char const *>(&outRow.front())) == (inWidthRup / 2 - 1) * numComponents);

		for (auto i = outRowsComplete; i; --i)
		{
			readRow(inFile, inRow0, inSpecification.width);
			readRow(inFile, inRow1, inSpecification.width);

			convert(inRow0, inRow1, outRow);

			writeRow(outFile, outRow);
		}

		// convert outstanding odd row
		if (inSpecification.height & 1)
		{
			readRow(inFile, inRow0, inSpecification.width);

			convert(inRow0, inRow0, outRow);

			writeRow(outFile, outRow);
		}
	}

	void convert(FILE * inFile, FILE * outFile)
	{
		// read input header
		auto inHeader = readObject<Header>(inFile);
		inspect(inHeader);

		// copy header
		auto outHeader = inHeader;
		outHeader.specification.xOrigin = inHeader.specification.xOrigin >> 1;
		outHeader.specification.yOrigin = inHeader.specification.yOrigin >> 1;
		outHeader.specification.height = (inHeader.specification.height + 1) >> 1;
		outHeader.specification.width = (inHeader.specification.width + 1) >> 1;

		// write output header
		writeObject(outFile, outHeader);

		// copy ID field
		auto idLength = inHeader.idLength;
		std::array <char, UINT8_MAX> idField;
		assert(idLength <= idField.size());

		auto begin = idField.data();
		readObjects(inFile, begin, idLength);
		writeObjects(outFile, begin, idLength);

		// copy pixels
		switch (inHeader.specification.bpp)
		{
		case 8:
			enforce(inHeader.type == Header::ImageType::uncompressedGrayScaleImage, ExitStatus::unsupportedInputFormat);
			convert<1>(inFile, outFile, inHeader.specification, outHeader.specification);
			break;

		case 16:
			enforce(inHeader.type == Header::ImageType::uncompressedGrayScaleImage, ExitStatus::unsupportedInputFormat);
			convert<2>(inFile, outFile, inHeader.specification, outHeader.specification);
			break;

		case 24:
			enforce(inHeader.type == Header::ImageType::uncompressedTrueColorImage, ExitStatus::unsupportedInputFormat);
			convert<3>(inFile, outFile, inHeader.specification, outHeader.specification);
			break;

		case 32:
			enforce(inHeader.type == Header::ImageType::uncompressedTrueColorImage, ExitStatus::unsupportedInputFormat);
			convert<4>(inFile, outFile, inHeader.specification, outHeader.specification);
			break;

		default:
			fail(ExitStatus::unsupportedInputFormat);
		}

		while (!std::feof(inFile))
		{
			fputc(fgetc(inFile), outFile);
		}

		assert(std::feof(inFile));
	}

	void convert(char const * inFilename, char const * outFilename)
	{
		FILE * inFile = std::fopen(inFilename, "rb");
		if (!inFile)
		{
			fail(ExitStatus::badInputFile);
		}

		FILE * outFile = std::fopen(outFilename, "wb");
		if (!outFile)
		{
			fail(ExitStatus::badOutputFile);
		}

		convert(inFile, outFile);
	}
}

int main(int numArgs, char * args[])
{
	if (numArgs != 3)
	{
		fail(ExitStatus::badArgs);
	}

	convert(args[1], args[2]);

	return static_cast<int>(ExitStatus::ok);
}
