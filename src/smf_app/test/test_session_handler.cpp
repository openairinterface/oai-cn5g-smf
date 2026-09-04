/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>

#include "3gpp_24.501.hpp"
#include "session_handler.hpp"
#include "smf_pfcp_association.hpp"

using namespace oai::app::smf;
using namespace oai::_3gpp::model;

// =============================================================================
// Anonymous Namespace: Test Factory Helpers
// =============================================================================
namespace {

std::unique_ptr<session_handler> make_session_handler(
    pdu_session_type_e type = pdu_session_type_e::PDU_SESSION_TYPE_E_IPV4) {
  auto handler = std::make_unique<session_handler>(type);

  // Initialize session graph to prevent null pointer dereference during flow
  // lookups
  auto graph = std::make_shared<upf_graph>();
  handler->set_session_graph(graph);

  return handler;
}

}  // namespace

// =============================================================================
// Release Flow Marking & Storage (TS 24.501 §9.11.4.12 & §9.11.4.13)
// =============================================================================

TEST(SessionHandlerTest, MarkQfiForRelease_CreatesAndStashesDeleteDescriptors) {
  auto handler = make_session_handler();

  pfcp::qfi_t target_qfi1;
  target_qfi1.qfi = 5;

  pfcp::qfi_t target_qfi2;
  target_qfi2.qfi = 9;

  // Mark flows for release
  handler->mark_qfi_for_release(target_qfi1);
  handler->mark_qfi_for_release(target_qfi2);

  // Retrieve stashed flows
  auto retrieved_list = handler->get_qos_flows_to_be_released();
  ASSERT_EQ(retrieved_list.size(), 2u);

  // Validate properties of the first marked flow
  const auto& release_flow = retrieved_list[0];
  EXPECT_TRUE(release_flow.to_be_removed);
  EXPECT_EQ(release_flow.qfi.qfi, 5);

  // Validate NAS QoS Rules contains a DELETE operation rule
  if (!release_flow.qos_rules.empty()) {
    const auto& qos_rule = release_flow.qos_rules.begin()->second;
    EXPECT_EQ(
        qos_rule.GetRuleOperationCode(),
        oai::nas::kQosRuleRuleOperationCodeDeleteExistingQosRule);
  }

  // Validate NAS QoS Flow Description contains DELETE operation
  const auto& flow_desc = release_flow.get_qos_flow_descriptions();
  EXPECT_EQ(flow_desc.GetQfi(), 5);
  EXPECT_EQ(
      flow_desc.GetOperationCode(),
      oai::nas::
          kQosFlowDescriptionRuleOperationCodeDeleteExistingQosFlowDescription);

  // Validate second marked flow
  EXPECT_EQ(retrieved_list[1].qfi.qfi, 9);
}

// =============================================================================
// Rollback Mechanics
// =============================================================================

TEST(SessionHandlerTest, ClearQosFlowsToBeReleased_EmptiesTheStashForRollback) {
  auto handler = make_session_handler();

  pfcp::qfi_t target_qfi;
  target_qfi.qfi = 7;

  // Mark a flow (simulating a PCF-initiated removal request)
  handler->mark_qfi_for_release(target_qfi);

  auto before_clear = handler->get_qos_flows_to_be_released();
  ASSERT_EQ(before_clear.size(), 1u);

  // Clear the stash (simulating a UPF N4 rejection rollback)
  handler->clear_qos_flows_to_be_released();

  auto after_clear = handler->get_qos_flows_to_be_released();
  EXPECT_TRUE(after_clear.empty());
}

// =============================================================================
// Bitrate Unit Conversions (TS 24.501)
// =============================================================================

TEST(SessionHandlerTest, ParseNasValueUnitToBps_CalculatesBitratesCorrectly) {
  // Unit = 0 (Value is in bps directly)
  EXPECT_EQ(session_handler::parse_nas_value_unit_to_bps(100, 0), 100u);

  // Unit = 1 (Multiples of 1 Kbps = 1024 bps)
  EXPECT_EQ(session_handler::parse_nas_value_unit_to_bps(1, 1), 1024u);

  // Unit = 2 (Multiples of 4 Kbps = 4096 bps)
  EXPECT_EQ(session_handler::parse_nas_value_unit_to_bps(1, 2), 4096u);
}