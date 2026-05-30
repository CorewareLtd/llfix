import os
import sys
import subprocess

# Example timetamp = 18:43:31.193747800
def parse_timestamp_to_nanoseconds(timestamp):
    main_part, nanoseconds_str = timestamp.split('.')
    hours, minutes, seconds = map(int, main_part.split(':'))
    nanoseconds = int(nanoseconds_str)
    total_nanoseconds = ((hours * 3600) + (minutes * 60) + seconds) * 1_000_000_000 + nanoseconds
    return total_nanoseconds

def calculate_message_rate(timestamps, interval_in_nanoseconds):
    if not timestamps:
        return 0

    total_messages = len(timestamps)
    start_ns = timestamps[0]
    end_ns = timestamps[total_messages-1]
    time_span_ns = end_ns - start_ns

    if time_span_ns == 0:
        raise ValueError("time_span_ns must not be zero.")

    time_span_specified = time_span_ns / interval_in_nanoseconds

    if time_span_specified == 0:
        raise ValueError("time_span_specified must not be zero.")

    message_rate = total_messages / time_span_specified

    return message_rate

def main(file_path):
    lines = None

    with open(file_path, 'r') as file:
        lines = file.readlines()

    if lines is None:
        print("Failed to read the file")
        return

    try:
        timestamps = []

        for i, line in enumerate(lines):
            if line.startswith("TIMESTAMP="):
                if i + 2 < len(lines) and "35=D" in lines[i + 2]:
                    timestamp_str = line.split('=')[1].strip()
                    nanoseconds = parse_timestamp_to_nanoseconds(timestamp_str)
                    timestamps.append(nanoseconds)

        if timestamps:
            # Discard the first 1% of samples
            cutoff = int(len(timestamps) * 0.01)
            timestamps = timestamps[cutoff:]

            message_rate = calculate_message_rate(timestamps, 1000_000_000)
            print("35=D count : " + str(len(timestamps)) + " (Ignoring first 1% of samples)")
            print(f"35=D message rate: {message_rate:.2f} messages per second")
            latency_per_msg_microsecs = 1000 / (message_rate / 1000)
            print("Latency per message : " + str(latency_per_msg_microsecs) + " microseconds")

        else:
            print("No timestamps found in the file.")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    message_serialisation_path = sys.argv[1] if len(sys.argv) > 1 else "messages"

    if not os.path.isdir(message_serialisation_path):
        print(f"Error: required directory '{message_serialisation_path}' does not exist. Run the benchmark first.")
        sys.exit(1)

    messages_file = "messages.txt"

    if os.path.exists(messages_file):
        os.remove(messages_file)

    command = "./deserialiser -i ./" + message_serialisation_path + " -o " + messages_file

    if sys.platform.startswith("win"):
        command = ".\\deserialiser.exe -i .\\" + message_serialisation_path + " -o " + messages_file
    else:
        subprocess.run("sudo chmod +x ./deserialiser", shell=True, check=True)

    subprocess.run(command, shell=True, check=True)

    main(messages_file)