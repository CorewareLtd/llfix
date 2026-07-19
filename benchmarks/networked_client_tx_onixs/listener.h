#pragma once

#include <string>
#include <iostream>

#include <OnixS/FIXEngine/FIX/ISessionListener.h>

class Listener : public OnixS::FIX::ISessionListener
{
public:
    Listener() {}
    ~Listener() ONIXS_FIXENGINE_OVERRIDE ONIXS_FIXENGINE_DEFAULT;

    bool started() const ONIXS_FIXENGINE_NOTHROW
    {
        return m_started;
    }

    bool finished() const ONIXS_FIXENGINE_NOTHROW
    {
        return m_finished;
    }

    void onStateChange(OnixS::FIX::SessionState::Enum newState, OnixS::FIX::SessionState::Enum prevState, OnixS::FIX::Session *) ONIXS_FIXENGINE_FINAL
    {
        if(newState == OnixS::FIX::SessionState::Active)
            m_started = true;
        else if(newState == OnixS::FIX::SessionState::Disconnected)
            m_finished = true;
    }

    void onError(OnixS::FIX::ErrorReason::Enum, const std::string & description, OnixS::FIX::Session *) ONIXS_FIXENGINE_FINAL
    {
        std::cerr << "\nSession-level error:" << description << std::endl;
        m_finished = true;
    }

    void onWarning(OnixS::FIX::WarningReason::Enum, const std::string & description, OnixS::FIX::Session *) ONIXS_FIXENGINE_FINAL
    {
        std::cerr << "\nSession-level warning:" << description << std::endl;
    }

    void onInboundApplicationMsg(OnixS::FIX::Message & msg, OnixS::FIX::Session * sn) ONIXS_FIXENGINE_FINAL
    {
    }

    void onInboundSessionMsg(OnixS::FIX::Message & msg, OnixS::FIX::Session *) ONIXS_FIXENGINE_FINAL
    {
    }

private:
    bool m_started = false;
    bool m_finished = false;
};