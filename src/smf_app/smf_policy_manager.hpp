/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef FILE_SMF_POLICY_MANAGER_HPP_SEEN
#define FILE_SMF_POLICY_MANAGER_HPP_SEEN

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "SmPolicyDecision.h"
#include "PccRule.h"
#include "QosData.h"
#include "TrafficControlData.h"
#include "FlowInformation.h"
#include "RuleReport.h"
#include "RuleStatus.h"
#include "RuleStatus_anyOf.h"
#include "FailureCode.h"
#include "FailureCode_anyOf.h"
#include "SessionRuleReport.h"
#include "3gpp_29.244.hpp"
#include "smf_qos_upf_edge.hpp"

namespace oai::app::smf {

/**
 * @brief Types of changes detected in policy comparison
 */
enum class policy_change_type {
  ADDED,     // New rule/QoS data added
  MODIFIED,  // Existing rule/QoS data modified
  REMOVED    // Existing rule/QoS data removed
};

/**
 * @brief Represents a single change in a PCC rule
 */
struct pcc_rule_change {
  policy_change_type type;
  std::string rule_id;
  std::optional<oai::_3gpp::model::PccRule> old_rule;
  std::optional<oai::_3gpp::model::PccRule> new_rule;
};

/**
 * @brief Represents a single change in QoS data
 */
struct qos_data_change {
  policy_change_type type;
  std::string qos_id;
  std::optional<oai::_3gpp::model::QosData> old_data;
  std::optional<oai::_3gpp::model::QosData> new_data;
};

/**
 * @brief Complete policy delta between current and new policy decisions
 *
 * This structure captures all differences between the current policy
 * stored in the session and a new policy decision from the PCF.
 */
struct smf_policy_delta {
  // PCC rule changes
  std::vector<pcc_rule_change> pcc_rule_changes;

  // QoS data changes
  std::vector<qos_data_change> qos_data_changes;

  // Traffic control changes (future)
  // std::vector<traffic_control_change> traffic_control_changes;

  // Quick access to changed rule/QoS IDs
  std::set<std::string> added_pcc_rules;
  std::set<std::string> modified_pcc_rules;
  std::set<std::string> removed_pcc_rules;

  std::set<std::string> added_qos_data;
  std::set<std::string> modified_qos_data;
  std::set<std::string> removed_qos_data;

  /**
   * @brief Check if there are any changes requiring UPF update
   * @return true if any PCC rule or QoS data changed
   */
  bool requires_upf_update() const {
    return !pcc_rule_changes.empty() || !qos_data_changes.empty();
  }

  /**
   * @brief Check if the delta is empty (no changes)
   * @return true if no changes detected
   */
  bool empty() const {
    return pcc_rule_changes.empty() && qos_data_changes.empty();
  }

  /**
   * @brief Get a summary string for logging
   * @return Human-readable summary of changes
   */
  std::string to_string() const;
};

/**
 * @brief Represents a single change in a qos flow to be applied to the UPF
 */
struct qos_flow_change {
    uint8_t qfi{};                                          // QoS Flow Identifier
    std::string pcc_rule_id{};                              // PCC rule ID for mapping
    oai::_3gpp::model::QosData qos_profile{};               // 5QI, ARP, GBR/MBR
    oai::_3gpp::model::FlowInformation flow_information{};  // SDF filter
    unsigned int precedence{};
};


struct policy_delta {
    std::vector<qos_flow_change> to_add;     // new QoS flows  → Create PDR/FAR/QER
    std::vector<qos_flow_change> to_modify;  // changed flows  → Update QER/FAR/PDR
    std::vector<qos_flow_change> to_remove;  // deleted flows  → Remove PDR/FAR/QER
};


/**
 * @brief policy report for validation results
 *
 * This structure holds the validation result including individual
 * RuleReports for failed PCC rules and SessionRuleReports for failed
 *
 *
 * session rules.
 */
struct smf_policy_report {
  // Reports for PCC rules that failed validation
  std::vector<_3gpp::model::RuleReport> rule_reports;

  // Reports for session rules that failed validation (empty for now)
  std::vector<_3gpp::model::SessionRuleReport> session_rule_reports;

  // Unique set of rule IDs that are effected
  std::set<std::string> effected_rule_ids;

  /**
   * @brief Check if all rules passed validation
   * @return true if no rules failed validation
   */
  bool all_rules_valid() const { return rule_reports.empty() and session_rule_reports.empty(); }

  /**
   * @brief Merge another report into this one
   * @param other The report to merge from
   */
  void merge(const smf_policy_report& other) {
    rule_reports.insert(
        rule_reports.end(),
        other.rule_reports.begin(),
        other.rule_reports.end());
    session_rule_reports.insert(
        session_rule_reports.end(),
        other.session_rule_reports.begin(),
        other.session_rule_reports.end());
    effected_rule_ids.insert(
        other.effected_rule_ids.begin(),
        other.effected_rule_ids.end());
  }
};

/**
 * @brief Policy manager for PCF-initiated QoS modifications
 *
 * This class handles:
 * - Parsing PCF policy decisions (SmPolicyDecision)
 * - Computing deltas between current and new policies
 * - Managing QFI allocation/deallocation
 * - Validating policy decisions against subscription limits
 *
 * Standards:
 * - TS 29.512 §4.2.3.2 (PCF-initiated SM Policy Association Modification)
 * - TS 29.512 §5.6.2 (Data structures)
 * - TS 23.503 §6.1.3 (Policy Control)
 */
class smf_policy_manager {
 public:
  smf_policy_manager() = default;
  ~smf_policy_manager() = default;

  /**
   * @brief Parse SmPolicyDecision from PCF notification
   *
   * Extracts PCC rules, QoS data, and traffic control decisions
   * from the PCF's SmPolicyDecision.
   *
   * @param policy_json JSON containing smPolicyDecision
   * @param[out] policy_decision Parsed SmPolicyDecision
   * @return true if parsing successful
   *
   * Standards:
   * - TS 29.512 §5.6.2.4 (SmPolicyDecision)
   */
  static bool parse_policy_decision(
      const nlohmann::json& policy_json,
      oai::_3gpp::model::SmPolicyDecision& policy_decision);

  /**
   * @brief Compute delta between current and new policy decisions
   *
   * Compares the current policy stored in the session with a new
   * policy decision from the PCF to identify:
   * - Added PCC rules and QoS data
   * - Modified PCC rules and QoS data
   * - Removed PCC rules and QoS data (NULL values in the map)
   *
   * @param current Current policy stored in session
   * @param requested New policy from PCF
   * @return smf_policy_delta containing all detected changes
   *
   * Standards:
   * - TS 29.512 §4.2.6.1 (PCC rule provisioning/removal)
   * - TS 29.512 §5.6.2.6 (PccRule - NULL means remove)
   */
  static smf_policy_delta compute_delta(
      const oai::_3gpp::model::SmPolicyDecision& current,
      const oai::_3gpp::model::SmPolicyDecision& requested);

  /**
   * @brief Convert policy delta to UPF-actionable delta
   *
   * Converts the detailed policy delta (what changed) into an actionable
   * delta for the UPF (what QFIs to add/modify/remove). This involves:
   * - Allocating QFIs for new flows
   * - Mapping PCC rules to qos_flow_change structures
   * - Identifying QFIs to remove for deleted rules
   *
   * @param delta The detailed policy delta from compute_delta()
   * @param new_policy The new policy decision (for flow descriptions)
   * @param current_qfis Current QFIs allocated on the session
   * @param rule_to_qfi_map Map of PCC rule IDs to their allocated QFIs
   * @return policy_delta with to_add, to_modify, to_remove
   *
   * Standards:
   * - TS 23.501 §5.7.1.4 (QFI allocation)
   * - TS 29.244 §5.4.4 (QoS Enforcement)
   */
  static policy_delta convert_to_upf_delta(
      const smf_policy_delta& delta,
      const oai::_3gpp::model::SmPolicyDecision& new_policy,
      std::map<std::string, uint8_t>& rule_to_qfi_map);

  /**
   * @brief Validate policy decision against subscription limits
   *
   * Checks that the policy decision is valid:
   * - Cumulative bandwidth within subscription limits
   * - PCC rule precedence values are unique
   * - 5QI values are supported
   * - ARP values are within allowed range
   *
   * @param policy Policy decision to validate
   * @param subscription_max_ul Max subscribed UL bandwidth (optional)
   * @param subscription_max_dl Max subscribed DL bandwidth (optional)
   * @return true if valid, false otherwise
   *
   * Standards:
   * - TS 23.503 §6.1.3.6 (Policy Control - QoS authorization)
   */
  static smf_policy_report validate_policy(
      const oai::_3gpp::model::SmPolicyDecision& policy,
      const smf_policy_delta& delta,
      const std::optional<std::string>& subscription_max_ul = std::nullopt,
      const std::optional<std::string>& subscription_max_dl = std::nullopt);

  /**
   * @brief Check if a 5QI value indicates a GBR flow
   *
   * @param fiveqi The 5QI value to check
   * @return true if the 5QI is for a GBR flow type
   *
   * Standards:
   * - TS 23.501 §5.7.4 (5QI characteristics table)
   */
  static bool is_gbr_5qi(int32_t fiveqi);

  /**
   * @brief Get the resource type for a 5QI value
   *
   * @param fiveqi The 5QI value
   * @return "GBR", "NON_GBR", or "DELAY_CRITICAL_GBR"
   *
   * Standards:
   * - TS 23.501 §5.7.4 (5QI to QoS characteristics mapping)
   */
  static std::string get_5qi_resource_type(int32_t fiveqi);

  /**
   * @brief Build a failure report from N4 (UPF) rejection
   *
   * Creates RuleReports for PCC rules that failed at the UPF level.
   * If specific_failed_rules is provided, only those rules are reported.
   * Otherwise, all rules in the delta are assumed to have failed.
   *
   * @param pfcp_cause The PFCP cause value from UPF response
   * @param delta The policy delta that was attempted
   * @param specific_failed_rules Optional set of specific PCC rule IDs that failed
   *        (from PFCP Failed Rule ID IE). If empty, assumes all rules failed.
   * @return smf_policy_report with RuleReports for failed rules
   *
   * Standards:
   * - TS 29.512 §5.6.3.9 (FailureCode: RES_ALLO_FAIL for UPF rejection)
   * - TS 29.244 §8.2.1 (PFCP Cause values)
   * - TS 29.244 §8.2.18 (Failed Rule ID)
   */
  static smf_policy_report build_n4_failure_report(
      uint8_t pfcp_cause,
      const policy_delta& delta);
};

}  // namespace oai::app::smf

#endif  // FILE_SMF_POLICY_MANAGER_HPP_SEEN


