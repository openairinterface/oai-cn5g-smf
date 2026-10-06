/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Lifecycle of the SM Policy Association the SMF holds at the PCF
// (3GPP TS 29.512 section 4.2.4, TS 23.502 section 4.3.4).
//
// The SMF must terminate the association on every path that ends a PDU
// session, not only on the one that happens to be driven by a PDU Session
// Release Complete from the UE. Each association the SMF forgets about
// without sending the DELETE stays at the PCF until the PCF process ends.

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "3gpp_24.501.hpp"
#include "itti.hpp"
#include "session_handler.hpp"
#include "smf.h"
#include "smf_context.hpp"
#include "smf_n7.hpp"
#include "smf_pfcp_association.hpp"

using namespace oai::app::smf;
using namespace oai::_3gpp::model;

extern itti_mw* itti_inst;

namespace {

constexpr uint32_t kTestPcfId       = 1;
constexpr pdu_session_id_t kTestPsi = 1;

// A policy_storage that records what the SMF asks of the PCF instead of
// talking to one, so a test can assert on the DELETEs the SMF did or did not
// send.
class recording_policy_storage : public n7::policy_storage {
 public:
  // What create_policy_association() returns. Set to something other than
  // CREATED to reproduce a PCF response the SMF could not process.
  n7::sm_policy_status_code create_result = n7::sm_policy_status_code::CREATED;

  // Whether a non-CREATED create still leaves a usable Location behind. This
  // is what smf_pcf_client::create_policy_association() does when the PCF
  // answered 201 Created but the body could not be parsed: it assigns
  // pcf_location and only then returns INTERNAL_ERROR.
  bool set_location_on_failure = false;

  std::vector<std::string> created_locations;
  std::vector<std::string> deleted_locations;

  n7::sm_policy_status_code create_policy_association(
      n7::policy_association& association) override {
    if (create_result == n7::sm_policy_status_code::CREATED ||
        set_location_on_failure) {
      association.pcf_location = next_location();
      created_locations.push_back(association.pcf_location);
    }
    return create_result;
  }

  n7::sm_policy_status_code remove_policy_association(
      const n7::policy_association& association,
      const SmPolicyDeleteData& delete_data) override {
    deleted_locations.push_back(association.pcf_location);
    return n7::sm_policy_status_code::OK;
  }

  n7::sm_policy_status_code update_policy_association(
      const SmPolicyUpdateContextData& update_data,
      std::shared_ptr<n7::policy_association>& association) override {
    return n7::sm_policy_status_code::OK;
  }

  n7::sm_policy_status_code get_policy_association(
      n7::policy_association& association) override {
    return n7::sm_policy_status_code::OK;
  }

  // Associations the PCF is still holding: created minus deleted, the same
  // way the issue counts them at the PCF.
  size_t outstanding() const {
    return created_locations.size() - deleted_locations.size();
  }

 private:
  std::string next_location() {
    return "http://pcf.test/npcf-smpolicycontrol/v1/sm-policies/" +
           std::to_string(++m_next_id);
  }

  uint64_t m_next_id = 0;
};

class SmPolicyAssociationLifecycleTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Every itti_msg numbers itself off itti_inst, and the release paths send
    // their response through it. No task is registered, so send_msg() warns
    // and returns an error, which these tests do not depend on.
    itti_inst = new itti_mw();

    storage = std::make_shared<recording_policy_storage>();
    n7::smf_n7::get_instance().clear_policy_storages();
    n7::smf_n7::get_instance().set_policy_storage(kTestPcfId, storage);
  }

  void TearDown() override {
    n7::smf_n7::get_instance().clear_policy_storages();
    delete itti_inst;
    itti_inst = nullptr;
  }

  // A PDU session that holds a live association at the PCF, as it would after
  // a successful establishment.
  std::shared_ptr<smf_pdu_session> make_session_with_policy(
      const std::string& location) {
    auto sp = std::make_shared<smf_pdu_session>(kTestPsi);
    sp->dnn = "default";
    // deallocate_ressources() walks the session graph, which a session built
    // by the real establishment procedure always has.
    sp->m_session_handler = std::make_shared<session_handler>(
        pdu_session_type_e::PDU_SESSION_TYPE_E_IPV4);
    sp->m_session_handler->set_session_graph(std::make_shared<upf_graph>());

    sp->policy_ptr               = std::make_shared<n7::policy_association>();
    sp->policy_ptr->id           = 42;
    sp->policy_ptr->pcf_id       = kTestPcfId;
    sp->policy_ptr->pcf_location = location;
    storage->created_locations.push_back(location);
    return sp;
  }

  std::shared_ptr<itti_sbi_release_sm_context_request> make_release_request() {
    return std::make_shared<itti_sbi_release_sm_context_request>(
        TASK_SMF_APP, TASK_SMF_APP, 0, "1");
  }

  std::shared_ptr<itti_sbi_release_sm_context_response>
  make_accepted_release_response() {
    auto resp = std::make_shared<itti_sbi_release_sm_context_response>(
        TASK_SMF_APP, TASK_SMF_APP, 0);
    resp->res.set_cause(k5gsmCauseRequestAccepted);
    resp->res.set_dnn("default");
    resp->res.set_pdu_session_id(kTestPsi);
    return resp;
  }

  std::shared_ptr<recording_policy_storage> storage;
};

}  // namespace

// =============================================================================
// Control: the recording storage observes the DELETE the SMF already sends
// =============================================================================

// Guards the tests below from passing vacuously: if smf_n7 could not reach the
// registered storage at all, every "no DELETE was sent" assertion would hold
// for the wrong reason.
TEST_F(SmPolicyAssociationLifecycleTest, RemoveSmPolicyAssociationReachesPcf) {
  auto sp = make_session_with_policy("http://pcf.test/sm-policies/control");

  SmPolicyDeleteData delete_data;
  EXPECT_EQ(
      n7::smf_n7::get_instance().remove_sm_policy_association(
          *sp->policy_ptr, delete_data),
      n7::sm_policy_status_code::OK);

  ASSERT_EQ(storage->deleted_locations.size(), 1u);
  EXPECT_EQ(
      storage->deleted_locations[0], "http://pcf.test/sm-policies/control");
  EXPECT_EQ(storage->outstanding(), 0u);
}

// =============================================================================
// AMF-driven release (TS 23.502 section 4.3.4)
// =============================================================================

// The Release SM Context Request from the AMF runs
// session_release_sm_context_procedure as DEREGISTRATION_UE_INITIATED and ends
// in send_pdu_session_release_response(). This is the ordinary teardown path:
// no PDU Session Release Complete is involved, so the UE-driven cleanup in
// handle_pdu_session_release_complete() never runs.
TEST_F(
    SmPolicyAssociationLifecycleTest,
    AmfDrivenReleaseTerminatesPolicyAssociation) {
  auto sc = std::make_shared<smf_context>();
  auto sp = make_session_with_policy("http://pcf.test/sm-policies/amf");
  ASSERT_TRUE(sc->add_pdu_session(kTestPsi, sp));

  sc->send_pdu_session_release_response(
      make_release_request(), make_accepted_release_response(),
      session_management_procedures_type_e::DEREGISTRATION_UE_INITIATED, sp);

  EXPECT_EQ(storage->deleted_locations.size(), 1u)
      << "the SMF released the session through the AMF without telling the "
         "PCF to drop the association";
  EXPECT_EQ(storage->outstanding(), 0u);
}

// The network-requested variant of the same procedure, reached when the
// release is driven by the SMF or the PCF rather than by the UE.
TEST_F(
    SmPolicyAssociationLifecycleTest,
    AmfInitiatedReleaseTerminatesPolicyAssociation) {
  auto sc = std::make_shared<smf_context>();
  auto sp = make_session_with_policy("http://pcf.test/sm-policies/amf-init");
  ASSERT_TRUE(sc->add_pdu_session(kTestPsi, sp));

  sc->send_pdu_session_release_response(
      make_release_request(), make_accepted_release_response(),
      session_management_procedures_type_e::PDU_SESSION_RELEASE_AMF_INITIATED,
      sp);

  EXPECT_EQ(storage->deleted_locations.size(), 1u);
  EXPECT_EQ(storage->outstanding(), 0u);
}

// Both release paths reach deallocate_ressources(), which a UE that answers
// the Release Command drives twice. The association must be dropped once.
TEST_F(
    SmPolicyAssociationLifecycleTest,
    RepeatedReleaseSendsOnlyOneDeleteToThePcf) {
  auto sc = std::make_shared<smf_context>();
  auto sp = make_session_with_policy("http://pcf.test/sm-policies/twice");
  ASSERT_TRUE(sc->add_pdu_session(kTestPsi, sp));

  for (int i = 0; i < 2; ++i) {
    sc->send_pdu_session_release_response(
        make_release_request(), make_accepted_release_response(),
        session_management_procedures_type_e::DEREGISTRATION_UE_INITIATED, sp);
  }

  EXPECT_EQ(storage->deleted_locations.size(), 1u);
}

// =============================================================================
// Establishment retransmission (TS 29.502 section 5.2.2.2)
// =============================================================================

// smf_app::handle_pdu_session_create_sm_context_request() drops the previous
// session when a second INITIAL_REQUEST arrives on the same PDU session
// identity, and its comment promises to free "any associated resources in the
// UPF and PCF". Every retransmission that is dropped this way takes a live
// association with it, which is why one session leaves more than one
// association behind at the PCF.
TEST_F(
    SmPolicyAssociationLifecycleTest,
    RemovePduSessionTerminatesPolicyAssociation) {
  auto sc = std::make_shared<smf_context>();
  auto sp = make_session_with_policy("http://pcf.test/sm-policies/collision");
  ASSERT_TRUE(sc->add_pdu_session(kTestPsi, sp));

  ASSERT_TRUE(sc->remove_pdu_session(kTestPsi));

  EXPECT_EQ(storage->deleted_locations.size(), 1u)
      << "the colliding session was dropped without terminating its PCF "
         "association";
  EXPECT_EQ(storage->outstanding(), 0u);
}

// The accumulation the issue reports: establishment retransmissions on one
// identity, each one dropping the previous session.
TEST_F(
    SmPolicyAssociationLifecycleTest,
    RetransmittedEstablishmentsDoNotAccumulateAssociations) {
  auto sc = std::make_shared<smf_context>();

  for (int attempt = 0; attempt < 3; ++attempt) {
    auto sp = make_session_with_policy(
        "http://pcf.test/sm-policies/retransmission-" +
        std::to_string(attempt));
    sc->remove_pdu_session(kTestPsi);  // collision handling for the retry
    ASSERT_TRUE(sc->add_pdu_session(kTestPsi, sp));
  }
  sc->remove_pdu_session(kTestPsi);  // the session is finally torn down

  EXPECT_EQ(storage->outstanding(), 0u)
      << "associations left at the PCF after the session ended";
}

// =============================================================================
// Create response the SMF could not process (TS 29.512 section 4.2.2.2)
// =============================================================================

// smf_pcf_client::create_policy_association() assigns pcf_location from the
// Location header of a 201 Created and only then parses the body. When that
// parse fails it returns INTERNAL_ERROR, and the caller drops the association
// object. The PCF created the association, and the SMF now holds the only
// reference to it.
TEST_F(
    SmPolicyAssociationLifecycleTest,
    FailedCreateTerminatesAnAssociationThePcfCreated) {
  storage->create_result           = n7::sm_policy_status_code::INTERNAL_ERROR;
  storage->set_location_on_failure = true;

  n7::policy_association association;
  association.pcf_id = kTestPcfId;

  EXPECT_NE(
      n7::smf_n7::get_instance().create_sm_policy_association(association),
      n7::sm_policy_status_code::CREATED);

  ASSERT_EQ(storage->created_locations.size(), 1u);
  EXPECT_EQ(storage->deleted_locations.size(), 1u)
      << "the PCF created the association but the SMF never sent the DELETE";
  EXPECT_EQ(storage->outstanding(), 0u);
}

// When the response carried no Location there is nothing to address a DELETE
// to, so the SMF must not invent one.
TEST_F(
    SmPolicyAssociationLifecycleTest,
    FailedCreateWithoutLocationSendsNoDelete) {
  storage->create_result           = n7::sm_policy_status_code::INTERNAL_ERROR;
  storage->set_location_on_failure = false;

  n7::policy_association association;
  association.pcf_id = kTestPcfId;

  EXPECT_NE(
      n7::smf_n7::get_instance().create_sm_policy_association(association),
      n7::sm_policy_status_code::CREATED);

  EXPECT_TRUE(storage->deleted_locations.empty());
}
