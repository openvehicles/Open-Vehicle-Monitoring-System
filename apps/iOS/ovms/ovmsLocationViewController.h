//
//  ovmsLocationViewController.h
//  ovms
//
//  Created by Mark Webb-Johnson on 16/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ovmsAppDelegate.h"
#import <MapKit/MapKit.h>


@interface TeslaAnnotation : NSObject <MKAnnotation> {
  NSString *_name;
  NSString *_description;
  CLLocationCoordinate2D _coordinate;
}
@property (nonatomic, retain) NSString *name;
@property (nonatomic, retain) NSString *description;
@property (nonatomic, readonly) CLLocationCoordinate2D coordinate;

-(id) initWithCoordinate:(CLLocationCoordinate2D) coordinate;

@end

@interface ovmsLocationViewController : UIViewController <MKMapViewDelegate, ovmsLocationDelegate> 

@property (strong, nonatomic) IBOutlet MKMapView *myMapView;
@property (nonatomic, retain) TeslaAnnotation *m_car_location;

-(void)displayMYMap;

@end
