#pragma once

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <chrono>
#include <vector>
#include <iostream>

class OTLPClient
{
    public:
        OTLPClient(const std::string& name, const std::string& endpoint, const std::string& temp_file_base_path="/tmp")
            : m_name(name), m_endpoint(endpoint)
        {
            m_json_file_path = temp_file_base_path + "/" + m_name + "_metrics.json";
        }

        int post(const std::string& rx_session_name, uint64_t rx_count, const std::string& tx_session_name, uint64_t tx_count)
        {
            if (write_otlp_metrics_json(rx_session_name, rx_count, tx_session_name, tx_count))
            {
                const std::string cmd =
                    "curl -s "
                    "-X POST " + m_endpoint + " "
                    "-H 'Content-Type: application/json' "
                    "--data-binary @" + m_json_file_path;

                return std::system(cmd.c_str());
            }

            return 1;
        }

        std::vector<std::string>& get_errors()
        {
            return m_errors;
        }

    private:
        std::string m_name;
        std::string m_endpoint;
        std::string m_json_file_path;
        std::vector<std::string> m_errors;

        bool write_otlp_metrics_json(const std::string& rx_session_name, uint64_t rx_count, const std::string& tx_session_name, uint64_t tx_count)
        {
            using namespace std::chrono;
            uint64_t ts = duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count(); // Nanoseconds since epoch

            std::ofstream out(m_json_file_path);
            if (!out)
            {
                m_errors.push_back("Failed to open file: " + m_json_file_path);
                return false;
            }

            out << R"({
  "resourceMetrics": [
    {
      "resource": {
        "attributes": [
          { "key": "service.name", "value": { "stringValue": ")" << m_name << R"(" } }
        ]
      },
      "scopeMetrics": [
        {
          "scope": { "name": ")" << m_name << R"(" },
          "metrics": [
            {
              "name": ")" << rx_session_name << R"(",
              "unit": "messages",
              "sum": {
                "aggregationTemporality": 2,
                "isMonotonic": true,
                "dataPoints": [
                  { "asInt": )" << rx_count << R"(, "timeUnixNano": )" << ts << R"(,
                    "attributes": [
                      { "key": "session", "value": { "stringValue": ")" << rx_session_name << R"(" } },
                      { "key": "direction", "value": { "stringValue": "rx" } }
                    ]
                  }
                ]
              }
            },
            {
              "name": ")" << tx_session_name << R"(",
              "unit": "messages",
              "sum": {
                "aggregationTemporality": 2,
                "isMonotonic": true,
                "dataPoints": [
                  { "asInt": )" << tx_count << R"(, "timeUnixNano": )" << ts << R"(,
                    "attributes": [
                      { "key": "session", "value": { "stringValue": ")" << tx_session_name << R"(" } },
                      { "key": "direction", "value": { "stringValue": "tx" } }
                    ]
                  }
                ]
              }
            }
          ]
        }
      ]
    }
  ]
})";
            return true;
        }
};