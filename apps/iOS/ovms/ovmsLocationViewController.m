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
@synthesize m_groupcar_locations;
@synthesize m_locationsnap;
@synthesize m_autotrack;
@synthesize m_lastregion;

- (void)didReceiveMemoryWarning
  {
  [super didReceiveMemoryWarning];
  // Release any cached data, images, etc that aren't in use.
  }

#pragma mark - View lifecycle

- (void)viewDidLoad
  {
  [super viewDidLoad];
  }

- (void)viewDidUnload
  {
  [self setMyMapView:nil];
    [self setM_locationsnap:nil];
  [super viewDidUnload];
  }

- (void)viewWillAppear:(BOOL)animated
  {
  [super viewWillAppear:animated];
  self.navigationItem.title = [ovmsAppDelegate myRef].sel_label;
  
  self.m_car_location = nil;
  if (m_groupcar_locations == nil)
    m_groupcar_locations = [[NSMutableDictionary alloc] init];
  self.m_autotrack = YES;
  m_locationsnap.style = UIBarButtonItemStyleDone;

  [[ovmsAppDelegate myRef] registerForUpdate:self];
  [self update];
  }

- (void)viewDidAppear:(BOOL)animated
  {
  [super viewDidAppear:animated];
  }

- (void)viewWillDisappear:(BOOL)animated
  {
	[super viewWillDisappear:animated];
  
  // As we are about to disappear, remove all the car location objects

  // Remove all existing annotations
  for (int k=0; k < [myMapView.annotations count]; k++)
    { 
      if ([[myMapView.annotations objectAtIndex:k] isKindOfClass:[ovmsVehicleAnnotation class]])
        {
        [myMapView removeAnnotation:[myMapView.annotations objectAtIndex:k]];
        }
    }
  self.m_car_location = nil;
  if (m_groupcar_locations != nil)
    {
    [m_groupcar_locations removeAllObjects];
    m_groupcar_locations = nil;
    }

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

- (IBAction)locationSnapped:(id)sender
  {
  if (self.m_autotrack)
    {
    // Turn off autotrack
    self.m_autotrack = NO;
    m_locationsnap.style = UIBarButtonItemStyleBordered;
    }
  else
    {
    // Turn on autotrack
    self.m_autotrack = YES;
    m_locationsnap.style = UIBarButtonItemStyleDone;
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
      [self.m_car_location setSpeed:[ovmsAppDelegate myRef].car_speed];
      [self.m_car_location setCoordinate: location];
      if (self.m_autotrack)
        {
        region.center=location;
        [myMapView setRegion:region animated:NO];
        }
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
      
      // Create the vehicle annotation
      ovmsVehicleAnnotation *pa = [[ovmsVehicleAnnotation alloc] initWithCoordinate:location];
      [pa setTitle:[ovmsAppDelegate myRef].sel_car];
      [pa setSubtitle:[NSString stringWithFormat:@"%f, %f", pa.coordinate.latitude, pa.coordinate.longitude]];
      [pa setImagefile:[ovmsAppDelegate myRef].sel_imagepath];
      [pa setDirection:([ovmsAppDelegate myRef].car_direction)%360];
      [pa setSpeed:[ovmsAppDelegate myRef].car_speed];
      [myMapView addAnnotation:pa];
      self.m_car_location = pa;
      
      // Setup the map to surround the vehicle
      MKCoordinateSpan span; 
      span.latitudeDelta=0.01; 
      span.longitudeDelta=0.01; 
      region.span=span; 
      region.center=location;
      [myMapView setRegion:region animated:YES];
      [myMapView regionThatFits:region]; 
      }
    }
  }

-(void) groupUpdate:(NSArray*)result
  {
  if (m_groupcar_locations == nil) return;

  if ([result count]>=10)
    {
    NSString *vehicleid = [result objectAtIndex:0];
    //NSString *groupid = [result objectAtIndex:1];
    //int soc = [[result objectAtIndex:2] intValue];
    int speed = [[result objectAtIndex:3] intValue];
    int direction = [[result objectAtIndex:4] intValue];
    //int altitude = [[result objectAtIndex:5] intValue];
    int gpslock = [[result objectAtIndex:6] intValue];
    int stalegps = [[result objectAtIndex:7] intValue];
    CLLocationCoordinate2D location = CLLocationCoordinate2DMake(
                                        [[result objectAtIndex:8] doubleValue], 
                                        [[result objectAtIndex:9] doubleValue]);

    if ((gpslock < 1)||(stalegps<1)) return; // No GPS lock or data
    if ([vehicleid isEqualToString:[ovmsAppDelegate myRef].sel_car]) return; // Not the selected car 

    NSLog(@"groupUpdate for %@", vehicleid);
    ovmsVehicleAnnotation *pa = [m_groupcar_locations objectForKey:vehicleid];
    if (pa != nil)
      {
      // Update an existing car
      [pa setCoordinate:location];
      [pa setTitle:vehicleid];
      [pa setSubtitle:[NSString stringWithFormat:@"%f, %f", pa.coordinate.latitude, pa.coordinate.longitude]];
      [pa setImagefile:@"connection_good.png"];
      [pa setDirection:direction];
      [pa setSpeed:speed];
      }
    else
      {
      // Create a new car
      pa = [[ovmsVehicleAnnotation alloc] initWithCoordinate:location];
      [pa setGroupCar:YES];
      [pa setTitle:vehicleid];
      [pa setSubtitle:[NSString stringWithFormat:@"%f, %f", pa.coordinate.latitude, pa.coordinate.longitude]];
      [pa setImagefile:@"car_default.png"];
      [pa setDirection:direction%360];
      [pa setSpeed:speed];
      [m_groupcar_locations setObject:pa forKey:vehicleid];
      [myMapView addAnnotation:pa];
      NSLog(@"groupCarCreated %@ count=%d", vehicleid,[[myMapView annotations] count]);
      }
    }  
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
    NSLog(@"setupView for %@", pa.title);
    [pa setupView:annotationView mapView:myMapView];
    return annotationView;
    }
  return nil;
  }

- (void)mapView:(MKMapView *)mapView regionWillChangeAnimated:(BOOL)animated
  {
  m_lastregion = mapView.region;
  }

- (void)mapView:(MKMapView *)mapView regionDidChangeAnimated:(BOOL)animated
  {
  MKCoordinateRegion newRegion = mapView.region;
  if (((m_lastregion.span.latitudeDelta/newRegion.span.latitudeDelta) > 1.5)||
      ((m_lastregion.span.latitudeDelta/newRegion.span.latitudeDelta) < 0.75))
    {
    // Kludgy redraw of all annotations, by removing then replacing the annotations...
    for (int k=0; k < [myMapView.annotations count]; k++)
      {
      ovmsVehicleAnnotation *pa = [myMapView.annotations objectAtIndex:k];
      if ([pa isKindOfClass:[ovmsVehicleAnnotation class]])
        {
        [myMapView removeAnnotation:[myMapView.annotations objectAtIndex:k]];
        }
      }
    if (m_car_location != nil)
      {
      [myMapView addAnnotation:m_car_location];
      [m_car_location redrawView];
      }
    NSEnumerator *enumerator = [m_groupcar_locations objectEnumerator];
    ovmsVehicleAnnotation *pa;    
    while ((pa = [enumerator nextObject]))
      {
      [myMapView addAnnotation:pa];
      [pa redrawView];
      }
    }
  }

-(void)zoomToFitMapAnnotations:(MKMapView*)mapView
  {
  if([mapView.annotations count] == 0)
    return;

  CLLocationCoordinate2D topLeftCoord;
  topLeftCoord.latitude = -90;
  topLeftCoord.longitude = 180;
  
  CLLocationCoordinate2D bottomRightCoord;
  bottomRightCoord.latitude = 90;
  bottomRightCoord.longitude = -180;

  for(ovmsVehicleAnnotation* annotation in mapView.annotations)
    {
    topLeftCoord.longitude = fmin(topLeftCoord.longitude, annotation.coordinate.longitude);
    topLeftCoord.latitude = fmax(topLeftCoord.latitude, annotation.coordinate.latitude);
    
    bottomRightCoord.longitude = fmax(bottomRightCoord.longitude, annotation.coordinate.longitude);
    bottomRightCoord.latitude = fmin(bottomRightCoord.latitude, annotation.coordinate.latitude);
    }

  MKCoordinateRegion region;
  region.center.latitude = topLeftCoord.latitude - (topLeftCoord.latitude - bottomRightCoord.latitude) * 0.5;
  region.center.longitude = topLeftCoord.longitude + (bottomRightCoord.longitude - topLeftCoord.longitude) * 0.5;
  region.span.latitudeDelta = fabs(topLeftCoord.latitude - bottomRightCoord.latitude) * 1.1; // Add a little extra space on the sides
  region.span.longitudeDelta = fabs(bottomRightCoord.longitude - topLeftCoord.longitude) * 1.1; // Add a little extra space on the sides

  region = [mapView regionThatFits:region];
  [mapView setRegion:region animated:YES];
  }

@end
