/*
  Copyright 1992, 1993, 1994 by Jutta Degener and Carsten Bormann,
  Technische Universitaet Berlin

  Any use of this software is permitted provided that this notice is not
  removed and that neither the authors nor the Technische Universitaet Berlin
  are deemed to have made any representations as to the suitability of this
  software for any purpose nor are held responsible for any defects of
  this software.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.

  As a matter of courtesy, the authors request to be informed about uses
  this software has found, about bugs in this software, and about any
  improvements that may be of general interest.

  Berlin, 28.11.1994
  Jutta Degener
  Carsten Bormann


   Code modified by Jean-Marc Valin

   Speex License:

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   
   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   
   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
   
   - Neither the name of the Xiph.org Foundation nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.
   
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/



/* LPC analysis
 *
 * The next two functions calculate linear prediction coefficients
 * and/or the related reflection coefficients from the first P_MAX+1
 * values of the autocorrelation function.
 */

/* Invented by N. Levinson in 1947, modified by J. Durbin in 1959.
 */

#include "lpc.h"

#ifdef FIXED_POINT
#include <math.h>

/* returns minimum mean square error    */
spx_word32_t _spx_lpc(
spx_coef_t       *lpc, /* out: [0...p-1] LPC coefficients      */
const spx_word16_t *ac,  /* in:  [0...p] autocorrelation values  */
int          p
)
{
   int i, j;  
   spx_word16_t r;
   spx_word16_t error = ac[0];
   spx_word16_t lpcq[10];

   if (ac[0] == 0)
   {
      for (i = 0; i < p; i++)
         lpc[i] = 0;
      return 0;
   }

   for (i = 0; i < p; i++) {

      /* Sum up this iteration's reflection coefficient */
      int rr = -ac[i + 1]<<13;
      for (j = 0; j < i; j++) 
         rr -= lpcq[j] * ac[i - j];
      r = DIV32_16(rr,error+16);

      /*  Update LPC coefficients and total error */
      lpcq[i] = r;
      for (j = 0; j < i>>1; j++) 
      {
         spx_word16_t tmp  = lpcq[j];
         lpcq[j]     += MULT16_16_Q13(r,lpcq[i-1-j]);
         lpcq[i-1-j] += MULT16_16_Q13(r,tmp);
      }
      if (i & 1) 
         lpcq[j] += MULT16_16_Q13(lpcq[j],r);

      error -= MULT16_16_Q13(r,MULT16_16_Q13(error,r));
   }
   for (i = 0; i < p; i++)
      lpc[i] = lpcq[i]/8192.;
   return error;
}


/* Compute the autocorrelation
 *                      ,--,
 *              ac(i) = >  x(n) * x(n-i)  for all n
 *                      `--'
 * for lags between 0 and lag-1, and x == 0 outside 0...n-1
 */
#include <stdio.h>
void _spx_autocorr(
const float *x,   /*  in: [0...n-1] samples x   */
spx_word16_t       *ac,  /* out: [0...lag-1] ac values */
int          lag, 
int          n
)
{
   spx_word32_t d;
   int i, j;
   spx_word32_t ac0=1;
   int shift, ac_shift;
   
   for (j=0;j<n;j++)
      ac0 += floor(x[j]*x[j])/256;
   ac0 += n;
   shift = 8;
   while (shift && ac0<0x40000000)
   {
      shift--;
      ac0 <<= 1;
   }
   ac_shift = 18;
   while (ac_shift && ac0<0x40000000)
   {
      ac_shift--;
      ac0 <<= 1;
   }
   
   
   for (i=0;i<lag;i++)
   {
      d=0;
      for (j=i;j<n;j++)
      {
         d += ((int)(floor(x[j]) * floor(x[j-i]))) >> shift;
      }
      
      ac[i] = d >> ac_shift;
   }
   /*ac[0] = 8192.;*/
   /*printf ("%d %d %d, %f\n", ac0, shift, ac_shift, ac[0]);*/
}


#else



/* returns minimum mean square error    */
spx_word32_t _spx_lpc(
spx_coef_t       *lpc, /* out: [0...p-1] LPC coefficients      */
const float *ac,  /* in:  [0...p] autocorrelation values  */
int          p
)
{
   int i, j;  float r, error = ac[0];

   if (ac[0] == 0)
   {
      for (i = 0; i < p; i++)
         lpc[i] = 0;
      return 0;
   }

   for (i = 0; i < p; i++) {

      /* Sum up this iteration's reflection coefficient */
      r = -ac[i + 1];
      for (j = 0; j < i; j++) 
         r -= lpc[j] * ac[i - j];
      r /= error;

      /*  Update LPC coefficients and total error */
      lpc[i] = r;
      for (j = 0; j < i/2; j++) 
      {
         float tmp  = lpc[j];
         lpc[j]     += r * lpc[i-1-j];
         lpc[i-1-j] += r * tmp;
      }
      if (i & 1) 
         lpc[j] += lpc[j] * r;

      error *= 1.0 - r * r;
   }
   return error;
}


/* Compute the autocorrelation
 *                      ,--,
 *              ac(i) = >  x(n) * x(n-i)  for all n
 *                      `--'
 * for lags between 0 and lag-1, and x == 0 outside 0...n-1
 */
void _spx_autocorr(
const float *x,   /*  in: [0...n-1] samples x   */
float       *ac,  /* out: [0...lag-1] ac values */
int          lag, 
int          n
)
{
   float d;
   int i;
   while (lag--) 
   {
      for (i = lag, d = 0; i < n; i++) 
         d += x[i] * x[i-lag];
      ac[lag] = d;
   }
   ac[0] += 10;
}

#endif


