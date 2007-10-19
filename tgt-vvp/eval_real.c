/*
 * Copyright (c) 2003-2005 Stephen Williams (steve@icarus.com)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
#ifdef HAVE_CVS_IDENT
#ident "$Id: eval_real.c,v 1.22 2007/06/12 02:36:58 steve Exp $"
#endif

/*
 * This file includes functions for evaluating REAL expressions.
 */
# include  "vvp_priv.h"
# include  <string.h>
#ifdef HAVE_MALLOC_H
# include  <malloc.h>
#endif
# include  <stdlib.h>
# include  <math.h>
# include  <assert.h>

static unsigned long word_alloc_mask = 0x0f;

int allocate_word()
{
      int res = 4;
      int max = 8*sizeof(word_alloc_mask);

      while (res < max && (1U << res) & word_alloc_mask)
	    res += 1;

      assert(res < max);
      word_alloc_mask |= 1U << res;
      return res;
}

void clr_word(int res)
{
      int max = 8*sizeof(word_alloc_mask);
      assert(res < max);
      word_alloc_mask &= ~ (1U << res);
}


static int draw_binary_real(ivl_expr_t exp)
{
      int l, r = -1;

      l = draw_eval_real(ivl_expr_oper1(exp));
      r = draw_eval_real(ivl_expr_oper2(exp));

      switch (ivl_expr_opcode(exp)) {

	  case '+':
	    fprintf(vvp_out, "    %%add/wr %d, %d;\n", l, r);
	    break;

	  case '-':
	    fprintf(vvp_out, "    %%sub/wr %d, %d;\n", l, r);
	    break;

	  case '*':
	    fprintf(vvp_out, "    %%mul/wr %d, %d;\n", l, r);
	    break;

	  case '/':
	    fprintf(vvp_out, "    %%div/wr %d, %d;\n", l, r);
	    break;

	  case '%':
	    fprintf(vvp_out, "    %%mod/wr %d, %d;\n", l, r);
	    break;
#if 0
	  case '%':
	      { struct vector_info res = draw_eval_expr(exp, STUFF_OK_XZ);
		l = allocate_word();
		fprintf(vvp_out, "    %%ix/get %d, %u, %u;\n",
			l, res.base, res.wid);
		fprintf(vvp_out, "    %%cvt/ri %d, %d;\n", l, l);
	        clr_vector(res);
	      }
	      break;
#endif
	  default:
	    fprintf(stderr, "XXXX draw_binary_real(%c)\n",
		    ivl_expr_opcode(exp));
	    assert(0);
      }

      if (r >= 0) clr_word(r);

      return l;
}

static int draw_number_real(ivl_expr_t exp)
{
      unsigned int idx;
      int res = allocate_word();
      const char*bits = ivl_expr_bits(exp);
      unsigned wid = ivl_expr_width(exp);
      unsigned long mant = 0;
      int vexp = 0x1000;

      for (idx = 0 ;  idx < wid ;  idx += 1) {
	    if (bits[idx] == '1')
		  mant |= 1 << idx;
      }

	/* If this is actually a negative number, then get the
	   positive equivalent, and set the sign bit in the exponent
	   field. 

	   To get the positive equivilent of mant we need to take the
	   negative of the mantissa (0-mant) but also be aware that
	   the bits may not have been as many bits as the width of the
	   mant variable. This would lead to spurious '1' bits in the
	   high bits of mant that are masked by ~((-1UL)<<wid). */
      if (ivl_expr_signed(exp) && (bits[wid-1] == '1')) {
	    mant = (0-mant) & ~((-1UL) << wid);
	    vexp |= 0x4000;
      }

      fprintf(vvp_out, "    %%loadi/wr %d, %lu, %d; load(num)= %c%lu\n",
	      res, mant, vexp, (vexp&0x4000)? '-' : '+', mant);
      return res;
}

static int draw_realnum_real(ivl_expr_t exp)
{
      int res = allocate_word();
      double value = ivl_expr_dvalue(exp);

      double fract;
      int expo, vexp;
      unsigned long mant;
      int sign = 0;

	/* Handle the special case that the value is +-inf. */
      if (isinf(value)) {
	    if (value > 0)
		  fprintf(vvp_out, "  %%loadi/wr %d, 0, %d; load=+inf\n",
			  res, 0x3fff);
	    else
		  fprintf(vvp_out, "  %%loadi/wr %d, 0, %d; load=-inf\n",
			  res, 0x7fff);
	    return res;
      }

      if (value < 0) {
	    sign = 0x4000;
	    value *= -1;
      }

      fract = frexp(value, &expo);
      fract = ldexp(fract, 31);
      mant = fract;
      expo -= 31;

      vexp = expo + 0x1000;
      assert(vexp >= 0);
      assert(vexp < 0x2000);
      vexp += sign;

      fprintf(vvp_out, "    %%loadi/wr %d, %lu, %d; load=%f\n",
	      res, mant, vexp, ivl_expr_dvalue(exp));

	/* Capture the residual bits, if there are any. Note that an
	   IEEE754 mantissa has 52 bits, 31 of which were accounted
	   for already. */
      fract -= floor(fract);
      fract = ldexp(fract, 22);
      mant = fract;
      expo -= 22;

      vexp = expo + 0x1000;
      assert(vexp >= 0);
      assert(vexp < 0x2000);
      vexp += sign;

      if (mant != 0) {
	    int tmp_word = allocate_word();
	    fprintf(vvp_out, "    %%loadi/wr %d, %lu, %d; load=%f\n",
		    tmp_word, mant, vexp, ivl_expr_dvalue(exp));
	    fprintf(vvp_out, "    %%add/wr %d, %d;\n", res, tmp_word);
	    clr_word(tmp_word);
      }

      return res;
}

static int draw_sfunc_real(ivl_expr_t exp)
{
      struct vector_info sv;
      int res;
      const char*sign_flag = "";

      switch (ivl_expr_value(exp)) {

	  case IVL_VT_REAL:
	    if (ivl_expr_parms(exp) == 0) {
		  res = allocate_word();
		  fprintf(vvp_out, "    %%vpi_func/r \"%s\", %d;\n",
			  ivl_expr_name(exp), res);

	    } else {
		  res = draw_vpi_rfunc_call(exp);
	    }
	    break;

	  case IVL_VT_VECTOR:
	      /* If the value of the sfunc is a vector, then evaluate
		 it as a vector, then convert the result to a real
		 (via an index register) for the result. */
	    sv = draw_eval_expr(exp, 0);
	    clr_vector(sv);

	    if (ivl_expr_signed(exp))
		  sign_flag = "/s";

	    res = allocate_word();
	    fprintf(vvp_out, "    %%ix/get%s %d, %u, %u;\n",
		    sign_flag, res, sv.base, sv.wid);

	    fprintf(vvp_out, "    %%cvt/ri %d, %d;\n", res, res);
	    break;

	  default:
	    assert(0);
	    res = -1;
      }

      return res;
}

/*
 * The real value of a signal is the integer value of a signal
 * converted to real.
 */
static int draw_signal_real_logic(ivl_expr_t exp)
{
      int res = allocate_word();
      struct vector_info sv = draw_eval_expr(exp, 0);
      const char*sign_flag = ivl_expr_signed(exp)? "/s" : "";

      fprintf(vvp_out, "    %%ix/get%s %d, %u, %u; logic signal as real\n",
	      sign_flag, res, sv.base, sv.wid);
      clr_vector(sv);

      fprintf(vvp_out, "    %%cvt/ri %d, %d;\n", res, res);

      return res;
}

static int draw_signal_real_real(ivl_expr_t exp)
{
      ivl_signal_t sig = ivl_expr_signal(exp);
      int res = allocate_word();
      unsigned long word = 0;

      if (ivl_signal_array_count(sig) > 1) {
	    ivl_expr_t ix = ivl_expr_oper1(exp);
	    if (!number_is_immediate(ix, 8*sizeof(word))) {
		    /* XXXX Need to generate a %load/ar instruction. */
		  assert(0);
		  return res;
	    }

	      /* The index is constant, so we can return to direct
	         readout with the specific word selected. */
	    word = get_number_immediate(ix);
      }

      fprintf(vvp_out, "   %%load/wr %d, v%p_%lu;\n", res, sig, word);

      return res;
}

static int draw_signal_real(ivl_expr_t exp)
{
      ivl_signal_t sig = ivl_expr_signal(exp);
      switch (ivl_signal_data_type(sig)) {
	  case IVL_VT_LOGIC:
	    return draw_signal_real_logic(exp);
	  case IVL_VT_REAL:
	    return draw_signal_real_real(exp);
	  default:
	    fprintf(stderr, "internal error: signal_data_type=%d\n",
		    ivl_signal_data_type(sig));
	    assert(0);
	    return -1;
      }
}

static int draw_ternary_real(ivl_expr_t exp)
{
      ivl_expr_t cond = ivl_expr_oper1(exp);
      ivl_expr_t true_ex = ivl_expr_oper2(exp);
      ivl_expr_t false_ex = ivl_expr_oper3(exp);

      struct vector_info tst;

      unsigned lab_true = local_count++;
      unsigned lab_false = local_count++;

      int tru, fal;
      int res = allocate_word();

      tst = draw_eval_expr(cond, STUFF_OK_XZ|STUFF_OK_RO);
      if ((tst.base >= 4) && (tst.wid > 1)) {
	    struct vector_info tmp;

	    fprintf(vvp_out, "    %%or/r %u, %u, %u;\n",
		    tst.base, tst.base, tst.wid);

	    tmp = tst;
	    tmp.base += 1;
	    tmp.wid -= 1;
	    clr_vector(tmp);

	    tst.wid = 1;
      }

      fprintf(vvp_out, "  %%jmp/0  T_%d.%d, %u;\n",
	      thread_count, lab_true, tst.base);

      tru = draw_eval_real(true_ex);
      fprintf(vvp_out, "  %%mov/wr %d, %d;\n", res, tru);
      fprintf(vvp_out, "  %%jmp T_%d.%d;\n", thread_count, lab_false);
      clr_word(tru);

      fprintf(vvp_out, "T_%d.%d ;\n", thread_count, lab_true);

      fal = draw_eval_real(false_ex);
      fprintf(vvp_out, "  %%mov/wr %d, %d;\n", res, fal);
      clr_word(fal);

      fprintf(vvp_out, "T_%d.%d ;\n", thread_count, lab_false);

      clr_vector(tst);

      return res;
}

static int draw_unary_real(ivl_expr_t exp)
{
      ivl_expr_t sube = ivl_expr_oper1(exp);
      int sub = draw_eval_real(sube);

      if (ivl_expr_opcode(exp) == '+')
	    return sub;

      if (ivl_expr_opcode(exp) == '-') {
	    int res = allocate_word();
	    fprintf(vvp_out, "  %%loadi/wr %d, 0, 0; load 0.0\n", res);
	    fprintf(vvp_out, "  %%sub/wr %d, %d;\n", res, sub);

	    clr_word(sub);
	    return res;
      }

      fprintf(vvp_out, "; XXXX unary (%c)\n", ivl_expr_opcode(exp));
      fprintf(stderr, "XXXX evaluate unary (%c)\n", ivl_expr_opcode(exp));
      return 0;
}

int draw_eval_real(ivl_expr_t exp)
{
      int res = 0;

      switch (ivl_expr_type(exp)) {

	  case IVL_EX_BINARY:
	    res = draw_binary_real(exp);
	    break;

	  case IVL_EX_NUMBER:
	    res = draw_number_real(exp);
	    break;

	  case IVL_EX_REALNUM:
	    res = draw_realnum_real(exp);
	    break;

	  case IVL_EX_SFUNC:
	    res = draw_sfunc_real(exp);
	    break;

	  case IVL_EX_SIGNAL:
	    res = draw_signal_real(exp);
	    break;

	  case IVL_EX_TERNARY:
	    res = draw_ternary_real(exp);
	    break;

	  case IVL_EX_UFUNC:
	    res = draw_ufunc_real(exp);
	    break;

	  case IVL_EX_UNARY:
	    res = draw_unary_real(exp);
	    break;

	  default:
	    if (ivl_expr_value(exp) == IVL_VT_VECTOR) {
		  struct vector_info sv = draw_eval_expr(exp, 0);
		  const char*sign_flag = ivl_expr_signed(exp)? "/s" : "";

		  clr_vector(sv);
		  res = allocate_word();

		  fprintf(vvp_out, "  %%ix/get%s %d, %u, %u;\n",
			  sign_flag, res, sv.base, sv.wid);

		  fprintf(vvp_out, "  %%cvt/ri %d, %d;\n", res, res);

	    } else {
		  fprintf(stderr, "XXXX Evaluate real expression (%d)\n",
			  ivl_expr_type(exp));
		  fprintf(vvp_out, " ; XXXX Evaluate real expression (%d)\n",
			  ivl_expr_type(exp));
		  return 0;
	    }
	    break;
      }

      return res;
}


/*
 * $Log: eval_real.c,v $
 * Revision 1.22  2007/06/12 02:36:58  steve
 *  handle constant inf values.
 *
 * Revision 1.21  2007/06/07 03:20:15  steve
 *  Properly handle signed conversion to real
 *
 * Revision 1.20  2007/02/26 19:49:50  steve
 *  Spelling fixes (larry doolittle)
 *
 * Revision 1.19  2007/02/20 05:58:36  steve
 *  Handle unary minus of real valued expressions.
 *
 * Revision 1.18  2007/02/14 05:59:46  steve
 *  Handle type of ternary expressions properly.
 *
 * Revision 1.17  2007/01/16 05:44:16  steve
 *  Major rework of array handling. Memories are replaced with the
 *  more general concept of arrays. The NetMemory and NetEMemory
 *  classes are removed from the ivl core program, and the IVL_LPM_RAM
 *  lpm type is removed from the ivl_target API.
 *
 * Revision 1.16  2006/10/10 23:54:28  steve
 *  Fix rendering of signed numbers in real expressions.
 *
 * Revision 1.15  2006/08/09 05:19:08  steve
 *  Add support for real valued modulus.
 *
 * Revision 1.14  2005/07/13 04:52:31  steve
 *  Handle functions with real values.
 *
 * Revision 1.13  2005/07/11 16:56:51  steve
 *  Remove NetVariable and ivl_variable_t structures.
 *
 * Revision 1.12  2005/07/07 16:22:50  steve
 *  Generalize signals to carry types.
 *
 * Revision 1.11  2004/10/04 01:10:57  steve
 *  Clean up spurious trailing white space.
 *
 * Revision 1.10  2003/12/19 01:27:10  steve
 *  Fix various unsigned compare warnings.
 *
 * Revision 1.9  2003/05/25 02:50:08  steve
 *  Add % in real expressions.
 *
 * Revision 1.8  2003/04/23 02:22:47  steve
 *  Fix word register leak.
 *
 * Revision 1.7  2003/03/28 02:33:56  steve
 *  Add support for division of real operands.
 *
 * Revision 1.6  2003/03/15 04:45:18  steve
 *  Allow real-valued vpi functions to have arguments.
 *
 * Revision 1.5  2003/03/08 01:04:01  steve
 *  Excess precision breaks some targets.
 *
 * Revision 1.4  2003/02/07 02:46:16  steve
 *  Handle real value subtract and comparisons.
 *
 * Revision 1.3  2003/01/28 04:15:50  steve
 *  Deliver residual bits of real value.
 *
 * Revision 1.2  2003/01/27 00:14:37  steve
 *  Support in various contexts the $realtime
 *  system task.
 *
 * Revision 1.1  2003/01/26 21:16:00  steve
 *  Rework expression parsing and elaboration to
 *  accommodate real/realtime values and expressions.
 *
 */

