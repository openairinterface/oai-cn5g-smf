/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "smf_subscription.hpp"
#include "itti.hpp"
#include "smf_app.hpp"

#include "common_defs.h"
#include "itti_msg_sbi.hpp"

using namespace oai::app::smf;

extern oai::app::smf::smf_app* smf_app_inst;
extern itti_mw* itti_inst;
