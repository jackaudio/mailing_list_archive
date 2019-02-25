# -*- coding: utf-8 -*-
from __future__ import print_function

#
# This client code should produce this error output to stderr:
#
#   cannot read result for request type 10 from server (Connection reset by peer)
#   cannot send event response to engine (Broken pipe)
#   cannot continue execution of the processing graph (Bad file descriptor)
#   jack_client_thread: graph error - exiting from JACK
#
# With this jackd output:
#
#   timeout waiting for client test-client to handle a port registered event
#   cannot send port registration notification to test-client (Resource temporarily unavailable)
#
# To reproduce the issue, do this in sequence:
#
#  1. In shell-1: "jackd -d dummy"
#  2. In shell-2: "python2 jack_callback_issue_demo.py"
#  3. In shell-3: "alsa_out -d hw:CARD=SB" (or whatever device name, see e.g. "aplay -L" output)
#  4. In shell-3: press Ctrl+C (to stop alsa_out)
#  5. In shell-3: "alsa_out -d hw:CARD=SB" (same exact command as in step 3)
#
# Now there should be aforementioned error output in shell-1 and shell-2.

import os, sys, time
import jack

class JackManagerTest(object):

    init = jc = None

    @staticmethod
    def p(fmt, *fmt_args, **fmt_kws):
        print(fmt.format(*fmt_args, **fmt_kws), file=sys.stderr)

    def __enter__(self):
        self.jc = jack.Client('test-client')
        self.jc.set_port_registration_callback(self.init_port)
        self.jc.activate()
        self.init = True
        return self

    def __exit__(self, *args):
        if self.init:
            self.jc.deactivate()
            self.jc.close()
            self.jc = None

    def run(self):
        assert self.init
        self.p('Loop started')
        while True: time.sleep(1)

    def init_port(self, port, port_new=None):
        self.p('Port-reg-callback: {} (new: {})', port, port_new)
        self.jc.get_all_connections(port)

with JackManagerTest() as jmt: jmt.run()
