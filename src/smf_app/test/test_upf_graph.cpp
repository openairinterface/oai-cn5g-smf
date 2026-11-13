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
  EXPECT_EQ(map_copy["rule-1"], std::vector<uint8_t>({5}));
  EXPECT_EQ(map_copy["rule-2"], std::vector<uint8_t>({6}));
}

TEST(UpfGraphTest, RegisterPccRuleQfi_RetainsEveryFlowQfi) {
  auto graph = make_test_graph();

  graph->register_pcc_rule_qfi("rule-multi-flow", 5);
  graph->register_pcc_rule_qfi("rule-multi-flow", 6);

  const auto map_copy = graph->get_pcc_rule_to_qfi_map();
  ASSERT_EQ(map_copy.at("rule-multi-flow").size(), 2u);
  EXPECT_EQ(map_copy.at("rule-multi-flow"), std::vector<uint8_t>({5, 6}));

  graph->release_qfi(5);
  EXPECT_EQ(
      graph->get_pcc_rule_to_qfi_map().at("rule-multi-flow"),
      std::vector<uint8_t>({6}));
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

// =============================================================================
// 3GPP TS 23.501 §5.7.1.1 - QFI allocation: a QFI is unique within a PDU
// session, so one that belongs to no flow of the session may be handed out
// again. TS 29.244 §8.2.89 bounds the value to 6 bits, i.e. 1..63 once 0 is
// reserved as the SMF's "unassigned" sentinel.
// =============================================================================

TEST(UpfGraphTest, GenerateQfi_FirstAllocationIsTheDefaultFlowQfi) {
  auto graph = make_test_graph();

  // The default QoS flow is the first flow of every PDU session and keeps the
  // QFI it has always been given.
  EXPECT_EQ(graph->generate_qfi(), 1);
  EXPECT_EQ(graph->generate_qfi(), 2);
  EXPECT_EQ(graph->generate_qfi(), 3);
}

TEST(UpfGraphTest, GenerateQfi_ReusesReleasedQfiAcrossManyAddRemoveCycles) {
  // The regression this fix is about: an AF that adds and removes one flow on
  // a UE over and over used to walk the QFI up by one per add and then fail
  // once it passed 63. The session only ever has two flows at a time here, so
  // every add must succeed.
  auto graph = make_test_graph();

  const uint8_t default_qfi = graph->generate_qfi();  // lives for the session
  ASSERT_EQ(default_qfi, 1);

  for (int cycle = 0; cycle < 200; ++cycle) {
    const uint8_t qfi = graph->generate_qfi();
    ASSERT_NE(qfi, 0) << "QFI pool exhausted on cycle " << cycle
                      << " with only one flow active";
    ASSERT_GE(qfi, 2) << "cycle " << cycle << " took the default flow's QFI";
    ASSERT_LE(qfi, 63) << "cycle " << cycle << " left the 6 bit QFI range";
    graph->release_qfi(qfi);
  }

  // Only the default flow still holds a QFI, so the other 62 are all free.
  for (int i = 0; i < 62; ++i) {
    const uint8_t qfi = graph->generate_qfi();
    ASSERT_NE(qfi, 0) << "only " << i << " QFIs left after 200 cycles";
    ASSERT_NE(qfi, default_qfi) << "handed out the default flow's QFI";
  }
  EXPECT_EQ(graph->generate_qfi(), 0);
}

TEST(UpfGraphTest, GenerateQfi_ExhaustsOnlyWhenAll63FlowsAreActive) {
  auto graph = make_test_graph();

  std::set<uint8_t> allocated;
  for (int i = 0; i < 63; ++i) {
    const uint8_t qfi = graph->generate_qfi();
    ASSERT_NE(qfi, 0) << "pool exhausted after " << i << " flows";
    EXPECT_TRUE(allocated.insert(qfi).second) << "QFI " << int(qfi) << " twice";
  }
  // 1..63, every value once
  EXPECT_EQ(allocated.size(), 63u);
  EXPECT_EQ(*allocated.begin(), 1);
  EXPECT_EQ(*allocated.rbegin(), 63);

  // Now, and only now, the pool really is exhausted.
  EXPECT_EQ(graph->generate_qfi(), 0);
  EXPECT_EQ(graph->generate_qfi(), 0);

  // Releasing one flow makes exactly one QFI available again.
  graph->release_qfi(20);
  EXPECT_EQ(graph->generate_qfi(), 20);
  EXPECT_EQ(graph->generate_qfi(), 0);
}

TEST(UpfGraphTest, GenerateQfi_WrapsRatherThanReturningAReleasedQfiAtOnce) {
  // A released QFI is the last to come back, not the first: on develop a
  // PCF-initiated removal is not signalled to the RAN or the UE, so the old
  // flow may still be configured under that QFI.
  auto graph = make_test_graph();

  for (int i = 0; i < 5; ++i) graph->generate_qfi();  // QFIs 1..5

  graph->release_qfi(2);

  // The next allocation continues after 5 instead of jumping back to 2.
  EXPECT_EQ(graph->generate_qfi(), 6);
  EXPECT_EQ(graph->generate_qfi(), 7);

  // 2 only returns once the allocator has wrapped around the whole range.
  uint8_t qfi = 0;
  for (int i = 0; i < 57; ++i) {  // 8..63, then the wrap to 2
    qfi = graph->generate_qfi();
    ASSERT_NE(qfi, 0);
  }
  EXPECT_EQ(qfi, 2);
}

TEST(UpfGraphTest, ReleaseQfi_IgnoresValuesOutsideTheSixBitRange) {
  auto graph = make_test_graph();

  const uint8_t first = graph->generate_qfi();
  ASSERT_EQ(first, 1);

  // 0 is the "unassigned" sentinel and 64+ cannot be a QFI at all; neither may
  // disturb the pool.
  graph->release_qfi(0);
  graph->release_qfi(64);
  graph->release_qfi(255);

  // Allocation carries on as if nothing had been released.
  EXPECT_EQ(graph->generate_qfi(), 2);
  EXPECT_EQ(graph->generate_qfi(), 3);

  // The range ends are themselves releasable.
  graph->release_qfi(1);
  for (int i = 0; i < 60; ++i) ASSERT_NE(graph->generate_qfi(), 0);  // 4..63
  EXPECT_EQ(graph->generate_qfi(), 1);  // wrapped onto the released QFI 1
  EXPECT_EQ(graph->generate_qfi(), 0);  // all 63 in use again
}

TEST(UpfGraphTest, CopiedGraphDoesNotReissueTheOriginalsQfis) {
  // upf_graph is copied while a subgraph is being built; the copy serves the
  // same PDU session, so it must not hand out QFIs that are already in use.
  auto graph = make_test_graph();

  ASSERT_EQ(graph->generate_qfi(), 1);
  ASSERT_EQ(graph->generate_qfi(), 2);
  ASSERT_EQ(graph->generate_qfi(), 3);

  upf_graph copy(*graph);

  EXPECT_EQ(copy.generate_qfi(), 4);
  EXPECT_EQ(copy.generate_qfi(), 5);
}
