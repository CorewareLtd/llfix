#!/usr/bin/python
import time
import signal
from tinyfix import *

IS_EXITING = False

class MyClientHandler(FixClientHandler):
    def handle(self):
        while not IS_EXITING:
            if self.fix_session.state == SessionState.DISCONNECTED:
                self.connect(5)

                if self.fix_session.state == SessionState.PENDING_LOGON:
                    self.send_logon()

                    self.send_orders()
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

    def on_disconnection(self):
        print("Connection lost\n")

    def send_logon(self):
        logon = FixMessage()
        logon.set_msg_type("A")
        logon.set_tag(141, "Y")
        logon.set_tag(1137, "7")
        logon.set_tag(98, "0")
        logon.set_tag(108, str(self.fix_session.heartbeat_interval))

        self.send(logon)
        print("Sent logon : " + logon.to_string() + "\n")

    def send_orders(self):
        for i in range(1, 11):
            order = FixMessage()
            order.set_msg_type("D")
            order.set_tag(11, i)
            order.set_tag(40, "2")
            order.set_tag(55, "SYMBOL")
            order.set_tag(54, "1")
            order.set_tag(44, "100")
            self.send(order)
            print("Sent order " + str(i) + " : " + order.to_string() + "\n")

    def send_heartbeat_if_necessary(self):
        if self.fix_session.last_sent_time is not None:
            if time.time() - self.fix_session.last_sent_time >= self.fix_session.heartbeat_interval:
                hb = FixMessage()
                hb.set_msg_type("0")

                self.send(hb)
                print("Sent Heartbeat.\n")

def signal_handler(signal, frame):
        global IS_EXITING
        IS_EXITING = True

def main():
    try:
        signal.signal(signal.SIGINT, signal_handler)

        # FIX SESSION
        fix_session = FixSession()
        fix_session.begin_string = "FIXT.1.1"
        fix_session.comp_id = "INBOUND1"
        fix_session.target_comp_id = "ROUTER"
        fix_session.endpoint_address = "127.0.0.1"
        fix_session.port = 5001
        #fix_session.bind_address="YOUR_NIC_ADDRESS"

        # FIX CLIENT
        client = FixClient(fix_session, MyClientHandler)
        client.start()

        while True:
            if IS_EXITING is True:
                break
            time.sleep(1)

        client.stop()

    except ValueError as err:
        print(err.args)

#Entry point
if __name__ == "__main__":
    main()