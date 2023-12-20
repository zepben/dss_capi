# Zepben DSS-CAPI changelog
## [0.13.4.1-zepben2] - UNRELEASED
### Breaking Changes
* OpenDSS reports are now sent to a RabbitMQ stream rather than a classic queue, which requires enabling the
  [stream plugin](https://www.rabbitmq.com/stream.html) and using a different port (usually 5552).
* Changed name and signature of RabbitMQ connect functions:
  * `int connect_rabbitmq(...)` &rarr; `void connect_to_stream(...)`. No exchange key is taken, and the function expects
    a port number corresponding to the TCP listener for the RabbitMQ Stream Adapter (usually 5552).
  * `int disconnect_rabbitmq()` &rarr; `void disconnect_from_stream()`.
  * Removed `int wait_for_outstanding_messages()`. `void disconnect_from_stream()` ensures all outstanding messages are
    sent before closing the connection.

### New Features
* OpenDSS reports are now sent to a RabbitMQ stream rather than a classic queue, improving throughput.

### Enhancements
* None.

### Fixes
* Update-changelog.sh doesn't check the released tag anymore, all flows fixed accordingly.

### Notes
* None.

## [0.13.4.1-zepben1] - 2024-09-03
### Breaking Changes
* Update to support dss_capi 0.13.4. This likely changes a bunch of results in subtle ways.

### New Features
* None.

### Enhancements
* None.

### Fixes
* None.

### Notes
* None.

## [0.12.1.2-zepben2] - 2024-07-26
### Breaking Changes
* None.

### New Features
* Added GH build actions

### Enhancements
* Improved the way we read publish confirms

### Fixes
* Fixed cosmetic bug where the msg/sec reported in stdout is inaccurate
* Fixed bug where the overload report was not getting populated with correct phase amp values.
* Dockerfile for Debian build now downloads Free Pascal from SourceForge instead of ftp.hu.freepascal.org,
  which wasn't working on some systems.

### Notes
* None.

## [0.12.1.1-zepben] 
### Breaking Changes

* Initial Release of the Zepben extended libs

### New Features
* Sending all reports to RabbitMQ

### Enhancements
* None.

### Fixes
* None.

### Notes
* None.

