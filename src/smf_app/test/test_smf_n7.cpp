/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>

#include "smf_n7.hpp"

using namespace oai::app::smf;

// =============================================================================
// 3GPP TS 29.512 §5.6.2.3 - SmPolicyContextData of Npcf_SMPolicyControl_Create
// =============================================================================

TEST(PolicyAssociationTest, SetContext_SuppliesSupportedFeatures) {
  n7::policy_association association   = {};
  snssai_t snssai                      = {};
  plmn_t plmn                          = {};
  pdu_session_type_t pdu_session_type  = {};
  subscribed_default_qos_t default_qos = {};
  session_ambr_t session_ambr          = {};
  association.set_context(
      "imsi-208950000000031", "oai", snssai, plmn, 1, pdu_session_type,
      default_qos, session_ambr);

  // The SMF supplies suppFeat in the POST, "0" when it offers no feature
  ASSERT_TRUE(association.context.suppFeatIsSet());
  EXPECT_EQ(association.context.getSuppFeat(), "0");
}
