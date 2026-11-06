/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_SMF_PAGING_RULES_HPP_SEEN
#define FILE_SMF_PAGING_RULES_HPP_SEEN

#include <netinet/in.h>

#include <string>

#include "3gpp_29.244.h"
#include "msg_pfcp.hpp"

namespace oai::app::smf::paging {

/**
 * Builds the FAR of the paging rule armed on AN release.
 * Apply Action is BUFF|NOCP: the packet that woke the session is buffered in
 * the UPF and the CP is notified once (the UPF latches notified_cp). NOCP
 * without BUFF is rejected. Forwarding Parameters are legal only with FORW,
 * so neither those nor a BAR ID are set.
 * @param [const pfcp::far_id_t&] far_id: FAR ID of the paging rule
 * @return pfcp::create_far: the Create FAR IE
 */
pfcp::create_far make_paging_create_far(const pfcp::far_id_t& far_id);

/**
 * Builds the PDR of the paging rule armed on AN release.
 * One PDR per PDU session, matching any downlink packet destined to the UE
 * IPv4 address. It carries no Outer Header Removal - a downlink (CORE) PDR
 * carrying it never matches anything - and no F-TEID, SDF filter, QFI,
 * traffic endpoint, QER or URR.
 * @param [const pfcp::pdr_id_t&] pdr_id: PDR ID of the paging rule
 * @param [const pfcp::far_id_t&] far_id: FAR ID created in the same message
 * @param [uint32_t] precedence: precedence of the rule (lower wins)
 * @param [const struct in_addr&] ue_ipv4: IPv4 address of the UE
 * @param [const std::string&] nw_instance: network instance, may be empty
 * @return pfcp::create_pdr: the Create PDR IE
 */
pfcp::create_pdr make_paging_create_pdr(
    const pfcp::pdr_id_t& pdr_id, const pfcp::far_id_t& far_id,
    uint32_t precedence, const struct in_addr& ue_ipv4,
    const std::string& nw_instance);

}  // namespace oai::app::smf::paging

#endif /* FILE_SMF_PAGING_RULES_HPP_SEEN */
