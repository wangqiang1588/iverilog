/*
 * Copyright (c) 2000 Stephen Williams (steve@icarus.com)
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
#if !defined(WINNT) && !defined(macintosh)
#ident "$Id: t-dll.cc,v 1.10 2000/10/05 05:03:01 steve Exp $"
#endif

# include  "compiler.h"
# include  "t-dll.h"
# include  <dlfcn.h>
# include  <malloc.h>

static struct dll_target dll_target_obj;

bool dll_target::start_design(const Design*des)
{
      dll_path_ = des->get_flag("DLL");
      dll_ = dlopen(dll_path_.c_str(), RTLD_NOW);
      if (dll_ == 0) {
	    cerr << dll_path_ << ": " << dlerror() << endl;
	    return false;
      }

      stmt_cur_ = 0;

	// Initialize the design object.
      des_.self = des;
      des_.root_ = (ivl_scope_t)calloc(1, sizeof(struct ivl_scope_s));
      des_.root_->self = des->find_root_scope();

      start_design_ = (start_design_f)dlsym(dll_, "target_start_design");
      end_design_   = (end_design_f)  dlsym(dll_, "target_end_design");

      net_bufz_   = (net_bufz_f)  dlsym(dll_, LU "target_net_bufz" TU);
      net_const_  = (net_const_f) dlsym(dll_, LU "target_net_const" TU);
      net_event_  = (net_event_f) dlsym(dll_, LU "target_net_event" TU);
      net_logic_  = (net_logic_f) dlsym(dll_, LU "target_net_logic" TU);
      net_probe_  = (net_probe_f) dlsym(dll_, LU "target_net_probe" TU);
      net_signal_ = (net_signal_f)dlsym(dll_, LU "target_net_signal" TU);
      process_    = (process_f)   dlsym(dll_, LU "target_process" TU);
      scope_      = (scope_f)     dlsym(dll_, LU "target_scope" TU);

      (start_design_)(&des_);
      return true;
}

void dll_target::end_design(const Design*)
{
      (end_design_)(&des_);
      dlclose(dll_);
}

bool dll_target::bufz(const NetBUFZ*net)
{
      if (net_bufz_) {
	    int rc = (net_bufz_)(net->name(), 0);
	    return rc == 0;

      } else {
	    cerr << dll_path_ << ": internal error: target DLL lacks "
		 << "target_net_bufz function." << endl;
	    return false;
      }

      return false;
}

void dll_target::event(const NetEvent*net)
{
      if (net_event_) {
	    (net_event_)(net->full_name().c_str(), 0);

      } else {
	    cerr << dll_path_ << ": internal error: target DLL lacks "
		 << "target_net_event function." << endl;
      }

      return;
}

void dll_target::logic(const NetLogic*net)
{
      struct ivl_net_logic_s obj;
      obj.dev_ = net;

      if (net_logic_) {
	    (net_logic_)(net->name(), &obj);

      } else {
	    cerr << dll_path_ << ": internal error: target DLL lacks "
		 << "target_net_logic function." << endl;
      }

      return;
}

bool dll_target::net_const(const NetConst*net)
{
      unsigned idx;
      ivl_net_const_t obj = (ivl_net_const_t)
	    calloc(1, sizeof(struct ivl_net_const_s));

      obj->width_ = net->pin_count();
      obj->bits_ = (char*)malloc(obj->width_);
      for (idx = 0 ;  idx < obj->width_ ;  idx += 1)
	    switch (net->value(idx)) {
		case verinum::V0:
		  obj->bits_[idx] = '0';
		  break;
		case verinum::V1:
		  obj->bits_[idx] = '1';
		  break;
		case verinum::Vx:
		  obj->bits_[idx] = 'x';
		  break;
		case verinum::Vz:
		  obj->bits_[idx] = 'z';
		  break;
	    }

      if (net_const_) {
	    int rc = (net_const_)(net->name(), obj);
	    return rc == 0;

      } else {
	    cerr << dll_path_ << ": internal error: target DLL lacks "
		 << "target_net_const function." << endl;
	    return false;
      }

      return false;
}

void dll_target::net_probe(const NetEvProbe*net)
{
      if (net_probe_) {
	    int rc = (net_probe_)(net->name(), 0);
	    return;

      } else {
	    cerr << dll_path_ << ": internal error: target DLL lacks "
		 << "target_net_probe function." << endl;
	    return;
      }

      return;
}

static ivl_scope_t find_scope(ivl_scope_t root, const NetScope*cur)
{
      ivl_scope_t parent, tmp;

      if (const NetScope*par = cur->parent()) {
	    parent = find_scope(root, par);

      } else {
	    assert(root->self == cur);
	    return root;
      }

      for (tmp = parent->child_ ;  tmp ;  tmp = tmp->sibling_)
	    if (tmp->self == cur)
		  return tmp;

      return 0;
}
      
void dll_target::scope(const NetScope*net)
{
      ivl_scope_t scope;

      if (net->parent() == 0) {
	    assert(des_.root_->self == net);
	    scope = des_.root_;

      } else {
	    scope = (ivl_scope_t)calloc(1, sizeof(struct ivl_scope_s));
	    scope->self = net;

	    ivl_scope_t parent = find_scope(des_.root_, net->parent());
	    assert(parent != 0);

	    scope->sibling_= parent->child_;
	    parent->child_ = scope;
      }

      if (scope_)
	    (scope_)(scope);
}

void dll_target::signal(const NetNet*net)
{
      if (net_signal_) {
	    int rc = (net_signal_)(net->name(), (ivl_signal_t)net);
	    return;

      } else {
	    cerr << dll_path_ << ": internal error: target DLL lacks "
		 << "target_net_signal function." << endl;
	    return;
      }
}

extern const struct target tgt_dll = { "dll", &dll_target_obj };


/*
 * $Log: t-dll.cc,v $
 * Revision 1.10  2000/10/05 05:03:01  steve
 *  xor and constant devices.
 *
 * Revision 1.9  2000/09/30 02:18:15  steve
 *  ivl_expr_t support for binary operators,
 *  Create a proper ivl_scope_t object.
 *
 * Revision 1.8  2000/09/24 15:46:00  steve
 *  API access to signal type and port type.
 *
 * Revision 1.7  2000/09/18 01:24:32  steve
 *  Get the structure for ivl_statement_t worked out.
 *
 * Revision 1.6  2000/08/27 15:51:51  steve
 *  t-dll iterates signals, and passes them to the
 *  target module.
 *
 *  Some of NetObj should return char*, not string.
 *
 * Revision 1.5  2000/08/26 00:54:03  steve
 *  Get at gate information for ivl_target interface.
 *
 * Revision 1.4  2000/08/20 04:13:57  steve
 *  Add ivl_target support for logic gates, and
 *  make the interface more accessible.
 *
 * Revision 1.3  2000/08/19 18:12:42  steve
 *  Add target calls for scope, events and logic.
 *
 * Revision 1.2  2000/08/14 04:39:57  steve
 *  add th t-dll functions for net_const, net_bufz and processes.
 *
 * Revision 1.1  2000/08/12 16:34:37  steve
 *  Start stub for loadable targets.
 *
 */

