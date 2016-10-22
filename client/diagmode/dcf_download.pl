#!/usr/bin/perl
# 
# Purpose:		Read Twizy SEVCON DCF (register dump), output in CSV format
# Needs:		OVMS Twizy firmware with DIAG mode support
# Author:		Michael Balzer
# Version:		1.0 (2015/12/25)
# Requires:		Win32::SerialPort for Windows or Device::SerialPort for UNIX
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
# 

use Getopt::Long;
use Symbol qw( gensym );
use IO::Handle;


#
# Command line arguments:
#

my $help = '';
my $debug = '';
my $init = '';
my $device = '/dev/ttyUSB0';
my $outfile = 'twizy-dcf.csv';
my $start = 1;

GetOptions (
	'help' => \$help,
	'debug' => \$debug,
	'init' => \$init,
	'device=s' => \$device,
	'outfile=s' => \$outfile,
	'start=i' => \$start );

if ($help) {
	print STDOUT
		"Usage: $0 [options]\n",
		"\n",
		"Read dictionary/DCF from SEVCON and output to CSV file.\n",
		"Note: the dictionary contains >1200 registers, full download will need ~ 1 hour.\n",
		"\n",
		"Options:\n",
		" --help          show this help\n",
		" --debug         enable debug output\n",
		" --init          initialize OVMS DIAG mode (use on module reset/powerup)\n",
		" --device=...    specify serial device, default=/dev/ttyUSB0\n",
		" --outfile=...   specify output file name, default=twizy-dcf.csv\n",
		" --start=n       start reading dictionary at entry #n (default 1)\n",
		"\n",
		"Use --start to resume a broken download, n>1 = append to CSV file.\n",
		"\n";
	exit;
}


#
# Global variables:
#

my $line;
my @response;


# 
# Open serial port
# 

my $handle = gensym();
my $port;

if ( $^O =~ m/Win32/ ) {
	require Win32::SerialPort;
	$port = tie (*$handle, "Win32::SerialPort", $device)
		or die "Cannot open $device\n";
}
else {
	require Device::SerialPort;
	$port = tie (*$handle, "Device::SerialPort", $device)
		or die "Cannot open $device\n";
}

$port->user_msg(OFF);
$port->baudrate(9600);
$port->parity("none");
$port->databits(8);
$port->stopbits(1);
$port->handshake("none");

$port->read_char_time(0);
$port->read_const_time(1000);
$port->are_match("\r","\n");

$port->write_settings;

$port->purge_all;


# 
# wait_for: wait for specific input line with timeout
# 	removes CR/LF and modem responses (OK/ERROR) from input
# 	returns undef if timeout occurs
# 
sub wait_for {
	my $pattern = shift // '.';		# valid input pattern
	my $timeout = shift // 10;		# read timeout in seconds / 0=no timeout
	
	my $i = 0;
	do {
		my $line = <$handle>;
		
		$i++ if !$line;
		
		if ($debug) {
			print STDOUT ($line ne "") ? "$line\n" : "... [wait $i]\n";
		}
		
		# remove CR/LF and modem responses
		$line =~ s/[\r\n]//g;
		$line =~ s/^(OK|ERROR)$//g;
		
		return $line if ($line =~ /$pattern/);
		
	} while ($timeout == 0 or $i < $timeout);
	
	return undef;
}


#
# do_cmd: send OVMS command, retry on timeout, return response (array of lines)
# 
# Example: send a command, read response lines until end tag or timeout:
# 	@response = do_cmd('S STAT');
# 
sub do_cmd {
	my $cmd = shift // '';			# empty = don't send, just listen
	my $expect = shift // '^# ';	# response intro pattern (will be removed from result)
	my $timeout = shift // 10;		# read timeout in seconds
	my $tries = shift // 3;			# number of send attempts
	my $endtag = shift // '^#.$';	# response end pattern (will be removed from result)
	
	my @result;
	my $line;
	my $try;
	
	print STDOUT "*** COMMAND: $cmd\n" if $debug;
	
	for ($try = 1; $try <= $tries; $try++) {
		
		# send command:
		if ($cmd ne "") {
			print STDERR "*** COMMAND RETRY: $cmd\n" if $try > 1;
			print $handle "$cmd\r";
		}
		
		return undef if $expect eq "";
		
		# wait for response intro:
		$line = wait_for($expect, $timeout);
		
		# retry send on timeout:
		next if !defined($line);
		
		# remove intro pattern:
		$line =~ s/$expect//;
		
		# save response:
		push @result, $line;
		
		# read more lines:
		while (($linecnt == 0 || --$linecnt > 0) && defined($line = wait_for('.', 1))) {
			print STDOUT "+ $line\n" if $debug;
			$line =~ s/$expect//;
			last if $line =~ /$endtag/;
			push @result, $line;
		}
		
		# done:
		return @result;
	}
	
	# no success:
	print STDERR "*** COMMAND FAILED: no response for '$cmd'\n";
	return undef;
}



#
# do_init: enter OVMS diagnostic mode
#
sub do_init {

	GODIAG: while (1) {
		print STDOUT "*** INIT: PLEASE POWER UP OR RESET OVMS MODULE NOW\n";
		$line = wait_for('RDY') || next GODIAG;
		@response = do_cmd('SETUP', 'OVMS DIAGNOSTICS MODE', 30) && last GODIAG;
	}

	print STDOUT "*** INIT: DIAG MODE ESTABLISHED\n";
}


#
# check_response: output error, return 1 on success or 0 on error/timeout
#
sub check_response {
	if (@response == 0 || @response[0] eq "") {
		# no response / timeout
		return 0;
	}
	elsif (@response[0] =~ /ERROR/i) {
		print STDERR "!!! @response[0]\n";
		return 0;
	}
	else {
		return 1;
	}
}


#
# do_cfg: general purpose CFG command wrapper
#
sub do_cfg {
	my $cmd = shift;
	@response = do_cmd("S CFG $cmd");
	return check_response();
}


#
# sdo_read: CFG READ wrapper, returns value or undef on timeout
#
sub sdo_read {
	my $index = shift;
	my $subindex = shift;
	
	# read SDO:
	@response = do_cmd(sprintf("S CFG READ %04x %02x", $index, $subindex));
	return undef if !check_response();
	
	# extract value:
	my $val = (@response[0] =~ / = ([0-9]+)/) ? $1 : undef;
	return $val;
}


#
# sdo_readstr: CFG READS wrapper, returns string or undef on timeout
#
sub sdo_readstr {
	my $index = shift;
	my $subindex = shift;
	
	# read SDO:
	@response = do_cmd(sprintf("S CFG READS %04x %02x", $index, $subindex));
	return undef if !check_response();
	
	# extract value:
	my $val = (@response[0] =~ /[^=]+=(.*)/) ? $1 : undef;
	return $val;
}


#
# sdo_write: CFG WRITE wrapper, returns 1/0 for success/fail, undef for timeout
#
sub sdo_write {
	my $index = shift;
	my $subindex = shift;
	my $val = shift;
	
	# write SDO:
	@response = do_cmd(sprintf("S CFG WRITE %04x %02x %ld", $index, $subindex, $val));
	return undef if !check_response();
	
	# extract new value:
	my $newval = (@response[0] =~ / NEW: ([0-9]+)/) ? $1 : undef;
	return ($newval == $val) ? 1 : 0;
}


#
# sdo_writeonly: CFG WRITEO wrapper, returns 1=success or undef=timeout
#
sub sdo_writeonly {
	my $index = shift;
	my $subindex = shift;
	my $val = shift;
	
	# write SDO:
	@response = do_cmd(sprintf("S CFG WRITEO %04x %02x %ld", $index, $subindex, $val));
	return undef if !check_response();
	
	return 1;
}



# ====================================================================
# MAIN
# ====================================================================

my $index;
my $subindex;
my $datatype;
my $access;
my $lowlimit;
my $highlimit;
my $objversion;
my $val;
my $str;

my @datatypename = (
	'Boolean', 'Integer8', 'Integer16', 'Integer32', 'Integer64',
	'Unsigned8', 'Unsigned16', 'Unsigned32', 'Unsigned64', 'VisibleString',
	'Domain', 'OctetString', 'Real32', 'Real64', 'Void',
	'Domain' );
my @accessname = (
	'RO', 'WO', 'RW', 'CONST' );


# open CSV output file:
my $csv;
if ($start == 1) {
	# start new CSV:
	open($csv, ">", $outfile) or die "Cannot open output file '$outfile'\n";
	print $csv "EntryNr,Index,SubIndex,Version,DataType,Access,LowLimit,HighLimit,ValueHex,ValueDec,ValueStr\n";
}
else {
	# append to CSV:
	open($csv, ">>", $outfile) or die "Cannot open output file '$outfile'\n";
}
$csv->autoflush(1);


# enter DIAG & PREOP mode:
do_init if $init;
print STDOUT "Entering PREOP mode...\n";
do_cfg("PRE") or die "Cannot enter PREOP mode\n";


# read dictionary length:
my $diclen = sdo_read(0x5630, 0x01) or die "Cannot read dictionary length\n";
print STDOUT "Dictionary length: $diclen entries\n";


# read dictionary entries:
for (my $i = $start; $i <= $diclen; $i++) {
	
	# read metadata:
	sdo_writeonly(0x5630, 0x01, $i-1) or die "Cannot select dictionary entry $i\n";
	$index = sdo_read(0x5630, 0x02);
	$subindex = sdo_read(0x5630, 0x03);
	$datatype = sdo_read(0x5630, 0x05);
	$access = sdo_read(0x5630, 0x06);
	$lowlimit = sdo_read(0x5630, 0x07);
	$highlimit = sdo_read(0x5630, 0x08);
	$objversion = sdo_read(0x5630, 0x0a);
	
	# read value:
	if ($access == 1) {
		# write-only:
		$val = undef;
		$str = undef;
	}
	elsif ($datatype >= 0 && $datatype <= 8) {
		# numerical:
		$val = sdo_read($index, $subindex);
		$str = undef;
	}
	elsif ($datatype == 9) {
		# visible_string:
		$val = undef;
		$str = sdo_readstr($index, $subindex);
		# CSV encode:
		$str =~ s/\"/\"\"/g;
		$str = '"' . $str . '"';
	}
	else {
		# unsupported datatype:
		$val = undef;
		$str = undef;
	}
	
	# write CSV record:
	if (defined($val)) {
		printf $csv "%d,0x%04x,0x%02x,%d,%s,%s,%ld,%ld,0x%lx,%ld,\n",
			$i, $index, $subindex, $objversion, @datatypename[$datatype], @accessname[$access],
			$lowlimit, $highlimit, $val, $val;
	}
	else {
		printf $csv "%d,0x%04x,0x%02x,%d,%s,%s,%ld,%ld,,,%s\n",
			$i, $index, $subindex, $objversion, @datatypename[$datatype], @accessname[$access],
			$lowlimit, $highlimit, $str;
	}
	
	# report progress:
	print STDOUT "got entry [$i/$diclen]\n";
}


# finish:
close $csv;

print STDOUT "Leaving PREOP mode...\n";
do_cfg("OP") or die "Cannot leave PREOP mode\n";

print STDOUT "Done.\n";
exit;
