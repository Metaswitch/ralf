Project Clearwater is backed by Metaswitch Networks.  We have discontinued active support for this project as of 1st December 2019.  The mailing list archive is available in GitHub.  All of the documentation and source code remains available for the community in GitHub.  Metaswitch’s Clearwater Core product, built on Project Clearwater, remains an active and successful commercial offering.  Please contact clearwater@metaswitch.com for more information. Note – this email is for commercial contacts with Metaswitch.  We are no longer offering support for Project Clearwater via this contact.

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
