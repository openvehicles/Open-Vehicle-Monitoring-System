//
//  ovmsVehicleInfo.h
//  Open Vehicle
//
//  Created by Mark Webb-Johnson on 18/3/12.
//  Copyright (c) 2012 Open Vehicle Systems. All rights reserved.
//

#import <UIKit/UIKit.h>

@interface ovmsVehicleInfo : UIViewController

@property (strong, nonatomic) IBOutlet UITextField *m_vehicleid;
@property (strong, nonatomic) IBOutlet UITextField *m_vin;
@property (strong, nonatomic) IBOutlet UITextField *m_type;
@property (strong, nonatomic) IBOutlet UITextField *m_gsm;
@property (strong, nonatomic) IBOutlet UITextField *m_serverfirmware;
@property (strong, nonatomic) IBOutlet UITextField *m_carfirmware;
@property (strong, nonatomic) IBOutlet UITextField *m_appfirmware;
@property (strong, nonatomic) IBOutlet UIImageView *m_gsm_signalbars;

- (IBAction)Done:(id)sender;

@end
