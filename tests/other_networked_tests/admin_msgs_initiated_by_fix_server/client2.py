#!/usr/bin/python
import time
import signal
from tinyfix import *

class MyClientHandler(FixClientHandler):
    def handle(self):
        self.received_any_test_request = False
        while True:
            if self.fix_session.state == SessionState.DISCONNECTED:
                self.connect(5)

                if self.fix_session.state == SessionState.PENDING_LOGON:
                    self.send_logon()
            else:

                current_msg = self.get_next_fix_message()

                if current_msg is None:
                    continue

                print("Received: " + current_msg.to_string() + "\n")
                msg_type = current_msg.get_tag_value(35)

                if msg_type == "A":
                    self.fix_session.state = SessionState.LOGGED_ON
                    print("Logon accepted.\n")

                if msg_type == "1":
                    if self.received_any_test_request is False:
                        id = current_msg.get_tag_value(112)
                        self.respond_to_test_request(id)

                    self.received_any_test_request = True

    def on_disconnection(self):
        print("Connection lost\n")

    def send_logon(self):
        logon = FixMessage()
        logon.set_msg_type("A")
        logon.set_tag(1137, "7")
        logon.set_tag(98, "0")
        logon.set_tag(108, str(self.fix_session.heartbeat_interval))

        self.send(logon)

    def respond_to_test_request(self, str_id):
        response = FixMessage()
        response.set_msg_type("1")
        response.set_tag(112, str_id)
        self.send(response)
        print("Sent : " + response.to_string())

def signal_handler(signal, frame):
        print('You pressed Ctrl+C!')
        sys.exit(0)

def main():
    try:
        signal.signal(signal.SIGINT, signal_handler)

        # FIX SESSION
        fix_session = FixSession()
        fix_session.begin_string = "FIXT.1.1"
        fix_session.comp_id = "CLIENT2"
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