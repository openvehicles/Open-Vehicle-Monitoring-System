//
//  ovmsVehicleInfo.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 18/3/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsAppDelegate.h"
#import "ovmsVehicleInfo.h"

@implementation ovmsVehicleInfo
@synthesize m_vehicleid;
@synthesize m_vin;
@synthesize m_type;
@synthesize m_gsm;
@synthesize m_serverfirmware;
@synthesize m_carfirmware;
@synthesize m_gsm_signalbars;

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
  {
  self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
  if (self) {
        // Custom initialization
    }
  return self;
  }

- (void)viewDidLoad
  {
  [super viewDidLoad];
	// Do any additional setup after loading the view.
  }

- (void)viewDidUnload
  {
  [self setM_vehicleid:nil];
  [self setM_vin:nil];
  [self setM_type:nil];
  [self setM_gsm:nil];
  [self setM_serverfirmware:nil];
  [self setM_carfirmware:nil];
  [self setM_gsm_signalbars:nil];
  [super viewDidUnload];
  // Release any retained subviews of the main view.
  }

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
  {
  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
    return YES;
  else
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
  }

- (void)viewWillAppear:(BOOL)animated
  {
  m_vehicleid.text = [ovmsAppDelegate myRef].sel_car;
  m_vin.text = [ovmsAppDelegate myRef].car_vin;
  m_type.text = [ovmsAppDelegate myRef].car_type;
  m_serverfirmware.text = [ovmsAppDelegate myRef].server_firmware;
  m_carfirmware.text = [ovmsAppDelegate myRef].car_firmware;
  
  int car_gsmlevel = [ovmsAppDelegate myRef].car_gsmlevel;
  int car_gsmdbm = 0;
  if (car_gsmlevel <= 31)
    car_gsmdbm = -113 + (car_gsmlevel*2);
  
  m_gsm.text = [NSString stringWithFormat:@"%d dBm",car_gsmdbm];

  int car_signalbars = 0;
  if ((car_gsmdbm < -121)||(car_gsmdbm >= 0))
    car_signalbars = 0;
  else if (car_gsmdbm < -107)
    car_signalbars = 1;
  else if (car_gsmdbm < -98)
    car_signalbars = 2;
  else if (car_gsmdbm < -87)
    car_signalbars = 3;
  else if (car_gsmdbm < -76)
    car_signalbars = 4;
  else
    car_signalbars = 5;
  
  m_gsm_signalbars.image = [UIImage imageNamed:[NSString stringWithFormat:@"signalbars-%d.png",car_signalbars]];

  [super viewWillAppear:animated];
  
  [TestFlight passCheckpoint:@"VEHICLE_INFO"];
  }

- (IBAction)Done:(id)sender
  {
  [self dismissModalViewControllerAnimated:YES];
  }

@end
