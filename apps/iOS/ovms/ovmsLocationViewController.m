//
//  ovmsLocationViewController.m
//  ovms
//
//  Created by Mark Webb-Johnson on 16/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import "ovmsLocationViewController.h"

@implementation TeslaAnnotation

@synthesize name = _name;
@synthesize description = _description;
@synthesize coordinate = _coordinate;

-(id) initWithCoordinate:(CLLocationCoordinate2D) coordinate{
  self=[super init];
  if(self){
    _coordinate=coordinate;
  }
  return self;
}

-(void) dealloc{
  self.name = nil;
  self.description = nil;
}

@end

@implementation ovmsLocationViewController

@synthesize myMapView;
@synthesize m_car_location;

- (void)didReceiveMemoryWarning
{
  [super didReceiveMemoryWarning];
  // Release any cached data, images, etc that aren't in use.
}

-(void) updateLocation
{
  CLLocationCoordinate2D location = [ovmsAppDelegate myRef].car_location;
  
  MKCoordinateRegion region = myMapView.region;
  if ( (region.center.latitude != location.latitude)&&
       (region.center.longitude != location.longitude) )
    {
    // Remove all existing annotations
    for (int k=0; k < [myMapView.annotations count]; k++)
      { 
      if ([[myMapView.annotations objectAtIndex:k] isKindOfClass:[TeslaAnnotation class]])
        {
        [myMapView removeAnnotation:[myMapView.annotations objectAtIndex:k]];
        }
      }
    
    TeslaAnnotation *pa = [[TeslaAnnotation alloc] initWithCoordinate:location];
    pa.name = @"EV915";
    pa.description = [NSString stringWithFormat:@"%f, %f", pa.coordinate.latitude, pa.coordinate.longitude];
    [myMapView addAnnotation:pa];
    self.m_car_location = pa;

    region.center=location;
    [myMapView setRegion:region animated:YES];
    }
}

#pragma mark - View lifecycle

- (void)viewDidLoad
{
  [super viewDidLoad];
  
	// Do any additional setup after loading the view, typically from a nib.
  [NSThread detachNewThreadSelector:@selector(displayMYMap) toTarget:self withObject:nil]; 
  [ovmsAppDelegate myRef].location_delegate = self;
  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
}

- (void)viewDidUnload
{
  [self setMyMapView:nil];
  [super viewDidUnload];
}

- (void)viewWillAppear:(BOOL)animated
{
  [super viewWillAppear:animated];
  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
  [self displayMYMap];
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

-(void)displayMYMap
{
  MKCoordinateRegion region; 
  MKCoordinateSpan span; 
  span.latitudeDelta=0.01; 
  span.longitudeDelta=0.01; 

  CLLocationCoordinate2D location = [ovmsAppDelegate myRef].car_location;
    
  region.span=span; 
  region.center=location; 

  // Remove all existing annotations
  for (int k=0; k < [myMapView.annotations count]; k++)
    { 
      if ([[myMapView.annotations objectAtIndex:k] isKindOfClass:[TeslaAnnotation class]])
        {
        [myMapView removeAnnotation:[myMapView.annotations objectAtIndex:k]];
        }
    }

  if ((location.latitude != 0)||(location.longitude != 0))
    {
    // Add in the new annotation for current car location
    TeslaAnnotation *pa = [[TeslaAnnotation alloc] initWithCoordinate:location];
    pa.name = @"EV915";
    pa.description = [NSString stringWithFormat:@"%f, %f", pa.coordinate.latitude, pa.coordinate.longitude];
    [myMapView addAnnotation:pa];
    self.m_car_location = pa;
    }
  
  [myMapView setRegion:region animated:YES]; 
  [myMapView regionThatFits:region]; 
}

 - (MKAnnotationView *)mapView:(MKMapView *)mapView
 viewForAnnotation:(id <MKAnnotation>)annotation
  {
  static NSString *teslaAnnotationIdentifier=@"TeslaAnnotationIdentifier";
  
 if ([annotation isKindOfClass:[MKUserLocation class]])
 return nil;
 
  if([annotation isKindOfClass:[TeslaAnnotation class]]){
    //Try to get an unused annotation, similar to uitableviewcells
//    MKAnnotationView *annotationView=[mapView dequeueReusableAnnotationViewWithIdentifier:teslaAnnotationIdentifier];
    //If one isn't available, create a new one
//    if(!annotationView){
      MKAnnotationView *annotationView=[[MKAnnotationView alloc] initWithAnnotation:annotation reuseIdentifier:teslaAnnotationIdentifier];
      //Here's where the magic happens
      annotationView.image=[UIImage imageNamed:[ovmsAppDelegate myRef].sel_imagepath];
      annotationView.contentMode = UIViewContentModeScaleAspectFill;
      annotationView.bounds = CGRectMake(0, 0, 32, 32);
//    }
    return annotationView;
  }
  return nil;
}

@end
