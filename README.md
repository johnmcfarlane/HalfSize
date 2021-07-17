# (Redacted) Programming Assignment

Author: John McFarlane
(`john at mcfarlane.name`)

## General Approach

- The input image is broken into 2x2 pixel squares.
- Each color component of each square is averaged, rounded up or down and written as the output pixel.
- Any remaining odd rows or columns are repated to make up the pair.

## Resources Used

- [Visual Studio Express 2012](http://www.microsoft.com/en-us/download/details.aspx?id=34673)
- [Wikipedia TGA Article](http://en.wikipedia.org/wiki/Truevision_TGA)
- [Creating TGA Image files](http://www.paulbourke.net/dataformats/tga/)
- [C++ Reference](http://cppreference.com/)
- [IrfanView](http://www.irfanview.com/main_download_engl.htm)
- [PearlMountain Image Converter](http://www.pearlmountainsoft.com/pearlmountain-image-converter/index.html)

## Algorithm

The program can be broken down roughly as follows:

- Successfully open input and output files
- Read and parse header from input file
- Generate and write header to output file
- Allocate enough memory to store two input rows and one output row
- For each pair of input rows:
  - For each row in the pair:
    - Read the row from file
    - If there are an odd number of columns:
      - Push a copy of the last pixel to the end
  - For each pair of columns in the input rows:
    - For each color component [RGB|I][A]:
      - Sum the value from the four input pixels
      - Add `2` to the value
      - Shift right by `2` bits
      - Store in output row
- If there is an odd remainder row:
  - Perform the same steps as for pairs of rows (above) but use the single input row to represent two rows

## Problems Encountered

### Ambiguities in TGA File Format

TGA specification has some known prblems. As mentioned on its Wikipedia page.

Additionally, the distinction between pixel attributes and alpha channel was not entirely clear so I ignore the attribute value in the image descriptor byte.
In particular, I found examples of TGAs with 32 bits of color and either 0 or 8 attribute bits.

### Testing

Testing was a chore as no sample files were provided, Windows has poor support for TGA and there are relatively few instances of TGA files remaining in the wild.
As a result, I had to run the gauntlet of malware downloads to find tools to handle this format.

### Deviation from Specification

The problem as stated would have been prohibitively hard to implement and could easily have lead to a solution with an extremely high maximum memory requirement.
To get around this, two exceptions are made to the specification as presented in HalfSize.txt:

1. The Standard C++ Library is assumed to be an exception to the 'no library' stipulation.
2. Reading, resizing and writing of image data is not performed as three separate steps.
