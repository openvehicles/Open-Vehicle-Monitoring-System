//
//  ovmsCarsFormViewController.m
//  ovms
//
//  Created by Mark Webb-Johnson on 23/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import "ovmsCarsFormViewController.h"
#import "Cars.h"

@implementation ovmsCarsFormViewController

@synthesize context = _context;
@synthesize carEditing = _carEditing;

@synthesize carImages;

@synthesize vehicleid;
@synthesize vehiclelabel;
@synthesize vehicleNetPass;
@synthesize vehicleUserPass;
@synthesize vehicleImage;

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
  self.carImages = [[NSMutableArray alloc] initWithCapacity:100];
  NSString *bundleRoot = [[NSBundle mainBundle] bundlePath];
  NSArray *dirContents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:bundleRoot error:nil];
  for (NSString *tString in dirContents)
    {
    if ([tString hasPrefix:@"car_"] && [tString hasSuffix:@".png"])
      {
      [self.carImages addObject: tString];
      }
    }
  _context = [ovmsAppDelegate myRef].managedObjectContext;
}

- (void)viewDidUnload
{
  [self setVehicleid:nil];
  [self setVehiclelabel:nil];
  [self setVehicleNetPass:nil];
  [self setVehicleUserPass:nil];
  [self setVehicleImage:nil];
  [self setContext:nil];

  [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

-(void) viewWillAppear:(BOOL)animated
{
  // only enable the vehicleid field if this is a NEW car
  if (self.carEditing==nil)
    {
    self.vehicleid.enabled = YES;
    self.title = @"New Car";
    }
  else
    {
    self.title = self.carEditing;
    self.vehicleid.text = self.carEditing;
    self.vehicleid.enabled = NO;

    NSManagedObjectContext *context = [ovmsAppDelegate myRef].managedObjectContext;
    NSFetchRequest *request = [[NSFetchRequest alloc] init];
    [request setEntity: [NSEntityDescription entityForName:@"Cars" inManagedObjectContext: context]];
    NSPredicate *predicate =
    [NSPredicate predicateWithFormat:@"vehicleid == %@", self.carEditing];
    [request setPredicate:predicate];
    NSError *error = nil;
    NSArray *array = [context executeFetchRequest:request error:&error];
    if (array != nil)
      {
      if ([array count]>0)
        {
        Cars* car = [array objectAtIndex:0];
        if (! [[ovmsAppDelegate myRef].sel_car isEqualToString:car.vehicleid])
          {
          // Switch the car
          [[ovmsAppDelegate myRef] switchCar:car.vehicleid];
          }
        self.vehiclelabel.text = car.label;
        self.vehicleNetPass.text = car.netpass;
        self.vehicleUserPass.text = car.userpass;
        NSString *imagepath = car.imagepath;
        for (int k=0;k<[carImages count];k++)
          {
          if ([[carImages objectAtIndex:k] isEqualToString:imagepath])
            {
            [vehicleImage selectRow:k inComponent:0 animated:NO];
            }
          }
        }
      }
    }
}

-(void) viewWillDisappear:(BOOL)animated {
  if ([self.navigationController.viewControllers indexOfObject:self]==NSNotFound) {
    // back button was pressed.  We know this is true because self is no longer
    // in the navigation stack.  
    if (self.carEditing==nil)
      {
      // We need to create a new car record
      if (([vehicleid.text length]==0)||
          ([vehiclelabel.text length]==0)||
          ([vehicleUserPass.text length]==0)||
          ([vehicleNetPass.text length]==0))
        {
        return;
        }
      NSError *error = nil;
      Cars *car = [NSEntityDescription
                   insertNewObjectForEntityForName:@"Cars"
                   inManagedObjectContext:_context];
      car.vehicleid = vehicleid.text;
      car.label = vehiclelabel.text;
      car.netpass = vehicleNetPass.text;
      car.userpass = vehicleUserPass.text;
      car.imagepath = [carImages objectAtIndex:[vehicleImage selectedRowInComponent:0]];
      if (![_context save:&error])
        {
        NSLog(@"Whoops, couldn't save: %@", [error localizedDescription]);
        }
      }
    else
      {
      // We need to update the existing car record
      NSManagedObjectContext *context = [ovmsAppDelegate myRef].managedObjectContext;
      NSFetchRequest *request = [[NSFetchRequest alloc] init];
      [request setEntity: [NSEntityDescription entityForName:@"Cars" inManagedObjectContext: context]];
      NSPredicate *predicate =
      [NSPredicate predicateWithFormat:@"vehicleid == %@", vehicleid.text];
      [request setPredicate:predicate];
      NSError *error = nil;
      NSArray *array = [context executeFetchRequest:request error:&error];
      if (array != nil)
        {
        if ([array count]>0)
          {
          NSError *error = nil;
          Cars* car = [array objectAtIndex:0];
          car.vehicleid = vehicleid.text;
          car.label = vehiclelabel.text;
          car.netpass = vehicleNetPass.text;
          car.userpass = vehicleUserPass.text;
          car.imagepath = [carImages objectAtIndex:[vehicleImage selectedRowInComponent:0]];
          if (![_context save:&error])
            {
            NSLog(@"Whoops, couldn't save: %@", [error localizedDescription]);
            }
          }
        }
      }
    // Switch the car
    [[ovmsAppDelegate myRef] switchCar:vehicleid.text];
  }
  [super viewWillDisappear:animated];
}

- (void)setCarEditing:(NSString *)newCarEditing
{
  _carEditing = newCarEditing;
}

#pragma mark -
#pragma mark PickerView DataSource

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView *)pickerView
{
  return 1;
}

- (NSInteger)pickerView:(UIPickerView *)pickerView numberOfRowsInComponent:(NSInteger)component
{
  return [carImages count];
}

- (CGFloat)pickerView:(UIPickerView *)pickerView rowHeightForComponent:(NSInteger)component
{
  return 151;
}

- (UIView *)pickerView:(UIPickerView *)pickerView viewForRow:(NSInteger)row
          forComponent:(NSInteger)component reusingView:(UIView *)view
{
  UIImageView *newview = [[UIImageView alloc] initWithImage:[UIImage imageNamed:[carImages objectAtIndex:row]]];
  newview.frame = CGRectMake(0, 0, 320, 151);
  newview.contentMode =  UIViewContentModeScaleAspectFit;
  return newview;
}

#pragma mark -
#pragma mark PickerView Delegate

-(void)pickerView:(UIPickerView *)pickerView didSelectRow:(NSInteger)row
      inComponent:(NSInteger)component
{
}

#pragma mark -
#pragma mark UITextField Delegate

-(BOOL) textFieldShouldReturn:(UITextField*) textField
{
  [textField resignFirstResponder]; 
  return YES;
}

@end
