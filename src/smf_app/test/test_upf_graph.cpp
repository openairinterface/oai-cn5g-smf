/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>

#include "UPInterfaceType.h"
#include "UPInterfaceType_anyOf.h"
#include "smf_config_types.hpp"
#include "smf_pfcp_association.hpp"
#include "smf_qos_upf_edge.hpp"

using namespace oai::app::smf;
using namespace oai::_3gpp::model;

// =============================================================================
// Anonymous Namespace: Test Factory Helpers
// =============================================================================
namespace {

std::shared_ptr<upf_graph> make_test_graph() {
  return std::make_shared<upf_graph>();
}

std::shared_ptr<pfcp_association> make_test_upf(const std::string& host) {
  oai::config::smf::upf upf_cfg(host, 8805, false, true, false, "");
  return std::make_shared<pfcp_association>(upf_cfg);
}

// An N3 (access) edge, so that get_access_edges() can observe it
std::shared_ptr<qos_upf_edge> make_access_edge(uint8_t qfi, bool is_default) {
  auto edge     = std::make_shared<qos_upf_edge>();
  edge->qfi.qfi = qfi;
  edge->type.setEnumValue(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N3);
  edge->default_qos    = is_default;
  edge->pdr_id.rule_id = qfi;
  edge->far_id.far_id  = qfi;
  edge->qer_id.qer_id  = qfi;
  return edge;
}

std::vector<uint8_t> access_qfis(const std::shared_ptr<upf_graph>& graph) {
  std::vector<uint8_t> qfis;
  for (const auto& edge : graph->get_access_edges())
    qfis.push_back(edge->qfi.qfi);
  std::sort(qfis.begin(), qfis.end());
  return qfis;
}

}  // namespace

// =============================================================================
// 3GPP TS 23.501 §5.7.1.4 - PCC Rule to QFI Registration & Map Management
// =============================================================================

TEST(UpfGraphTest, RegisterAndRetrievePccRuleQfi) {
  auto graph = make_test_graph();

  graph->register_pcc_rule_qfi("rule-internet", 5);
  graph->register_pcc_rule_qfi("rule-ims", 1);

  EXPECT_EQ(graph->get_qfi_for_pcc_rule_id("rule-internet"), 5);
  EXPECT_EQ(graph->get_qfi_for_pcc_rule_id("rule-ims"), 1);
  EXPECT_EQ(graph->get_qfi_for_pcc_rule_id("non-existent-rule"), 0);
}

TEST(UpfGraphTest, GetPccRuleToQfiMap_ReturnsCopyOfAllMappings) {
  auto graph = make_test_graph();

  graph->register_pcc_rule_qfi("rule-1", 5);
  graph->register_pcc_rule_qfi("rule-2", 6);

  auto map_copy = graph->get_pcc_rule_to_qfi_map();

  EXPECT_EQ(map_copy.size(), 2u);
  EXPECT_EQ(map_copy["rule-1"], 5);
  EXPECT_EQ(map_copy["rule-2"], 6);
}

// =============================================================================
// 3GPP TS 23.501 §5.7.1.4 - Cascade Deletion of PCC Rules on QFI Release
// =============================================================================

TEST(UpfGraphTest, ReleaseQfi_FreesQfiAndErasesMappedPccRules) {
  auto graph = make_test_graph();

  graph->register_pcc_rule_qfi("rule-1", 5);
  graph->register_pcc_rule_qfi("rule-2", 5);  // Multiple rules sharing QFI 5
  graph->register_pcc_rule_qfi("rule-3", 9);

  // Release QFI 5
  graph->release_qfi(5);

  EXPECT_EQ(graph->get_qfi_for_pcc_rule_id("rule-1"), 0);
  EXPECT_EQ(graph->get_qfi_for_pcc_rule_id("rule-2"), 0);
  EXPECT_EQ(graph->get_qfi_for_pcc_rule_id("rule-3"), 9);

  auto map_copy = graph->get_pcc_rule_to_qfi_map();
  EXPECT_EQ(map_copy.size(), 1u);
  EXPECT_EQ(map_copy.count("rule-1"), 0u);
  EXPECT_EQ(map_copy.count("rule-2"), 0u);
}
// =============================================================================
// 3GPP TS 23.501 §5.7.1 / TS 29.244 §7.5.4 - Flow removal committed to the
// local forwarding state only after the UPF accepted the N4 modification
// =============================================================================

TEST(UpfGraphTest, RemoveQosFlowEdge_DropsOnlyTheReleasedFlow) {
  auto graph = make_test_graph();
  auto upf   = make_test_upf("127.0.0.1");

  graph->add_qos_flow_edge(upf, make_access_edge(1, true));   // default flow
  graph->add_qos_flow_edge(upf, make_access_edge(5, false));  // to be released
  graph->add_qos_flow_edge(upf, make_access_edge(6, false));

  ASSERT_EQ(access_qfis(graph), (std::vector<uint8_t>{1, 5, 6}));

  EXPECT_TRUE(graph->remove_qos_flow_edge(5));

  EXPECT_EQ(access_qfis(graph), (std::vector<uint8_t>{1, 6}));
}

TEST(UpfGraphTest, RemoveQosFlowEdge_RemovesBothDirectionsOfTheFlow) {
  auto graph = make_test_graph();
  auto upf   = make_test_upf("127.0.0.1");

  auto dl    = make_access_edge(5, false);
  auto ul    = make_access_edge(5, false);
  ul->uplink = true;
  graph->add_qos_flow_edge(upf, dl);
  graph->add_qos_flow_edge(upf, ul);
  graph->add_qos_flow_edge(upf, make_access_edge(1, true));

  ASSERT_EQ(graph->get_access_edges().size(), 3u);

  EXPECT_TRUE(graph->remove_qos_flow_edge(5));

  EXPECT_EQ(access_qfis(graph), (std::vector<uint8_t>{1}));
}

TEST(UpfGraphTest, RemoveQosFlowEdge_KeepsTheDefaultQosFlow) {
  auto graph = make_test_graph();
  auto upf   = make_test_upf("127.0.0.1");

  graph->add_qos_flow_edge(upf, make_access_edge(1, true));

  // The default flow lives as long as the PDU session
  EXPECT_FALSE(graph->remove_qos_flow_edge(1));
  EXPECT_EQ(access_qfis(graph), (std::vector<uint8_t>{1}));
}

TEST(UpfGraphTest, RemoveQosFlowEdge_UnknownQfiIsANoOp) {
  auto graph = make_test_graph();
  auto upf   = make_test_upf("127.0.0.1");

  graph->add_qos_flow_edge(upf, make_access_edge(5, false));

  EXPECT_FALSE(graph->remove_qos_flow_edge(9));
  EXPECT_EQ(access_qfis(graph), (std::vector<uint8_t>{5}));
}

TEST(UpfGraphTest, ReleasedFlowLeavesNoEdgeAndNoPccRuleMapping) {
  // Mirrors what commit_staged_flow_removals() does once the UPF accepted the
  // removal: the edge, the QFI and the PCC rule mapping all go away together.
  auto graph = make_test_graph();
  auto upf   = make_test_upf("127.0.0.1");

  graph->add_qos_flow_edge(upf, make_access_edge(1, true));
  graph->add_qos_flow_edge(upf, make_access_edge(5, false));
  graph->register_pcc_rule_qfi("rule-default", 1);
  graph->register_pcc_rule_qfi("rule-to-remove", 5);

  graph->remove_qos_flow_edge(5);
  graph->release_qfi(5);

  EXPECT_EQ(access_qfis(graph), (std::vector<uint8_t>{1}));
  EXPECT_EQ(graph->get_qfi_for_pcc_rule_id("rule-to-remove"), 0);
  EXPECT_EQ(graph->get_qfi_for_pcc_rule_id("rule-default"), 1);
}
