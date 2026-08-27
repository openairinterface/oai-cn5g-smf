/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "3gpp_29.512.h"
#include "ErrorReport.h"
#include "PartialSuccessReport.h"
#include "RuleReport.h"
#include "RuleStatus.h"
#include "RuleStatus_anyOf.h"
#include "FailureCode.h"
#include "FailureCode_anyOf.h"
#include "smf_msg.hpp"

using namespace oai::app::smf;
using namespace oai::_3gpp::model;

// =============================================================================
// PartialSuccessReport JSON Serialization (TS 29.512 §5.6.2.5)
// =============================================================================

TEST(PcfQodWorkflowTest, PartialSuccessReport_SerializesCorrectly) {
  PartialSuccessReport report;

  RuleReport rule_report;
  rule_report.setPccRuleIds({"rule-failed-1"});

  RuleStatus status;
  status.setEnumValue(RuleStatus_anyOf::eRuleStatus_anyOf::INACTIVE);
  rule_report.setRuleStatus(status);

  FailureCode failure;
  failure.setEnumValue(FailureCode_anyOf::eFailureCode_anyOf::UNSUCC_QOS_VAL);
  rule_report.setFailureCode(failure);

  report.setRuleReports({rule_report});

  nlohmann::json report_json;
  to_json(report_json, report);
  report_json["failureCause"] = "PCC_RULE_EVENT";

  nlohmann::json response_body = nlohmann::json::array();
  response_body.push_back(report_json);

  ASSERT_TRUE(response_body.is_array());
  ASSERT_EQ(response_body.size(), 1u);
  EXPECT_EQ(response_body[0]["failureCause"], "PCC_RULE_EVENT");
  EXPECT_EQ(
      response_body[0]["ruleReports"][0]["pccRuleIds"][0], "rule-failed-1");
}

// =============================================================================
// ErrorReport Serialization for PCF Notification Failure
// =============================================================================

TEST(PcfQodWorkflowTest, ErrorReport_BuildsProblemDetailsWithRuleReports) {
  ErrorReport error_report;
  ProblemDetails details;
  details.setStatus(500);
  details.setCause("RULE_PERMANENT_ERROR");
  error_report.setError(details);

  RuleReport rule_report;
  rule_report.setPccRuleIds({"rule-add-1"});
  RuleStatus status;
  status.setEnumValue(RuleStatus_anyOf::eRuleStatus_anyOf::INACTIVE);
  rule_report.setRuleStatus(status);
  error_report.setRuleReports({rule_report});

  nlohmann::json json_data;
  to_json(json_data, error_report);

  pdu_session_sm_policy_update_notify_response response;
  response.set_json_data(json_data);
  response.set_json_format("application/problem+json");
  response.set_http_code(500);

  nlohmann::json serialized;
  response.to_json(serialized);

  EXPECT_EQ(serialized["http_code"], 500);
  EXPECT_EQ(serialized["json_format"], "application/problem+json");
  EXPECT_EQ(serialized["json_data"]["error"]["status"], 500);
  EXPECT_EQ(serialized["json_data"]["error"]["cause"], "RULE_PERMANENT_ERROR");
  EXPECT_EQ(
      serialized["json_data"]["ruleReports"][0]["pccRuleIds"][0], "rule-add-1");
}