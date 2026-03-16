#ifndef _DUMMY_CLIENT_H_
#define _DUMMY_CLIENT_H_

#include <llfix/core/compiler/unused.h>
#include <llfix/core/utilities/tcp_connector.h>
#include <llfix/fix_client.h>

class DummyClient : public llfix::FixClient<llfix::TCPConnector>
{
    private:

    public:

        DummyClient()
        {
            specify_repeating_group("8", 453, 448, 447, 452);
        }

        void on_execution_report(const llfix::IncomingFixMessage* message) override
        {
            LLFIX_UNUSED(message);
        }
};

#endif