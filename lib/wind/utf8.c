/*
 * Copyright (c) 2004, 2006, 2007, 2008 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "windlocl.h"

RCSID("$Id$");

/**
 * Convert an UTF-8 string to an UCS4 string.
 *
 * @param in an UTF-8 string to convert.
 * @param out the resulting UCS4 strint, must be at least
 * wind_utf8ucs4_length() long.  If out is NULL, the function will
 * calculate the needed space for the out variable (just like
 * wind_utf8ucs4_length()).
 * @param out_len before processing out_len should be the length of
 * the out variable, after processing it will be the length of the out
 * string.
 * 
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_utf8ucs4(const char *in, uint32_t *out, size_t *out_len)
{
    const unsigned char *p;
    size_t o = 0;

    for (p = (const unsigned char *)in; *p != '\0'; ++p) {
	unsigned c = *p;
	uint32_t u;

	if (c & 0x80) {
	    if ((c & 0xE0) == 0xC0) {
		const unsigned c2 = *++p;
		if ((c2 & 0xC0) == 0x80) {
		    u =  ((c  & 0x1F) << 6)
			| (c2 & 0x3F);
		} else {
		    return -1;
		}
	    } else if ((c & 0xF0) == 0xE0) {
		const unsigned c2 = *++p;
		if ((c2 & 0xC0) == 0x80) {
		    const unsigned c3 = *++p;
		    if ((c3 & 0xC0) == 0x80) {
			u =   ((c  & 0x0F) << 12)
			    | ((c2 & 0x3F) << 6)
			    |  (c3 & 0x3F);
		    } else {
			return -1;
		    }
		} else {
		    return -1;
		}
	    } else if ((c & 0xF8) == 0xF0) {
		const unsigned c2 = *++p;
		if ((c2 & 0xC0) == 0x80) {
		    const unsigned c3 = *++p;
		    if ((c3 & 0xC0) == 0x80) {
			const unsigned c4 = *++p;
			if ((c4 & 0xC0) == 0x80) {
			    u =   ((c  & 0x07) << 18)
				| ((c2 & 0x3F) << 12)
				| ((c3 & 0x3F) <<  6)
				|  (c4 & 0x3F);
			} else {
			    return -1;
			}
		    } else {
			return -1;
		    }
		} else {
		    return -1;
		}
	    } else {
		return -1;
	    }
	} else {
	    u = c;
	}
	if (out) {
	    if (o >= *out_len)
		return -1;
	    out[o++] = u;
	}
    }
    *out_len = o;
    return 0;
}

/**
 * Calculate the length of from converting a UTF-8 string to a UCS4
 * string.
 *
 * @param in an UTF-8 string to convert.
 * @param out_len the length of the resulting UCS4 string.
 * 
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_utf8ucs4_length(const char *in, size_t *out_len)
{
    return wind_utf8ucs4(in, NULL, out_len);
}

static const char first_char[4] = 
    { 0x00, 0xC0, 0xE0, 0xF0 };

/**
 * Convert an UCS4 string to a UTF-8 string.
 *
 * @param in an UCS4 string to convert.
 * @param in_len the length input array.
 * @param out the resulting UTF-8 strint, must be at least
 * wind_ucs4utf8_length() long.  If out is NULL, the function will
 * calculate the needed space for the out variable (just like
 * wind_ucs4utf8_length()).
 * @param out_len before processing out_len should be the length of
 * the out variable, after processing it will be the length of the out
 * string.
 * 
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_ucs4utf8(const uint32_t *in, size_t in_len, char *out, size_t *out_len)
{
    uint32_t ch;
    size_t i, len, o;

    for (o = 0, i = 0; i < in_len; i++) {
	ch = in[i];
	
	if (ch < 0x80) {
	    len = 1;
	} else if (ch < 0x800) {
	    len = 2;
	} else if (ch < 0x10000) {
	    len = 3;
	} else if (ch <= 0x10FFFF) {
	    len = 4;
	} else
	    return -1;
	
	o += len;

	if (out) {
	    if (o >= *out_len)
		return -1;

	    switch(len) {
	    case 4:
		out[3] = (ch | 0x80) & 0xbf;
		ch = ch << 6;
	    case 3:
		out[2] = (ch | 0x80) & 0xbf;
		ch = ch << 6;
	    case 2:
		out[1] = (ch | 0x80) & 0xbf;
		ch = ch << 6;
	    case 1:
		out[0] = ch | first_char[len - 1];
	    }
	}
	out += len;
    }
    if (out) {
	if (o + 1 >= *out_len)
	    return -1;
	*out = '\0';
    }
    *out_len = o;
    return 0;
}

/**
 * Calculate the length of from converting a UCS4 string to an UTF-8 string.
 *
 * @param in an UCS4 string to convert.
 * @param in_len the length of UCS4 string to convert.
 * @param out_len the length of the resulting UTF-8 string.
 * 
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_ucs4utf8_length(const uint32_t *in, size_t in_len, size_t *out_len)
{
    return wind_ucs4utf8(in, in_len, NULL, out_len);
}

/**
 * Read in an UCS2 from a buffer.
 *
 * @param ptr The input buffer to read from
 * @param len the length of the input buffer, must be an even number.
 * @param out the output UCS2, the array must be at least out/2 long.
 * 
 * @return returns 0 on success, an wind error code otherwise.
 * @ingroup wind
 */

ssize_t
_wind_ucs2read(void *ptr, size_t len, uint16_t *out)
{
    unsigned char *p = ptr;
    int little = 1;

    if (len & 1)
	return -1;
    /* check for BOM */

    while (len) {
	if (little)
	    *out = (p[1] << 8) + p[0];
	else
	    *out = (p[0] << 8) + p[1];
	out++; p += 2;
    }
    return (p - (unsigned char *)ptr) >> 1;
}

/**
 * Convert an UCS2 string to a UTF-8 string.
 *
 * @param in an UCS2 string to convert.
 * @param in_len the length of the in UCS2 string.
 * @param out the resulting UTF-8 strint, must be at least
 * wind_ucs2utf8_length() long.  If out is NULL, the function will
 * calculate the needed space for the out variable (just like
 * wind_ucs2utf8_length()).
 * @param out_len before processing out_len should be the length of
 * the out variable, after processing it will be the length of the out
 * string.
 * 
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_ucs2utf8(const uint16_t *in, size_t in_len, char *out, size_t *out_len)
{
    uint16_t ch;
    size_t i, len, o;

    for (o = 0, i = 0; i < in_len; i++) {
	ch = in[i];
	
	if (ch < 0x80) {
	    len = 1;
	} else if (ch < 0x800) {
	    len = 2;
	} else
	    len = 3;
	
	o += len;

	if (out) {
	    if (o >= *out_len)
		return -1;

	    switch(len) {
	    case 3:
		out[2] = (ch | 0x80) & 0xbf;
		ch = ch << 6;
	    case 2:
		out[1] = (ch | 0x80) & 0xbf;
		ch = ch << 6;
	    case 1:
		out[0] = ch | first_char[len - 1];
	    }
	}
	out += len;
    }
    if (out) {
	if (o + 1 >= *out_len)
	    return -1;
	*out = '\0';
    }
    *out_len = o;
    return 0;
}

/**
 * Calculate the length of from converting a UCS2 string to an UTF-8 string.
 *
 * @param in an UCS2 string to convert.
 * @param in_len an UCS2 string length to convert.
 * @param out_len the length of the resulting UTF-8 string.
 * 
 * @return returns 0 on success, an wind error code otherwise
 * @ingroup wind
 */

int
wind_ucs2utf8_length(const uint16_t *in, size_t in_len, size_t *out_len)
{
    return wind_ucs2utf8(in, in_len, NULL, out_len);
}
