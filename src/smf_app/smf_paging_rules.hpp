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
 * Builds the FAR of the paging rule (M1).
 * Apply Action is BUFF | NOCP (0x0C): the packet that woke us up is buffered
 * in the UPF and the CP is notified exactly once (the UPF latches
 * notified_cp). Nothing else is set: no Forwarding Parameters - they are
 * legal only when FORW is set - and no BAR ID.
 * @param [const pfcp::far_id_t&] far_id: FAR ID of the paging rule
 * @return pfcp::create_far: the Create FAR IE
 */
pfcp::create_far make_paging_create_far(const pfcp::far_id_t& far_id);

/**
 * Builds the PDR of the paging rule (M1).
 * One PDR per PDU session, matching ANY downlink packet destined to the UE
 * IPv4 address. It deliberately carries no Outer Header Removal (a CORE PDR
 * carrying it never matches), no F-TEID, no SDF Filter, no QFI, no Traffic
 * Endpoint ID, no QER ID and no URR ID.
 * @param [const pfcp::pdr_id_t&] pdr_id: PDR ID of the paging rule
 * @param [const pfcp::far_id_t&] far_id: FAR ID created in the SAME message
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
