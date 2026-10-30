/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>

#include "3gpp_24.501.hpp"
#include "3gpp_29.244.h"
#include "UPInterfaceType.h"
#include "UPInterfaceType_anyOf.h"
#include "smf_config_types.hpp"
#include "smf_context.hpp"
#include "smf_pfcp_association.hpp"
#include "smf_procedure.hpp"
#include "smf_qos_upf_edge.hpp"

using namespace oai::app::smf;
using namespace oai::_3gpp::model;

// =============================================================================
// Anonymous Namespace: Test Factory Helpers
// =============================================================================
namespace {

std::shared_ptr<pfcp_association> make_test_upf() {
  oai::config::smf::upf upf_cfg("127.0.0.1", 8805, false, true, false, "");
  pfcp::recovery_time_stamp_t recovery_time_stamp = {};
  pfcp::up_function_features_s function_features  = {};
  // The UPF allocates the F-TEID (CH/CHID), so the builder never has to ask
  // the (absent) smf_app for one.
  function_features.ftup = 1;
  return std::make_shared<pfcp_association>(
      upf_cfg, recovery_time_stamp, function_features);
}

std::shared_ptr<qos_upf_edge> make_edge(
    UPInterfaceType_anyOf::eUPInterfaceType_anyOf type, bool uplink) {
  auto edge = std::make_shared<qos_upf_edge>();
  edge->type.setEnumValue(type);
  edge->uplink         = uplink;
  edge->precedence     = 100;
  edge->qfi.qfi        = 5;
  edge->pdr_id.rule_id = 1;
  edge->far_id.far_id  = 1;
  edge->qer_id.qer_id  = 1;
  edge->flow_information =
      upf_selection_criteria::get_default_flow_information();
  edge->source_upf = make_test_upf();
  // The PDR points at the FAR of the edge on the other side of the UPF.
  auto associated_edge           = std::make_shared<qos_upf_edge>();
  associated_edge->far_id.far_id = 2;
  associated_edge->qer_id.qer_id = 2;
  edge->associated_edge          = associated_edge;
  return edge;
}

std::shared_ptr<smf_pdu_session> make_pdu_session() {
  auto sps = std::make_shared<smf_pdu_session>(1);
  sps->pdu_session_type.pdu_session_type =
      pdu_session_type_e::PDU_SESSION_TYPE_E_IPV4;
  sps->ipv4 = true;
  return sps;
}

}  // namespace

// =============================================================================
// TS 29.244 §7.5.2.2 / §8.2.118 - Source Interface Type within the PDI
//
// The Source Interface Type IE carries a 3GPP Interface Type (Table 8.2.118-1)
// and "indicates the 3GPP interface type of the Source Interface within the
// PDR IE" (§8.2.118). It is optional in the PDI (§7.5.2.2), so for a PDI whose
// source is not a 3GPP tunnel endpoint it is left out rather than filled with
// an unrelated value: 0 is S1-U, not "unset".
// =============================================================================

TEST(SmfProcedureTest, BuildPdi_N6EdgeOmitsSourceInterfaceType) {
  auto sps = make_pdu_session();
  auto edge =
      make_edge(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N6, true);
  smf_session_procedure procedure(sps);

  pfcp::pdi pdi = procedure.pfcp_build_pdi(edge, false);

  // No 3GPP interface type describes the N6 source, so the optional IE is
  // absent. Sending it with the default-constructed value would advertise
  // 0 = S1-U (Table 8.2.118-1) for traffic coming from the data network.
  EXPECT_FALSE(pdi.source_interface_type.first)
      << "N6 PDI carries Source Interface Type "
      << static_cast<int>(
             pdi.source_interface_type.second.interface_type_value);

  // The mandatory Source Interface (Table 8.2.2-1) is what the UPF matches on
  // and stays CORE for the downlink PDR.
  ASSERT_TRUE(pdi.source_interface.first);
  EXPECT_EQ(
      static_cast<int>(pdi.source_interface.second.interface_value),
      static_cast<int>(pfcp::SOURCE_INTERFACE_VALUE_CORE));
}

TEST(SmfProcedureTest, BuildPdi_N3EdgeSetsN3AccessInterfaceType) {
  auto sps = make_pdu_session();
  auto edge =
      make_edge(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N3, false);
  smf_session_procedure procedure(sps);

  pfcp::pdi pdi = procedure.pfcp_build_pdi(edge, true);

  ASSERT_TRUE(pdi.source_interface_type.first);
  EXPECT_EQ(
      static_cast<int>(pdi.source_interface_type.second.interface_type_value),
      static_cast<int>(pfcp::_3GPP_INTERFACE_TYPE_N3_3GPP_ACCESS));
  ASSERT_TRUE(pdi.source_interface.first);
  EXPECT_EQ(
      static_cast<int>(pdi.source_interface.second.interface_value),
      static_cast<int>(pfcp::SOURCE_INTERFACE_VALUE_ACCESS));
}

TEST(SmfProcedureTest, BuildPdi_N9EdgeSetsN9InterfaceType) {
  auto sps = make_pdu_session();
  auto edge =
      make_edge(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N9, false);
  smf_session_procedure procedure(sps);

  pfcp::pdi pdi = procedure.pfcp_build_pdi(edge, true);

  ASSERT_TRUE(pdi.source_interface_type.first);
  EXPECT_EQ(
      static_cast<int>(pdi.source_interface_type.second.interface_type_value),
      static_cast<int>(pfcp::_3GPP_INTERFACE_TYPE_N9));
}

// =============================================================================
// TS 29.244 §7.5.4.2 - the PDI of an Update PDR replaces the stored one, so
// the same rule has to hold on the modification path.
// =============================================================================

TEST(SmfProcedureTest, UpdatePdr_N6EdgeOmitsSourceInterfaceType) {
  auto sps = make_pdu_session();
  auto edge =
      make_edge(UPInterfaceType_anyOf::eUPInterfaceType_anyOf::N6, true);
  smf_session_procedure procedure(sps);

  pfcp::update_pdr update_pdr = procedure.pfcp_update_pdr(edge);

  ASSERT_TRUE(update_pdr.pdi.first);
  EXPECT_FALSE(update_pdr.pdi.second.source_interface_type.first)
      << "Update PDR replaces the stored N6 PDI with one claiming "
         "Source Interface Type "
      << static_cast<int>(update_pdr.pdi.second.source_interface_type.second
                              .interface_type_value);
}
