//
//  ovmsLocationViewController.h
//  ovms
//
//  Created by Mark Webb-Johnson on 16/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <MapKit/MapKit.h>
#import "ovmsAppDelegate.h"
#import "ovmsVehicleAnnotation.h"

@interface ovmsLocationViewController : UIViewController <MKMapViewDelegate, ovmsUpdateDelegate> 

@property (strong, nonatomic) IBOutlet MKMapView *myMapView;
@property (nonatomic, retain) ovmsVehicleAnnotation *m_car_location;

@end
