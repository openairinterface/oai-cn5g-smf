/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_SMF_POLICY_TYPES_HPP_SEEN
#define FILE_SMF_POLICY_TYPES_HPP_SEEN

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace oai::app::smf {

// A PCC rule can contain more than one flow description. Each description is
// installed as a separate QoS flow, so policy state retains every QFI owned by
// the rule rather than just the most recently allocated one.
using pcc_rule_qfi_map = std::map<std::string, std::vector<uint8_t>>;

}  // namespace oai::app::smf

#endif
