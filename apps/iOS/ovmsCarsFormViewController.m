//
//  ovmsCarsFormViewController.m
//  ovms
//
//  Created by Mark Webb-Johnson on 23/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import "ovmsCarsFormViewController.h"

@implementation ovmsCarsFormViewController

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
}

- (void)viewDidUnload
{
  [self setVehicleid:nil];
  [self setVehiclelabel:nil];
  [self setVehicleNetPass:nil];
  [self setVehicleUserPass:nil];
  [self setVehicleImage:nil];
  [super viewDidUnload];
    // Release any retained subviews of the main view.
    // e.g. self.myOutlet = nil;
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    // Return YES for supported orientations
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

- (IBAction)textFieldReturn:(id)sender
{
  [sender resignFirstResponder];
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

@end
