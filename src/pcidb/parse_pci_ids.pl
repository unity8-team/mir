#!/usr/bin/perl
#
# Copyright 2007 Red Hat Inc.
# This crappy script written by Dave Airlie to avoid hassle of adding
# ids in every place.
#
use strict;
use warnings;
use Text::CSV_XS;

my $file = $ARGV[0];

my $atioutfile = 'ati_pciids_gen.h';
my $radeonpcichipsetfile = 'radeon_pci_chipset_gen.h';
my $radeonpcidevicematchfile = 'radeon_pci_device_match_gen.h';
my $radeonchipsetfile = 'radeon_chipset_gen.h';
my $radeonchipinfofile  = 'radeon_chipinfo_gen.h';

my $csv = Text::CSV_XS->new();

open (CSV, "<", $file) or die $!;

open (ATIOUT, ">", $atioutfile) or die;
open (PCICHIPSET, ">", $radeonpcichipsetfile) or die;
open (PCIDEVICEMATCH, ">", $radeonpcidevicematchfile) or die;
open (RADEONCHIPSET, ">", $radeonchipsetfile) or die;
open (RADEONCHIPINFO, ">", $radeonchipinfofile) or die;

print RADEONCHIPSET "/* This file is autogenerated please do not edit */\n";
print RADEONCHIPSET "static SymTabRec RADEONChipsets[] = {\n";
print PCICHIPSET "/* This file is autogenerated please do not edit */\n";
print PCICHIPSET "PciChipsets RADEONPciChipsets[] = {\n";
print PCIDEVICEMATCH "/* This file is autogenerated please do not edit */\n";
print PCIDEVICEMATCH "static const struct pci_id_match radeon_device_match[] = {\n";
print RADEONCHIPINFO "/* This file is autogenerated please do not edit */\n";
print RADEONCHIPINFO "static RADEONCardInfo RADEONCards[] = {\n";
while (<CSV>) {
  if ($csv->parse($_)) {
    my @columns = $csv->fields();

    if ((substr($columns[0], 0, 1) ne "#")) {

      print ATIOUT "#define PCI_CHIP_$columns[1] $columns[0]\n";

      if (($columns[2] ne "R128") && ($columns[2] ne "MACH64") && ($columns[2] ne "MACH32")) {
	print PCICHIPSET " { PCI_CHIP_$columns[1], PCI_CHIP_$columns[1], RES_SHARED_VGA },\n";
	
	print PCIDEVICEMATCH " ATI_DEVICE_MATCH( PCI_CHIP_$columns[1], 0 ),\n";

	print RADEONCHIPSET "  { PCI_CHIP_$columns[1], \"$columns[8]\" },\n";

	print RADEONCHIPINFO " { $columns[0], CHIP_FAMILY_$columns[2], ";

	if ($columns[3] eq "1") {
	  print RADEONCHIPINFO "1, ";
	} else {
	  print RADEONCHIPINFO "0, ";
	}

	if ($columns[4] eq "1") {
	  print RADEONCHIPINFO "1, ";
	} else {
	  print RADEONCHIPINFO "0, ";
	}

	if ($columns[5] eq "1") {
	  print RADEONCHIPINFO "1, ";
	} else {
	  print RADEONCHIPINFO "0, ";
	}

	if ($columns[6] eq "1") {
	  print RADEONCHIPINFO "1, ";
	} else {
	  print RADEONCHIPINFO "0, ";
	}

	if ($columns[7] eq "1") {
	  print RADEONCHIPINFO "1 ";
	} else {
	  print RADEONCHIPINFO "0 ";
	}

	print RADEONCHIPINFO "},\n";
      }
    }
  } else {
    my $err = $csv->error_input;
    print "Failed to parse line: $err";
  }
}

print RADEONCHIPINFO "};\n";
print RADEONCHIPSET "  { -1,                 NULL }\n};\n";
print PCICHIPSET " { -1,                 -1,                 RES_UNDEFINED }\n};\n";
print PCIDEVICEMATCH " { 0, 0, 0 }\n};\n";
close CSV;
close ATIOUT;
close PCICHIPSET;
close PCIDEVICEMATCH;
close RADEONCHIPSET;
close RADEONCHIPINFO;
