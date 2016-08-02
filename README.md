# T.O.W.L.
## Telemetry over Opportunistic WiFi Links

*Note: This is an experimental project.  No warranties, functionality, or
suitability for any application are implied.*


- Near-real time GPS data via random open / captive portal hotspots
- Uses DNS recursion to send telemetry data
- Stores waypoints in RAM between successful transmissions
- Designed to be subscription-free, super-low cost telemetry device

### Hardware:
- Digistump Oak ESP8266 development board  (http://digistump.com/products/145)
- 3.3V Serial NMEA GPS module

#### Example GPS modules.
*YMMV.  Also consider eBay.*

Generic: http://www.banggood.com/1-5Hz-VK2828U7G5LF-TTL-Ublox-GPS-Module-With-Antenna-p-965540.html

uBlox: http://amzn.to/2avXoXr

#### Connection:

Connect GPS power & ground as appropriate and wire the GPS TX line to Pin 3 
(RX) on the digistump oak.  (I recommend testing the GPS module with an FTDI
serial adapter first to ensure you're receiving NMEA data at the expected baud
rate.)

Be sure to configure the GPS baud rate at the top of the towl .ino file. 
(See README)


### Server

You'll need to add an NS record to the DNS table of a domain you control,
designating a subdomain namesrever for the TOWL telemetry query catcher.

E.g. if you own the domain "MyDomain.com", you could designate a server to
receive the TOWL queries by creating a NS record for "TOWL.MyDomain.com",
pointing at the server you intend to run the catcher on.  If said server is
at IP address 1.2.3.4, then that record looks something akin to:

TOWL  IN NS  1.2.3.4

Run the PoC code on the designated server. Be sure to configure both the TOWL
devices and the server code for the "TOWL.MyDomain.com" domain name.  (See
README under each directory for instructions.)


Have fun experimenting!
K.C. -/- phreakmonkey@gmail.com

