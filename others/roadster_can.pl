#!/usr/bin/perl

#    Project:       Open Vehicle Monitor System
#    Date:          4 November 2011
#
#    Changes:
#    1.0  Initial release
#
#    (C) 2011  Mark Webb-Johnson
#
# Based on information and analysis provided by Scott451, Michael Stegen,
# and others at the Tesla Motors Club forums, as well as personal analysis
# of the CAN bus on a 2011 Tesla Roadster.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# CREDIT
# Thanks to Scott451 for figuring out many of the Roadster CAN bus messages used by the OVMS,
# and the pinout of the CAN bus socket in the Roadster.
# http://www.teslamotorsclub.com/showthread.php/4388-iPhone-app?p=49456&viewfull=1#post49456"]iPhone app
# Thanks to fuzzylogic for further analysis and messages such as door status, unlock/lock, speed, VIN, etc.
# Thanks to markwj for further analysis and messages such as Trip, Odometer, TPMS, etc.

# Input is a can-do CSV log. For example:
#   TYPE,TIME,ID,D1,D2,D3,D4,D5,D6,D7,D8
#   RD11, 0.0452,400,02,AB,01,80,12,81,55,00
#   RD11, 0.0954,400,01,01,00,00,00,00,4C,1D
#   RD11, 0.1452,400,02,AB,01,80,12,81,55,00
#   RD11, 0.1674,100,93,00,FF,FF,00,00,00,00
#
# Output is a textual analysis of the messages

LINE: while(<>)
  {
  while (/[\r\n]$/) { chop; }
  tr/a-z/A-z/;
  my ($type,$time,$id,@d) = split /[,\s]+/;
  foreach (@d) { $_ = '0'.$_ if (length($_)==1); }
  my $msg = '';
  my $msgt = '';
  if ($type eq 'RD11')
    {
    if ($id eq '100')
      {
      $msgt = '->VDS';
      if ($d[0] eq '80')
        {
        $msg .= "Range";
        $msg .= " (SOC ".hex($d[1])."%)";        # Adjusted state of charge
        $msg .= " (ideal ".hex($d[3].$d[2]).")"; # Ideal range
        $msg .= " (est ".hex($d[7].$d[6]).")";   # Estimated range
        }
      elsif ($d[0] eq '81')
        {
        $msg .= "Time/Date UTC";
        my $cartime = hex($d[7].$d[6].$d[5].$d[4]);
        $msg .= " (time ".scalar gmtime($cartime).")";
        }
      elsif ($d[0] eq '82')
        {
        $msg .= "Ambient Temperature (" . hex($d[1]) . " celcius)";
        }
      elsif ($d[0] eq '83')
        {
        $msg .= "GPS latitude";
        my $latitude = (hex($d[7].$d[6].$d[5].$d[4]))/2048/3600;
        $msg .= " (latitude ".sprintf("%0.6f",$latitude).")";
        }
      elsif ($d[0] eq '84')
        {
        $msg .= "GPS longitude";
        my $longitude = (hex($d[7].$d[6].$d[5].$d[4]))/2048/3600;
        $msg .= " (longitude ".sprintf("%0.6f",$longitude).")";
        }
      elsif ($d[0] eq '85')
        {
        $msg .= "GPS status";
        my $lock = hex($d[1]);
        if ($lock == 0)
          {
          $msg .= " (no gps lock)";
          }
        else
          {
          $msg .= sprintf(" (direction %d deg)",hex($d[3].$d[2]));
          if ($d[5] ne 'FF')
            {
            $msg .= sprintf(" (altitude %dm)",hex($d[5].$d[4]));
            }
          }
        }
      elsif ($d[0] eq '88')
        {
        $msg .= "Charger settings";
        $msg .= " (limit ".hex($d[6])."A)";           # Charge limit
        $msg .= " (current ".hex($d[1])."A)";         # Charging current
        $msg .= " (duration ".hex($d[3].$d[2])."mins)";  # Charge duration in minutes (see charger V1.5 sub state 9)
        }
      elsif ($d[0] eq '89')
        {
        $msg .= "Charger interface";
        $msg .= " (speed ".hex($d[1])."mph)";         # speed in miles/hour
        $msg .= " (vline ".hex($d[3].$d[2])."V)";     # Vline
        $msg .= " (Iavailable ".hex($d[5])."A)";      # Iavailable(from pilot PWM)
        }
      elsif ($d[0] eq '95')
        {
        $msg .= "Charger v1.5";
        my ($st,$ss,$md) = ($d[1],$d[2],hex($d[5])>>4);
        if ($st eq '01')     { $msg .= " (charging)"; }
        elsif ($st eq '02')  { $msg .= " (top-off)"; }
        elsif ($st eq '04')  { $msg .= " (done)"; }
        elsif ($st eq '0D')  { $msg .= " (preparing-to-charge)"; }
        elsif (($st eq '15')||($st eq '16')||($st eq '17')||($st eq '18')||($st eq '19')) { $msg .= " (stopped-charging)"; }
        else  { $msg .= " (??state?? ".hex($st).")"; }          # (1=charging, 2=top off, 4=done, 13=preparing to charge, 21-25=stopped charging)
        if ($ss eq '02')     { $msg .= " (scheduled-start)"; }
        elsif ($ss eq '03')  { $msg .= " (by-request)"; }
        elsif ($ss eq '07')  { $msg .= " (conn-pwr-cable)"; }
        elsif ($ss eq '09')  { $msg .= " (xxMin-".hex($d[7])."kWH)"; }
        else { $msg .= " (??sub-state?? ".hex($ss).")"; }       # (2=scheduled start, 3=by request, 7=connect power cable, 9=xxMinutes-yyKWHrs,)
        if ($md eq 0)     { $msg .= " (standard)"; }
        elsif ($md eq 1)  { $msg .= " (storage)"; }
        elsif ($md eq 3)  { $msg .= " (range)"; }
        elsif ($md eq 4)  { $msg .= " (performance)"; }
        else { $msg .= " (??mode?? ".$md.")"; }            # (0=standard, 1=storage,3=range,4=performance)
        }
      elsif ($d[0] eq '96')
        {
        $msg .= "Doors";
        $msg .= " (l-door: ".((hex($d[1])&0x01)?"open":"closed").")";        # bit0=left door (open=1/closed=0)
        $msg .= " (r-door: ".((hex($d[1])&0x02)?"open":"closed").")";        # bit1=right door (open=1/closed=0)
        $msg .= " (chargeport: ".((hex($d[1])&0x04)?"open":"closed").")";    # bit2=Charge port (open=1/closed=0)
        $msg .= " (pilot: ".((hex($d[1])&0x08)?"true":"false").")";          # Pilot present (true=1/false=0)
        $msg .= " (charging: ".((hex($d[1])&0x10)?"true":"false").")";       # Charging (true=1/false=0)
        $msg .= " (bits ".$d[1].")";            # bit2=Charge port (open=1/closed=0), bit3=Pilot present (true=1/false=0), bit4=Charging (true=1/false=0)
        }
      elsif ($d[0] eq '97')
        {
        $msg .= "Odometer";
        my $miles = sprintf("%0.1f",hex($d[6].$d[5].$d[4])/10);
        my $km = sprintf("%0.1f",$miles*1.609344);
        $msg .= " (miles: ".$miles." km: ".$km.")";
        }
      elsif ($d[0] eq '9A')
        {
        $msg .= 'HVAC ';
        $msg .= " (TcabinOutlet ".hex($d[6])." celcius)";
        }
      elsif ($d[0] eq '9C')
        {
        $msg .= 'Trip->VDS';
        $msg .= " (trip ".sprintf("%0.1f",hex($d[3].$d[2])/10)."miles)";
        }
      elsif ($d[0] eq 'A3')
        {
        $msg .= "TEMPS";
        $msg .= " (Tpem ".hex($d[1]).")" if ($d[1] ne '00');
        $msg .= " (Tmotor ".hex($d[2]).")" if ($d[2] ne '00');
        $msg .= " (Tbattery ".hex($d[6]).")" if ($d[6] ne '00');
        }
      elsif ($d[0] eq 'A4')
        {
        $msg .= 'VIN1';
        $msg .= " (vin ".chr(hex($d[1]))
                        .chr(hex($d[2]))
                        .chr(hex($d[3]))
                        .chr(hex($d[4]))
                        .chr(hex($d[5]))
                        .chr(hex($d[6]))
                        .chr(hex($d[7])).")";   # 7 VIN bytes i.e. "SFZRE2B"
        }
      elsif ($d[0] eq 'A5')
        {
        $msg .= 'VIN2';
        $msg .= " (vin ".chr(hex($d[1]))
                        .chr(hex($d[2]))
                        .chr(hex($d[3]))
                        .chr(hex($d[4]))
                        .chr(hex($d[5]))
                        .chr(hex($d[6]))
                        .chr(hex($d[7])).")";   # 7 VIN bytes i.e. "39A3000"
        }
      elsif ($d[0] eq 'A6')
        {
        $msg .= 'VIN3';
        $msg .= " (vin ".chr(hex($d[1]))
                        .chr(hex($d[2]))
                        .chr(hex($d[3])).")";   # 3 VIN bytes i.e. "359"
        }
      else
        {
        $msg .="(message to VDS ".$d[0].")";
        }
      }
    elsif ($id eq '102')
      {
      $msgt = 'VDS->';
      if (($d[0] eq '05')&&($d[1] eq '19'))
        {
        $msg .= "Set charge mode";
        $msg .= " (mode ".hex($d[4]).")";            # (0=standard, 1=storage,3=range,4=performance)
        }
      elsif (($d[0] eq '05')&&($d[1] eq '03')&&($d[2] eq '00')&&($d[3] eq '00')&&($d[5] eq '00')&&($d[6] eq '00')&&($d[7] eq '00'))
        {
        $msg .= "Start/stop charge";
        $msg .= " (st/st ".hex($d[4]).")";           # Stop=0x00, Start=0x01
        }
      elsif (($d[0] eq '0E')&&($d[1] eq '04'))
        {
        $msg .= "Car locked";
        }
      elsif (($d[0] eq '0E')&&($d[1] eq '05'))
        {
        $msg .= "Car unlocked";
        }
      else
        {
        $msg .="(message from VDS ".$d[0].")";
        }
      }
    elsif ($id eq '344')
      {
      $msgt = "---";
      $msg = "TPMS";
      my @vals;
      push (@vals,sprintf('f-l %dpsi,%dC',hex($d[0])/2.755,hex($d[1])-40)) if ($d[0] ne '00');
      push (@vals,sprintf('f-r %dpsi,%dC',hex($d[2])/2.755,hex($d[3])-40)) if ($d[2] ne '00');
      push (@vals,sprintf('r-l %dpsi,%dC',hex($d[4])/2.755,hex($d[5])-40)) if ($d[4] ne '00');
      push (@vals,sprintf('r-r %dpsi,%dC',hex($d[6])/2.755,hex($d[7])-40)) if ($d[6] ne '00');
      $msg .= " (".join(' ',@vals).")";
      }
    elsif ($id eq '402')
      {
      $msgt = '402??';
      if ($d[0] eq 'FA')
        {
        $msg .= "Odometer";
        my $miles = sprintf("%0.1f",hex($d[5].$d[4].$d[3])/10);
        my $km = sprintf("%0.1f",$miles*1.609344);
        $msg .= " (miles: ".$miles." km: ".$km.")";
        $msg .= " (trip ".sprintf("%0.1f",hex($d[7].$d[6])/10)."miles)";
        }
      else
        {
        $msg .= "(message id 402)";
        }
      }
    # Output the result
    my @vals;
    foreach (0 .. 7) { push @vals,(defined $d[$_])?$d[$_]:"  "; }
    printf "%15s %3s %s   %6s %s\n",$time,$id,join(' ',@vals),$msgt,$msg;
    }
  elsif ($type eq 'TD11')
    {
    $msg = "PING: TD11 transmit";
    my @vals;
    foreach (0 .. 7) { push @vals,(defined $d[$_])?$d[$_]:"  "; }
    printf "%15s %3s %s   %6s %s\n",$time,$id,join(' ',@vals),$msgt,$msg;
    }
  }

