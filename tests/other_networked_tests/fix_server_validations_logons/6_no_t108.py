#!/usr/bin/python
import sys
import time
from tinyfix import *

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from invalid_fix_message import *

class MyClientHandler(FixClientHandler):
    def handle(self):
        self.logon_attempt_count = 0
        while True:
            if self.fix_session.state == SessionState.DISCONNECTED:
                self.connect(5)

                if self.fix_session.state == SessionState.PENDING_LOGON:
                    self.send_invalid_logon()

                    time.sleep(5)
                    self.close()
                    self.connect(5)
                    self.send_valid_logon()
            else:

                current_msg = self.get_next_fix_message()

                if current_msg is None:
                    continue

                print("Received: " + current_msg.to_string() + "\n")
                msg_type = current_msg.get_tag_value(35)

                if msg_type == "A":
                    self.fix_session.state = SessionState.LOGGED_ON
                    print("Logon accepted.\n")

    def on_disconnection(self):
        print("Connection lost\n")

    def send_invalid_logon(self):
        logon = FixMessage()
        logon.set_msg_type("A")
        logon.set_tag(1137, "7")
        logon.set_tag(98, "0")
        #logon.set_tag(108, str(self.fix_session.heartbeat_interval))

        self.send(logon)

    def send_valid_logon(self):
        logon = FixMessage()
        logon.set_msg_type("A")
        logon.set_tag(1137, "7")
        logon.set_tag(98, "0")
        logon.set_tag(108, str(self.fix_session.heartbeat_interval))

        self.send(logon)

def main():
    try:
        # FIX SESSION
        fix_session = FixSession()
        fix_session.begin_string = "FIXT.1.1"
        fix_session.comp_id = "CLIENT6"
        fix_session.target_comp_id = "EXECUTOR"
        fix_session.endpoint_address = "127.0.0.1"
        fix_session.port = 5001

        # FIX CLIENT
        client = FixClient(fix_session, MyClientHandler)
        client.start()

        while True:
            time.sleep(1)

    except ValueError as err:
        print(err.args)

#Entry point
if __name__ == "__main__":
    main()