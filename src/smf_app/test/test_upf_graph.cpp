/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>

#include "smf_pfcp_association.hpp"

using namespace oai::app::smf;

// =============================================================================
// Anonymous Namespace: Test Factory Helpers
// =============================================================================
namespace {

std::shared_ptr<upf_graph> make_test_graph() {
  return std::make_shared<upf_graph>();
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