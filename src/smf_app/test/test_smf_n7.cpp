/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <gtest/gtest.h>

#include "smf_n7.hpp"

using namespace oai::app::smf;

// =============================================================================
// Anonymous Namespace: Test Factory Helpers
// =============================================================================
namespace {

oai::_3gpp::model::SmPolicyContextData make_context(
    const subscribed_default_qos_t& default_qos) {
  n7::policy_association association  = {};
  snssai_t snssai                     = {};
  plmn_t plmn                         = {};
  pdu_session_type_t pdu_session_type = {};
  session_ambr_t session_ambr         = {};
  association.set_context(
      "imsi-208950000000031", "oai", snssai, plmn, 1, pdu_session_type,
      default_qos, session_ambr);
  return association.context;
}

subscribed_default_qos_t make_default_qos(
    uint8_t arp_priority_level, uint8_t priority_level) {
  subscribed_default_qos_t default_qos = {};
  default_qos._5qi                     = 9;
  default_qos.arp.priority_level       = arp_priority_level;
  default_qos.arp.preempt_cap          = "NOT_PREEMPT";
  default_qos.arp.preempt_vuln         = "PREEMPTABLE";
  default_qos.priority_level           = priority_level;
  return default_qos;
}

}  // namespace

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

// =============================================================================
// 3GPP TS 29.571 §5.4.4.1 - SubscribedDefaultQos in SmPolicyContextData
// =============================================================================

TEST(PolicyAssociationTest, SetContext_SuppliesArpAnd5qiPriorityLevels) {
  auto context = make_context(make_default_qos(8, 90));

  ASSERT_TRUE(context.subsDefQosIsSet());
  auto def_qos = context.getSubsDefQos();
  EXPECT_EQ(def_qos.getR5qi(), 9);
  // ARP priority from the ARP, not the 5QI priority level
  EXPECT_EQ(def_qos.getArp().getPriorityLevel(), 8);
  // The 5QI priority level travels in its own attribute
  ASSERT_TRUE(def_qos.priorityLevelIsSet());
  EXPECT_EQ(def_qos.getPriorityLevel(), 90);
}

TEST(PolicyAssociationTest, SetContext_OmitsUnsubscribed5qiPriorityLevel) {
  auto context = make_context(make_default_qos(8, 0));

  ASSERT_TRUE(context.subsDefQosIsSet());
  auto def_qos = context.getSubsDefQos();
  // 0 is no priority level: the 5QI's standardized value applies
  EXPECT_FALSE(def_qos.priorityLevelIsSet());
  EXPECT_EQ(def_qos.getArp().getPriorityLevel(), 8);
}
