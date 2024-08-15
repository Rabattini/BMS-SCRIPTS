#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define RINGBUF_SIZE 0x1000
#define INITIAL_WR 0xFEE
#define MASK 0xFFF
#define MIN_MATCH 3
#define MAX_MATCH 18

typedef struct {
    uint8_t *src;
    uint8_t *src_end;
    uint8_t *dst;
    uint8_t *dst_end;
    uint8_t ringbuf[RINGBUF_SIZE];
    uint16_t wr;
    uint8_t flagbits;
    int numbits;
} Context;

// Funções de compressão
int find_longest_match(Context *ctx, uint16_t *match_pos) {
    int longest_match = 0;
    int match_len;
    uint16_t best_match_pos = 0;

    for (int i = 1; i <= RINGBUF_SIZE; i++) {
        match_len = 0;
        while (match_len < MAX_MATCH &&
               ctx->src + match_len < ctx->src_end &&
               ctx->ringbuf[(ctx->wr - i + match_len) & MASK] == ctx->src[match_len]) {
            match_len++;
        }
        if (match_len > longest_match) {
            longest_match = match_len;
            best_match_pos = i;
            if (longest_match == MAX_MATCH) break;
        }
    }

    *match_pos = best_match_pos;
    return longest_match;
}

void compress_vramwad(Context *ctx) {
    memset(ctx->ringbuf, 0, RINGBUF_SIZE);
    ctx->wr = INITIAL_WR;
    ctx->numbits = 0;
    uint8_t *flagbits_pos = ctx->dst++;
    ctx->flagbits = 0;

    while (ctx->src < ctx->src_end) {
        uint16_t match_pos = 0;
        int match_len = find_longest_match(ctx, &match_pos);

        if (match_len >= MIN_MATCH) {
            ctx->flagbits <<= 1;
            ctx->numbits++;

            uint16_t rd = (ctx->wr - match_pos) & MASK;
            *ctx->dst++ = ((rd >> 4) & 0xF0) | (match_len - MIN_MATCH);
            *ctx->dst++ = rd & 0xFF;

            for (int i = 0; i < match_len; i++) {
                ctx->ringbuf[ctx->wr & MASK] = *ctx->src++;
                ctx->wr++;
            }
        } else {
            ctx->flagbits <<= 1;
            ctx->flagbits |= 1;
            ctx->numbits++;

            uint8_t data = *ctx->src++;
            *ctx->dst++ = data;
            ctx->ringbuf[ctx->wr & MASK] = data;
            ctx->wr++;
        }

        if (ctx->numbits == 8) {
            *flagbits_pos = ctx->flagbits;
            flagbits_pos = ctx->dst++;
            ctx->flagbits = 0;
            ctx->numbits = 0;
        }
    }

    if (ctx->numbits > 0) {
        ctx->flagbits <<= (8 - ctx->numbits);
        *flagbits_pos = ctx->flagbits;
    }
}

// Funções de descompressão
void initialize_ringbuf(uint8_t *ringbuf) {
    memset(ringbuf, 0, RINGBUF_SIZE);
}

void decompress_vramwad(Context *ctx) {
    initialize_ringbuf(ctx->ringbuf);
    ctx->wr = INITIAL_WR;
    ctx->numbits = 0;

    while (ctx->dst < ctx->dst_end) {
        if (ctx->numbits == 0) {
            ctx->flagbits = *ctx->src++;
            ctx->numbits = 8;
        }
        ctx->numbits--;

        if (ctx->flagbits & 0x80) {
            uint8_t data = *ctx->src++;
            *ctx->dst++ = data;
            ctx->ringbuf[ctx->wr & MASK] = data;
            ctx->wr++;
        } else {
            uint16_t rd = ctx->src[1] + ((*ctx->src & 0xF0) << 4);
            uint8_t len = (*ctx->src & 0x0F) + 3;
            ctx->src += 2;

            for (int i = 0; i < len; i++) {
                uint8_t data = ctx->ringbuf[rd & MASK];
                *ctx->dst++ = data;
                ctx->ringbuf[ctx->wr & MASK] = data;
                ctx->wr++;
                rd++;
            }
        }

        ctx->flagbits <<= 1;
    }
}

// Função principal
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "LZ5 - Variant-DeCompressor made by Rabatini (Luke)\nUsage: %s -c|-d <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    FILE *infile = fopen(argv[2], "rb");
    if (!infile) {
        perror("Error opening input file");
        return 1;
    }

    fseek(infile, 0, SEEK_END);
    long input_size = ftell(infile);
    fseek(infile, 0, SEEK_SET);

    uint8_t *input_data = malloc(input_size);
    if (!input_data) {
        perror("Memory allocation failed");
        fclose(infile);
        return 1;
    }

    if (fread(input_data, 1, input_size, infile) != input_size) {
        perror("Error reading input file");
        free(input_data);
        fclose(infile);
        return 1;
    }
    fclose(infile);

    if (strcmp(argv[1], "-c") == 0) {
        uint8_t *compressed_data = malloc(input_size * 2); // Aloca mais espaço para o pior caso
        if (!compressed_data) {
            perror("Memory allocation failed");
            free(input_data);
            return 1;
        }

        Context ctx = {
            .src = input_data,
            .src_end = input_data + input_size,
            .dst = compressed_data
        };

        compress_vramwad(&ctx);

        FILE *outfile = fopen(argv[3], "wb");
        if (!outfile) {
            perror("Error opening output file");
            free(input_data);
            free(compressed_data);
            return 1;
        }

        if (fwrite("VRAM-WAD", 1, 8, outfile) != 8) {
            perror("Error writing to output file");
            free(input_data);
            free(compressed_data);
            fclose(outfile);
            return 1;
        }

        uint32_t compressed_size = ctx.dst - compressed_data;
        if (fwrite(&compressed_size, 4, 1, outfile) != 1 || fwrite(&input_size, 4, 1, outfile) != 1 || fwrite(compressed_data, 1, compressed_size, outfile) != compressed_size) {
            perror("Error writing to output file");
            free(input_data);
            free(compressed_data);
            fclose(outfile);
            return 1;
        }
        fclose(outfile);

        free(input_data);
        free(compressed_data);

        printf("Compression complete. Output written to %s\n", argv[3]);
    } else if (strcmp(argv[1], "-d") == 0) {
        if (input_size < 16 || strncmp((char*)input_data, "VRAM-WAD", 8) != 0) {
            fprintf(stderr, "Invalid or corrupted input file\n");
            free(input_data);
            return 1;
        }

        uint32_t compressed_size = *(uint32_t*)(input_data + 8);
        uint32_t decompressed_size = *(uint32_t*)(input_data + 12);

        if (compressed_size + 16 != input_size) {
            fprintf(stderr, "Corrupted input file\n");
            free(input_data);
            return 1;
        }

        uint8_t *decompressed_data = malloc(decompressed_size);
        if (!decompressed_data) {
            perror("Memory allocation failed");
            free(input_data);
            return 1;
        }

        Context ctx = {
            .src = input_data + 16,
            .dst = decompressed_data,
            .dst_end = decompressed_data + decompressed_size
        };

        decompress_vramwad(&ctx);

        FILE *outfile = fopen(argv[3], "wb");
        if (!outfile) {
            perror("Error opening output file");
            free(input_data);
            free(decompressed_data);
            return 1;
        }

        fwrite(decompressed_data, 1, decompressed_size, outfile);
        fclose(outfile);

        free(input_data);
        free(decompressed_data);

        printf("Decompression complete. Output written to %s\n", argv[3]);
    } else {
        fprintf(stderr, "Invalid option. Use -c to compress or -d to decompress.\n");
        free(input_data);
        return 1;
    }

    return 0;
}
