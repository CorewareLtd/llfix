#!/usr/bin/python
import signal
import time

import sys
import os
from tinyfix import *

class FixServerHandler(FixServerHandler):
    def handle(self):
        responded_to_first_test_request = False

        while True:
            current_msg = self.get_next_fix_message()
            if current_msg is not None:
                if self.server.supports_client_session(current_msg):

                    if current_msg.get_tag_value(35) == "A":
                        self.initialise_session_from_logon_message(current_msg)

                        print("Logon received : " + current_msg.to_string() + "\n")

                        logon_response = FixMessage()
                        logon_response.set_msg_type("A")

                        self.send(logon_response)
                        print("Sent logon response : " + logon_response.to_string() + "\n")

                    if current_msg.get_tag_value(35) == "1":
                        print("Test request received : " + current_msg.to_string())

                        if responded_to_first_test_request is False:
                            test_request_id = current_msg.get_tag_value(112)

                            testreq_response = FixMessage()
                            testreq_response.set_msg_type("1")
                            testreq_response.set_tag(112, test_request_id)

                            self.send(testreq_response)
                            print("Sent test request response : " + testreq_response.to_string() + "\n")

                            responded_to_first_test_request = True

def signal_handler(signal, frame):
        print('You pressed Ctrl+C!')
        sys.exit(0)

def main():
    try:
        signal.signal(signal.SIGINT, signal_handler)

        # FIX SESSION
        session = FixSession()
        session.begin_string = "FIXT.1.1"
        session.comp_id = "EXECUTOR"
        session.target_comp_id = "CLIENT1"

        # FIX SERVER
        port_number = 5001
        server = FixServer(('127.0.0.1', port_number), FixServerHandler)
        server.add_client_fix_session(session)
        server.start()

        while True:
            time.sleep(1)

    except ValueError as err:
        print(err.args)

#Entry point
if __name__ == "__main__":
    main()