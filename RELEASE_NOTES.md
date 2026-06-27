## RELEASE NOTES

1.0.4 : 27 June 2026

- Root privilege requirement narrowed in scope: now required only when lock_pages is enabled
- TcpReactorScalable now emits a warning to stderr when the host has only one physical core
- FixServer::does_any_session_use_path no longer checks serialisation paths when message serialisation is disabled

1.0.3 : 30 May 2026

- Added new examples 'programmatic_administration' & 'docker'
- High availability message syncing throughput improvements and new reliability config: 'message_request_retry_microseconds'
- High availability renamed 'max_request_message_count_per_query_response' config to 'max_message_batch_size'
- Removed .p2align 5 from hot TX/RX loops to avoid constraining PGO users

1.0.2 : 10 May 2026

- Added new examples 'order_router' and 'otlp'
- Correcting high availability example's configuration
- Added const_iterator for llfix::IncomingFixMessage


1.0.1 : 12 April 2026

- Fixed potential compiler reordering issue and switched to TTAS from TAS in UserspaceSpinlock
- Additional error logging for llfix::MemoryMappedFile::flush failures

1.0.0 : 16 March 2026

- Initial version