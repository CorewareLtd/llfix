#!/usr/bin/python
import time
import signal
from tinyfix import *

IS_EXITING = False

class MyFixServerHandler(FixServerHandler):
    def handle(self):
        while not IS_EXITING:
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

                    elif current_msg.get_tag_value(35) == "0":

                        print("Client heartbeat received\n")

                        heartbeat_response = FixMessage()
                        heartbeat_response.set_msg_type("0")

                        self.send(heartbeat_response)

                        print("Sent heartbeat response : " + heartbeat_response.to_string() + "\n")
                    else:
                        print("Received message : " + current_msg.to_string() + "\n")

    def on_disconnection(self):
        print("Connection lost\n")

def signal_handler(signal, frame):
        global IS_EXITING
        IS_EXITING = True

def main():
    try:
        signal.signal(signal.SIGINT, signal_handler)

        # FIX SESSIONS
        session1 = FixSession()
        session1.begin_string = "FIXT.1.1"
        session1.comp_id = "OUTBOUND2"
        session1.target_comp_id = "ROUTER"

        # FIX SERVER
        port_number = 5003
        server = FixServer(('127.0.0.1', port_number), MyFixServerHandler)
        server.add_client_fix_session(session1)
        server.start()

        while True:
            if IS_EXITING is True:
                break
            time.sleep(1)

        server.stop()

    except ValueError as err:
        print(err.args)

#Entry point
if __name__ == "__main__":
    main()