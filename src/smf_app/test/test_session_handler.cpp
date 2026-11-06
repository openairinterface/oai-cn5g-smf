/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>

#include <optional>

#include "3gpp_24.501.hpp"
#include "UPInterfaceType_anyOf.h"
#include "EthFlowDescription.h"
#include "FlowDirection.h"
#include "FlowDirectionRm.h"
#include "FlowInformation.h"
#include "session_handler.hpp"
#include "smf_config_types.hpp"
#include "smf_pfcp_association.hpp"
#include "smf_qos_upf_edge.hpp"

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

// An N3 (access) edge, so that get_edge_for_qfi() can find it
std::shared_ptr<qos_upf_edge> make_access_edge(uint8_t qfi, bool is_default) {
  auto edge     = std::make_shared<qos_upf_edge>();
  edge->qfi.qfi = qfi;
  edge->type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N3);
  edge->default_qos = is_default;
  return edge;
}

// A session handler whose session graph has one access edge per QFI
std::unique_ptr<session_handler> make_session_handler_with_flows(
    const std::vector<uint8_t>& qfis) {
  auto handler = std::make_unique<session_handler>(
      pdu_session_type_e::PDU_SESSION_TYPE_E_IPV4);
  auto graph = std::make_shared<upf_graph>();
  oai::config::smf::upf upf_cfg("127.0.0.1", 8805, false, true, false, "");
  auto upf = std::make_shared<pfcp_association>(upf_cfg);
  for (const auto qfi : qfis) {
    graph->add_qos_flow_edge(upf, make_access_edge(qfi, qfi == qfis.front()));
  }
  handler->set_session_graph(graph);
  return handler;
}

oai::nas::QosRule make_qos_rule(uint8_t rule_id, uint8_t qfi) {
  oai::nas::QosRule rule;
  rule.SetQosRuleId(rule_id);
  rule.SetRuleOperationCode(
      oai::nas::
          kQosRuleRuleOperationCodeModifyExistingQosRuleAndReplaceAllPacketFilters);
  rule.SetQfi(qfi);
  return rule;
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
// =============================================================================
// Resource release without a session graph
// =============================================================================

// A PDU session rejected before UPF selection (e.g. UE IP address pool
// exhausted) has no session graph. A later INITIAL_REQUEST with the same PDU
// Session ID releases that stale session, which calls deallocate_resources().
TEST(SessionHandlerTest, DeallocateResources_WithoutSessionGraph_DoesNothing) {
  session_handler handler(pdu_session_type_e::PDU_SESSION_TYPE_E_IPV4);
  ASSERT_FALSE(handler.has_session_graph());

  handler.deallocate_resources();

  EXPECT_FALSE(handler.has_session_graph());
}

// =============================================================================
// UE-requested QoS rule update (TS 24.501 §6.4.2.2, §9.11.4.13)
// =============================================================================

// The QoS rule IE carries both a QoS rule identifier and a QFI. The rule is
// bound to the QoS flow of its QFI, not to a flow whose QFI equals the rule id.
TEST(SessionHandlerTest, UpdateQosRule_UsesTheFlowOfTheRuleQfi) {
  auto handler = make_session_handler_with_flows({1, 5});
  handler->add_qos_rule(make_qos_rule(1, 5));

  auto flow = handler->update_qos_rule(make_qos_rule(1, 5));

  EXPECT_EQ(flow.qfi.qfi, 5);
}

// A UE-requested new QoS rule gets a generated rule id (e.g. 2) that is not
// the QFI of any flow. This must not crash the SMF.
TEST(
    SessionHandlerTest, UpdateQosRule_RuleIdWithoutFlowOfThatQfi_DoesNotCrash) {
  auto handler = make_session_handler_with_flows({1});
  handler->add_qos_rule(make_qos_rule(2, 1));

  auto flow = handler->update_qos_rule(make_qos_rule(2, 1));

  EXPECT_EQ(flow.qfi.qfi, 1);
}

TEST(SessionHandlerTest, UpdateQosRule_UnknownQfi_ReturnsNoFlow) {
  auto handler = make_session_handler_with_flows({1});
  handler->add_qos_rule(make_qos_rule(3, 7));

  auto flow = handler->update_qos_rule(make_qos_rule(3, 7));

  EXPECT_EQ(flow.qfi.qfi, 0);
}

// =============================================================================
// Ethernet flows: direction and address orientation (TS 29.512 §5.6.2.14,
// TS 29.514 §5.6.2.17 NOTE 2)
// =============================================================================

namespace {

// A downlink-encoded Ethernet flow: destMacAddr is the UE's
FlowInformation eth_flow(
    std::optional<FlowDirection_anyOf::eFlowDirection_anyOf> f_dir,
    std::optional<FlowDirection_anyOf::eFlowDirection_anyOf> direction) {
  EthFlowDescription eth_flow;
  eth_flow.setDestMacAddr("02-00-00-00-00-38");
  eth_flow.setEthType("88B5");
  if (f_dir.has_value()) {
    FlowDirection value;
    value.setEnumValue(f_dir.value());
    eth_flow.setFDir(value);
  }
  FlowInformation flow;
  flow.setEthFlowDescription(eth_flow);
  if (direction.has_value()) {
    FlowDirectionRm value;
    value.setEnumValue(direction.value());
    flow.setFlowDirection(value);
  }
  return flow;
}

}  // namespace

TEST(
    SessionHandlerTest,
    GetEthFlowDescription_KeepsDownlinkAddressesForDownlinkPackets) {
  const EthFlowDescription eth = session_handler::get_eth_flow_description(
      eth_flow(std::nullopt, std::nullopt), false);
  ASSERT_TRUE(eth.destMacAddrIsSet());
  EXPECT_EQ(eth.getDestMacAddr(), "02-00-00-00-00-38");
  EXPECT_FALSE(eth.sourceMacAddrIsSet());
  EXPECT_EQ(eth.getEthType(), "88B5");
}

TEST(
    SessionHandlerTest,
    GetEthFlowDescription_SwapsDownlinkAddressesForUplinkPackets) {
  // Uplink packets carry the UE's MAC as source
  const EthFlowDescription eth = session_handler::get_eth_flow_description(
      eth_flow(std::nullopt, std::nullopt), true);
  ASSERT_TRUE(eth.sourceMacAddrIsSet());
  EXPECT_EQ(eth.getSourceMacAddr(), "02-00-00-00-00-38");
  EXPECT_FALSE(eth.destMacAddrIsSet());
  EXPECT_EQ(eth.getEthType(), "88B5");
}

TEST(
    SessionHandlerTest,
    GetEthFlowDescription_KeepsUplinkAddressesForUplinkPackets) {
  // fDir UPLINK: the addresses are already in the uplink sense
  const EthFlowDescription eth = session_handler::get_eth_flow_description(
      eth_flow(FlowDirection_anyOf::eFlowDirection_anyOf::UPLINK, std::nullopt),
      true);
  ASSERT_TRUE(eth.destMacAddrIsSet());
  EXPECT_FALSE(eth.sourceMacAddrIsSet());
}

TEST(SessionHandlerTest, EthFlowDirection_FollowsFlowDirectionNotFDir) {
  // fDir DOWNLINK only states the address encoding: both directions apply
  const FlowInformation bidirectional = eth_flow(
      FlowDirection_anyOf::eFlowDirection_anyOf::DOWNLINK,
      FlowDirection_anyOf::eFlowDirection_anyOf::BIDIRECTIONAL);
  EXPECT_TRUE(session_handler::is_uplink_eth_flow_direction(bidirectional));
  EXPECT_TRUE(session_handler::is_downlink_eth_flow_direction(bidirectional));

  const FlowInformation downlink = eth_flow(
      std::nullopt, FlowDirection_anyOf::eFlowDirection_anyOf::DOWNLINK);
  EXPECT_FALSE(session_handler::is_uplink_eth_flow_direction(downlink));
  EXPECT_TRUE(session_handler::is_downlink_eth_flow_direction(downlink));

  // No flowDirection: bidirectional
  const FlowInformation unset = eth_flow(std::nullopt, std::nullopt);
  EXPECT_TRUE(session_handler::is_uplink_eth_flow_direction(unset));
  EXPECT_TRUE(session_handler::is_downlink_eth_flow_direction(unset));
}
