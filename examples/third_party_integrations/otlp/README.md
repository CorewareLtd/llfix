# OpenTelemetry metrics integration example

This example demonstrates exporting serialised FIX message counts from llfix to an OTLP metrics endpoint for observability and metrics collection.

PREREQUISITES

curl must be installed and available on the host system.

STEPS TO RUN

1. Start your llfix application.

2. Run an OTLP metrics receiver and configure `otlp_endpoint` to point at its HTTP metrics endpoint. For example, to start Prometheus with its OTLP receiver enabled:

```bash
prometheus --config.file=/etc/prometheus/prometheus.yml --web.listen-address=0.0.0.0:9090 --web.enable-otlp-receiver
```
 
3. Update the configuration file (config.cfg) as required.

4. Build the example (make release) and run it. The example will continuously post configured sessions' message counts to the configured OTLP endpoint.
