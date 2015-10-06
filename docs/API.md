# Ralf - API Guide

Ralf provides a RESTful API which can be used by any component wishing to send Rf billing messages.

    /call-id/<call ID>
    /call-id/<call ID>/timer-interim=true

Make a POST request to this address with a JSON body, such as the example below.

> {"peers":{"ccf":["cdf.example.com"]},"event":{"Accounting-Record-Type":1,"Acct-Interim-Interval":600,"Event-Timestamp":1444118158,"Service-Information":{"IMS-Information":{"Event-Type":{"SIP-Method":"INVITE"},"Role-Of-Node":1,"Node-Functionality":2,"User-Session-Id":"084972d9749c214876eb0ba4700ab1fa","Calling-Party-Address":["sip:6515550098@example.com"],"Called-Party-Address":"sip:6515550026@example.com","Time-Stamps":{"SIP-Request-Timestamp":1444118158,"SIP-Request-Timestamp-Fraction":92},"Inter-Operator-Identifier":[{"Originating-IOI":"example.com"}],"IMS-Charging-Identifier":"084972d9749c214876eb0ba4700ab1fa","Server-Capabilities":{"Mandatory-Capability":[],"Optional-Capability":[],"Server-Name":["sip:sprout.example.com"]},"From-Address":"<sip:6515550098@example.com>;tag=d8d2645dea83f80087c5a4b8167e59e1"}}}}

Ralf requires the `peers` object, and at least one `ccf` to be specified at the start of an Rf session (i.e. if this is a START or an EVENT ACR). Ralf uses these ccfs as the Destination-Host field on Rf messages it sends.

The `event` object contains the data for the ACR. Each field is mapped exactly to an ACR, and as such the names of each field must match the name of an ACR. Ralf rejects requests that don't contain all of the following objects:
* An `Accounting-Record-Type` object.
* A `Service-Information` object and an `IMS-Information` object underneath it.
* A `Node-Functionality` object underneath the `IMS-Information` object.
* A `Role-Of-Node` object underneath the `IMS-Information` object.

For more detailed information on the fields in an ACR, see [RFC6733](https://tools.ietf.org/html/rfc6733) and [3GPP TS32.299](http://www.3gpp.org/DynaReport/32299.htm).

The `timer-interim` API is used by Chronos to trigger an INTERIM ACR. The CDF specifies a session refresh time, and so Ralf must send INTERIM ACRs regularly to keep the session alive.
