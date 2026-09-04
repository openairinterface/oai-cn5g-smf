/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <memory>

#include "http_client.hpp"
#include "smf_config.hpp"

class itti_mw;
class smf_app;

itti_mw* itti_inst    = nullptr;
smf_app* smf_app_inst = nullptr;
std::unique_ptr<oai::config::smf::smf_config> smf_cfg;
std::shared_ptr<oai::http::http_client> http_client_inst = nullptr;