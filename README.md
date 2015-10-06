Ralf
==============

Overview
--------
Ralf is a component of the Metaswitch Clearwater project, designed to act as the CTF (Charging Trigger Function) for Clearwater nodes in an IMS compliant deployment. It converts JSON bodies in HTTP requests from IMS components into Diameter Rf ACRs. It uses memcached to store Rf session information for the duration of a session, and it uses [Chronos](https://github.com/Metaswitch/chronos) to send regular INTERIM ACRs to keep the session alive.

Use in Clearwater
-----------------
Both Sprout and Bono use Ralf as their CTF.

Further info
------------
* [Install guide](http://clearwater.readthedocs.org/en/stable/Installation_Instructions/index.html)
* [Development guide](docs/Development.md)
* [Ralf API guide](docs/API.md)
