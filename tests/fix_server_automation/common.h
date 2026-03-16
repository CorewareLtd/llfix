#pragma once

static constexpr int NEW_ORDER_COUNT = 1000;

static constexpr int MODE1 = 1; // Server triggers clients' resend request while it is in gap fill mode

static constexpr int MODE2 = 2; // Server triggers clients' resend request while it is in replay mode

static constexpr int MODE3 = 3; // Clients trigger server's resend request ( Some clients will be in gap fill mode and some will be in replay mode )