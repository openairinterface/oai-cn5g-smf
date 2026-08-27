/*
* SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "smf_msg.hpp"

using namespace oai::app::smf;

// =============================================================================
// 3GPP TS 29.512 §4.2.3.2 - HTTP PCF Notification Response Serialization
// =============================================================================

TEST(SmfMsgTest, SmPolicyUpdateNotifyResponse_DefaultsTo200Json) {
  pdu_session_sm_policy_update_notify_response response;

  EXPECT_EQ(response.get_http_code(), 200u);
  EXPECT_EQ(response.get_json_format(), "application/json");
}

TEST(SmfMsgTest, SmPolicyUpdateNotifyResponse_ToJsonSerializesAllFields) {
  pdu_session_sm_policy_update_notify_response response;

  response.set_http_code(204);
  response.set_json_format("application/json");

  nlohmann::json inner_data;
  inner_data["status"] = "SUCCESS";
  response.set_json_data(inner_data);

  nlohmann::json serialized_json;
  response.to_json(serialized_json);

  EXPECT_EQ(serialized_json["http_code"], 204);
  EXPECT_EQ(serialized_json["json_format"], "application/json");
  EXPECT_EQ(serialized_json["json_data"]["status"], "SUCCESS");
}