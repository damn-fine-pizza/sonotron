#pragma once

#include <string>

// cli-arrangrr's `--connect PATH` mode (docs/design/orchestrator-pipeline-
// extraction.md §17.5 Phase 3b): a PURE socket client of an already-running
// sonotron-server. This translation unit -- and everything it #includes --
// is deliberately arrangrr-free: no arrangrr/abi.hpp, no Command, no
// OutEvent, no Engine. It only ever speaks L1 TEXT out and JSONL in over
// the socket, exactly the D38 discipline apps/gui-sonotron already proves
// for the GUI, extended here to cli-arrangrr's own client half. Verify
// structurally with `grep -n '#include' client_mode.cpp` -- arrangrr/*.hpp
// must never appear.

namespace arrangrr::client {

// Connects to the UDS control socket at `sock_path`, reads L1 command
// lines from stdin (forwarding each verbatim to the server -- a plain,
// flat REPL, mirroring run_script()'s own simplicity rather than the live
// TUI's), and prints every inbound wire line plus, for each recognized
// kParamState echo, a "[mirror] <field>=<value>" summary line built from
// the locally-folded ParamStateMirror -- observable proof a gesture's
// round trip landed. On stdin EOF it drains the socket for a short grace
// period, then exits. Returns 0 on a clean session, 2 if the connection
// could not be established.
int run_connect(const std::string& sock_path);

}  // namespace arrangrr::client
