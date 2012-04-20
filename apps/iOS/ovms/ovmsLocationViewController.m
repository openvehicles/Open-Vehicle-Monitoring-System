//
//  ovmsLocationViewController.m
//  ovms
//
//  Created by Mark Webb-Johnson on 16/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import "ovmsLocationViewController.h"

@implementation ovmsLocationViewController

@synthesize myMapView;
@synthesize m_car_location;

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
  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
  [NSThread detachNewThreadSelector:@selector(displayMYMap) toTarget:self withObject:nil];
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

  [[ovmsAppDelegate myRef] registerForUpdate:self];
  [self displayMYMap];
  }

- (void)viewDidAppear:(BOOL)animated
  {
  [super viewDidAppear:animated];
  }

- (void)viewWillDisappear:(BOOL)animated
  {
	[super viewWillDisappear:animated];
  [[ovmsAppDelegate myRef] deregisterFromUpdate:self];
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

-(void) update
  {
  // The car has reported updated information, and we may need to reflect that
  CLLocationCoordinate2D location = [ovmsAppDelegate myRef].car_location;
  
  MKCoordinateRegion region = myMapView.region;
  if ( (region.center.latitude != location.latitude)&&
      (region.center.longitude != location.longitude) )
    {
    if (self.m_car_location)
      {
      [UIView beginAnimations:@"ovmsVehicleAnnotationAnimation" context:nil];
      [UIView setAnimationBeginsFromCurrentState:YES];
      [UIView setAnimationDuration:5.0];
      [UIView setAnimationCurve:UIViewAnimationCurveLinear];
      [self.m_car_location setDirection:[ovmsAppDelegate myRef].car_direction%360];
      [self.m_car_location setCoordinate: location];
      region.center=location;
      [myMapView setRegion:region animated:NO];
      [UIView commitAnimations];
      //      [UIView setAnimationDidStopSelector:@selector(animationDidStop:finished:context:)];
      }
    else
      {
      // Remove all existing annotations
      for (int k=0; k < [myMapView.annotations count]; k++)
        { 
          if ([[myMapView.annotations objectAtIndex:k] isKindOfClass:[ovmsVehicleAnnotation class]])
            {
            [myMapView removeAnnotation:[myMapView.annotations objectAtIndex:k]];
            }
        }
      
      ovmsVehicleAnnotation *pa = [[ovmsVehicleAnnotation alloc] initWithCoordinate:location];
      [pa setTitle:@"EV915"];
      [pa setSubtitle:[NSString stringWithFormat:@"%f, %f", pa.coordinate.latitude, pa.coordinate.longitude]];
      [pa setImagefile:[ovmsAppDelegate myRef].sel_imagepath];
      [pa setDirection:([ovmsAppDelegate myRef].car_direction)%360];
      [myMapView addAnnotation:pa];
      self.m_car_location = pa;
      region.center=location;
      [myMapView setRegion:region animated:YES];
      }
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
      if ([[myMapView.annotations objectAtIndex:k] isKindOfClass:[ovmsVehicleAnnotation class]])
        {
        [myMapView removeAnnotation:[myMapView.annotations objectAtIndex:k]];
        }
    }

  if ((location.latitude != 0)||(location.longitude != 0))
    {
    // Add in the new annotation for current car location
    ovmsVehicleAnnotation *pa = [[ovmsVehicleAnnotation alloc] initWithCoordinate:location];
    [pa setTitle:@"EV915"];
    [pa setSubtitle:[NSString stringWithFormat:@"%f, %f", pa.coordinate.latitude, pa.coordinate.longitude]];
    [pa setImagefile:[ovmsAppDelegate myRef].sel_imagepath];
    [pa setDirection:([ovmsAppDelegate myRef].car_direction)%360];
    [myMapView addAnnotation:pa];
    self.m_car_location = pa;
    }
  
  [myMapView setRegion:region animated:YES]; 
  [myMapView regionThatFits:region]; 
  }

 - (MKAnnotationView *)mapView:(MKMapView *)mapView
                                viewForAnnotation:(id <MKAnnotation>)annotation
  {
  // Provide a custom view for the ovmsVehicleAnnotation
  
  static NSString *ovmsAnnotationIdentifier=@"OVMSAnnotationIdentifier";
  
  if ([annotation isKindOfClass:[MKUserLocation class]])
    return nil;
 
  if([annotation isKindOfClass:[ovmsVehicleAnnotation class]])
    {
    MKAnnotationView *annotationView=[[MKAnnotationView alloc] initWithAnnotation:annotation reuseIdentifier:ovmsAnnotationIdentifier];

    //Here's where the magic happens
    ovmsVehicleAnnotation *pa = (ovmsVehicleAnnotation*)annotation;
    [pa setupView:annotationView mapView:myMapView];
    return annotationView;
    }
  return nil;
  }

@end
