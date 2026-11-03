/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once

#include "../common-src/sba/nf_service.hpp"

namespace oai::app::smf {

#if defined(USE_NGHTTP2_CLIENT) && USE_NGHTTP2_CLIENT
namespace client_api = oai::nghttp2;
#else
namespace client_api = oai::http;
#endif

}  // namespace oai::app::smf
