//
//  ovmsVehicleAnnotation.m
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 20/4/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import "ovmsVehicleAnnotation.h"

@implementation ovmsVehicleAnnotation

@synthesize myMapView;
@synthesize imageset;
@synthesize coordinate;
@synthesize title;
@synthesize subtitle;
@synthesize imagefile;
@synthesize direction;
@synthesize speed;
@synthesize groupcar;

-(id) initWithCoordinate:(CLLocationCoordinate2D) newCoordinate
  {
  self = [super init];
  
  if(self)
    {
    myMapView = nil;
    imageset = NO;
    coordinate = newCoordinate;
    title = @"";
    subtitle = @"";
    imagefile = @"";
    direction = -1;
    speed = @"";
    groupcar = NO;
    }
  
  return self;
  }

-(void)setCoordinate:(CLLocationCoordinate2D)newCoordinate
  {
  if ((newCoordinate.latitude != coordinate.latitude)&&
      (newCoordinate.longitude != coordinate.longitude))
    {
    coordinate=newCoordinate;
    }
  }

-(void)setTitle:(NSString *)newTitle
  {
  title = newTitle;
  }

-(void)setSubtitle:(NSString *)newSubtitle
  {
  subtitle = newSubtitle;
  }

-(void)setImagefile:(NSString *)newImagefile
  {
  if (! [imagefile isEqualToString:newImagefile])
    {
    imagefile = newImagefile;
    imageset = NO;
    [self updateAnnotationView];
    }
  }

-(void)setDirection:(int)newDirection
  {
  if (direction != newDirection)
    {
    direction = newDirection;
    [self updateAnnotationView];
    }
  }

-(void)setSpeed:(NSString*)newSpeed
  {
  if (![speed isEqualToString:newSpeed])
    {
    speed = newSpeed;
    [self setSubtitle:newSpeed];
    [self updateAnnotationView];
    }
  }

-(void)setGroupCar:(BOOL)newGroupCar
  {
  if (groupcar != newGroupCar)
    {
    groupcar = newGroupCar;
    [self updateAnnotationView];
    }
  }

-(void)setupView:(MKAnnotationView*)annotationView mapView:(MKMapView*)mapView;
  {
  if (mapView != nil)
    myMapView = mapView; // Store for later use

  if (!imageset)
    {
    NSString *carimage;
    carimage = [NSString stringWithFormat:@"map_%@",imagefile];
    annotationView.image=[UIImage imageNamed:carimage];
    annotationView.canShowCallout = YES;
    annotationView.contentMode = UIViewContentModeScaleAspectFill;
    if (groupcar)
      {
      annotationView.bounds = CGRectMake(0, 0, 32, 32);
      }
    else
      {
      annotationView.bounds = CGRectMake(0, 0, 48, 48);
      }
    imageset = YES;
    }
  // Simple rotational transformation...
  float rad = DEGREES_TO_RADIANS(direction);
  annotationView.transform = CGAffineTransformMakeRotation(rad);
  }

-(void)updateAnnotationView
  {
  if (myMapView == nil) return;
  
  MKAnnotationView* aView = [myMapView viewForAnnotation: self];
  if (aView != nil)
    {
    [self setupView:aView mapView:nil];
    }
  }

-(void)redrawView
  {
  imageset = NO;
  [self updateAnnotationView];
  }

-(void) dealloc
  {
  myMapView = nil;
  title = nil;
  subtitle = nil;
  imagefile = nil;
  }

@end
