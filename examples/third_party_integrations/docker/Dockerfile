# Base image
FROM rockylinux:9

# Install build dependencies and git
RUN dnf install -y \
    git \
    cmake \
    gcc-c++ \
    make \
    && dnf clean all

# Set working directory for llfix source
WORKDIR /opt/llfix

# Clone llfix source from GitHub
RUN git clone https://github.com/CorewareLtd/llfix.git .

# Build client example
WORKDIR /opt/llfix/examples/fix_trade_client/build
RUN cmake .. && make -j

# Build server example
WORKDIR /opt/llfix/examples/fix_exchange_simulator/build
RUN cmake .. && make -j

# Default command when container starts (server)
CMD ["/opt/llfix/examples/fix_exchange_simulator/build/example_fix_exchange_simulator"]