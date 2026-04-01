/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "smf_event.hpp"
#include "itti.hpp"
#include "smf_app.hpp"
#include "smf_subscription.hpp"

using namespace oai::app::smf;
extern oai::app::smf::smf_app* smf_app_inst;
extern itti_mw* itti_inst;

//------------------------------------------------------------------------------
bs2::connection smf_event::subscribe_sm_context_status(
    const sm_context_status_sig_t::slot_type& sig) {
  return sm_context_status.connect(sig);
}

//------------------------------------------------------------------------------
bs2::connection smf_event::subscribe_ee_pdu_session_release(
    const ee_pdu_session_release_sig_t::slot_type& sig) {
  return ee_pdu_session_release.connect(sig);
}

//------------------------------------------------------------------------------
bs2::connection smf_event::subscribe_ee_ue_ip_change(
    const ee_ue_ip_change_sig_t::slot_type& sig) {
  return ee_ue_ip_change.connect(sig);
}

//------------------------------------------------------------------------------
bs2::connection smf_event::subscribe_ee_plmn_change(
    const ee_plmn_change_sig_t::slot_type& sig) {
  return ee_plmn_change.connect(sig);
}

//------------------------------------------------------------------------------

bs2::connection smf_event::subscribe_ee_ddds(
    const ee_ddds_sig_t::slot_type& sig) {
  return ee_ddds.connect(sig);
}
//------------------------------------------------------------------------------

bs2::connection smf_event::subscribe_ee_pdusesest(
    const ee_pdusesest_sig_t::slot_type& sig) {
  return ee_pdusesest.connect(sig);
}

//------------------------------------------------------------------------------
bs2::connection smf_event::subscribe_ee_qos_monitoring(
    const ee_qos_monitoring_sig_t::slot_type& sig) {
  return ee_qos_monitoring.connect(sig);
}

//------------------------------------------------------------------------------
bs2::connection smf_event::subscribe_ee_flexcn_event(
    const ee_flexcn_sig_t::slot_type& sig) {
  return ee_flexcn.connect(sig);
}
