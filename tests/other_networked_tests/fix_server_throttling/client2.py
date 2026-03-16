#!/usr/bin/python
import time
import signal
from tinyfix import *

class MyClientHandler(FixClientHandler):
    def handle(self):
        self.order_id = 0
        while True:
            if self.fix_session.state == SessionState.DISCONNECTED:
                self.connect(5)

                if self.fix_session.state == SessionState.PENDING_LOGON:
                    self.send_logon()
            else:
                self.send_heartbeat_if_necessary()

                current_msg = self.get_next_fix_message()

                if current_msg is None:
                    continue

                print("Received: " + current_msg.to_string() + "\n")
                msg_type = current_msg.get_tag_value(35)

                if msg_type == "A":
                    self.fix_session.state = SessionState.LOGGED_ON
                    print("Logon accepted.\n")

                    for i in range(1, 111):
                        if self.fix_session.state == SessionState.LOGGED_ON:
                            self.send_new_order()
                        else:
                            print("Connection lost during sends")
                            break

    def on_disconnection(self):
        print("Connection lost\n")

    def send_logon(self):
        logon = FixMessage()
        logon.set_msg_type("A")
        logon.set_tag(1137, "7")
        logon.set_tag(98, "0")
        logon.set_tag(108, str(self.fix_session.heartbeat_interval))

        self.send(logon)

    def send_new_order(self):
        order = FixMessage()
        order.set_msg_type("D")

        self.order_id = self.order_id + 1
        order.set_tag(11, str(self.order_id))

        order.set_tag(38, "1")
        order.set_tag(44, "100")
        order.set_tag(40, "2")
        order.set_tag(55, "SYMB")
        order.set_tag(54, "1")
        order.set_tag(60, self.fix_session.get_current_datetime_string())

        self.send(order)
        print("Sent : " + order.to_string())

    def send_heartbeat_if_necessary(self):
        if time.time() - self.fix_session.last_sent_time >= self.fix_session.heartbeat_interval:
            self.send_heartbeat()

    def send_heartbeat(self):
        hb = FixMessage()
        hb.set_msg_type("0")

        self.send(hb)
        print("Sent Heartbeat.\n")

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
        fix_session.heartbeat_interval = 5

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