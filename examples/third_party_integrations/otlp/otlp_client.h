#pragma once

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <chrono>
#include <vector>

class OTLPClient
{
    public:
        OTLPClient(const std::string& name, const std::string& endpoint, const std::string& auth_username, const std::string& auth_password, const std::string& temp_file_base_path="/tmp")
            : m_name(name), m_endpoint(endpoint), m_auth_username(auth_username), m_auth_password(auth_password)
        {
            m_json_file_path = temp_file_base_path + "/" + m_name + "_metrics.json";
        }

        int post_stats_metric(const std::string& param_name, uint64_t count, const std::string& units)
        {
            if (write_otlp_stats_metric_json(param_name, count, units))
            {
                std::string cmd;

                if (m_auth_username.empty() == false && m_auth_password.empty() == false)
                {
                    cmd =
                        "curl -s "
                        "-X POST '" + m_endpoint + "' "
                        "-u '" + m_auth_username + ":" + m_auth_password + "' "
                        "-H 'Content-Type: application/json' "
                        #ifdef __linux__
                        "-o /dev/null "
                        #endif
                        "--data-binary '@" + m_json_file_path + "'";
                }
                else
                {
                    cmd =
                        "curl -s "
                        "-X POST " + m_endpoint + " "
                        "-H 'Content-Type: application/json' "
                        #ifdef __linux__
                        "-o /dev/null "
                        #endif
                        "--data-binary '@" + m_json_file_path + "'";
                }

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
        std::string m_auth_username;
        std::string m_auth_password;
        std::string m_json_file_path;
        std::vector<std::string> m_errors;

        bool write_otlp_stats_metric_json(const std::string& param_name, uint64_t count, const std::string& units)
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
              "name": ")" << param_name << R"(",
              "unit": ")" << units << R"(",
              "sum": {
                "aggregationTemporality": 2,
                "isMonotonic": true,
                "dataPoints": [
                  { "asInt": )" << count << R"(, "timeUnixNano": )" << ts << R"( }
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