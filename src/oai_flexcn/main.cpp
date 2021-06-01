/*
 * Copyright (c) 2017 Sprint
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "async_shell_cmd.hpp"
#include "common_defs.h"
#include "itti.hpp"
#include "logger.hpp"
#include "options.hpp"
#include "pid_file.hpp"
#include "flexcn_app.hpp"
#include "smf_config.hpp"
#include "flexcn-api-server.h"
#include "pistache/endpoint.h"
#include "pistache/http.h"
#include "pistache/router.h"
#include "flexcn-http2-server.h"

#include <iostream>
#include <thread>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>  // srand
#include <unistd.h>  // get_pid(), pause()

using namespace smf;
using namespace util;
using namespace std;
using namespace oai::smf_server::api;

itti_mw* itti_inst                    = nullptr;
async_shell_cmd* async_shell_cmd_inst = nullptr;
flexcn_app* flexcn_app_inst                 = nullptr;
flexcn_config flexcn_cfg;
FLEXCNApiServer* flexcn_api_server_1     = nullptr;
smf_http2_server* flexcn_api_server_2 = nullptr;

void send_heartbeat_to_tasks(const uint32_t sequence);

//------------------------------------------------------------------------------
void send_heartbeat_to_tasks(const uint32_t sequence) {
  itti_msg_ping* itti_msg = new itti_msg_ping(TASK_FLEXCN_APP, TASK_ALL, sequence);
  std::shared_ptr<itti_msg_ping> i = std::shared_ptr<itti_msg_ping>(itti_msg);
  int ret                          = itti_inst->send_broadcast_msg(i);
  if (RETURNok != ret) {
    Logger::flexcn_app().error(
        "Could not send ITTI message %s to task TASK_ALL", i->get_msg_name());
  }
}

//------------------------------------------------------------------------------
void my_app_signal_handler(int s) {
  std::cout << "Caught signal " << s << std::endl;
  Logger::system().startup("exiting");
  itti_inst->send_terminate_msg(TASK_FLEXCN_APP);
  itti_inst->wait_tasks_end();
  std::cout << "Freeing Allocated memory..." << std::endl;
  if (async_shell_cmd_inst) delete async_shell_cmd_inst;
  async_shell_cmd_inst = nullptr;
  std::cout << "Async Shell CMD memory done." << std::endl;
  if (flexcn_api_server_1) {
    flexcn_api_server_1->shutdown();
    delete flexcn_api_server_1;
    flexcn_api_server_1 = nullptr;
  }
  if (flexcn_api_server_2) {
    flexcn_api_server_2->stop();
    delete flexcn_api_server_2;
    flexcn_api_server_2 = nullptr;
  }
  std::cout << "FLEXCN API Server memory done." << std::endl;
  if (itti_inst) delete itti_inst;
  itti_inst = nullptr;
  std::cout << "ITTI memory done." << std::endl;
  if (flexcn_app_inst) delete flexcn_app_inst;
  flexcn_app_inst = nullptr;
  std::cout << "FLEXCN APP memory done." << std::endl;
  std::cout << "Freeing Allocated memory done" << std::endl;
  exit(0);
}
//------------------------------------------------------------------------------
int main(int argc, char** argv) {
  srand(time(NULL));

  // Command line options
  if (!Options::parse(argc, argv)) {
    std::cout << "Options::parse() failed" << std::endl;
    return 1;
  }

  // Logger
  Logger::init("flexcn", Options::getlogStdout(), Options::getlogRotFilelog());
  Logger::flexcn_app().startup("Options parsed");

  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = my_app_signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  // Config
  flexcn_cfg.load(Options::getlibconfigConfig());
  flexcn_cfg.display();

  // Inter-task Interface
  itti_inst = new itti_mw();
  itti_inst->start(flexcn_cfg.itti.itti_timer_sched_params);

  // system command
  async_shell_cmd_inst =
      new async_shell_cmd(flexcn_cfg.itti.async_cmd_sched_params);

  // SMF application layer
  flexcn_app_inst = new flexcn_app(Options::getlibconfigConfig());

  // PID file
  // Currently hard-coded value. TODO: add as config option.
  string pid_file_name = get_exe_absolute_path("/var/run", flexcn_cfg.instance);
  if (!is_pid_file_lock_success(pid_file_name.c_str())) {
    Logger::flexcn_app().error("Lock PID file %s failed\n", pid_file_name.c_str());
    exit(-EDEADLK);
  }

  // SMF Pistache API server (HTTP1)
  Pistache::Address addr(
      std::string(inet_ntoa(*((struct in_addr*) &flexcn_cfg.sbi.addr4))),
      Pistache::Port(flexcn_cfg.sbi.port));
  flexcn_api_server_1 = new FLEXCNApiServer(addr, flexcn_app_inst);
  flexcn_api_server_1->init(2);
  // flexcn_api_server_1->start();
  std::thread smf_http1_manager(&FLEXCNApiServer::start, flexcn_api_server_1);
  // SMF NGHTTP API server (HTTP2)
  flexcn_api_server_2 = new smf_http2_server(
      conv::toString(flexcn_cfg.sbi.addr4), flexcn_cfg.sbi_http2_port, flexcn_app_inst);
  // flexcn_api_server_2->start();
  std::thread smf_http2_manager(&smf_http2_server::start, flexcn_api_server_2);

  smf_http1_manager.join();
  smf_http2_manager.join();

  FILE* fp             = NULL;
  std::string filename = fmt::format("/tmp/flexcn_{}.status", getpid());
  fp                   = fopen(filename.c_str(), "w+");
  fprintf(fp, "STARTED\n");
  fflush(fp);
  fclose(fp);

  pause();
  return 0;
}
