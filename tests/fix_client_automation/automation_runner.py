#!/usr/bin/python
import os
import shutil
import subprocess
import platform

def run_automation(fix_version: str, server_gap_fills_enabled: str):
    os.chdir("automation")

    for folder in ["messages_incoming", "messages_outgoing"]:
        if os.path.exists(folder):
            shutil.rmtree(folder)

    file_patterns = [
        ".store",
        "log.txt",
        "outgoing.txt",
        "incoming.txt",
        "new_orders.txt",
        "cancel_orders.txt",
        "replace_orders.txt",
        "outgoing_resend_requests.txt"
    ]

    for filename in os.listdir("."):
        for pattern in file_patterns:
            if filename.endswith(pattern):
                try:
                    os.remove(filename)
                except Exception as e:
                    print(f"Failed to delete {filename}: {e}")

    if platform.system() == "Windows":
        subprocess.run(["automation.exe", fix_version, server_gap_fills_enabled])
    else:
        subprocess.run(["chmod", "+x", "./deserialiser"])
        subprocess.run(["./automation", fix_version, server_gap_fills_enabled])