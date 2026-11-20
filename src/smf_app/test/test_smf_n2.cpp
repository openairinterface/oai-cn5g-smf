/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>

#include "PduSessionResourceModifyRequestTransfer.hpp"
#include "smf_n2.hpp"

using namespace oai::app::smf;

// =============================================================================
// TS 38.413 §9.3.4.3 - PCF-initiated PDU Session Resource Modify Request
// Transfer
// =============================================================================

namespace {

qos_flow_context_updated make_released_flow(uint8_t qfi) {
  qos_flow_context_updated flow = {};
  flow.qfi.qfi                  = qfi;
  flow.to_be_removed            = true;
  return flow;
}

}  // namespace

// A PCF decision that only removes rules has nothing to add or modify; the
// transfer must still be built and carry the QoS Flow To Release List.
TEST(SmfN2Test, PcfModifyTransfer_RemovalOnlyCarriesReleaseList) {
  pdu_session_update_sm_context_response sm_context_res      = {};
  const std::map<uint8_t, qos_flow_context_updated> no_flows = {};
  const std::vector<qos_flow_context_updated> released       = {
      make_released_flow(2), make_released_flow(3)};

  std::string ngap_msg;
  ASSERT_TRUE(smf_n2::get_instance().create_n2_pcf_modify_request_transfer(
      sm_context_res, no_flows, released, n2_sm_info_type_e::PDU_RES_MOD_REQ,
      ngap_msg));
  ASSERT_FALSE(ngap_msg.empty());

  oai::ngap::PduSessionResourceModifyRequestTransfer transfer = {};
  ASSERT_TRUE(transfer.decode(
      reinterpret_cast<uint8_t*>(ngap_msg.data()), ngap_msg.size()));

  std::optional<oai::ngap::QosFlowListWithCause> release_list;
  transfer.getQosFlowToReleaseList(release_list);
  ASSERT_TRUE(release_list.has_value());

  std::vector<oai::ngap::QosFlowWithCauseItem> items;
  release_list.value().get(items);
  ASSERT_EQ(items.size(), 2u);
  std::vector<long> qfis;
  for (const auto& item : items) {
    oai::ngap::QosFlowIdentifier qfi = {};
    ASSERT_TRUE(item.getQosFlowIdentifier(qfi));
    qfis.push_back(qfi.get());
  }
  EXPECT_EQ(qfis, (std::vector<long>{2, 3}));
}

TEST(SmfN2Test, PcfModifyTransfer_NothingToAddOrReleaseFails) {
  pdu_session_update_sm_context_response sm_context_res = {};
  std::string ngap_msg;
  EXPECT_FALSE(smf_n2::get_instance().create_n2_pcf_modify_request_transfer(
      sm_context_res, {}, {}, n2_sm_info_type_e::PDU_RES_MOD_REQ, ngap_msg));
  EXPECT_TRUE(ngap_msg.empty());
}
