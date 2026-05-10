# OpenTelemetry integration example

This example demonstrates exporting serialised FIX messages from llfix to Prometheus via the OpenTelemetry Protocol (OTLP) for observability and metrics collection.

PREREQUISITES

curl must be installed and available on the host system.

STEPS TO RUN

1. Start your llfix application.

2. Run your OTLP receiver. For example to start Prometheus :

```bash
prometheus --config.file=/etc/prometheus/prometheus.yml --web.listen-address=0.0.0.0:9090 --web.enable-otlp-receiver
```
 
3. Update the configuration file (config.cfg) as required.

4. Build the example (make release) and run it. The example will continuously post configured sessions' message counts to your OTLP receiver.