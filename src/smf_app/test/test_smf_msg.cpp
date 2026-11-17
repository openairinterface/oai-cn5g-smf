/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "http_definitions.hpp"
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

  // The API server reads the envelope back under these very keys
  EXPECT_EQ(serialized_json[oai::http::kSbiResponseHttpResponseCode], 204);
  EXPECT_EQ(
      serialized_json[oai::http::kSbiResponseJsonData]["status"], "SUCCESS");
  EXPECT_EQ(serialized_json["json_format"], "application/json");
}

// =============================================================================
// 3GPP TS 24.501 §8.3.8 - PDU SESSION MODIFICATION REJECT
// =============================================================================

#include "3gpp_24.501.hpp"
#include "smf_n1.hpp"

// The reject echoes the PDU session ID and the PTI of the UE request and
// carries the 5GSM cause as a V IE.
TEST(SmfN1Test, PduSessionModificationReject_EchoesTheRequestHeader) {
  std::string nas = {};

  ASSERT_TRUE(smf_n1::get_instance().create_n1_pdu_session_modification_reject(
      5, 7, nas, k5gsmCause5gsQosNotAccepted));

  EXPECT_EQ(
      std::vector<uint8_t>(nas.begin(), nas.end()),
      (std::vector<uint8_t>{0x2e, 0x05, 0x07, 0xca, 0x25}));
}
