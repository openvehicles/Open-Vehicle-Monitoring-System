//
//  ovmsVehicleAnnotation.h
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 20/4/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import <MapKit/MapKit.h>
#import <math.h>

#define DEGREES_TO_RADIANS(angle) ((angle) / 180.0 * M_PI)

@interface ovmsVehicleAnnotation : NSObject <MKAnnotation>

@property (nonatomic, readonly, retain) MKMapView* myMapView;
@property (nonatomic, readonly) BOOL imageset;
@property (nonatomic, readonly) CLLocationCoordinate2D coordinate;
@property (nonatomic, readonly, copy) NSString *title;
@property (nonatomic, readonly, copy) NSString *subtitle;
@property (nonatomic, readonly, copy) NSString *imagefile;
@property (nonatomic, readonly) int direction;
@property (nonatomic, readonly) int speed;
@property (nonatomic, readonly) BOOL groupcar;

-(id) initWithCoordinate:(CLLocationCoordinate2D)newCoordinate;
-(void)setCoordinate:(CLLocationCoordinate2D)newCoordinate;
-(void)setTitle:(NSString *)newTitle;
-(void)setSubtitle:(NSString *)newSubtitle;
-(void)setImagefile:(NSString *)newImagefile;
-(void)setDirection:(int)newDirection;
-(void)setSpeed:(int)newSpeed;
-(void)setGroupCar:(BOOL)newGroupCar;

-(void)setupView:(MKAnnotationView*)annotationView mapView:(MKMapView*)mapView;
-(void)updateAnnotationView;
-(void)redrawView;

@end
