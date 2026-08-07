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
* PVSystem generation is now recorded by EnergyMeters.

### New Features
* OpenDSS reports are now sent to a RabbitMQ stream rather than a classic queue, improving throughput.
* Added `Transformers_Get_NormHkVA` and `Transformers_Get_EmergHkVA`, plus context API and C++ wrapper declarations, for reading transformer normal and emergency kVA ratings.
* Added the `LinearYearly` solution mode (enum value 18), which runs the standard Yearly time loop using direct, constant-admittance solves.

### Enhancements
* None.

### Fixes
* Update-changelog.sh doesn't check the released tag anymore, all flows fixed accordingly.
* Handle RabbitMQ stream publishes timing out by retrying publishes.
* Refresh time-dependent power-conversion admittances before direct Yearly solves and stop sampling monitors and meters after a failed direct solve.

### Notes
* `LinearYearly` is a linear constant-admittance approximation. It ignores the iterative solution algorithm setting and reports convergence as direct numerical-solve success.
* StorageController support in `LinearYearly` is limited to controller timing and discrete Storage state transitions. Continuous kW, kvar, and percentage-rate redispatch is unsupported and emits a targeted warning.

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
