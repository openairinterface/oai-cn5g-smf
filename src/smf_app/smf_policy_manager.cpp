/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "smf_policy_manager.hpp"

#include <algorithm>
#include <sstream>

#include "logger.hpp"
#include "Arp.h"
#include "PreemptionCapability.h"
#include "PreemptionCapability_anyOf.h"
#include "PreemptionVulnerability.h"
#include "PreemptionVulnerability_anyOf.h"
#include "RuleReport.h"
#include "RuleStatus.h"
#include "RuleStatus_anyOf.h"
#include "FailureCode.h"
#include "FailureCode_anyOf.h"
#include "SessionRuleReport.h"

using namespace oai::app::smf;
using namespace oai::_3gpp::model;

//------------------------------------------------------------------------------
oai::_3gpp::model::QosData pcf_qos_flow::to_qos_data() const {
  oai::_3gpp::model::QosData qos_data;
  qos_data.setQosId(qos_data_id);
  qos_data.setR5qi(fiveqi);
  qos_data.setPriorityLevel(priority_level);

  Arp arp;
  arp.setPriorityLevel(arp_priority);

  PreemptionCapability preempt_cap;
  preempt_cap.setEnumValue(
      arp_preempt_cap
          ? PreemptionCapability_anyOf::ePreemptionCapability_anyOf::MAY_PREEMPT
          : PreemptionCapability_anyOf::ePreemptionCapability_anyOf::
                NOT_PREEMPT);
  arp.setPreemptCap(preempt_cap);

  PreemptionVulnerability preempt_vuln;
  preempt_vuln.setEnumValue(
      arp_preempt_vuln ? PreemptionVulnerability_anyOf::
                             ePreemptionVulnerability_anyOf::PREEMPTABLE
                       : PreemptionVulnerability_anyOf::
                             ePreemptionVulnerability_anyOf::NOT_PREEMPTABLE);
  arp.setPreemptVuln(preempt_vuln);

  qos_data.setArp(arp);

  if (gbr_ul.has_value()) {
    qos_data.setGbrUl(gbr_ul.value());
  }
  if (gbr_dl.has_value()) {
    qos_data.setGbrDl(gbr_dl.value());
  }
  if (maxbr_ul.has_value()) {
    qos_data.setMaxbrUl(maxbr_ul.value());
  }
  if (maxbr_dl.has_value()) {
    qos_data.setMaxbrDl(maxbr_dl.value());
  }

  return qos_data;
}

//------------------------------------------------------------------------------
pcf_qos_flow pcf_qos_flow::from_qos_data(
    const oai::_3gpp::model::QosData& qos_data, const std::string& qos_id) {
  pcf_qos_flow flow;
  flow.qos_data_id = qos_id;
  flow.fiveqi = qos_data.getR5qi();

  if (qos_data.priorityLevelIsSet()) {
    flow.priority_level = qos_data.getPriorityLevel();
  }

  if (qos_data.arpIsSet()) {
    const auto& arp = qos_data.getArp();
    flow.arp_priority = arp.getPriorityLevel();

    flow.arp_preempt_cap =
        arp.getPreemptCap().getEnumValue() ==
        PreemptionCapability_anyOf::ePreemptionCapability_anyOf::MAY_PREEMPT;

    flow.arp_preempt_vuln =
        arp.getPreemptVuln().getEnumValue() ==
        PreemptionVulnerability_anyOf::ePreemptionVulnerability_anyOf::
            PREEMPTABLE;
  }

  if (qos_data.gbrUlIsSet()) {
    flow.gbr_ul = qos_data.getGbrUl();
  }
  if (qos_data.gbrDlIsSet()) {
    flow.gbr_dl = qos_data.getGbrDl();
  }
  if (qos_data.maxbrUlIsSet()) {
    flow.maxbr_ul = qos_data.getMaxbrUl();
  }
  if (qos_data.maxbrDlIsSet()) {
    flow.maxbr_dl = qos_data.getMaxbrDl();
  }

  return flow;
}

//------------------------------------------------------------------------------
std::string smf_policy_delta::to_string() const {
  std::stringstream ss;
  ss << "Policy Delta: ";
  ss << added_pcc_rules.size() << " PCC rules added, ";
  ss << modified_pcc_rules.size() << " PCC rules modified, ";
  ss << removed_pcc_rules.size() << " PCC rules removed; ";
  ss << added_qos_data.size() << " QoS data added, ";
  ss << modified_qos_data.size() << " QoS data modified, ";
  ss << removed_qos_data.size() << " QoS data removed";
  return ss.str();
}

//------------------------------------------------------------------------------
bool smf_policy_manager::parse_policy_decision(
    const nlohmann::json& policy_json, SmPolicyDecision& policy_decision) {
  try {
    if (policy_json.find("smPolicyDecision") != policy_json.end()) {
      from_json(policy_json["smPolicyDecision"], policy_decision);
      return true;
    } else if (policy_json.is_object() && policy_json.contains("pccRules")) {
      // Direct SmPolicyDecision object (not wrapped)
      from_json(policy_json, policy_decision);
      return true;
    }
    Logger::smf_app().warn(
        "Policy JSON does not contain smPolicyDecision field");
    return false;
  } catch (const nlohmann::json::exception& e) {
    Logger::smf_app().error(
        "Failed to parse SmPolicyDecision: %s", e.what());
    return false;
  } catch (const std::exception& e) {
    Logger::smf_app().error(
        "Unexpected error parsing SmPolicyDecision: %s", e.what());
    return false;
  }
}

//------------------------------------------------------------------------------
smf_policy_delta smf_policy_manager::compute_delta(
    const SmPolicyDecision& current, const SmPolicyDecision& requested) {
  smf_policy_delta delta;

  // --- Process PCC Rules ---
  // TS 29.512 §4.2.6.1: NULL value in pccRules map means rule is removed
  std::map<std::string, PccRule> current_rules;
  std::map<std::string, PccRule> requested_rules;

  if (current.pccRulesIsSet()) {
    current_rules = current.getPccRules();
  }
  if (requested.pccRulesIsSet()) {
    requested_rules = requested.getPccRules();
  }

  // Find added and modified PCC rules
  for (const auto& [rule_id, new_rule] : requested_rules) {
    auto current_it = current_rules.find(rule_id);

    if (current_it == current_rules.end()) {
      // New rule - ADDED
      pcc_rule_change change;
      change.type = policy_change_type::ADDED;
      change.rule_id = rule_id;
      change.new_rule = new_rule;
      delta.pcc_rule_changes.push_back(change);
      delta.added_pcc_rules.insert(rule_id);

      Logger::smf_app().debug(
          "Policy delta: PCC rule '%s' ADDED (precedence %d)",
          rule_id.c_str(), new_rule.getPrecedence());
    } else if (!pcc_rules_equal(current_it->second, new_rule)) {
      // Existing rule changed - MODIFIED
      pcc_rule_change change;
      change.type = policy_change_type::MODIFIED;
      change.rule_id = rule_id;
      change.old_rule = current_it->second;
      change.new_rule = new_rule;
      delta.pcc_rule_changes.push_back(change);
      delta.modified_pcc_rules.insert(rule_id);

      Logger::smf_app().debug(
          "Policy delta: PCC rule '%s' MODIFIED", rule_id.c_str());
    }
    // else: unchanged, no action needed
  }

  // Find removed PCC rules (in current but not in requested)
  for (const auto& [rule_id, old_rule] : current_rules) {
    if (requested_rules.find(rule_id) == requested_rules.end()) {
      pcc_rule_change change;
      change.type = policy_change_type::REMOVED;
      change.rule_id = rule_id;
      change.old_rule = old_rule;
      delta.pcc_rule_changes.push_back(change);
      delta.removed_pcc_rules.insert(rule_id);

      Logger::smf_app().debug(
          "Policy delta: PCC rule '%s' REMOVED", rule_id.c_str());
    }
  }

  // --- Process QoS Data ---
  std::map<std::string, QosData> current_qos;
  std::map<std::string, QosData> requested_qos;

  if (current.qosDecsIsSet()) {
    current_qos = current.getQosDecs();
  }
  if (requested.qosDecsIsSet()) {
    requested_qos = requested.getQosDecs();
  }

  // Find added and modified QoS data
  for (const auto& [qos_id, new_qos] : requested_qos) {
    auto current_it = current_qos.find(qos_id);

    if (current_it == current_qos.end()) {
      // New QoS data - ADDED
      qos_data_change change;
      change.type = policy_change_type::ADDED;
      change.qos_id = qos_id;
      change.new_data = new_qos;
      delta.qos_data_changes.push_back(change);
      delta.added_qos_data.insert(qos_id);

      Logger::smf_app().debug(
          "Policy delta: QoS data '%s' ADDED (5QI=%d, ARP=%d)",
          qos_id.c_str(), new_qos.getR5qi(),
          new_qos.arpIsSet() ? new_qos.getArp().getPriorityLevel() : 0);
    } else if (!qos_data_equal(current_it->second, new_qos)) {
      // Existing QoS data changed - MODIFIED
      qos_data_change change;
      change.type = policy_change_type::MODIFIED;
      change.qos_id = qos_id;
      change.old_data = current_it->second;
      change.new_data = new_qos;
      delta.qos_data_changes.push_back(change);
      delta.modified_qos_data.insert(qos_id);

      Logger::smf_app().debug(
          "Policy delta: QoS data '%s' MODIFIED (5QI=%d→%d)",
          qos_id.c_str(), current_it->second.getR5qi(), new_qos.getR5qi());
    }
  }

  // Find removed QoS data
  for (const auto& [qos_id, old_qos] : current_qos) {
    if (requested_qos.find(qos_id) == requested_qos.end()) {
      qos_data_change change;
      change.type = policy_change_type::REMOVED;
      change.qos_id = qos_id;
      change.old_data = old_qos;
      delta.qos_data_changes.push_back(change);
      delta.removed_qos_data.insert(qos_id);

      Logger::smf_app().debug(
          "Policy delta: QoS data '%s' REMOVED", qos_id.c_str());
    }
  }

  Logger::smf_app().info("%s", delta.to_string().c_str());
  return delta;
}

//------------------------------------------------------------------------------
std::vector<pcf_qos_flow> smf_policy_manager::extract_qos_flows(
    const SmPolicyDecision& policy) {
  std::vector<pcf_qos_flow> flows;

  if (!policy.pccRulesIsSet() || !policy.qosDecsIsSet()) {
    Logger::smf_app().debug(
        "Policy has no PCC rules or QoS data, no flows to extract");
    return flows;
  }

  const auto& pcc_rules = policy.getPccRules();
  const auto& qos_decs = policy.getQosDecs();

  for (const auto& [rule_id, pcc_rule] : pcc_rules) {
    if (!pcc_rule.refQosDataIsSet()) {
      Logger::smf_app().debug(
          "PCC rule '%s' has no refQosData, skipping", rule_id.c_str());
      continue;
    }

    // A PCC rule can reference multiple QoS data entries
    for (const auto& qos_ref : pcc_rule.getRefQosData()) {
      auto qos_it = qos_decs.find(qos_ref);
      if (qos_it == qos_decs.end()) {
        Logger::smf_app().warn(
            "PCC rule '%s' references unknown QoS data '%s'",
            rule_id.c_str(), qos_ref.c_str());
        continue;
      }

      pcf_qos_flow flow = pcf_qos_flow::from_qos_data(qos_it->second, qos_ref);
      flow.pcc_rule_id = rule_id;
      flow.precedence = pcc_rule.getPrecedence();

      // Extract flow descriptions from PCC rule
      if (pcc_rule.flowInfosIsSet()) {
        flow.flow_descriptions = pcc_rule.getFlowInfos();
      }

      // Check if this is the default rule
      if (qos_it->second.defQosFlowIndicationIsSet() &&
          qos_it->second.isDefQosFlowIndication()) {
        flow.is_default = true;
      }

      flows.push_back(flow);

      Logger::smf_app().debug(
          "Extracted QoS flow: PCC rule '%s' -> QoS '%s' (5QI=%d, prec=%d)",
          rule_id.c_str(), qos_ref.c_str(), flow.fiveqi, flow.precedence);
    }
  }

  return flows;
}

//------------------------------------------------------------------------------
uint8_t smf_policy_manager::allocate_qfi(
    const std::set<uint8_t>& allocated_qfis) {
  // QFI range is 0-63 (6 bits), but 0 is often reserved
  // TS 23.501 §5.7.1.4: QFI uniquely identifies a QoS flow within a PDU session
  for (uint8_t qfi = 1; qfi <= 63; ++qfi) {
    if (allocated_qfis.find(qfi) == allocated_qfis.end()) {
      Logger::smf_app().debug("Allocated QFI %d", qfi);
      return qfi;
    }
  }

  Logger::smf_app().error("QFI pool exhausted, cannot allocate new QFI");
  return 0;  // Indicates allocation failure
}

//------------------------------------------------------------------------------
smf_policy_report smf_policy_manager::validate_policy(
    const SmPolicyDecision& policy,
    const std::optional<std::string>& subscription_max_ul,
    const std::optional<std::string>& subscription_max_dl) {

  smf_policy_report result;
  std::set<std::string> unique_failed_pcc_rules;

  // Build a map from QoS data ID to the PCC rule IDs that reference it
  std::map<std::string, std::vector<std::string>> qos_to_pcc_rules;
  if (policy.pccRulesIsSet()) {
    const auto& rules = policy.getPccRules();
    std::set<int32_t> precedences;

    for (const auto& [rule_id, rule] : rules) {
      if (rule.refQosDataIsSet()) {
        for (const auto& qos_ref : rule.getRefQosData()) {
          qos_to_pcc_rules[qos_ref].push_back(rule_id);
        }
      }

      if (rule.precedenceIsSet()) {
        if (precedences.count(rule.getPrecedence()) > 0) {
          Logger::smf_app().warn(
                  "Duplicate PCC rule precedence %d detected", rule.getPrecedence());
        }
        precedences.insert(rule.getPrecedence());
      }
    }
  }

  // Validate QoS data and create RuleReports for failures
  if (policy.qosDecsIsSet()) {
    const auto& qos_decs = policy.getQosDecs();
    for (const auto& [qos_id, qos_data] : qos_decs) {
      bool qos_valid         = true;
      std::string fail_reason;

      // Validate 5QI is in valid range (1-255 per TS 23.501)
      int32_t fiveqi = qos_data.getR5qi();
      if (fiveqi < 1 || fiveqi > 255) {
        Logger::smf_app().error(
            "Invalid 5QI value %d in QoS data '%s'", fiveqi, qos_id.c_str());
        qos_valid   = false;
        fail_reason = "Invalid 5QI value";
      }

      // Validate ARP priority level (1-15 per TS 23.501)
      if (qos_valid && qos_data.arpIsSet()) {
        int32_t arp = qos_data.getArp().getPriorityLevel();
        if (arp < 1 || arp > 15) {
          Logger::smf_app().warn(
              "Invalid ARP priority %d in QoS data '%s'", arp, qos_id.c_str());
          qos_valid   = false;
          fail_reason = "Invalid ARP priority level";
        }
      }

      // Validate GBR flows have required bitrates (warning only)
      if (qos_valid && is_gbr_5qi(fiveqi)) {
        if (!qos_data.gbrUlIsSet() || !qos_data.gbrDlIsSet()) {
          Logger::smf_app().warn(
              "GBR 5QI %d in QoS data '%s' missing GBR bitrates", fiveqi,
              qos_id.c_str());
          // Warning only - PCF should enforce this
        }
      }

      // If QoS validation failed, create RuleReport for associated PCC rules
      if (!qos_valid) {
        // Find PCC rules that reference this QoS data
        auto it = qos_to_pcc_rules.find(qos_id);
        if (it != qos_to_pcc_rules.end() && !it->second.empty()) {
          for (const auto& pcc_rule_id : it->second) {
            Logger::smf_app().warn(
                "QoS validation failed for '%s' (%s), affecting PCC rule '%s'",
                qos_id.c_str(), fail_reason.c_str(), pcc_rule_id.c_str());
            unique_failed_pcc_rules.insert(pcc_rule_id);
          }
        } else {
          Logger::smf_app().warn(
              "QoS validation failed for '%s' (%s), but no PCC rules reference it",
              qos_id.c_str(), fail_reason.c_str());
        }
      }
    }

    // Create a RuleReport for the failed PCC rules
    if (!unique_failed_pcc_rules.empty()) {
      RuleReport rule_report;

      std::vector<std::string> failed_vector(unique_failed_pcc_rules.begin(), unique_failed_pcc_rules.end());
      rule_report.setPccRuleIds(failed_vector);

      // Set rule status to INACTIVE
      RuleStatus rule_status;
      rule_status.setEnumValue(
              RuleStatus_anyOf::eRuleStatus_anyOf::INACTIVE);
      rule_report.setRuleStatus(rule_status);

      // Set failure code to UNSUCC_QOS_VAL
      FailureCode failure_code;
      failure_code.setEnumValue(
              FailureCode_anyOf::eFailureCode_anyOf::UNSUCC_QOS_VAL);
      rule_report.setFailureCode(failure_code);

      result.rule_reports.push_back(rule_report);
      result.effected_rule_ids.insert(unique_failed_pcc_rules.begin(), unique_failed_pcc_rules.end());

      Logger::smf_app().warn(
              "QoS validation failed for %zu PCC rule(s) due to QoS data issues",
              unique_failed_pcc_rules.size());
    }
  }

  // TODO: Add bandwidth validation against subscription limits
  (void) subscription_max_ul;
  (void) subscription_max_dl;

  return result;
}

//------------------------------------------------------------------------------
bool smf_policy_manager::is_gbr_5qi(int32_t fiveqi) {
  // TS 23.501 §5.7.4 Table 5.7.4-1: GBR and Delay-Critical GBR 5QIs
  // GBR: 1, 2, 3, 4, 65, 66, 67
  // Delay-Critical GBR: 82, 83, 84, 85
  switch (fiveqi) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 65:
    case 66:
    case 67:
    case 82:
    case 83:
    case 84:
    case 85:
      return true;
    default:
      return false;
  }
}

//------------------------------------------------------------------------------
std::string smf_policy_manager::get_5qi_resource_type(int32_t fiveqi) {
  // TS 23.501 §5.7.4 Table 5.7.4-1
  switch (fiveqi) {
    // GBR
    case 1:
    case 2:
    case 3:
    case 4:
    case 65:
    case 66:
    case 67:
      return "GBR";

    // Delay-Critical GBR
    case 82:
    case 83:
    case 84:
    case 85:
      return "DELAY_CRITICAL_GBR";

    // Non-GBR (all others)
    default:
      return "NON_GBR";
  }
}

//------------------------------------------------------------------------------
bool smf_policy_manager::pcc_rules_equal(
    const PccRule& a, const PccRule& b) {
  // Compare key fields that affect UPF configuration
  if (a.getPrecedence() != b.getPrecedence()) return false;

  // Compare refQosData
  if (a.refQosDataIsSet() != b.refQosDataIsSet()) return false;
  if (a.refQosDataIsSet() && a.getRefQosData() != b.getRefQosData())
    return false;

  // Compare flow infos (SDF filters)
  if (a.flowInfosIsSet() != b.flowInfosIsSet()) return false;
  if (a.flowInfosIsSet()) {
    const auto& a_flows = a.getFlowInfos();
    const auto& b_flows = b.getFlowInfos();
    if (a_flows.size() != b_flows.size()) return false;

    // Simple comparison - could be made more sophisticated
    for (size_t i = 0; i < a_flows.size(); ++i) {
      if (a_flows[i].flowDescriptionIsSet() !=
          b_flows[i].flowDescriptionIsSet())
        return false;
      if (a_flows[i].flowDescriptionIsSet() &&
          a_flows[i].getFlowDescription() != b_flows[i].getFlowDescription())
        return false;
    }
  }

  return true;
}

//------------------------------------------------------------------------------
bool smf_policy_manager::qos_data_equal(const QosData& a, const QosData& b) {
  // Compare key QoS parameters that affect UPF configuration
  if (a.getR5qi() != b.getR5qi()) return false;

  // Compare ARP
  if (a.arpIsSet() != b.arpIsSet()) return false;
  if (a.arpIsSet()) {
    if (a.getArp().getPriorityLevel() != b.getArp().getPriorityLevel())
      return false;
  }

  // Compare bitrates
  if (a.gbrUlIsSet() != b.gbrUlIsSet()) return false;
  if (a.gbrUlIsSet() && a.getGbrUl() != b.getGbrUl()) return false;

  if (a.gbrDlIsSet() != b.gbrDlIsSet()) return false;
  if (a.gbrDlIsSet() && a.getGbrDl() != b.getGbrDl()) return false;

  if (a.maxbrUlIsSet() != b.maxbrUlIsSet()) return false;
  if (a.maxbrUlIsSet() && a.getMaxbrUl() != b.getMaxbrUl()) return false;

  if (a.maxbrDlIsSet() != b.maxbrDlIsSet()) return false;
  if (a.maxbrDlIsSet() && a.getMaxbrDl() != b.getMaxbrDl()) return false;

  return true;
}


