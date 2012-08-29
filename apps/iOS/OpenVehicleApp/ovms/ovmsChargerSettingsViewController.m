//
//  ovmsChargerSettingsViewController.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 17/2/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsChargerSettingsViewController.h"

@implementation ovmsChargerSettingsViewController
@synthesize m_charger_mode;
@synthesize m_charger_current;
@synthesize m_charger_warning;

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
  int cl_row = [ovmsAppDelegate myRef].car_chargelimit-10;
  [m_charger_current selectRow:cl_row inComponent:0 animated:NO];
  
  int cm_row = [ovmsAppDelegate myRef].car_chargemodeN;
  if (cm_row > 2) cm_row--;
  m_charger_mode.selectedSegmentIndex = cm_row;
  
  switch (cm_row)
    {
    case 2:
      m_charger_warning.text = @"Range Mode limits power\nFrequent use will reduce long-term battery life";
      break;
    case 3:
      m_charger_warning.text = @"Frequent use of Performance Mode\nwill reduce long-term battery life";
      break;
    default:
      m_charger_warning.text = @"";
      break;
    }
}

- (void)viewDidUnload
{
  [self setM_charger_mode:nil];
  [self setM_charger_current:nil];
  [self setM_charger_warning:nil];
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
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
  {
  if (UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPad)
    return YES;
  else
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
  }

- (IBAction)ModeChanged:(id)sender
  {
  int cm_row = m_charger_mode.selectedSegmentIndex;
  
  switch (cm_row)
    {
    case 2:
      m_charger_warning.text = @"Range Mode limits power\nFrequent use will reduce long-term battery life";
      break;
    case 3:
      m_charger_warning.text = @"Frequent use of Performance Mode\nwill reduce long-term battery life";
      break;
    default:
      m_charger_warning.text = @"";
      break;
    }
  }

- (IBAction)CancelButton:(id)sender
  {
  [self dismissModalViewControllerAnimated:YES];
  }

- (IBAction)DoneButton:(id)sender
  {
  int ncm = m_charger_mode.selectedSegmentIndex;
  if (ncm>=2) ncm++;
  int ncl = [m_charger_current selectedRowInComponent:0] + 10;

  [self dismissModalViewControllerAnimated:YES];

  if ((ncm != [ovmsAppDelegate myRef].car_chargemodeN)&&
      (ncl != [ovmsAppDelegate myRef].car_chargelimit))
    {
    // We need to change the car charge mode and limit
    [[ovmsAppDelegate myRef] commandDoSetChargeModecurrent:ncm current:ncl];
    }
  else if (ncm != [ovmsAppDelegate myRef].car_chargemodeN)
    {
    [[ovmsAppDelegate myRef] commandDoSetChargeMode:ncm];
    }
  else if (ncl != [ovmsAppDelegate myRef].car_chargelimit)
    {
    [[ovmsAppDelegate myRef] commandDoSetChargeCurrent:ncl];
    }
  }

#pragma mark -
#pragma mark PickerView DataSource

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
  return 1;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component
{
  return (70-10)+1;
}

- (NSString *)pickerView:(UIPickerView *)pickerView
             titleForRow:(NSInteger)row
            forComponent:(NSInteger)component
{
  return [NSString stringWithFormat:@"%d Amps",10+row];
} 

#pragma mark -
#pragma mark PickerView Delegate

-(void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row
      inComponent:(NSInteger)component
{
}

#pragma mark -
#pragma mark UITextField Delegate

@end
