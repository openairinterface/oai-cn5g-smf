/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "FailureCode.h"
#include "FailureCode_anyOf.h"
#include "FlowInformation.h"
#include "PccRule.h"
#include "QosData.h"
#include "RuleStatus.h"
#include "RuleStatus_anyOf.h"
#include "SmPolicyDecision.h"
#include "smf_policy_manager.hpp"

using namespace oai::app::smf;
using namespace oai::_3gpp::model;

// =============================================================================
// JSON Parsing Tests
// =============================================================================

TEST(SmfPolicyManagerTest, ParsePolicyDecision_HandlesInvalidAndValidJson) {
  SmPolicyDecision decision;

  nlohmann::json invalid_json = R"({"someOtherKey": 123})"_json;
  EXPECT_FALSE(
      smf_policy_manager::parse_policy_decision(invalid_json, decision));

  nlohmann::json malformed_json = R"({"smPolicyDecision": "notAnObject"})"_json;
  EXPECT_FALSE(
      smf_policy_manager::parse_policy_decision(malformed_json, decision));

  nlohmann::json valid_wrapped = R"({
    "smPolicyDecision": {
      "pccRules": {}
    }
  })"_json;
  EXPECT_TRUE(
      smf_policy_manager::parse_policy_decision(valid_wrapped, decision));

  nlohmann::json valid_direct = R"({
    "pccRules": {}
  })"_json;
  EXPECT_TRUE(
      smf_policy_manager::parse_policy_decision(valid_direct, decision));
}

// =============================================================================
// Policy Delta Computation (`compute_delta`)
// =============================================================================

TEST(SmfPolicyManagerTest, ComputeDelta_DetectsAddedModifiedAndRemovedRules) {
  SmPolicyDecision current;
  SmPolicyDecision requested;

  PccRule rule1;
  rule1.setPrecedence(100);
  PccRule rule2;
  rule2.setPrecedence(200);

  current.setPccRules({{"rule-1", rule1}, {"rule-2", rule2}});

  // Requested has rule-1 modified (precedence 150), rule-2 removed, rule-3
  // added
  PccRule rule1_mod;
  rule1_mod.setPrecedence(150);
  PccRule rule3_new;
  rule3_new.setPrecedence(300);

  requested.setPccRules({{"rule-1", rule1_mod}, {"rule-3", rule3_new}});

  smf_policy_delta delta =
      smf_policy_manager::compute_delta(current, requested);

  EXPECT_TRUE(delta.requires_upf_update());
  EXPECT_EQ(delta.added_pcc_rules.size(), 1u);
  EXPECT_EQ(delta.modified_pcc_rules.size(), 1u);
  EXPECT_EQ(delta.removed_pcc_rules.size(), 1u);

  EXPECT_TRUE(delta.added_pcc_rules.count("rule-3"));
  EXPECT_TRUE(delta.modified_pcc_rules.count("rule-1"));
  EXPECT_TRUE(delta.removed_pcc_rules.count("rule-2"));
}

// =============================================================================
// UPF Delta Translation (`convert_to_upf_delta`)
// =============================================================================

TEST(SmfPolicyManagerTest, ConvertToUpfDelta_SkipsRuleWithUnmatchedQosRef) {
  smf_policy_delta delta;
  pcc_rule_change change;
  change.type    = policy_change_type::ADDED;
  change.rule_id = "rule-unmatched";

  PccRule rule;
  rule.setRefQosData({"qos-nonexistent"});
  FlowInformation flow_info;
  flow_info.setFlowDescription("permit out ip from any to any");
  rule.setFlowInfos({flow_info});
  change.new_rule = rule;
  delta.pcc_rule_changes.push_back(change);

  SmPolicyDecision new_policy;  // qosDecs is empty
  std::map<std::string, uint8_t> rule_to_qfi_map;

  policy_delta upf_delta = smf_policy_manager::convert_to_upf_delta(
      delta, new_policy, rule_to_qfi_map);

  EXPECT_TRUE(upf_delta.to_add.empty());
}

TEST(
    SmfPolicyManagerTest,
    ConvertToUpfDelta_HandlesMultipleFlowInformationEntries) {
  smf_policy_delta delta;
  pcc_rule_change change;
  change.type    = policy_change_type::ADDED;
  change.rule_id = "rule-multi-flow";

  PccRule rule;
  rule.setRefQosData({"qos-1"});

  FlowInformation flow1;
  flow1.setFlowDescription("permit in ip from 10.0.0.1 to any");
  FlowInformation flow2;
  flow2.setFlowDescription("permit out ip from any to 10.0.0.1");
  rule.setFlowInfos({flow1, flow2});
  change.new_rule = rule;
  delta.pcc_rule_changes.push_back(change);

  SmPolicyDecision new_policy;
  QosData qos_data;
  qos_data.setQosId("qos-1");
  qos_data.setR5qi(5);
  std::map<std::string, QosData> qos_map = {{"qos-1", qos_data}};
  new_policy.setQosDecs(qos_map);

  std::map<std::string, uint8_t> rule_to_qfi_map;

  policy_delta upf_delta = smf_policy_manager::convert_to_upf_delta(
      delta, new_policy, rule_to_qfi_map);

  ASSERT_EQ(upf_delta.to_add.size(), 2u);
  EXPECT_EQ(upf_delta.to_add[0].pcc_rule_id, "rule-multi-flow");
  EXPECT_EQ(upf_delta.to_add[1].pcc_rule_id, "rule-multi-flow");
}

// -----------------------------------------------------------------------------
// QoS-data-only changes: a PCF snapshot can leave the PCC rule untouched and
// only change the QosData it references. The rule-driven branches see no
// change at all, so the delta has to be resolved through refQosData.
// -----------------------------------------------------------------------------

namespace {

// Builds a policy with one PCC rule 'rule-1' referencing 'qos-1'
SmPolicyDecision make_single_rule_policy(
    const std::string& gbr_ul, const std::string& gbr_dl, int32_t fiveqi = 5) {
  SmPolicyDecision policy;

  PccRule rule;
  rule.setPccRuleId("rule-1");
  rule.setPrecedence(100);
  rule.setRefQosData({"qos-1"});
  FlowInformation flow_info;
  flow_info.setFlowDescription("permit out ip from any to any");
  rule.setFlowInfos({flow_info});
  policy.setPccRules({{"rule-1", rule}});

  QosData qos_data;
  qos_data.setQosId("qos-1");
  qos_data.setR5qi(fiveqi);
  qos_data.setGbrUl(gbr_ul);
  qos_data.setGbrDl(gbr_dl);
  policy.setQosDecs({{"qos-1", qos_data}});

  return policy;
}

}  // namespace

TEST(SmfPolicyManagerTest, ConvertToUpfDelta_EmitsModifyForQosDataOnlyChange) {
  SmPolicyDecision current   = make_single_rule_policy("1 Mbps", "2 Mbps");
  SmPolicyDecision requested = make_single_rule_policy("5 Mbps", "10 Mbps");

  smf_policy_delta delta =
      smf_policy_manager::compute_delta(current, requested);

  // The PCC rule is byte-identical, only the referenced QoS data changed
  ASSERT_TRUE(delta.pcc_rule_changes.empty());
  ASSERT_EQ(delta.modified_qos_data.size(), 1u);
  EXPECT_TRUE(delta.modified_qos_data.count("qos-1"));

  std::map<std::string, uint8_t> rule_to_qfi_map = {{"rule-1", 6}};

  policy_delta upf_delta = smf_policy_manager::convert_to_upf_delta(
      delta, requested, rule_to_qfi_map);

  ASSERT_EQ(upf_delta.to_modify.size(), 1u);
  EXPECT_EQ(upf_delta.to_modify[0].qfi, 6);
  EXPECT_EQ(upf_delta.to_modify[0].pcc_rule_id, "rule-1");
  EXPECT_EQ(upf_delta.to_modify[0].qos_profile.getGbrUl(), "5 Mbps");
  EXPECT_EQ(upf_delta.to_modify[0].qos_profile.getGbrDl(), "10 Mbps");
  // Flow description and precedence come from the unchanged PCC rule
  EXPECT_EQ(upf_delta.to_modify[0].precedence, 100u);
  EXPECT_EQ(
      upf_delta.to_modify[0].flow_information.getFlowDescription(),
      "permit out ip from any to any");

  EXPECT_TRUE(upf_delta.to_add.empty());
  EXPECT_TRUE(upf_delta.to_remove.empty());
}

TEST(
    SmfPolicyManagerTest,
    ConvertToUpfDelta_QosDataOnlyChangeWithoutQfiEmitsNothing) {
  SmPolicyDecision current   = make_single_rule_policy("1 Mbps", "2 Mbps");
  SmPolicyDecision requested = make_single_rule_policy("5 Mbps", "10 Mbps");

  smf_policy_delta delta =
      smf_policy_manager::compute_delta(current, requested);

  // No QFI allocated for 'rule-1' yet
  std::map<std::string, uint8_t> rule_to_qfi_map;

  policy_delta upf_delta = smf_policy_manager::convert_to_upf_delta(
      delta, requested, rule_to_qfi_map);

  EXPECT_TRUE(upf_delta.to_add.empty());
  EXPECT_TRUE(upf_delta.to_modify.empty());
  EXPECT_TRUE(upf_delta.to_remove.empty());
}

TEST(
    SmfPolicyManagerTest,
    ConvertToUpfDelta_QosDataOnlyChangeWithoutReferencingRuleEmitsNothing) {
  SmPolicyDecision current   = make_single_rule_policy("1 Mbps", "2 Mbps");
  SmPolicyDecision requested = make_single_rule_policy("1 Mbps", "2 Mbps");

  // An extra QoS data entry no PCC rule refers to
  QosData orphan;
  orphan.setQosId("qos-orphan");
  orphan.setR5qi(9);
  auto qos_decs          = requested.getQosDecs();
  qos_decs["qos-orphan"] = orphan;
  requested.setQosDecs(qos_decs);

  smf_policy_delta delta =
      smf_policy_manager::compute_delta(current, requested);
  ASSERT_EQ(delta.added_qos_data.size(), 1u);

  std::map<std::string, uint8_t> rule_to_qfi_map = {{"rule-1", 6}};

  policy_delta upf_delta = smf_policy_manager::convert_to_upf_delta(
      delta, requested, rule_to_qfi_map);

  EXPECT_TRUE(upf_delta.to_add.empty());
  EXPECT_TRUE(upf_delta.to_modify.empty());
  EXPECT_TRUE(upf_delta.to_remove.empty());
}

TEST(
    SmfPolicyManagerTest,
    ConvertToUpfDelta_DoesNotDuplicateRuleChangedTogetherWithItsQosData) {
  SmPolicyDecision current = make_single_rule_policy("1 Mbps", "2 Mbps");

  // Both the rule (precedence) and the referenced QoS data change
  SmPolicyDecision requested = make_single_rule_policy("5 Mbps", "10 Mbps");
  auto rules                 = requested.getPccRules();
  rules["rule-1"].setPrecedence(50);
  requested.setPccRules(rules);

  smf_policy_delta delta =
      smf_policy_manager::compute_delta(current, requested);
  ASSERT_EQ(delta.modified_pcc_rules.size(), 1u);
  ASSERT_EQ(delta.modified_qos_data.size(), 1u);

  std::map<std::string, uint8_t> rule_to_qfi_map = {{"rule-1", 6}};

  policy_delta upf_delta = smf_policy_manager::convert_to_upf_delta(
      delta, requested, rule_to_qfi_map);

  // A single entry carrying the new QoS profile, not one per change source
  ASSERT_EQ(upf_delta.to_modify.size(), 1u);
  EXPECT_EQ(upf_delta.to_modify[0].qfi, 6);
  EXPECT_EQ(upf_delta.to_modify[0].precedence, 50u);
  EXPECT_EQ(upf_delta.to_modify[0].qos_profile.getGbrUl(), "5 Mbps");
}

TEST(
    SmfPolicyManagerTest,
    ConvertToUpfDelta_RemovedRuleIsNotRevivedByItsQosDataChange) {
  SmPolicyDecision current = make_single_rule_policy("1 Mbps", "2 Mbps");

  // The PCF drops the rule but keeps a (changed) QoS data entry around
  SmPolicyDecision requested = make_single_rule_policy("5 Mbps", "10 Mbps");
  requested.setPccRules({});

  smf_policy_delta delta =
      smf_policy_manager::compute_delta(current, requested);
  ASSERT_EQ(delta.removed_pcc_rules.size(), 1u);

  std::map<std::string, uint8_t> rule_to_qfi_map = {{"rule-1", 6}};

  policy_delta upf_delta = smf_policy_manager::convert_to_upf_delta(
      delta, requested, rule_to_qfi_map);

  ASSERT_EQ(upf_delta.to_remove.size(), 1u);
  EXPECT_EQ(upf_delta.to_remove[0].qfi, 6);
  EXPECT_TRUE(upf_delta.to_modify.empty());
}

TEST(SmfPolicyManagerTest, BuildQosToPccRules_IndexesAllReferences) {
  SmPolicyDecision policy;

  PccRule rule1;
  rule1.setRefQosData({"qos-1"});
  PccRule rule2;
  rule2.setRefQosData({"qos-1", "qos-2"});
  PccRule rule3;  // no refQosData at all
  policy.setPccRules({{"rule-1", rule1}, {"rule-2", rule2}, {"rule-3", rule3}});

  const auto index = smf_policy_manager::build_qos_to_pcc_rules(policy);

  ASSERT_EQ(index.size(), 2u);
  EXPECT_EQ(index.at("qos-1").size(), 2u);
  EXPECT_EQ(index.at("qos-2").size(), 1u);
  EXPECT_EQ(index.at("qos-2")[0], "rule-2");
}

// =============================================================================
// Policy Validation (`validate_policy`)
// =============================================================================

TEST(SmfPolicyManagerTest, ValidatePolicy_CatchesInvalid5QiAndArp) {
  SmPolicyDecision policy;

  PccRule rule;
  rule.setRefQosData({"qos-invalid"});
  policy.setPccRules({{"rule-1", rule}});

  QosData qos_data;
  qos_data.setQosId("qos-invalid");
  qos_data.setR5qi(0);  // Invalid (valid range: 1-255)

  Arp arp;
  arp.setPriorityLevel(16);  // Invalid (valid range: 1-15)
  qos_data.setArp(arp);

  policy.setQosDecs({{"qos-invalid", qos_data}});

  smf_policy_delta delta;

  smf_policy_report report = smf_policy_manager::validate_policy(policy, delta);

  EXPECT_FALSE(report.all_rules_valid());
  ASSERT_FALSE(report.rule_reports.empty());

  const auto& rule_report = report.rule_reports[0];
  EXPECT_EQ(
      rule_report.getFailureCode().getEnumValue(),
      FailureCode_anyOf::eFailureCode_anyOf::UNSUCC_QOS_VAL);
}

// =============================================================================
// N4 Failure Report Generation (`build_n4_failure_report`)
// =============================================================================

TEST(SmfPolicyManagerTest, BuildN4FailureReport_MapsPfcpCausesToFailureCodes) {
  policy_delta delta;

  qos_flow_change add_flow;
  add_flow.pcc_rule_id = "rule-add";
  delta.to_add.push_back(add_flow);

  qos_flow_change mod_flow;
  mod_flow.pcc_rule_id = "rule-mod";
  delta.to_modify.push_back(mod_flow);

  qos_flow_change rem_flow;
  rem_flow.pcc_rule_id = "rule-rem";
  delta.to_remove.push_back(rem_flow);

  // Test PFCP Cause 75 (No Resources Available)
  smf_policy_report report_75 =
      smf_policy_manager::build_n4_failure_report(75, delta);
  ASSERT_EQ(report_75.rule_reports.size(), 3u);

  // to_add -> INACTIVE
  EXPECT_EQ(
      report_75.rule_reports[0].getRuleStatus().getEnumValue(),
      RuleStatus_anyOf::eRuleStatus_anyOf::INACTIVE);
  EXPECT_EQ(
      report_75.rule_reports[0].getFailureCode().getEnumValue(),
      FailureCode_anyOf::eFailureCode_anyOf::RES_ALLO_FAIL);

  // to_modify -> ACTIVE
  EXPECT_EQ(
      report_75.rule_reports[1].getRuleStatus().getEnumValue(),
      RuleStatus_anyOf::eRuleStatus_anyOf::ACTIVE);

  // to_remove -> ACTIVE
  EXPECT_EQ(
      report_75.rule_reports[2].getRuleStatus().getEnumValue(),
      RuleStatus_anyOf::eRuleStatus_anyOf::ACTIVE);

  // Test PFCP Cause 81 -> APP_ID_ERR
  smf_policy_report report_77 =
      smf_policy_manager::build_n4_failure_report(77, delta);
  EXPECT_EQ(
      report_77.rule_reports[0].getFailureCode().getEnumValue(),
      FailureCode_anyOf::eFailureCode_anyOf::AN_GW_FAILED);
}

// =============================================================================
// 5QI Helper Utilities
// =============================================================================

TEST(SmfPolicyManagerTest, HelperFunctions_GbrAndResourceType) {
  // GBR 5QIs
  EXPECT_TRUE(smf_policy_manager::is_gbr_5qi(1));
  EXPECT_TRUE(smf_policy_manager::is_gbr_5qi(82));
  EXPECT_EQ(smf_policy_manager::get_5qi_resource_type(1), "GBR");
  EXPECT_EQ(
      smf_policy_manager::get_5qi_resource_type(82), "DELAY_CRITICAL_GBR");

  // Non-GBR 5QIs
  EXPECT_FALSE(smf_policy_manager::is_gbr_5qi(5));
  EXPECT_FALSE(smf_policy_manager::is_gbr_5qi(9));
  EXPECT_EQ(smf_policy_manager::get_5qi_resource_type(5), "NON_GBR");
}