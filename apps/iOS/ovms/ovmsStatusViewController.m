//
//  ovmsStatusViewController.m
//  ovms
//
//  Created by Mark Webb-Johnson on 16/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import "ovmsStatusViewController.h"

@implementation ovmsStatusViewController
@synthesize m_car_connection_state;
@synthesize m_car_image;
@synthesize m_car_charge_state;
@synthesize m_car_charge_type;
@synthesize m_car_soc;
@synthesize m_battery_front;
@synthesize m_car_range;

- (void)didReceiveMemoryWarning
{
  [super didReceiveMemoryWarning];
  // Release any cached data, images, etc that aren't in use.
}

#pragma mark - View lifecycle

- (void)viewDidLoad
{
  [super viewDidLoad];
	// Do any additional setup after loading the view, typically from a nib.
  [ovmsAppDelegate myRef].status_delegate = self;
  
  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;

  [self updateStatus];
}

- (void)viewDidUnload
{
    [self setM_car_image:nil];
    [self setM_car_charge_state:nil];
    [self setM_car_charge_type:nil];
    [self setM_car_soc:nil];
    [self setM_battery_front:nil];
    [self setM_car_range:nil];
    [self setM_car_connection_state:nil];
  [super viewDidUnload];
  // Release any retained subviews of the main view.
  // e.g. self.myOutlet = nil;
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
  [self updateStatus];
}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
}

- (void)viewWillDisappear:(BOOL)animated
{
	[super viewWillDisappear:animated];
}

- (void)viewDidDisappear:(BOOL)animated
{
	[super viewDidDisappear:animated];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
  // Return YES for supported orientations
  if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
    return (interfaceOrientation != UIInterfaceOrientationPortraitUpsideDown);
  } else {
    return YES;
  }
}

-(void) updateStatus
{
  NSString* units;
  if ([[ovmsAppDelegate myRef].car_units isEqualToString:@"K"])
    units = @"km";
  else
    units = @"m";
  
  time_t lastupdated = [ovmsAppDelegate myRef].car_lastupdated;
  int minutes = (time(0)-lastupdated)/60;
  if (lastupdated == 0)
    {
    m_car_connection_state.text = @"";
    m_car_connection_state.textColor = [UIColor whiteColor];
    }
  else if (minutes == 0)
    {
    m_car_connection_state.text = @"Connected (just now)";
    m_car_connection_state.textColor = [UIColor whiteColor];
    }
  else if (minutes == 1)
    {
    m_car_connection_state.text = @"Connected (1 minute ago)";
    m_car_connection_state.textColor = [UIColor whiteColor];
    }
  else if (minutes >= 20)
    {
    m_car_connection_state.text = [NSString stringWithFormat:@"No connection (for %d mins)",minutes];
    m_car_connection_state.textColor = [UIColor redColor];
    }
  else
    {
    m_car_connection_state.text = [NSString stringWithFormat:@"Connected (%d mins ago)",minutes];
    m_car_connection_state.textColor = [UIColor whiteColor];
    }
  
  m_car_image.image=[UIImage imageNamed:[ovmsAppDelegate myRef].sel_imagepath];
  m_car_soc.text = [NSString stringWithFormat:@"%d%%",[ovmsAppDelegate myRef].car_soc];
  m_car_range.text = [NSString stringWithFormat:@"Range: %d%s (%d%s estimated)",
                      [ovmsAppDelegate myRef].car_idealrange,
                      [units UTF8String],
                      [ovmsAppDelegate myRef].car_estimatedrange,
                      [units UTF8String]];
  CGRect bounds = m_battery_front.bounds;
  CGPoint center = m_battery_front.center;
  CGFloat oldwidth = bounds.size.width;
  CGFloat newwidth = (((0.0+[ovmsAppDelegate myRef].car_soc)/100.0)*(233-17))+17;
  bounds.size.width = newwidth;
  center.x = center.x + ((newwidth - oldwidth)/2);
  m_battery_front.bounds = bounds;
  m_battery_front.center = center;
  bounds = m_battery_front.bounds;
  if ([[ovmsAppDelegate myRef].car_chargestate isEqualToString:@"charging"])
    {
    m_car_charge_state.text = [NSString stringWithFormat:@"Charging (%@)", [ovmsAppDelegate myRef].car_chargemode];
    m_car_charge_type.text = [NSString stringWithFormat:@"%dV @%dA",
    [ovmsAppDelegate myRef].car_linevoltage,
    [ovmsAppDelegate myRef].car_chargecurrent];
    }
  else if ([[ovmsAppDelegate myRef].car_chargestate isEqualToString:@"topoff"])
    {
    m_car_charge_state.text = @"Topping off";
    m_car_charge_type.text = [NSString stringWithFormat:@"%dV @%dA",
                              [ovmsAppDelegate myRef].car_linevoltage,
                              [ovmsAppDelegate myRef].car_chargecurrent];
    }
  else if ([[ovmsAppDelegate myRef].car_chargestate isEqualToString:@"done"])
    {
    m_car_charge_state.text = @"";
    m_car_charge_type.text = @"";
    }
  else if ([[ovmsAppDelegate myRef].car_chargestate isEqualToString:@"stopped"])
    {
    m_car_charge_state.text = @"Charging stopped";
    m_car_charge_type.text = @"";
    }
  else
    {
    m_car_charge_state.text = @"";
    m_car_charge_type.text = @"";
    }
}

@end
