//
//  ovmsStatusViewController.h
//  ovms
//
//  Created by Mark Webb-Johnson on 16/11/11.
//  Copyright (c) 2011 Hong Hay Villa. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ovmsAppDelegate.h"

@interface ovmsStatusViewController : UIViewController <ovmsStatusDelegate>

@property (strong, nonatomic) IBOutlet UIImageView *m_car_connection_image;
@property (strong, nonatomic) IBOutlet UILabel *m_car_connection_state;
@property (strong, nonatomic) IBOutlet UIImageView *m_car_image;
@property (strong, nonatomic) IBOutlet UILabel *m_car_charge_state;
@property (strong, nonatomic) IBOutlet UILabel *m_car_charge_type;
@property (strong, nonatomic) IBOutlet UILabel *m_car_soc;
@property (strong, nonatomic) IBOutlet UIImageView *m_battery_front;
@property (strong, nonatomic) IBOutlet UILabel *m_car_range;

@end
