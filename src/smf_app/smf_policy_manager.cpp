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
#include "sdf_conversions.hpp"

using namespace oai::app::smf;
using namespace oai::_3gpp::model;

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
    } else if (current_it->second !=  new_rule) {
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
  //TODO: only remove if the key is mapped to null value
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
    } else if (current_it->second != new_qos) {
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
policy_delta smf_policy_manager::convert_to_upf_delta(
    const smf_policy_delta& delta,
    const SmPolicyDecision& new_policy,
    std::map<std::string, uint8_t>& rule_to_qfi_map) {

  policy_delta upf_delta = {};

  // Get QoS data from new policy for looking up parameters
  std::map<std::string, QosData> qos_decs;
  if (new_policy.qosDecsIsSet()) {
    qos_decs = new_policy.getQosDecs();
  }

  for (const auto& change : delta.pcc_rule_changes) {
    // Process ADDED PCC rules -> to_add
    if (change.type == policy_change_type::ADDED  && change.new_rule.has_value()) {

    const auto& pcc_rule = change.new_rule.value();

      // Get QoS data referenced by this PCC rule
      if (!pcc_rule.refQosDataIsSet() || pcc_rule.getRefQosData().empty()) {
        Logger::smf_app().warn(
            "PCC rule '%s' has no refQosData, skipping", change.rule_id.c_str());
        continue;
      }

      // Use the first referenced QoS data
      auto qos_refs = pcc_rule.getRefQosData()[0];
      auto qos_it = qos_decs.find(qos_refs);
      if (qos_it == qos_decs.end()) {
        Logger::smf_app().warn(
            "PCC rule '%s' references unknown QoS data '%s'",
            change.rule_id.c_str(), qos_refs.c_str());
        continue;
      }
      if (!pcc_rule.flowInfosIsSet() || pcc_rule.getFlowInfos().empty()) {
        Logger::smf_app().warn("PCC rule '%s' has no flow information, skipping", change.rule_id.c_str());
        continue;
      }

      const auto& flow_infos = pcc_rule.getFlowInfos();
      for (const auto& flow_info : flow_infos) {
        // Build qos_flow_change for to_add
        qos_flow_change flow_change = {};
        flow_change.qfi = 0;  // QFI will be allocated when applying to graph
        flow_change.pcc_rule_id = change.rule_id;
        flow_change.qos_profile = qos_it->second;
        flow_change.precedence = pcc_rule.precedenceIsSet() ? pcc_rule.getPrecedence() : 255;
        flow_change.flow_information = flow_info;
        upf_delta.to_add.push_back(flow_change);
      }
      Logger::smf_app().debug(
          "convert_to_upf_delta: Added %zu flows for rule '%s' (5QI=%d)",
          flow_infos.size(), change.rule_id.c_str(), qos_it->second.getR5qi());
    }
    // Process MODIFIED PCC rules -> to_modify
    if (change.type == policy_change_type::MODIFIED && change.new_rule.has_value()) {
      // Find the existing QFI for this rule
      auto qfi_it = rule_to_qfi_map.find(change.rule_id);
      if (qfi_it == rule_to_qfi_map.end()) {
        Logger::smf_app().warn(
            "Modified PCC rule '%s' has no allocated QFI, treating as add",
            change.rule_id.c_str());
        // TODO: Could treat as add here
        continue;
      }

      const auto& pcc_rule = change.new_rule.value();

      // Get QoS data
      if (!pcc_rule.refQosDataIsSet() || pcc_rule.getRefQosData().empty()) {
        continue;
      }
      const std::string qos_ref = pcc_rule.getRefQosData()[0];
      auto qos_it = qos_decs.find(qos_ref);
      if (qos_it == qos_decs.end()) {
        continue;
      }

      qos_flow_change flow_change = {};
      flow_change.qfi = qfi_it->second;
      flow_change.pcc_rule_id = change.rule_id;
      flow_change.qos_profile = qos_it->second;
      flow_change.precedence = pcc_rule.getPrecedence();

      if (pcc_rule.flowInfosIsSet() && !pcc_rule.getFlowInfos().empty()) {
        flow_change.flow_information = pcc_rule.getFlowInfos()[0];
      }

      upf_delta.to_modify.push_back(flow_change);
      Logger::smf_app().debug(
          "convert_to_upf_delta: Modified flow QFI=%d for rule '%s'",
          qfi_it->second, change.rule_id.c_str());
    }

    // Process REMOVED PCC rules -> to_remove
    if (change.type == policy_change_type::REMOVED) {
      auto qfi_it = rule_to_qfi_map.find(change.rule_id);
      if (qfi_it == rule_to_qfi_map.end()) {
        Logger::smf_app().warn(
            "Removed PCC rule '%s' has no allocated QFI", change.rule_id.c_str());
        continue;
      }

      pfcp::qfi_t qfi_to_remove = {};
      qfi_to_remove.qfi = qfi_it->second;
      upf_delta.to_remove.push_back(qfi_to_remove);

      // Remove from map
      rule_to_qfi_map.erase(qfi_it);

      Logger::smf_app().debug(
          "convert_to_upf_delta: Removed flow QFI=%d for rule '%s'",
          qfi_to_remove.qfi, change.rule_id.c_str());
    }
  }

  Logger::smf_app().info(
      "convert_to_upf_delta: %zu to_add, %zu to_modify, %zu to_remove",
      upf_delta.to_add.size(), upf_delta.to_modify.size(),
      upf_delta.to_remove.size());

  return upf_delta;
}

//------------------------------------------------------------------------------
smf_policy_report smf_policy_manager::validate_policy(
    const SmPolicyDecision& policy,
    const smf_policy_delta& delta,
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

  // Prepare subscription max values
  uint32_t sub_max_ul = 0, sub_max_dl = 0;
  if (subscription_max_ul.has_value() || subscription_max_dl.has_value()) {
    // Compare against subscription limits
    if (subscription_max_ul.has_value()) {
      utils::sdf_conversions::parse_bitrate_string_to_unit(
          subscription_max_ul.value(), utils::sdf_conversions::bitrate_unit_e::KBPS, sub_max_ul);
    }
    if (subscription_max_dl.has_value()) {
      utils::sdf_conversions::parse_bitrate_string_to_unit(
          subscription_max_dl.value(), utils::sdf_conversions::bitrate_unit_e::KBPS, sub_max_dl);
    }
  }

  // Validate QoS data and create RuleReports for failures
  if (policy.qosDecsIsSet()) {
    uint64_t total_mbr_ul = 0, total_mbr_dl = 0;
    std::vector<std::string> mod_pcc_rules;
    std::vector<std::string> add_pcc_rules;
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

    // TODO: Session AMBR verification
    // Calculate what is occupied by current functions
    // Verify each modification if it will still within the limit
    // Verify each additional rule if it is still within the limit


    // Create a RuleReport for the failed PCC rules
    if (!unique_failed_pcc_rules.empty()) {
      std::vector<std::string> inactive_failed_rules;
      std::vector<std::string> active_failed_rules;

      for (const auto& pcc_rule_id : unique_failed_pcc_rules) {
        if (delta.modified_pcc_rules.count(pcc_rule_id) > 0) {
          active_failed_rules.push_back(pcc_rule_id);
        } else {
          inactive_failed_rules.push_back(pcc_rule_id);
        }
      }

      FailureCode failure_code;
      failure_code.setEnumValue(
              FailureCode_anyOf::eFailureCode_anyOf::UNSUCC_QOS_VAL);

      if (!inactive_failed_rules.empty()) {
        RuleReport rule_report_inactive;
        rule_report_inactive.setPccRuleIds(inactive_failed_rules);
        RuleStatus rule_status_inactive;
        rule_status_inactive.setEnumValue(
                RuleStatus_anyOf::eRuleStatus_anyOf::INACTIVE);
        rule_report_inactive.setRuleStatus(rule_status_inactive);
        rule_report_inactive.setFailureCode(failure_code);
        result.rule_reports.push_back(rule_report_inactive);
      }

      if (!active_failed_rules.empty()) {
        RuleReport rule_report_active;
        rule_report_active.setPccRuleIds(active_failed_rules);
        RuleStatus rule_status_active;
        rule_status_active.setEnumValue(
                RuleStatus_anyOf::eRuleStatus_anyOf::ACTIVE);
        rule_report_active.setRuleStatus(rule_status_active);
        rule_report_active.setFailureCode(failure_code);
        result.rule_reports.push_back(rule_report_active);
      }

      result.effected_rule_ids.insert(unique_failed_pcc_rules.begin(), unique_failed_pcc_rules.end());

      Logger::smf_app().warn(
              "QoS validation failed for %zu PCC rule(s) due to QoS data issues",
              unique_failed_pcc_rules.size());
    }
  }

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


