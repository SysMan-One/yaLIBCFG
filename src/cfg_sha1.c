#define	__MODULE__	"CFG_SHA1"
#define	__IDENT__	"X.01-01"
#define	__REV__		"1.1.0"
#define	__FAC__		"CFG"

/*
**++
**
**  FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
**
**  MODULE:	cfg_sha1.c
**
**  ABSTRACT:	SHA-1 (FIPS 180-4), only for the name-based UUID (RFC 9562, version 5) of the
**		fingerprint of a configuration.
**
**  DESCRIPTION: SHA-1 is not used here for security: a UUID of version 5 is defined over it,
**		and the fingerprint only tells configurations apart.  StarLet has no hash of its
**		own, so the small one stands here.
**
**  AUTHOR:	StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
**
**  CREATION DATE:  22-NOV-2026
**
**  MODIFICATION HISTORY:
**
**	X.01-01		22-NOV-2026	RRL
**		First edition.
**
**--
*/

#include	<string.h>

#include	"cfg_int.h"

#define	$ROL(x, n)	( ((x) << (n)) | ((x) >> (32 - (n))) )



/*
**  DESCRIPTION: One 64-octet block into the state.
*/
static void	s_block	(
		uint32_t	*a_h,
	const	uint8_t		*a_blk
			)
{
uint32_t	l_w[80], l_a, l_b, l_c, l_d, l_e, l_f, l_k, l_t;

	for ( int i = 0; i < 16; i++ )
		l_w[i] = ((uint32_t) a_blk[i * 4] << 24) | ((uint32_t) a_blk[i * 4 + 1] << 16) | ((uint32_t) a_blk[i * 4 + 2] << 8) | a_blk[i * 4 + 3];

	for ( int i = 16; i < 80; i++ )
		l_w[i] = $ROL(l_w[i - 3] ^ l_w[i - 8] ^ l_w[i - 14] ^ l_w[i - 16], 1);

	l_a = a_h[0];
	l_b = a_h[1];
	l_c = a_h[2];
	l_d = a_h[3];
	l_e = a_h[4];

	for ( int i = 0; i < 80; i++ )
		{
		if ( i < 20 )
			{
			l_f = (l_b & l_c) | (~l_b & l_d);
			l_k = 0x5A827999U;
			}
		else if ( i < 40 )
			{
			l_f = l_b ^ l_c ^ l_d;
			l_k = 0x6ED9EBA1U;
			}
		else if ( i < 60 )
			{
			l_f = (l_b & l_c) | (l_b & l_d) | (l_c & l_d);
			l_k = 0x8F1BBCDCU;
			}
		else	{
			l_f = l_b ^ l_c ^ l_d;
			l_k = 0xCA62C1D6U;
			}

		l_t = $ROL(l_a, 5) + l_f + l_e + l_k + l_w[i];
		l_e = l_d;
		l_d = l_c;
		l_c = $ROL(l_b, 30);
		l_b = l_a;
		l_a = l_t;
		}

	a_h[0] += l_a;
	a_h[1] += l_b;
	a_h[2] += l_c;
	a_h[3] += l_d;
	a_h[4] += l_e;
}



/*
**  DESCRIPTION: Start a hash.
*/
void	cfg$$sha1_init	(
		CFG$SHA1	*a_ctx
			)
{
	memset(a_ctx, 0, sizeof(CFG$SHA1));

	a_ctx->h[0] = 0x67452301U;
	a_ctx->h[1] = 0xEFCDAB89U;
	a_ctx->h[2] = 0x98BADCFEU;
	a_ctx->h[3] = 0x10325476U;
	a_ctx->h[4] = 0xC3D2E1F0U;
}



/*
**  DESCRIPTION: Feed octets.
*/
void	cfg$$sha1_update	(
		CFG$SHA1	*a_ctx,
	const	void		*a_data,
		size_t		a_len
			)
{
const	uint8_t	*l_p = a_data;

	a_ctx->total += a_len;

	while ( a_len )
		{
		size_t	l_take = 64 - a_ctx->used;

		if ( l_take > a_len )
			l_take = a_len;

		memcpy(a_ctx->blk + a_ctx->used, l_p, l_take);
		a_ctx->used += (uint32_t) l_take;
		l_p += l_take;
		a_len -= l_take;

		if ( a_ctx->used == 64 )
			{
			s_block(a_ctx->h, a_ctx->blk);
			a_ctx->used = 0;
			}
		}
}



/*
**  DESCRIPTION: Finish a hash.
**
**  OUTPUTS:
**	a_out:	20 octets
*/
void	cfg$$sha1_final	(
		CFG$SHA1	*a_ctx,
		uint8_t		*a_out
			)
{
uint64_t	l_bits = a_ctx->total * 8;
uint8_t		l_pad = 0x80, l_zero = 0, l_len[8];

	cfg$$sha1_update(a_ctx, &l_pad, 1);

	while ( a_ctx->used != 56 )
		cfg$$sha1_update(a_ctx, &l_zero, 1);

	for ( int i = 0; i < 8; i++ )
		l_len[i] = (uint8_t) (l_bits >> (56 - i * 8));

	cfg$$sha1_update(a_ctx, l_len, 8);

	for ( int i = 0; i < 5; i++ )
		{
		a_out[i * 4] = (uint8_t) (a_ctx->h[i] >> 24);
		a_out[i * 4 + 1] = (uint8_t) (a_ctx->h[i] >> 16);
		a_out[i * 4 + 2] = (uint8_t) (a_ctx->h[i] >> 8);
		a_out[i * 4 + 3] = (uint8_t) a_ctx->h[i];
		}
}
