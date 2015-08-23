INTENDED USE
============

For vehicles that do not support charge control via CAN: automatically switch on/off charging according to the current SOC level using a home automation plug.

This can be used to keep the vehicle charged when parking for long periods, and/or to terminate charging below 100%.


REQUIREMENTS
============

1. An Edimax Home Automation Smart Plug Series device, i.e.:
   - SP-1101W
   - SP-2101W
   See: http://us.edimax.com/edimax/merchandise/merchandise_list/data/edimax/us/home_automation_smart_plug/

2. Some sort of unixoid server running some sort of cron daemon (a Raspberry Pi will do).

3. Configured OVMS perl client.


CONFIGURATION
=============

1. Setup your Edimax as described in the quick setup guide.

2. Copy the "edimax_*" files into your OVMS client folder.

3. Fill in the Edimax IP address, user name and password in "edimax_config.txt"...

4. ...and set the SOC_ON and SOC_OFF levels as desired.

5. Add the "edimax_check.sh" script to your cron tab, schedule i.e. once per minute (see example crontab).

That's all.


OPERATION
=========

On each invocation "edimax_check.sh" will read the current SOC level of your car using the "status.pl" script.

If the SOC level is at or below the SOC_ON threshold configured, the Edimax plug will be switched on. If it's at or above the SOC_OFF level, the plug will be switched off.

The switching is done by the "edimax_switch.sh" script (expecting "ON" or "OFF" as it's single argument), "edimax_get.sh" outputs the current state.

To start or stop charging at other SOC levels, you can push the power button at the plug, use the App or "edimax_switch.sh".


