#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SMALLEST_LEN   2           /* smallest len usable at all */
#define MIN_LEN        3           /* smallest short length encodable */
#define MAX_LEN        (MIN_LEN + 6 - 2)   /* largest short length encodable */
#define MIN_SUPERLEN   (MIN_LEN + 6 - 1)   /* shortest long length encodable */
#define MAX_SUPERLEN   (MIN_SUPERLEN + 254) /* largest long length encodable */
#define SUPERLEN_CODE  (MIN_SUPERLEN - MIN_LEN) /* means "read superlength" */

typedef unsigned char UBYTE;         /* just for convenience */

int LZNP_Decode(UBYTE *in, UBYTE *out)
{
    int i, j;
    unsigned int flags;                       /* now works with 16-bit ints */
    UBYTE * OriginalOut = out;

    for (flags = 0;;)
    {
        if (((flags >>= 1) & 0xff00) == 0)
        {
            flags = (*in++) | 0xff00;         /* uses higher byte cleverly */
        }                                       /* (to count eight) */

        if (!(flags & 1))
        {
            *out++ = *in++;
        }
        else
        {
            i = *in++;

            if (i >= 0x60)
            {
                i = 0x100 - i;           /* i = copy offset */
                j = SMALLEST_LEN;       /* j = copy length */
            }
            else
            {
                j = i >> 4;
                i = (i & 0x0F) << 8;
                i |= *in++;

                if (i == 0)               /* offset of zero terminates LZNP data */
                    break;

                if (j != SUPERLEN_CODE)
                    j += MIN_LEN;
                else
                    j = MIN_SUPERLEN + (*in++);
            }

            for (i = -i, j++; --j; out++)
            {
                out[0] = out[i];
            }
        }
    }

    /* Return size */
    return (out - OriginalOut);
}

void decompressFile(const char *inputFilename, const char *outputFilename)
{
    FILE *inFile = fopen(inputFilename, "rb");
    FILE *outFile = fopen(outputFilename, "wb");
    UBYTE *compressedData;
    UBYTE *decompressedData;
    unsigned int decompressedSize;
    size_t bytesRead;

    if (!inFile || !outFile)
    {
        perror("File opening failed");
        exit(EXIT_FAILURE);
    }

    /* Read and verify the magic number */
    char magicNumber[4];
    if (fread(magicNumber, sizeof(char), 4, inFile) != 4)
    {
        perror("Failed to read magic number");
        fclose(inFile);
        fclose(outFile);
        exit(EXIT_FAILURE);
    }

    if (strncmp(magicNumber, "LZNP", 4) != 0)
    {
        fprintf(stderr, "Invalid magic number\n");
        fclose(inFile);
        fclose(outFile);
        exit(EXIT_FAILURE);
    }

    /* Read the decompressed size (4 bytes) */
    unsigned char sizeBytes[4];
    if (fread(sizeBytes, 1, sizeof(sizeBytes), inFile) != sizeof(sizeBytes))
    {
        perror("Failed to read decompressed size");
        fclose(inFile);
        fclose(outFile);
        exit(EXIT_FAILURE);
    }

    decompressedSize = (sizeBytes[0] << 24) |
                        (sizeBytes[1] << 16) |
                        (sizeBytes[2] << 8) |
                        (sizeBytes[3]);

    /* Ensure the size is not too large */
    if (decompressedSize > 0x7FFFFFFF) {
        fprintf(stderr, "Decompressed size too large\n");
        fclose(inFile);
        fclose(outFile);
        exit(EXIT_FAILURE);
    }
    printf("Decompressed size: %u bytes\n", decompressedSize);

    /* Allocate buffer for compressed data */
    size_t initialBufferSize = 4096;
    compressedData = (UBYTE *)malloc(initialBufferSize);
    if (!compressedData)
    {
        perror("Memory allocation failed");
        fclose(inFile);
        fclose(outFile);
        exit(EXIT_FAILURE);
    }

    /* Read the compressed data */
    size_t totalBytesRead = 0;
    while ((bytesRead = fread(compressedData + totalBytesRead, 1, initialBufferSize - totalBytesRead, inFile)) > 0)
    {
        totalBytesRead += bytesRead;
        if (totalBytesRead == initialBufferSize)
        {
            initialBufferSize *= 2;
            compressedData = (UBYTE *)realloc(compressedData, initialBufferSize);
            if (!compressedData)
            {
                perror("Memory reallocation failed");
                fclose(inFile);
                fclose(outFile);
                exit(EXIT_FAILURE);
            }
        }
    }

    if (ferror(inFile))
    {
        perror("Failed to read compressed data");
        free(compressedData);
        fclose(inFile);
        fclose(outFile);
        exit(EXIT_FAILURE);
    }

    /* Allocate buffer for decompressed data */
    decompressedData = (UBYTE *)malloc(decompressedSize);
    if (!decompressedData)
    {
        perror("Memory allocation failed");
        free(compressedData);
        fclose(inFile);
        fclose(outFile);
        exit(EXIT_FAILURE);
    }

    /* Decompress */
    if (LZNP_Decode(compressedData, decompressedData) != decompressedSize)
    {
        fprintf(stderr, "Decompression failed\n");
        free(compressedData);
        free(decompressedData);
        fclose(inFile);
        fclose(outFile);
        exit(EXIT_FAILURE);
    }

    /* Write decompressed data to output file */
    if (fwrite(decompressedData, 1, decompressedSize, outFile) != decompressedSize)
    {
        perror("Failed to write decompressed data");
    }

    /* Clean up */
    free(compressedData);
    free(decompressedData);
    fclose(inFile);
    fclose(outFile);
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    decompressFile(argv[1], argv[2]);

    return EXIT_SUCCESS;
}
