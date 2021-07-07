#ifndef GMB_PROBLEM_EOM_CCSD_H_
#define GMB_PROBLEM_EOM_CCSD_H_
#include <molpro/linalg/itsolv/IterativeSolver.h>
#include <vector>
#include "problem_eom.h"
#include "expressions/eom-ccsd/eom-ccsd.h"
#include "expressions/update.h"

extern molpro::Profiler prof;
// extern std::unique_ptr<molpro::Profiler> pprof;

/**
 * @brief EOM-CCSD
 * 
 */

class problem_eom_ccsd : public problem_eom {
private:
  mutable supercontainer<> m_int;       ///> intermediates
public:
  problem_eom_ccsd(const hamiltonian<> &ham, const amplitudes<> &ampl)
  : problem_eom(ham, ampl) {
    init();
  }

  void init() {

    // auto p = prof.push("eom-ccsd init");
    // auto p = pprof->push("eom-ccsd init");

    auto tau = ccsd_tau(m_tampl.m2get(t1), m_tampl.m4get(t2));
    auto if_oo = eom_ccsd_if_oo(m_tampl.m2get(t1), m_tampl.m4get(t2), m_ham.m2get(f_oo), m_ham.m2get(f_ov),
                   m_ham.m4get(i_ooov), m_ham.m4get(i_oovv));    
    auto if_ov = eom_ccsd_if_ov(m_tampl.m2get(t1), m_ham.m2get(f_ov), m_ham.m4get(i_oovv)); 
    auto if_vv = eom_ccsd_if_vv(m_tampl.m2get(t1), m_tampl.m4get(t2), m_ham.m2get(f_ov), m_ham.m2get(f_vv), 
                   m_ham.m4get(i_oovv), m_ham.m4get(i_ovvv));

    auto iw_oooo = eom_ccsd_iw_oooo(m_tampl.m2get(t1), tau, 
                    m_ham.m4get(i_oooo), m_ham.m4get(i_ooov), m_ham.m4get(i_oovv));    
    auto iw_ooov = eom_ccsd_iw_ooov(m_tampl.m2get(t1), m_tampl.m4get(t2), tau, if_ov,
                    iw_oooo, m_ham.m4get(i_ooov), m_ham.m4get(i_oovv), m_ham.m4get(i_ovov), m_ham.m4get(i_ovvv));
    auto iw_ovov = eom_ccsd_iw_ovov(m_tampl.m2get(t1), m_tampl.m4get(t2), 
                    m_ham.m4get(i_ooov), m_ham.m4get(i_oovv), m_ham.m4get(i_ovov), m_ham.m4get(i_ovvv));
    auto iw_vvvv = eom_ccsd_iw_vvvv(m_tampl.m2get(t1), tau, m_ham.m4get(i_oovv), m_ham.m4get(i_ovvv), m_ham.m4get(i_vvvv));
    auto iw_ovvv = eom_ccsd_iw_ovvv(m_tampl.m2get(t1), m_tampl.m4get(t2), tau, if_ov,  
                    m_ham.m4get(i_ooov), m_ham.m4get(i_oovv), m_ham.m4get(i_ovov), m_ham.m4get(i_ovvv), iw_vvvv);
    auto iw2_ooov = eom_ccsd_iw2_ooov(m_tampl.m2get(t1), m_ham.m4get(i_ooov), m_ham.m4get(i_oovv));
    auto iw2_ovvv = eom_ccsd_iw2_ovvv(m_tampl.m2get(t1), m_ham.m4get(i_oovv), m_ham.m4get(i_ovvv));
 
    m_int.set("tau", tau);
    m_int.set("if_oo", if_oo);
    m_int.set("if_ov", if_ov);
    m_int.set("if_vv", if_vv);
    m_int.set("iw_oooo", iw_oooo);
    m_int.set("iw_ooov", iw_ooov);
    m_int.set("iw_ovov", iw_ovov);
    m_int.set("iw_ovvv", iw_ovvv);
    m_int.set("iw_vvvv", iw_vvvv);
    m_int.set("iw2_ooov", iw2_ooov);
    m_int.set("iw2_ovvv", iw2_ovvv);   
  }

  bool diagonals(container_t &d) const override {
    // auto p = prof.push("eom-ccsd diagonals");
    // auto p = pprof->push("eom-ccsd diagonals");
    auto d_ov = diag_ov(m_int.m2get("if_oo"), m_int.m2get("if_ov"), m_int.m2get("if_vv"));
    auto d_oovv = diag_oovv(d_ov, m_ham.m4get(i_oovv));
    auto id_oo = id_xx(m_ham.m2get(f_oo));
    auto id_vv = id_xx(m_ham.m2get(f_vv));
    auto d1 = hbar_ov(d_ov);
    auto d2 = hbar_oovv(id_oo, id_vv, d_oovv);
    d.set(r1,d1);
    d.set(r2,d2);
    return true;
  }

  void create_guess(std::vector<amplitudes<>>& v_rampl) {
    auto diag = v_rampl[0];
    diagonals(diag);
    auto map = diag.select(v_rampl.size());
    size_t k{0};
      for (auto const& [key, val] : map) {
        v_rampl[k].fill_guess(key);
        k++;
      }
  }

  void precondition(const VecRef<container_t>& residual, 
                    const std::vector<value_t>& shift, 
                    const container_t& diagonals) const override {
    // auto p = pprof->push("eom-ccsd precondition");
    // auto p = prof.push("eom-ccsd precondition");
    for (int k = 0; k < residual.size(); k++) {
      auto &a = residual[k].get();   
      auto &d = const_cast<container_t&> (diagonals);  
      auto a1 = precond_ov(a.m2get(r1), d.m2get(r1), shift[k]);
      auto id_oo = id_xx(m_ham.m2get(f_oo));
      auto id_vv = id_xx(m_ham.m2get(f_vv));
      auto a2 = precond_oovv(a.m4get(r2),d.m4get(r2), shift[k]);
      a.set(r1,a1);
      a.set(r2,a2);
    }
  }

  void action(const CVecRef<container_t> &parameters, const VecRef<container_t> &actions) const override {
    // auto p = pprof->push("eom-ccsd action");
    // auto p = prof.push("eom-ccsd action");
    for (int k = 0; k < parameters.size(); k++) {
      // auto pk = pprof->push("eom-ccsd action: "+std::to_string(k));
      auto &ccp = const_cast<container_t&> (parameters[k].get());     
      auto &a = actions[k].get();  
      // compute intermediates
      auto ir1_vv = eom_ccsd_ir1_vv(m_ham.m2get(f_vv),ccp.m2get(r1),m_int.m4get("iw2_ovvv"));          
      auto ir1_oo = eom_ccsd_ir1_oo(m_ham.m2get(f_oo),ccp.m2get(r1),m_int.m4get("iw2_ooov"));          
      auto ir2_oo = eom_ccsd_ir2_oo(m_ham.m2get(f_oo),ccp.m4get(r2),m_ham.m4get(i_oovv));          
      auto ir2_vv = eom_ccsd_ir2_vv(m_ham.m2get(f_vv),ccp.m4get(r2),m_ham.m4get(i_oovv));          
      // compute r1
      {
      // auto pr1 = pprof->push("compute r1");
      auto r1_new = eom_ccsd_r1(ccp.m2get(r1), ccp.m4get(r2), m_int.m2get("if_oo"), m_int.m2get("if_ov"), m_int.m2get("if_vv"),  
                    m_int.m4get("iw_ovov"), m_int.m4get("iw2_ooov"), m_int.m4get("iw2_ovvv"));

      // project out cross terms
      libtensor::block_tensor_wr_ctrl<2, double> ctrl(r1_new);
      libtensor::orbit_list<2, double> ol(ctrl.req_const_symmetry());
      for (libtensor::orbit_list<2, double>::iterator it = ol.begin(); it != ol.end(); it++) {
        libtensor::index<2> bidx;
        ol.get_index(it, bidx);
          if ((bidx[0] != bidx[1])) 
            ctrl.req_zero_block(bidx);
      }
      a.set(r1, r1_new);
      }
      // compute r2
      {
      // auto pr2 = pprof->push("compute r2");
      auto r2_new = eom_ccsd_r2(ccp.m2get(r1), ccp.m4get(r2), m_tampl.m4get(t2), m_int.m2get("if_oo"), m_int.m2get("if_vv"),  
                    ir1_oo, ir2_oo, ir1_vv, ir2_vv, 
                    m_ham.m4get(i_oovv), m_int.m4get("iw_oooo"), m_int.m4get("iw_ooov"), m_int.m4get("iw2_ooov"), m_int.m4get("iw_ovov"), m_int.m4get("iw_ovvv"), m_int.m4get("iw2_ovvv"), m_int.m4get("iw_vvvv"));  
      a.set(r2, r2_new);
      }
    }
  }

  void print(std::ostream& s) const {
    s << "EOM-CCSD";
  }

};

std::ostream& operator<<(std::ostream& s, const problem_eom_ccsd& problem) {
  s<<"Problem: EOM-CCSD ";
  return s;
}
#endif //GMB_PROBLEM_EOM_CCSD_H_