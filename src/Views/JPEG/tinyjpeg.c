
/*
 * Small jpeg decoder library
 *
 * Copyright (c) 2006, Luc Saillard <luc@saillard.org>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 * - Neither the name of the author nor the names of its contributors may be
 *  used to endorse or promote products derived from this software without
 *  specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "tinyjpeg.h"
#include "tinyjpeg-internal.h"


/* Global variable to return the last error found while deconding */
static char error_string[256];

static const unsigned char zigzag[64] = {
    0,  1,  5,  6, 14, 15, 27, 28,
    2,  4,  7, 13, 16, 26, 29, 42,
    3,  8, 12, 17, 25, 30, 41, 43,
    9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};


/* Set up the standard Huffman tables (cf. JPEG standard section K.3) */
/* IMPORTANT: these are only valid for 8-bit data precision! */
static const unsigned char bits_dc_luminance[17] = {
    0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0
};
static const unsigned char val_dc_luminance[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};
static const unsigned char bits_dc_chrominance[17] = {
    0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0
};
static const unsigned char val_dc_chrominance[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};
static const unsigned char bits_ac_luminance[17] = {
    0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d
};
static const unsigned char val_ac_luminance[] = {
    0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
    0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
    0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
    0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
    0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
    0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
    0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
    0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
    0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
    0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
    0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
    0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
    0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
    0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
    0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
    0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};
static const unsigned char bits_ac_chrominance[17] = {
    0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77
};
static const unsigned char val_ac_chrominance[] = {
    0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
    0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
    0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
    0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
    0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
    0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
    0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
    0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
    0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
    0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
    0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
    0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
    0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
    0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
    0xf9, 0xfa
};


/*
 * 4 functions to manage the stream
 *
 *  fill_nbits: put at least nbits in the reservoir of bits.
 *              But convert any 0xff,0x00 into 0xff
 *  get_nbits: read nbits from the stream, and put it in result,
 *             bits is removed from the stream and the reservoir is filled
 *             automaticaly. The result is signed according to the number of
 *             bits.
 *  look_nbits: read nbits from the stream without marking as read.
 *  skip_nbits: read nbits from the stream but do not return the result.
 *
 * stream: current pointer in the jpeg data (read bytes per bytes)
 * nbits_in_reservoir: number of bits filled into the reservoir
 * reservoir: register that contains bits information. Only nbits_in_reservoir
 *            is valid.
 *                          nbits_in_reservoir
 *                        <--    17 bits    -->
 *            Ex: 0000 0000 1010 0000 1111 0000   <== reservoir
 *                        ^
 *                        bit 1
 *            To get two bits from this example
 *                 result = (reservoir >> 15) & 3
 *
 */
#define fill_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted) do {
while (nbits_in_reservoir < nbits_wanted)
{
    const unsigned char c = *stream++;
    reservoir <<= 8;
    if (c == 0xff && *stream == 0x00)
        stream++;
    reservoir |= c;
    nbits_in_reservoir += 8;
}
}  while (0);


/* Signed version !!!! */
#define get_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted,result) do {
fill_nbits(reservoir, nbits_in_reservoir, stream, (nbits_wanted));
result = ((reservoir) >> (nbits_in_reservoir - (nbits_wanted)));
nbits_in_reservoir -= (nbits_wanted);
reservoir &= ((1U << nbits_in_reservoir) - 1);
if (result < (1UL << ((nbits_wanted) - 1)))
    result += (0xFFFFFFFFUL << (nbits_wanted)) + 1;
}  while (0);


#define look_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted,result) do {
fill_nbits(reservoir, nbits_in_reservoir, stream, (nbits_wanted));
result = ((reservoir) >> (nbits_in_reservoir - (nbits_wanted)));
}  while (0);


#define skip_nbits(reservoir,nbits_in_reservoir,stream,nbits_wanted) do {
fill_nbits(reservoir, nbits_in_reservoir, stream, (nbits_wanted));
nbits_in_reservoir -= (nbits_wanted);
reservoir &= ((1U << nbits_in_reservoir) - 1);
}  while (0);


#define be16_to_cpu(x) (((x)[0]<<8)|(x)[1])


/**
 * Get the next (valid) huffman code in the stream.
 *
 * To speedup the procedure, we look HUFFMAN_HASH_NBITS bits and the code is
 * lower than HUFFMAN_HASH_NBITS we have automaticaly the length of the code
 * and the value by using two lookup table.
 * Else if the value is not found, just search (linear) into an array for each
 * bits is the code is present.
 *
 * If the code is not present for any reason, -1 is return.
 */
static int get_next_huffman_code(struct jdec_private *priv, struct huffman_table *huffman_table)
{
    int value, hcode;
    unsigned int extra_nbits, nbits;
    uint16_t *slowtable;
    look_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, HUFFMAN_HASH_NBITS, hcode);
    value = huffman_table->lookup[hcode];
    if (value >= 0) {
        int code_size = huffman_table->code_size[value];
        skip_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, code_size);
        return value;
    }
    /* Decode more bits each time ... */
    for (extra_nbits = 0; extra_nbits < 16 - HUFFMAN_HASH_NBITS; extra_nbits++) {
        nbits = HUFFMAN_HASH_NBITS + 1 + extra_nbits;
        look_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, nbits, hcode);
        slowtable = huffman_table->slowtable[extra_nbits];
        /* Search if the code is in this array */
        while (slowtable[0]) {
            if (slowtable[0] == hcode) {
                skip_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, nbits);
                return slowtable[1];
            }
            slowtable += 2;
        }
    }
    return 0;
}


/**
 *
 * Decode a single block that contains the DCT coefficients.
 * The table coefficients is already dezigzaged at the end of the operation.
 *
 */
void tinyjpeg_process_Huffman_data_unit(struct jdec_private *priv, int component)
{
    unsigned char j;
    int huff_code;
    unsigned char size_val, count_0;
    struct component *c = &priv->component_infos[component];
    short int DCT[64];
    /* Initialize the DCT coef table */
    memset(DCT, 0, sizeof(DCT));
    /* DC coefficient decoding */
    huff_code = get_next_huffman_code(priv, c->DC_table);
    if (huff_code) {
        get_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, huff_code, DCT[0]);
        DCT[0] += c->previous_DC;
        c->previous_DC = DCT[0];
    } else {
        DCT[0] = c->previous_DC;
    }
    /* AC coefficient decoding */
    j = 1;
    while (j < 64) {
        huff_code = get_next_huffman_code(priv, c->AC_table);
        size_val = huff_code & 0xF;
        count_0 = huff_code >> 4;
        if (size_val == 0) {
            /* RLE */
            if (count_0 == 0)
                break;    /* EOB found, go out */
            else if (count_0 == 0xF)
                j += 16;  /* skip 16 zeros */
        } else {
            j += count_0;   /* skip count_0 zeroes */
            get_nbits(priv->reservoir, priv->nbits_in_reservoir, priv->stream, size_val, DCT[j]);
            j++;
        }
    }
    for (j = 0; j < 64; j++)
        c->DCT[j] = DCT[zigzag[j]];
}


/*
 * Takes two array of bits, and build the huffman table for size, and code
 *
 * lookup will return the symbol if the code is less or equal than HUFFMAN_HASH_NBITS.
 * code_size will be used to known how many bits this symbol is encoded.
 * slowtable will be used when the first lookup didn't give the result.
 */
static void build_huffman_table(const unsigned char *bits, const unsigned char *vals, struct huffman_table *table)
{
    unsigned int i, j, code, code_size, val, nbits;
    unsigned char huffsize[257], *hz;
    unsigned int huffcode[257], *hc;
    int next_free_entry;
    /*
     * Build a temp array
     *   huffsize[X] => numbers of bits to write vals[X]
     */
    hz = huffsize;
    for (i = 1; i <= 16; i++) {
        for (j = 1; j <= bits[i]; j++)
            *hz++ = i;
    }
    *hz = 0;
    memset(table->lookup, 0xff, sizeof(table->lookup));
    for (i = 0; i < (16 - HUFFMAN_HASH_NBITS); i++)
        table->slowtable[i][0] = 0;
    /* Build a temp array
     *   huffcode[X] => code used to write vals[X]
     */
    code = 0;
    hc = huffcode;
    hz = huffsize;
    nbits = *hz;
    while (*hz) {
        while (*hz == nbits) {
            *hc++ = code++;
            hz++;
        }
        code <<= 1;
        nbits++;
    }
    /*
     * Build the lookup table, and the slowtable if needed.
     */
    next_free_entry = -1;
    for (i = 0; huffsize[i]; i++) {
        val = vals[i];
        code = huffcode[i];
        code_size = huffsize[i];
        trace("val=%2.2x code=%8.8x codesize=%2.2dn", i, code, code_size);
        table->code_size[val] = code_size;
        if (code_size <= HUFFMAN_HASH_NBITS) {
            /*
             * Good: val can be put in the lookup table, so fill all value of this
             * column with value val
             */
            int repeat = 1UL << (HUFFMAN_HASH_NBITS - code_size);
            code <<= HUFFMAN_HASH_NBITS - code_size;
            while ( repeat-- )
                table->lookup[code++] = val;
        } else {
            /* Perhaps sorting the array will be an optimization */
            uint16_t *slowtable = table->slowtable[code_size - HUFFMAN_HASH_NBITS - 1];
            while (slowtable[0])
                slowtable += 2;
            slowtable[0] = code;
            slowtable[1] = val;
            slowtable[2] = 0;
            /* TODO: NEED TO CHECK FOR AN OVERFLOW OF THE TABLE */
        }
    }
}


static void build_default_huffman_tables(struct jdec_private *priv)
{
    if (   (priv->flags & TINYJPEG_FLAGS_MJPEG_TABLE)
           && priv->default_huffman_table_initialized)
        return;
    build_huffman_table(bits_dc_luminance, val_dc_luminance, &priv->HTDC[0]);
    build_huffman_table(bits_ac_luminance, val_ac_luminance, &priv->HTAC[0]);
    build_huffman_table(bits_dc_chrominance, val_dc_chrominance, &priv->HTDC[1]);
    build_huffman_table(bits_ac_chrominance, val_ac_chrominance, &priv->HTAC[1]);
    priv->default_huffman_table_initialized = 1;
}


/*******************************************************************************
 *
 * Colorspace conversion routine
 *
 *
 * Note:
 * YCbCr is defined per CCIR 601-1, except that Cb and Cr are
 * normalized to the range 0..MAXJSAMPLE rather than -0.5 .. 0.5.
 * The conversion equations to be implemented are therefore
 *      R = Y                + 1.40200 * Cr
 *      G = Y - 0.34414 * Cb - 0.71414 * Cr
 *      B = Y + 1.77200 * Cb
 *
 ******************************************************************************/
static void print_SOF(const unsigned char *stream)
{
    int width, height, nr_components, precision;
#if DEBUG
    const char *nr_components_to_string[] = {
        "????",
        "Grayscale",
        "????",
        "YCbCr",
        "CYMK"
    };
#endif
    precision = stream[2];
    height = be16_to_cpu(stream + 3);
    width  = be16_to_cpu(stream + 5);
    nr_components = stream[7];
    trace("> SOF markern");
    trace("Size:%dx%d nr_components:%d (%s)  precision:%dn",
          width, height,
          nr_components, nr_components_to_string[nr_components],
          precision);
}


/*******************************************************************************
 *
 * JPEG/JFIF Parsing functions
 *
 * Note: only a small subset of the jpeg file format is supported. No markers,
 * nor progressive stream is supported.
 *
 ******************************************************************************/
static void build_quantization_table(float *qtable, const unsigned char *ref_table)
{
    /* Taken from libjpeg. Copyright Independent JPEG Group's LLM idct.
     * For float AA&N IDCT method, divisors are equal to quantization
     * coefficients scaled by scalefactor[row]*scalefactor[col], where
     *   scalefactor[0] = 1
     *   scalefactor[k] = cos(k*PI/16) * sqrt(2)    for k=1..7
     * We apply a further scale factor of 8.
     * What's actually stored is 1/divisor so that the inner loop can
     * use a multiplication rather than a division.
     */
    int i, j;
    static const double aanscalefactor[8] = {
        1.0, 1.387039845, 1.306562965, 1.175875602,
        1.0, 0.785694958, 0.541196100, 0.275899379
    };
    const unsigned char *zz = zigzag;
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            *qtable++ = ref_table[*zz++] * aanscalefactor[i] * aanscalefactor[j];
        }
    }
}


static int parse_DQT(struct jdec_private *priv, const unsigned char *stream)
{
    int length, qi;
    float *table;
    trace("> DQT markern");
    length = be16_to_cpu(stream) - 2;
    stream += 2;  /* Skip length */
    while (length > 0) {
        qi = *stream++;
#if SANITY_CHECK
        if (qi >> 4)
            error("16 bits quantization table is not supportedn");
        if (qi > 4)
            error("No more 4 quantization table is supported (got %d)n", qi);
#endif
        table = priv->Q_tables[qi];
        build_quantization_table(table, stream);
        stream += 64;
        length -= 65;
    }
    return 0;
}


static int parse_SOF(struct jdec_private *priv, const unsigned char *stream)
{
    int i, width, height, nr_components, cid, sampling_factor;
    int Q_table;
    struct component *c;
    print_SOF(stream);
    height = be16_to_cpu(stream + 3);
    width  = be16_to_cpu(stream + 5);
    nr_components = stream[7];
#if SANITY_CHECK
    if (stream[2] != 8)
        error("Precision other than 8 is not supportedn");
    if (width > 2048 || height > 2048)
        error("Width and Height (%dx%d) seems suspiciousn", width, height);
    if (nr_components != 3)
        error("We only support YUV imagesn");
    if (height % 16)
        error("Height need to be a multiple of 16 (current height is %d)n", height);
    if (width % 16)
        error("Width need to be a multiple of 16 (current Width is %d)n", width);
#endif
    stream += 8;
    for (i = 0; i < nr_components; i++) {
        cid = *stream++;
        sampling_factor = *stream++;
        Q_table = *stream++;
        c = &priv->component_infos[cid];
        c->Vfactor = sampling_factor & 0xf;
        c->Hfactor = sampling_factor >> 4;
        c->Q_table = priv->Q_tables[Q_table];
        trace("Component:%d  factor:%dx%d  Quantization table:%dn",
              cid, c->Hfactor, c->Hfactor, Q_table );
    }
    priv->width = width;
    priv->height = height;
    return 0;
}
static int parse_SOS(struct jdec_private *priv, const unsigned char *stream)
{
    unsigned int i, cid, table;
    unsigned int nr_components = stream[2];
    trace("> SOS markern");
#if SANITY_CHECK
    if (nr_components != 3)
        error("We only support YCbCr imagen");
#endif
    stream += 3;
    for (i = 0; i < nr_components; i++) {
        cid = *stream++;
        table = *stream++;
#if SANITY_CHECK
        if ((table & 0xf) >= 4)
            error("We do not support more than 2 AC Huffman tablen");
        if ((table >> 4) >= 4)
            error("We do not support more than 2 DC Huffman tablen");
        trace("ComponentId:%d  tableAC:%d tableDC:%dn", cid, table & 0xf, table >> 4);
#endif
        priv->component_infos[cid].AC_table = &priv->HTAC[table & 0xf];
        priv->component_infos[cid].DC_table = &priv->HTDC[table >> 4];
    }
    priv->stream = stream + 3;
    return 0;
}


static int parse_DHT(struct jdec_private *priv, const unsigned char *stream)
{
    unsigned int count, i;
    unsigned char huff_bits[17];
    int length, index;
    length = be16_to_cpu(stream) - 2;
    stream += 2;  /* Skip length */
    trace("> DHT marker (length=%d)n", length);
    while (length > 0) {
        index = *stream++;
        /* We need to calculate the number of bytes 'vals' will takes */
        huff_bits[0] = 0;
        count = 0;
        for (i = 1; i < 17; i++) {
            huff_bits[i] = *stream++;
            count += huff_bits[i];
        }
#if SANITY_CHECK
        if (count > 1024)
            error("No more than 1024 bytes is allowed to describe a huffman table");
        if ( (index & 0xf) >= HUFFMAN_TABLES)
            error("No mode than %d Huffman tables is supportedn", HUFFMAN_TABLES);
        trace("Huffman table %s n%dn", (index & 0xf0) ? "AC" : "DC", index & 0xf);
        trace("Length of the table: %dn", count);
#endif
        if (index & 0xf0 )
            build_huffman_table(huff_bits, stream, &priv->HTAC[index & 0xf]);
        else
            build_huffman_table(huff_bits, stream, &priv->HTDC[index & 0xf]);
        length -= 1;
        length -= 16;
        length -= count;
    }
    trace("< DHT markern");
    return 0;
}
static void resync(struct jdec_private *priv)
{
    int i;
    /* Init DC coefficients */
    for (i = 0; i < COMPONENTS; i++)
        priv->component_infos[i].previous_DC = 0;
    priv->reservoir = 0;
    priv->nbits_in_reservoir = 0;
}
static int parse_JFIF(struct jdec_private *priv, const unsigned char *stream)
{
    int chuck_len;
    int marker;
    int sos_marker_found = 0;
    int dht_marker_found = 0;
    const unsigned char *next_chunck;
    /* Parse marker */
    while (!sos_marker_found) {
        if (*stream++ != 0xff)
            goto bogus_jpeg_format;
        /* Skip any padding ff byte (this is normal) */
        while (*stream == 0xff)
            stream++;
        marker = *stream++;
        chuck_len = be16_to_cpu(stream);
        next_chunck = stream + chuck_len;
        switch (marker) {
        case SOF:
            if (parse_SOF(priv, stream) < 0)
                return -1;
            break;
        case DQT:
            if (parse_DQT(priv, stream) < 0)
                return -1;
            break;
        case SOS:
            if (parse_SOS(priv, stream) < 0)
                return -1;
            sos_marker_found = 1;
            break;
        case DHT:
            if (parse_DHT(priv, stream) < 0)
                return -1;
            dht_marker_found = 1;
            break;
        default:
            trace("> Unknown marker %2.2xn", marker);
            break;
        }
        stream = next_chunck;
    }
    if (!dht_marker_found) {
        trace("No Huffman table loaded, using the default onen");
        build_default_huffman_tables(priv);
    }
#ifdef SANITY_CHECK
    if (   (priv->component_infos[cY].Hfactor < priv->component_infos[cCb].Hfactor)
           || (priv->component_infos[cY].Hfactor < priv->component_infos[cCr].Hfactor))
        error("Horizontal sampling factor for Y should be greater than horitontal sampling factor for Cb or Crn");
    if (   (priv->component_infos[cY].Vfactor < priv->component_infos[cCb].Vfactor)
           || (priv->component_infos[cY].Vfactor < priv->component_infos[cCr].Vfactor))
        error("Vertical sampling factor for Y should be greater than vertical sampling factor for Cb or Crn");
    if (   (priv->component_infos[cCb].Hfactor != 1)
           || (priv->component_infos[cCr].Hfactor != 1)
           || (priv->component_infos[cCb].Vfactor != 1)
           || (priv->component_infos[cCr].Vfactor != 1))
        error("Sampling other than 1x1 for Cr and Cb is not supported");
#endif
    return 0;
bogus_jpeg_format:
    trace("Bogus jpeg formatn");
    return -1;
}
/*******************************************************************************
 *
 * Functions exported of the library.
 *
 * Note: Some applications can access directly to internal pointer of the
 * structure. It's is not recommended, but if you have many images to
 * uncompress with the same parameters, some functions can be called to speedup
 * the decoding.
 *
 ******************************************************************************/
/**
 * Allocate a new tinyjpeg decoder object.
 *
 * Before calling any other functions, an object need to be called.
 */
struct jdec_private *tinyjpeg_init(void)
{
    struct jdec_private *priv;
    priv = (struct jdec_private *)calloc(1, sizeof(struct jdec_private));
    if (priv == NULL)
        return NULL;
    return priv;
}
/**
 * Free a tinyjpeg object.
 *
 * No others function can be called after this one.
 */
void tinyjpeg_free(struct jdec_private *priv)
{
    int i;
    for (i = 0; i < COMPONENTS; i++) {
        if (priv->components[i])
            free(priv->components[i]);
        priv->components[i] = NULL;
    }
    free(priv);
}
/**
 * Initialize the tinyjpeg object and prepare the decoding of the stream.
 *
 * Check if the jpeg can be decoded with this jpeg decoder.
 * Fill some table used for preprocessing.
 */
int tinyjpeg_parse_header(struct jdec_private *priv, const unsigned char *buf, unsigned int size)
{
    int ret;
    /* Identify the file */
    if ((buf[0] != 0xFF) || (buf[1] != SOI))
        error("Not a JPG file ?n");
    priv->stream_begin = buf + 2;
    priv->stream_length = size - 2;
    ret = parse_JFIF(priv, priv->stream_begin);
    return ret;
}
/**
 * Decode and convert the jpeg image into @pixfmt@ image
 *
 * Note: components will be automaticaly allocated if no memory is attached.
 */
int tinyjpeg_decode(struct jdec_private *priv,
                    const struct tinyjpeg_colorspace *pixfmt)
{
    unsigned int x, y, xstride_by_mcu, ystride_by_mcu;
    unsigned int bytes_per_blocklines[3], bytes_per_mcu[3];
    decode_MCU_fct decode_MCU;
    const decode_MCU_fct *decode_mcu_table;
    convert_colorspace_fct convert_to_pixfmt;
    decode_mcu_table = pixfmt->decode_mcu_table;
    /* Fix: check return value */
    pixfmt->initialize(priv, bytes_per_blocklines, bytes_per_mcu);
    xstride_by_mcu = ystride_by_mcu = 8;
    if ((priv->component_infos[cY].Hfactor | priv->component_infos[cY].Vfactor) == 1) {
        decode_MCU = decode_mcu_table[0];
        convert_to_pixfmt = pixfmt->convert_colorspace[0];
        trace("Use decode 1x1 samplingn");
    } else if (priv->component_infos[cY].Hfactor == 1) {
        decode_MCU = decode_mcu_table[1];
        convert_to_pixfmt = pixfmt->convert_colorspace[1];
        ystride_by_mcu = 16;
        trace("Use decode 1x2 sampling (not supported)n");
    } else if (priv->component_infos[cY].Vfactor == 2) {
        decode_MCU = decode_mcu_table[3];
        convert_to_pixfmt = pixfmt->convert_colorspace[3];
        xstride_by_mcu = 16;
        ystride_by_mcu = 16;
        trace("Use decode 2x2 samplingn");
    } else {
        decode_MCU = decode_mcu_table[2];
        convert_to_pixfmt = pixfmt->convert_colorspace[2];
        xstride_by_mcu = 16;
        trace("Use decode 2x1 samplingn");
    }
    resync(priv);
    /* Don't forget to that block can be either 8 or 16 lines */
    bytes_per_blocklines[0] *= ystride_by_mcu;
    bytes_per_blocklines[1] *= ystride_by_mcu;
    bytes_per_blocklines[2] *= ystride_by_mcu;
    bytes_per_mcu[0] *= xstride_by_mcu / 8;
    bytes_per_mcu[1] *= xstride_by_mcu / 8;
    bytes_per_mcu[2] *= xstride_by_mcu / 8;
    /* Just the decode the image by macroblock (size is 8x8, 8x16, or 16x16) */
    for (y = 0; y < priv->height / ystride_by_mcu; y++) {
        //trace("Decoding row %dn", y);
        priv->plane[0] = priv->components[0] + (y * bytes_per_blocklines[0]);
        priv->plane[1] = priv->components[1] + (y * bytes_per_blocklines[1]);
        priv->plane[2] = priv->components[2] + (y * bytes_per_blocklines[2]);
        for (x = 0; x < priv->width; x += xstride_by_mcu) {
            decode_MCU(priv);
            convert_to_pixfmt(priv);
            priv->plane[0] += bytes_per_mcu[0];
            priv->plane[1] += bytes_per_mcu[1];
            priv->plane[2] += bytes_per_mcu[2];
        }
    }
    return 0;
}
const char *tinyjpeg_get_errorstring(struct jdec_private *priv)
{
    return error_string;
}
void tinyjpeg_get_size(struct jdec_private *priv, unsigned int *width, unsigned int *height)
{
    *width = priv->width;
    *height = priv->height;
}
int tinyjpeg_get_components(struct jdec_private *priv, unsigned char **components, unsigned int ncomponents)
{
    int i;
    if (ncomponents > COMPONENTS)
        ncomponents = COMPONENTS;
    for (i = 0; i < ncomponents; i++)
        components[i] = priv->components[i];
    return 0;
}
int tinyjpeg_set_components(struct jdec_private *priv, unsigned char *const *components, unsigned int ncomponents)
{
    int i;
    if (ncomponents > COMPONENTS)
        ncomponents = COMPONENTS;
    for (i = 0; i < ncomponents; i++)
        priv->components[i] = components[i];
    return 0;
}
int tinyjpeg_get_bytes_per_row(struct jdec_private *priv,
                               unsigned int *bytes,
                               unsigned int ncomponents)
{
    int i;
    if (ncomponents > COMPONENTS)
        ncomponents = COMPONENTS;
    for (i = 0; i < ncomponents; i++)
        bytes[i] = priv->bytes_per_row[i];
    return 0;
}
int tinyjpeg_set_bytes_per_row(struct jdec_private *priv,
                               const unsigned int *bytes,
                               unsigned int ncomponents)
{
    int i;
    if (ncomponents > COMPONENTS)
        ncomponents = COMPONENTS;
    for (i = 0; i < ncomponents; i++)
        priv->bytes_per_row[i] = bytes[i];
    return 0;
}
int tinyjpeg_set_flags(struct jdec_private *priv, int flags)
{
    int oldflags = priv->flags;
    priv->flags = flags;
    return oldflags;
}

