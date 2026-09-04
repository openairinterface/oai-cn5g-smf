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