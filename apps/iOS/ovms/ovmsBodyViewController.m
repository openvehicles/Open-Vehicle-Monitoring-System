//
//  ovmsBodyViewController.m
//  ovms
//
//  Created by Mark Webb-Johnson on 9/12/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import <QuartzCore/QuartzCore.h>
#import "ovmsBodyViewController.h"
#import "ovmsControlPINEntry.h"

@implementation ovmsBodyViewController
@synthesize m_car_lockunlock;
@synthesize m_car_outlineimage;
@synthesize m_car_lights;
@synthesize m_car_valetonoff;
@synthesize m_car_door_ld;
@synthesize m_car_door_rd;
@synthesize m_car_door_hd;
@synthesize m_car_door_cp;
@synthesize m_car_door_tr;
@synthesize m_car_wheel_fr_pressure;
@synthesize m_car_wheel_fr_temp;
@synthesize m_car_wheel_rr_pressure;
@synthesize m_car_wheel_rr_temp;
@synthesize m_car_wheel_fl_pressure;
@synthesize m_car_wheel_fl_temp;
@synthesize m_car_wheel_rl_pressure;
@synthesize m_car_wheel_rl_temp;
@synthesize m_car_temp_pem;
@synthesize m_car_temp_motor;
@synthesize m_car_temp_battery;
@synthesize m_car_temp_pem_l;
@synthesize m_car_temp_motor_l;
@synthesize m_car_temp_battery_l;
@synthesize m_car_ambient_temp;
@synthesize m_car_weather;
@synthesize m_car_tpmsboxes;
@synthesize m_lock_button;
@synthesize m_valet_button;
@synthesize m_wakeup_button;

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (void)didReceiveMemoryWarning
{
    // Releases the view if it doesn't have a superview.
    [super didReceiveMemoryWarning];
    
    // Release any cached data, images, etc that aren't in use.
}

#pragma mark - View lifecycle

/*
// Implement loadView to create a view hierarchy programmatically, without using a nib.
- (void)loadView
{
}
*/

// Implement viewDidLoad to do additional setup after loading the view, typically from a nib.
- (void)viewDidLoad
{
  [super viewDidLoad];
  [ovmsAppDelegate myRef].car_delegate = self;
  
  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
  
//  m_car_lockunlock.userInteractionEnabled = YES;
//  UITapGestureRecognizer *tap1 = [[UITapGestureRecognizer alloc] initWithTarget:self action:@selector(LockTapped:)];
//  [m_car_lockunlock addGestureRecognizer:tap1];
}

- (void)viewDidUnload
{
  [self setM_car_lockunlock:nil];
  [self setM_car_door_ld:nil];
  [self setM_car_door_rd:nil];
  [self setM_car_door_hd:nil];
  [self setM_car_door_cp:nil];
  [self setM_car_door_tr:nil];
  [self setM_car_temp_pem:nil];
  [self setM_car_temp_motor:nil];
  [self setM_car_temp_battery:nil];
  [self setM_car_wheel_fr_pressure:nil];
  [self setM_car_wheel_fr_temp:nil];
  [self setM_car_wheel_rr_pressure:nil];
  [self setM_car_wheel_rr_temp:nil];
  [self setM_car_wheel_fl_pressure:nil];
  [self setM_car_wheel_fl_temp:nil];
  [self setM_car_wheel_rl_pressure:nil];
  [self setM_car_wheel_rl_temp:nil];
  [self setM_car_temp_pem_l:nil];
  [self setM_car_temp_motor_l:nil];
  [self setM_car_temp_battery_l:nil];
  [self setM_car_outlineimage:nil];
  [self setM_car_ambient_temp:nil];
    [self setM_car_valetonoff:nil];
    [self setM_car_lights:nil];
  [self setM_lock_button:nil];
  [self setM_valet_button:nil];
  [self setM_wakeup_button:nil];
    [self setM_car_weather:nil];
    [self setM_car_tpmsboxes:nil];
  [super viewDidUnload];
  // Release any retained subviews of the main view.
  // e.g. self.myOutlet = nil;
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
  [self updateCar];
  
  // Setup animation for the charging port...
//  CABasicAnimation *theAnimation;
//  theAnimation=[CABasicAnimation animationWithKeyPath:@"opacity"];
//  theAnimation.duration=0.75;
//  theAnimation.repeatCount= 3.4E38;
//  theAnimation.autoreverses=YES;
//  theAnimation.fromValue=[NSNumber numberWithFloat:1.0];
//  theAnimation.toValue=[NSNumber numberWithFloat:0.35];
//  [m_car_door_cp.layer addAnimation:theAnimation forKey:@"animateOpacity"];
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
  if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone)
    {
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
    }
  else
    {
    return YES;
    }
}

-(void) updateCar
{
  if ([ovmsAppDelegate myRef].car_online)
    {
    m_lock_button.enabled=YES;
    m_valet_button.enabled=YES;
    m_wakeup_button.enabled=YES;
    }
  else
    {
    m_lock_button.enabled=NO;
    m_valet_button.enabled=NO;
    m_wakeup_button.enabled=NO;    
    }
  
  m_car_outlineimage.image=[UIImage
                            imageNamed:[NSString stringWithFormat:@"ol_%@",
                                        [ovmsAppDelegate myRef].sel_imagepath]];

  if ([ovmsAppDelegate myRef].car_doors2 & 0x08)
    m_car_lockunlock.image = [UIImage imageNamed:@"carlock.png"];
  else
    m_car_lockunlock.image = [UIImage imageNamed:@"carunlock.png"];

  if ([ovmsAppDelegate myRef].car_doors2 & 0x10)
    m_car_valetonoff.image = [UIImage imageNamed:@"carvaleton.png"];
  else
    m_car_valetonoff.image = [UIImage imageNamed:@"carvaletoff.png"];

  if ([ovmsAppDelegate myRef].car_doors2 & 0x20)
    m_car_lights.hidden = 0;
  else
    m_car_lights.hidden = 1;

  if ([ovmsAppDelegate myRef].car_doors1 & 0x01)
    m_car_door_ld.hidden = 0;
  else
    m_car_door_ld.hidden = 1;
  
  if ([ovmsAppDelegate myRef].car_doors1 & 0x02)
    m_car_door_rd.hidden = 0;
  else
    m_car_door_rd.hidden = 1;

  if ([ovmsAppDelegate myRef].car_doors1 & 0x04)
    m_car_door_cp.hidden = 0;
  else
    m_car_door_cp.hidden = 1;

  if ([ovmsAppDelegate myRef].car_doors2 & 0x40)
    m_car_door_hd.hidden = 0;
  else
    m_car_door_hd.hidden = 1;

  if ([ovmsAppDelegate myRef].car_doors2 & 0x80)
    m_car_door_tr.hidden = 0;
  else
    m_car_door_tr.hidden = 1;

  int car_stale_pemtemps = [ovmsAppDelegate myRef].car_stale_pemtemps;
  if (car_stale_pemtemps < 0)
    {
    // No PEM temperatures
    m_car_temp_pem.hidden = YES;
    m_car_temp_motor.hidden = YES;
    m_car_temp_battery.hidden = YES;
    }
  else if (car_stale_pemtemps == 0)
    {
    // Stale PEM temperatures
    m_car_temp_pem.hidden = NO;
    m_car_temp_motor.hidden = NO;
    m_car_temp_battery.hidden = NO;
    m_car_temp_pem.textColor = [UIColor grayColor];
    m_car_temp_motor.textColor = [UIColor grayColor];
    m_car_temp_battery.textColor = [UIColor grayColor];
    m_car_temp_pem.text = [NSString stringWithFormat:@"%dºC",
                           [ovmsAppDelegate myRef].car_tpem];
    m_car_temp_motor.text = [NSString stringWithFormat:@"%dºC",
                             [ovmsAppDelegate myRef].car_tmotor];
    m_car_temp_battery.text = [NSString stringWithFormat:@"%dºC",
                               [ovmsAppDelegate myRef].car_tbattery];
    }
  else
    {
    // OK PEM temperatures
    m_car_temp_pem.hidden = NO;
    m_car_temp_motor.hidden = NO;
    m_car_temp_battery.hidden = NO;
    m_car_temp_pem.textColor = [UIColor whiteColor];
    m_car_temp_motor.textColor = [UIColor whiteColor];
    m_car_temp_battery.textColor = [UIColor whiteColor];
    m_car_temp_pem.text = [NSString stringWithFormat:@"%dºC",
                           [ovmsAppDelegate myRef].car_tpem];
    m_car_temp_motor.text = [NSString stringWithFormat:@"%dºC",
                             [ovmsAppDelegate myRef].car_tmotor];
    m_car_temp_battery.text = [NSString stringWithFormat:@"%dºC",
                               [ovmsAppDelegate myRef].car_tbattery];
    }

  int car_stale_ambienttemps = [ovmsAppDelegate myRef].car_stale_ambienttemps;
  if (car_stale_ambienttemps < 0)
    {
    // No Ambient temperature
    }
  else if (car_stale_ambienttemps == 0)
    {
    // Stale Ambient temperature
    m_car_ambient_temp.textColor = [UIColor grayColor];
    m_car_ambient_temp.text = [NSString stringWithFormat:@"%dºC",
                               [ovmsAppDelegate myRef].car_ambient_temp];
    }
  else
    {
    // OK Ambient temperature
    m_car_ambient_temp.textColor = [UIColor whiteColor];
    m_car_ambient_temp.text = [NSString stringWithFormat:@"%dºC",
                               [ovmsAppDelegate myRef].car_ambient_temp];
    }
    
  int car_stale_tpms = [ovmsAppDelegate myRef].car_stale_tpms;
  if (car_stale_tpms < 0)
    {
    // No TPMS
    m_car_tpmsboxes.hidden = YES;
    }
  else if (car_stale_tpms == 0)
    {
    // Stale TPMS
    m_car_tpmsboxes.hidden = NO;
    m_car_wheel_fr_pressure.textColor = [UIColor grayColor];
    m_car_wheel_fr_temp.textColor = [UIColor grayColor];
    m_car_wheel_fl_pressure.textColor = [UIColor grayColor];
    m_car_wheel_fl_temp.textColor = [UIColor grayColor];
    m_car_wheel_rr_pressure.textColor = [UIColor grayColor];
    m_car_wheel_rr_temp.textColor = [UIColor grayColor];
    m_car_wheel_rl_pressure.textColor = [UIColor grayColor];
    m_car_wheel_rl_temp.textColor = [UIColor grayColor];
    }
  else
    {
    // OK TPMS
    m_car_tpmsboxes.hidden = NO;
    m_car_wheel_fr_pressure.textColor = [UIColor whiteColor];
    m_car_wheel_fr_temp.textColor = [UIColor whiteColor];
    m_car_wheel_fl_pressure.textColor = [UIColor whiteColor];
    m_car_wheel_fl_temp.textColor = [UIColor whiteColor];
    m_car_wheel_rr_pressure.textColor = [UIColor whiteColor];
    m_car_wheel_rr_temp.textColor = [UIColor whiteColor];
    m_car_wheel_rl_pressure.textColor = [UIColor whiteColor];
    m_car_wheel_rl_temp.textColor = [UIColor whiteColor];
    }
  
  if ([ovmsAppDelegate myRef].car_tpms_fr_temp > 0)
    {
    m_car_wheel_fr_pressure.text = [NSString stringWithFormat:@"%0.1f PSI",
                                    [ovmsAppDelegate myRef].car_tpms_fr_pressure];
    m_car_wheel_fr_temp.text = [NSString stringWithFormat:@"%dºC",
                                [ovmsAppDelegate myRef].car_tpms_fr_temp];
    }
  else
    {
    m_car_wheel_fr_pressure.text = @"";
    m_car_wheel_fr_temp.text = @"";
    }

  if ([ovmsAppDelegate myRef].car_tpms_rr_temp > 0)
    {
    m_car_wheel_rr_pressure.text = [NSString stringWithFormat:@"%0.1f PSI",
                                    [ovmsAppDelegate myRef].car_tpms_rr_pressure];
    m_car_wheel_rr_temp.text = [NSString stringWithFormat:@"%dºC",
                                [ovmsAppDelegate myRef].car_tpms_rr_temp];
    }
  else
    {
    m_car_wheel_rr_pressure.text = @"";
    m_car_wheel_rr_temp.text = @"";
    }

  if ([ovmsAppDelegate myRef].car_tpms_fl_temp > 0)
    {
    m_car_wheel_fl_pressure.text = [NSString stringWithFormat:@"%0.1f PSI",
                                    [ovmsAppDelegate myRef].car_tpms_fl_pressure];
    m_car_wheel_fl_temp.text = [NSString stringWithFormat:@"%dºC",
                                [ovmsAppDelegate myRef].car_tpms_fl_temp];
    }
  else
    {
    m_car_wheel_fl_pressure.text = @"";
    m_car_wheel_fl_temp.text = @"";
    }
  
  if ([ovmsAppDelegate myRef].car_tpms_rl_temp > 0)
    {
    m_car_wheel_rl_pressure.text = [NSString stringWithFormat:@"%0.1f PSI",
                                    [ovmsAppDelegate myRef].car_tpms_rl_pressure];
    m_car_wheel_rl_temp.text = [NSString stringWithFormat:@"%dºC",
                                [ovmsAppDelegate myRef].car_tpms_rl_temp];
    }
  else
    {
    m_car_wheel_rl_pressure.text = @"";
    m_car_wheel_rl_temp.text = @"";
    }
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
  {
  if ([[segue identifier] isEqualToString:@"ValetMode"])
    {
    if ([ovmsAppDelegate myRef].car_doors2 & 0x10)
      { // Valet is ON, let's offer to deactivate it
        [[segue destinationViewController] setInstructions:@"Enter PIN\nto deactivate valet mode"];
        [[segue destinationViewController] setHeading:@"Valet Mode"];
        [[segue destinationViewController] setFunction:@"Valet Off"];
        [[segue destinationViewController] setDelegate:self];
      }
    else
      { // Valet is OFF, let's offer to activate it
        [[segue destinationViewController] setInstructions:@"Enter PIN\nto activate valet mode"];
        [[segue destinationViewController] setHeading:@"Valet Mode"];
        [[segue destinationViewController] setFunction:@"Valet On"];
        [[segue destinationViewController] setDelegate:self];
      }
    }
  else if ([[segue identifier] isEqualToString:@"LockUnlock"])
    {
    if ([ovmsAppDelegate myRef].car_doors2 & 0x08)
      { // Car is locked, let's offer to unlock it
        [[segue destinationViewController] setInstructions:@"Enter PIN\nto unlock car"];
        [[segue destinationViewController] setHeading:@"Unlock Car"];
        [[segue destinationViewController] setFunction:@"Unlock Car"];
        [[segue destinationViewController] setDelegate:self];
      }
    else
      { // Car is unlocked, let's offer to lock it
        [[segue destinationViewController] setInstructions:@"Enter PIN\nto lock car"];
        [[segue destinationViewController] setHeading:@"Lock Car"];
        [[segue destinationViewController] setFunction:@"Lock Car"];
        [[segue destinationViewController] setDelegate:self];
      }
    }
  }

- (void)omvsControlPINEntryDelegateDidCancel:(NSString*)fn
  {
  }

- (void)omvsControlPINEntryDelegateDidSave:(NSString*)fn pin:(NSString*)pin
  {
  if ([fn isEqualToString:@"Valet On"])
    {
    [[ovmsAppDelegate myRef] commandDoActivateValet:pin];
    }
  else if ([fn isEqualToString:@"Valet Off"])
    {
    [[ovmsAppDelegate myRef] commandDoDeactivateValet:pin];
    }    
  else if ([fn isEqualToString:@"Lock Car"])
    {
    [[ovmsAppDelegate myRef] commandDoLockCar:pin];
    }
  else if ([fn isEqualToString:@"Unlock Car"])
    {
    [[ovmsAppDelegate myRef] commandDoUnlockCar:pin];
    }    
  }

- (IBAction)WakeupButton:(id)sender
  {
  // The wakeup button has been pressed - let's wakeup the car
  UIActionSheet *actionSheet = [[UIActionSheet alloc] initWithTitle:@"Wakeup Car"
                                                           delegate:self
                                                  cancelButtonTitle:@"Cancel"
                                                  destructiveButtonTitle:nil
                                                  otherButtonTitles:@"Wakeup",nil];
  [actionSheet showInView:self.view];
  }

- (void)actionSheet:(UIActionSheet *)sender clickedButtonAtIndex:(int)index
  {
  if (index == [sender firstOtherButtonIndex])
    {
    [[ovmsAppDelegate myRef] commandDoWakeupCar];
    }
  }

@end
