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
 * @brief Represents a QoS flow derived from PCF policy
 *
 * This structure holds all the information needed to create, modify,
 * or remove a QoS flow on the UPF based on PCF policy decisions.
 * It maps PCC rules and QoS data to the PFCP/N4 domain.
 */
struct pcf_qos_flow {
  uint8_t qfi{0};  // QoS Flow Identifier (0 = unallocated)

  // PCF policy identifiers
  std::string pcc_rule_id;   // Reference to the PCC rule
  std::string qos_data_id;   // Reference to the QoS data entry

  // QoS parameters from QosData (TS 29.512 §5.6.2.8)
  int32_t fiveqi{9};        // 5G QoS Identifier (default: best-effort)
  int32_t arp_priority{15}; // ARP priority level (1-15, 15=lowest)
  bool arp_preempt_cap{false};   // Preemption capability
  bool arp_preempt_vuln{true};   // Preemption vulnerability
  int32_t priority_level{0};     // QoS flow priority

  // Bitrates (optional, for GBR flows)
  std::optional<std::string> gbr_ul;    // Guaranteed Bit Rate UL
  std::optional<std::string> gbr_dl;    // Guaranteed Bit Rate DL
  std::optional<std::string> maxbr_ul;  // Maximum Bit Rate UL
  std::optional<std::string> maxbr_dl;  // Maximum Bit Rate DL

  // Flow description from PCC rule (TS 29.512 §5.6.2.14)
  std::vector<oai::_3gpp::model::FlowInformation> flow_descriptions;

  // Precedence from PCC rule
  int32_t precedence{255};  // Lower value = higher priority

  // State tracking
  bool is_default{false};  // True for the default QoS flow

  // PFCP rule IDs (populated after N4 establishment)
  pfcp::pdr_id_t pdr_id_ul{};
  pfcp::pdr_id_t pdr_id_dl{};
  pfcp::far_id_t far_id_ul{};
  pfcp::far_id_t far_id_dl{};
  pfcp::qer_id_t qer_id_ul{};
  pfcp::qer_id_t qer_id_dl{};

  /**
   * @brief Check if this is a GBR flow
   * @return true if GBR bitrates are set
   */
  bool is_gbr() const { return gbr_ul.has_value() || gbr_dl.has_value(); }

  /**
   * @brief Convert to QosData model object
   * @return QosData populated with this flow's parameters
   */
  oai::_3gpp::model::QosData to_qos_data() const;

  /**
   * @brief Create from QosData model object
   * @param qos_data The QosData from PCF
   * @param qos_id The QoS data ID
   * @return pcf_qos_flow populated with QosData parameters
   */
  static pcf_qos_flow from_qos_data(
      const oai::_3gpp::model::QosData& qos_data, const std::string& qos_id);
};

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
  std::vector<oai::_3gpp::model::RuleReport> rule_reports;

  // Reports for session rules that failed validation (empty for now)
  std::vector<oai::_3gpp::model::SessionRuleReport> session_rule_reports;

  // Unique set of rule IDs that are effected
  std::set<std::string> effected_rule_ids;

  /**
   * @brief Check if all rules passed validation
   * @return true if no rules failed validation
   */
  bool all_rules_valid() const { return rule_reports.empty() and session_rule_reports.empty(); }
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
   * @brief Extract QoS flows from policy decision
   *
   * Creates pcf_qos_flow objects from PCC rules and their
   * referenced QoS data entries.
   *
   * @param policy Policy decision to extract flows from
   * @return Vector of QoS flows
   *
   * Standards:
   * - TS 29.512 §5.6.2.6 (PccRule.refQosData references QosDecs)
   */
  static std::vector<pcf_qos_flow> extract_qos_flows(
      const oai::_3gpp::model::SmPolicyDecision& policy);

  /**
   * @brief Allocate a QFI for a new QoS flow
   *
   * Allocates the next available QFI from the per-session pool.
   * QFI values are 6-bit (0-63), with 0 typically reserved.
   *
   * @param allocated_qfis Set of already allocated QFIs
   * @return Allocated QFI, or 0 if pool exhausted
   *
   * Standards:
   * - TS 23.501 §5.7.1.4 (QFI allocation)
   * - TS 29.244 §8.2.89 (QFI in PFCP)
   */
  static uint8_t allocate_qfi(const std::set<uint8_t>& allocated_qfis);

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

 private:
  /**
   * @brief Compare two PCC rules for equality (ignoring rule ID)
   */
  static bool pcc_rules_equal(
      const oai::_3gpp::model::PccRule& a, const oai::_3gpp::model::PccRule& b);

  /**
   * @brief Compare two QoS data entries for equality (ignoring qosId)
   */
  static bool qos_data_equal(
      const oai::_3gpp::model::QosData& a, const oai::_3gpp::model::QosData& b);
};

}  // namespace oai::app::smf

#endif  // FILE_SMF_POLICY_MANAGER_HPP_SEEN


