## RELEASE NOTES

1.0.2 :

- Added new examples 'order_router' and 'otlp_prometheus'
- Correcting high availability example's configuration
- Added const_iterator for llfix::IncomingFixMessage


1.0.1 :
- Fixed potential compiler reordering issue and switched to TTAS from TAS in UserspaceSpinlock
- Additional error logging for llfix::MemoryMappedFile::flush failures

1.0.0 : Initial version