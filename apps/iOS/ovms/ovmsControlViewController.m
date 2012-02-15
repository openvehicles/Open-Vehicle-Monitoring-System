//
//  ovmsControlViewController.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 2/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsControlViewController.h"
#import "ovmsControlPINEntry.h"
#import "JHNotificationManager.h"

@implementation ovmsControlViewController
@synthesize m_chargemode;

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
//  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
}

- (void)viewDidUnload
{
  [self setM_chargemode:nil];
    [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

-(void) viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
}

-(void) viewWillDisappear:(BOOL)animated
{
  [super viewWillDisappear:animated];
  
  [[ovmsAppDelegate myRef] commandCancel];
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

- (IBAction)doneButton:(id)sender {
  [self dismissModalViewControllerAnimated:YES];
}

- (IBAction)startChargingButton:(id)sender {
  [[ovmsAppDelegate myRef] commandDoStartCharge];
}

- (IBAction)stopChargingButton:(id)sender {
  [[ovmsAppDelegate myRef] commandDoStopCharge];
}

- (IBAction)chargeModeButton:(id)sender {
}

- (IBAction)lockButton:(id)sender {
}

- (IBAction)valetButton:(id)sender {
}

- (IBAction)featuresButton:(id)sender {
}

- (IBAction)parametersButton:(id)sender {
}

- (IBAction)cellularUsageButton:(id)sender {
}

- (IBAction)resetModuleButton:(id)sender {
  [[ovmsAppDelegate myRef] commandDoReboot];
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
  if ([[segue identifier] isEqualToString:@"ValetMode"])
    {
    if ([ovmsAppDelegate myRef].car_doors2 & 0x10)
      { // Valet is ON, let's offer to deactivate it
      [[segue destinationViewController] setInstructions:@"Enter PIN to deactivate valet mode"];
      [[segue destinationViewController] setHeading:@"Valet Mode"];
      [[segue destinationViewController] setFunction:@"Valet Off"];
      [[segue destinationViewController] setDelegate:self];
      }
    else
      { // Valet is OFF, let's offer to activate it
      [[segue destinationViewController] setInstructions:@"Enter PIN to activate valet mode"];
      [[segue destinationViewController] setHeading:@"Valet Mode"];
      [[segue destinationViewController] setFunction:@"Valet On"];
      [[segue destinationViewController] setDelegate:self];
      }
    }
  else if ([[segue identifier] isEqualToString:@"LockUnlock"])
    {
    if ([ovmsAppDelegate myRef].car_doors2 & 0x08)
      { // Car is locked, let's offer to unlock it
        [[segue destinationViewController] setInstructions:@"Enter PIN to unlock car"];
        [[segue destinationViewController] setHeading:@"Unlock Car"];
        [[segue destinationViewController] setFunction:@"Unlock Car"];
        [[segue destinationViewController] setDelegate:self];
      }
    else
      { // Car is unlocked, let's offer to lock it
        [[segue destinationViewController] setInstructions:@"Enter PIN to lock car"];
        [[segue destinationViewController] setHeading:@"Lock Car"];
        [[segue destinationViewController] setFunction:@"Lock Car"];
        [[segue destinationViewController] setDelegate:self];
      }
    }}

- (void)omvsControlPINEntryDelegateDidCancel:(NSString*)fn
{
  [JHNotificationManager notificationWithMessage:@"PIN Cancelled"];

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

@end
