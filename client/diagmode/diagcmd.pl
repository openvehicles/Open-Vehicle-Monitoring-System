#!/usr/bin/perl

# Purpose:		Issue OVMS commands via serial line using DIAG mode
# Needs:		OVMS firmware with DIAG mode support
# Author:		Michael Balzer
# Version:		1.0 (2015/12/25)
# Requires:		Win32::SerialPort for Windows or Device::SerialPort for UNIX

use Getopt::Long;
use Symbol qw( gensym );


#
# Command line arguments:
#

my $help = '';
my $debug = '';
my $init = '';
my $device = '/dev/ttyUSB0';
my $lines = 0;

GetOptions (
	'help' => \$help,
	'debug' => \$debug,
	'init' => \$init,
	'device=s' => \$device,
	'lines=i' => \$lines );

if ($help) {
	print STDOUT
		"Usage: $0 [options] cmd [cmd...]\n",
		"\n",
		"Commands can be all OVMS DIAG mode commands.\n",
		"Modem commands (AT...) are not yet supported.\n",
		"\n",
		"Options:\n",
		" --help          show this help\n",
		" --debug         enable debug output\n",
		" --init          initialize OVMS DIAG mode (use on module reset/powerup)\n",
		" --device=...    specify serial device, default=/dev/ttyUSB0\n",
		" --lines=n       specify response line count (faster, no read timeout necessary)\n",
		"\n",
		"Examples:\n",
		"1. init and get battery status:\n  $0 --init \"S STAT\"\n",
		"2. get parameters and features:\n  $0 \"M 1\" \"M 3\"\n",
		"3. get firmware version fast:\n  $0 --lines=1 \"S VERSION\"\n",
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
$port->are_match("\r\n");

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
# Example: send a command, read response lines until timeout:
# 	@response = do_cmd('S STAT');
# 
# To speed up processing set $linecnt.
#
sub do_cmd {
	my $cmd = shift // '';			# empty = don't send, just listen
	my $linecnt = shift // 0;		# 0 = read lines until timeout
	my $expect = shift // '^# ';	# response intro pattern (will be removed from result)
	my $timeout = shift // 10;		# read timeout in seconds
	my $tries = shift // 3;			# number of send attempts
	
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
		$line = wait_for('GPS Ready') || next GODIAG;
		@response = do_cmd('SETUP', 1, 'OVMS DIAGNOSTICS MODE') && last GODIAG;
	}

	print STDOUT "*** INIT: DIAG MODE ESTABLISHED\n";
}



# ====================================================================
# MAIN
# ====================================================================

do_init if $init;

for $cmd (@ARGV) {
	
	@response = do_cmd($cmd, $lines);
	
	for ($i = 0; $i < @response; $i++) {
		print STDOUT "@response[$i]\n";
	}
}

exit;
