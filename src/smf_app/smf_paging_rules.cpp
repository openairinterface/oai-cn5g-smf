/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "smf_paging_rules.hpp"

namespace oai::app::smf::paging {

//------------------------------------------------------------------------------
pfcp::create_far make_paging_create_far(const pfcp::far_id_t& far_id) {
  pfcp::create_far create_far       = {};
  pfcp::apply_action_t apply_action = {};

  // BUFF | NOCP = 0x0C. Exactly one of DROP/FORW/BUFF may be set and NOCP
  // only together with BUFF, so no Forwarding Parameters here.
  apply_action.buff = 1;
  apply_action.nocp = 1;

  create_far.set(far_id);
  create_far.set(apply_action);
  return create_far;
}

//------------------------------------------------------------------------------
pfcp::create_pdr make_paging_create_pdr(
    const pfcp::pdr_id_t& pdr_id, const pfcp::far_id_t& far_id,
    uint32_t precedence, const struct in_addr& ue_ipv4,
    const std::string& nw_instance) {
  pfcp::create_pdr create_pdr               = {};
  pfcp::pdi pdi                             = {};
  pfcp::precedence_t pdr_precedence         = {};
  pfcp::source_interface_t source_interface = {};
  pfcp::ue_ip_address_t ue_ip_address       = {};

  create_pdr.set(pdr_id);

  pdr_precedence.precedence = precedence;
  create_pdr.set(pdr_precedence);

  // the packet comes from the DN, so the source interface is CORE
  source_interface.interface_value = pfcp::INTERFACE_VALUE_CORE;
  pdi.set(source_interface);

  //-------------------
  // Network Instance
  //-------------------
  if (!nw_instance.empty()) {
    pfcp::network_instance_t network_instance = {};
    network_instance.network_instance         = nw_instance;
    pdi.set(network_instance);
  }

  // downlink: the UE IPv4 address is the DESTINATION of the packet (S/D = 1)
  ue_ip_address.v4           = 1;
  ue_ip_address.sd           = 1;
  ue_ip_address.ipv4_address = ue_ipv4;
  pdi.set(ue_ip_address);

  create_pdr.set(pdi);

  // MUST be the FAR created in the same message
  create_pdr.set(far_id);
  return create_pdr;
}

}  // namespace oai::app::smf::paging
