
/*							btdtr.c
 *
 *	Beta distribution
 *
 *
 *
 * SYNOPSIS:
 *
 * double a, b, x, y, btdtr();
 *
 * y = btdtr( a, b, x );
 *
 *
 *
 * DESCRIPTION:
 *
 * Returns the area from zero to x under the beta density
 * function:
 *
 *
 *                          x
 *            -             -
 *           | (a+b)       | |  a-1      b-1
 * P(x)  =  ----------     |   t    (1-t)    dt
 *           -     -     | |
 *          | (a) | (b)   -
 *                         0
 *
 *
 * This function is identical to the incomplete beta
 * integral function incbet(a, b, x).
 *
 * The complemented function is
 *
 * 1 - P(1-x)  =  incbet( b, a, x );
 *
 *
 * ACCURACY:
 *
 * See incbet.c.
 *
 */

/*								btdtr()	*/


/*
Cephes Math Library Release 2.8:  June, 2000
Copyright 1984, 1987, 1995, 2000 by Stephen L. Moshier
*/

#include <gretl/cephes/mconf.h>

double btdtr( double a, double b, double x )
{
    return( incbet( a, b, x ) );
}
