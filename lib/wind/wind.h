/*
 * Copyright (c) 2004 Kungliga Tekniska H�gskolan
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

/* $Id$ */

#ifndef _WIND_H_
#define _WIND_H_

#include <stddef.h>
#include <stdint.h>

typedef unsigned int wind_profile_flags;

#define WIND_PROFILE_NAME 1
#define WIND_PROFILE_LDAP 2
#define WIND_PROFILE_SASL 4

int wind_stringprep(const unsigned *in, size_t in_len,
		    unsigned *out, size_t *out_len,
		    wind_profile_flags flags);
int wind_profile(const char *, wind_profile_flags *);

int wind_punycode_toascii(const uint32_t *in, size_t in_len,
			  char *out, size_t *out_len);

int wind_utf8ucs4(const char *, uint32_t *, size_t *);
int wind_utf8ucs4_length(const char *, size_t *);

int wind_ucs4utf8(const uint32_t *, size_t, char *, size_t *);
int wind_ucs4utf8_length(const uint32_t *, size_t, size_t *);

int wind_ucs2utf8(const uint16_t *, size_t, char *, size_t *);
int wind_ucs2utf8_length(const uint16_t *, size_t, size_t *);

#endif /* _WIND_H_ */
